// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "pch.h"

#include "../include/api.h"
#include "handle_pool.h"
#include "log_helpers.h"
#include "native_renderer.h"

/// Callback fired when a local or remote (depending on use) video frame is
/// available to be consumed by the caller, usually for display.
/// The video frame is encoded in I420 triplanar format (NV12).
using PeerConnectionI420AVideoFrameCallback =
    void(MRS_CALL*)(void* user_data, const mrsI420AVideoFrame& frame);

extern "C" MRS_API void MRS_CALL
mrsPeerConnectionRegisterI420ARemoteVideoFrameCallback(
    PeerConnectionHandle peerHandle,
    PeerConnectionI420AVideoFrameCallback callback,
    void* user_data) noexcept;

#pragma warning(disable : 4302 4311 4312)

// Mutex locking hierarchy. You may nest locks in this order only. Never go the
// other way. You don't necessarily have to  have a higher-order guard in place
// to lock a lower one, but once a lower one is locked, a higher one must not be
// subsequently locked.
//  1. g_lock -- Global lock (file-level)
//  2. s_lock -- Static lock (class-level)
//  3. m_lock -- Local lock (instance-level)

static std::mutex g_lock;
static HandlePool<NativeRenderer> g_renderHandlePool;
static std::shared_ptr<RenderApi> g_renderApi;
static std::vector<NativeRendererHandle> g_videoUpdateQueue;

void I420VideoFrame::CopyFrame(const mrsI420AVideoFrame& frame) {
  width = frame.width_;
  height = frame.height_;
  ystride = frame.ystride_;
  ustride = frame.ustride_;
  vstride = frame.vstride_;
  size_t ysize = (size_t)ystride * height;
  size_t usize = (size_t)ustride * height / 2;
  size_t vsize = (size_t)vstride * height / 2;
  ybuffer.resize(ysize);
  ubuffer.resize(usize);
  vbuffer.resize(vsize);
  memcpy(ybuffer.data(), frame.ydata_, ysize);
  memcpy(ubuffer.data(), frame.udata_, usize);
  memcpy(vbuffer.data(), frame.vdata_, vsize);
}

NativeRendererHandle NativeRenderer::Create(PeerConnectionHandle peerHandle) {
  auto renderer =
      std::shared_ptr<NativeRenderer>(new NativeRenderer(peerHandle));
  {
    {
      // Global lock
      std::lock_guard guard(g_lock);

      auto handle = g_renderHandlePool.bind(renderer);

      if (handle == nullptr) {
        return nullptr;
      }

      renderer->m_handle = handle;
      return handle;
    }
  }
}

void NativeRenderer::Destroy(NativeRendererHandle handle) {
  std::shared_ptr<NativeRenderer> renderer;

  {
    // Global lock
    std::lock_guard guard(g_lock);
    renderer = g_renderHandlePool.unbind(handle);
  }

  if (renderer) {
    renderer->Shutdown();
  }
}

std::shared_ptr<NativeRenderer> NativeRenderer::Get(
    NativeRendererHandle handle) {
  {
    // Global lock
    std::lock_guard guard(g_lock);
    return g_renderHandlePool.get(handle);
  }
}

std::vector<std::shared_ptr<NativeRenderer>> NativeRenderer::MultiGet(
    const std::vector<NativeRendererHandle>& handles) {
  std::vector<std::shared_ptr<NativeRenderer>> renderers;
  {
    {
      // Global lock
      std::lock_guard guard(g_lock);
      for (auto handle : handles) {
        renderers.push_back(g_renderHandlePool.get(handle));
      }
    }
  }
  return std::move(renderers);
}

NativeRenderer::NativeRenderer(PeerConnectionHandle peerHandle)
    : m_peerHandle(peerHandle), m_handle(nullptr) {}

NativeRenderer::~NativeRenderer() {
  Log_Debug("NativeRenderer::~NativeRenderer");
}

void NativeRenderer::Shutdown() {
  Log_Debug("NativeRenderer::Shutdown");
  UnregisterRemoteTextures();
}

void NativeRenderer::RegisterRemoteTextures(VideoKind format,
                                            TextureDesc textDescs[],
                                            int textureCount) {
  Log_Debug("NativeRenderer::RegisterRemoteTextures: %p, %p, %p",
            textDescs[0].texture, textDescs[1].texture, textDescs[2].texture);
  {
    // Instance lock
    std::lock_guard guard(m_lock);
    m_remoteVideoFormat = format;
    switch (format) {
      case VideoKind::kI420:
        if (textureCount == 3) {
          m_remoteTextures.resize(3);
          m_remoteTextures[0] = textDescs[0];
          m_remoteTextures[1] = textDescs[1];
          m_remoteTextures[2] = textDescs[2];
          mrsPeerConnectionRegisterI420ARemoteVideoFrameCallback(
              m_peerHandle, NativeRenderer::I420ARemoteVideoFrameCallback,
              m_handle);
        }
        break;

      case VideoKind::kARGB:
      case VideoKind::kNone:
        // TODO
        break;
    }
  }
}

void NativeRenderer::UnregisterRemoteTextures() {
  Log_Debug("NativeRenderer::UnregisterRemoteTextures");
  {
    {
      // Instance lock
      std::lock_guard guard(m_lock);
      m_remoteTextures.clear();
      m_remoteVideoFormat = VideoKind::kNone;
    }
  }
}

void NativeRenderer::I420ARemoteVideoFrameCallback(
    void* user_data,
    const mrsI420AVideoFrame& frame) {
  if (auto renderer = NativeRenderer::Get(user_data)) {
    // TODO: Do we need to keep a frame queue or is it fine to just render the
    // most recent frame?

    // Copy the video frame.
    {
      // Instance lock
      std::lock_guard guard(renderer->m_lock);
      if (renderer->m_nextI420RemoteVideoFrame == nullptr) {
        if (renderer->m_freeI420VideoFrames.size()) {
          renderer->m_nextI420RemoteVideoFrame =
              renderer->m_freeI420VideoFrames.back();
          renderer->m_freeI420VideoFrames.pop_back();
        } else {
          renderer->m_nextI420RemoteVideoFrame =
              std::make_shared<I420VideoFrame>();
        }
      }
      renderer->m_nextI420RemoteVideoFrame->CopyFrame(frame);
    }

    // Register for the next video update.
    {
      // Global lock
      std::lock_guard guard(g_lock);
      // Queue for texture render, unless already queued.
      intptr_t slot = (intptr_t)renderer->m_handle & 0xffff;
      if (g_videoUpdateQueue.size() <= slot) {
        g_videoUpdateQueue.resize((size_t)slot + 1);
      }
      g_videoUpdateQueue[slot] = renderer->m_handle;
    }
  }
}

void MRS_CALL NativeRenderer::DoVideoUpdate() {
  if (!g_renderApi)
    return;

  // Render current frame of all queued NativeRenderers.
  std::vector<NativeRendererHandle> videoUpdateQueue;
  {
    // Global lock
    std::lock_guard guard(g_lock);
    // Copy and zero out the video update queue.
    videoUpdateQueue = g_videoUpdateQueue;
    for (size_t i = 0; i < g_videoUpdateQueue.size(); ++i) {
      g_videoUpdateQueue[i] = nullptr;
    }
  }

  // Gather the queued renderer instances (this is a sparse array).
  auto renderers = NativeRenderer::MultiGet(videoUpdateQueue);

  for (auto renderer : renderers) {
    // TODO: Support ARGB format.
    // TODO: Support local video.
    if (!renderer)
      continue;

    std::vector<TextureDesc> textures;
    std::shared_ptr<I420VideoFrame> remoteI420Frame;
    {
      // Instance lock
      std::lock_guard guard(renderer->m_lock);
      // Copy the remote textures and current video frame.
      textures = renderer->m_remoteTextures;
      remoteI420Frame = renderer->m_nextI420RemoteVideoFrame;
      renderer->m_nextI420RemoteVideoFrame = nullptr;
    }

    if (textures.size() < 3)
      continue;

    if (!remoteI420Frame)
      continue;

#if 0
    // Validate the frame has real data, changing over time.
    uint64_t yhash = 0, uhash = 0, vhash = 0;
    for (auto value : remoteI420Frame->ybuffer)
      yhash += value;
    for (auto value : remoteI420Frame->ubuffer)
      uhash += value;
    for (auto value : remoteI420Frame->vbuffer)
      vhash += value;
    Log_Debug("yhash: %llx, uhash: %llx, vhash: %llx", yhash, uhash, vhash);
#endif

    {
      int index = 0;
      for (const TextureDesc& textureDesc : textures) {
#if 1
        const std::vector<uint8_t>& src = remoteI420Frame->GetBuffer(index);
        g_renderApi->SimpleUpdateTexture(textureDesc.texture, textureDesc.width,
                                         textureDesc.height, src.data(),
                                         src.size());
#else
        // WARNING!!! There is an egregious memory leak somewhere in
        // this code block. I suspect it is in the render API.
        VideoDesc videoDesc = {VideoFormat::R8, (uint32_t)textureDesc.width,
                               (uint32_t)textureDesc.height};
        RenderApi::TextureUpdate update;
        if (g_renderApi->BeginModifyTexture(videoDesc, &update)) {
          int copyPitch = std::min<int>(videoDesc.width, update.rowPitch);
          uint8_t* dst = static_cast<uint8_t*>(update.data);
          const uint8_t* src = remoteI420Frame->GetBuffer(index).data();
          for (int32_t r = 0; r < textureDesc.height; ++r) {
            memcpy(dst, src, copyPitch);
            dst += update.rowPitch;
            src += textureDesc.width;
          }
          g_renderApi->EndModifyTexture(textureDesc.texture, update, videoDesc);
        }
#endif
        ++index;
      }
    }

    // Recycle the frame
    {
      // Instance lock
      std::lock_guard guard(renderer->m_lock);
      renderer->m_freeI420VideoFrames.push_back(remoteI420Frame);
    }
  }
}

void NativeRenderer::OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType,
                                           UnityGfxRenderer deviceType,
                                           IUnityInterfaces* unityInterfaces) {
  if (eventType == kUnityGfxDeviceEventInitialize) {
    g_renderApi = CreateRenderApi(deviceType);
  } else if (eventType == kUnityGfxDeviceEventShutdown) {
    g_renderApi = nullptr;
  }

  if (g_renderApi) {
    g_renderApi->ProcessDeviceEvent(eventType, unityInterfaces);
  }
}
