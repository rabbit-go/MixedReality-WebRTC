// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "pch.h"

#include "api.h"
#include "native_renderer.h"

#pragma warning(disable : 4302 4311 4312)

// Mutex locking hierarchy. You may nest locks in this order only. Never go the
// other way. You don't necessarily have to  have a higher-order guard in place
// to lock a lower one, but once a lower one is locked, a higher one must not be
// subsequently locked.
//  1. g_lock -- Global lock (file-level)
//  2. s_lock -- Static lock (class-level)
//  3. m_lock -- Local lock (instance-level)

static std::mutex g_lock;
static std::vector<std::shared_ptr<NativeRenderer>> g_instances;
static std::vector<int> g_generations;
static std::vector<int> g_freeSlots;
static std::shared_ptr<RenderApi> g_renderApi;
static std::vector<NativeRendererHandle> g_videoUpdateQueue;

void I420AVideoFrame::CopyFrame(const void* yptr,
                                const void* uptr,
                                const void* vptr,
                                const int ystride_,
                                const int ustride_,
                                const int vstride_,
                                const int width_,
                                const int height_) {
  width = width_;
  height = height_;
  ystride = ystride_;
  ustride = ustride_;
  vstride = vstride_;
  size_t ysize = (size_t)ystride * height;
  size_t usize = (size_t)ustride * height / 2;
  size_t vsize = (size_t)vstride * height / 2;
  ybuffer.resize(ysize);
  ubuffer.resize(usize);
  vbuffer.resize(vsize);
  memcpy(ybuffer.data(), yptr, ysize);
  memcpy(ubuffer.data(), uptr, usize);
  memcpy(vbuffer.data(), vptr, vsize);
}

NativeRendererHandle NativeRenderer::Create(PeerConnectionHandle peerHandle) {
  NativeRenderer* renderer = new NativeRenderer(peerHandle);
  {
    // Handle format is:
    //  High 16 bits: generation
    //  Low 16 bits : slot

    // Global lock
    std::lock_guard guard(g_lock);
    if (g_instances.size() >= 0x10000 && g_freeSlots.size() == 0) {
      return nullptr;
    }
    int slot;
    if (g_freeSlots.size()) {
      // Use a free slot.
      slot = g_freeSlots.back();
      g_freeSlots.pop_back();
    } else {
      // Allocate a new slot.
      slot = (int)g_instances.size();
      g_instances.resize(g_instances.size() + 1);
    }
    slot &= 0xffff;
    // Increment the generation of this slot. This is a guard against stale
    // handles pointing to newer instances occupying a recycled slot.
    g_generations.resize(g_instances.size());
    g_generations[slot] = (g_generations[slot] + 1) & 0xffff;
    // Generation cannot be zero.
    if (!g_generations[slot]) {
      g_generations[slot]++;
    }
    int gen = g_generations[slot];
    NativeRendererHandle handle = (NativeRendererHandle)((gen << 16) | slot);
    renderer->m_myHandle = handle;
    g_instances[slot] = std::shared_ptr<NativeRenderer>(renderer);
    return handle;
  }
}

void NativeRenderer::Destroy(NativeRendererHandle handle) {
  std::shared_ptr<NativeRenderer> renderer;
  {
    // Global lock
    std::lock_guard guard(g_lock);
    int slot = (int)handle & 0xffff;
    int gen = ((int)handle >> 16) & 0xffff;
    if (g_generations.size() > slot && g_generations[slot] == gen) {
      renderer = g_instances[slot];
      // Clear the shared_ptr holding a reference to the NativeRenderer. Once
      // all other references go away, the object will be deleted.
      g_instances[slot] = nullptr;
      g_freeSlots.push_back(slot);
    }
  }
  if (renderer) {
    renderer->Shutdown();
  }
}

std::shared_ptr<NativeRenderer> NativeRenderer::Get(
    NativeRendererHandle handle) {
  {
    std::lock_guard guard(g_lock);
    int slot = (int)handle & 0xffff;
    int gen = ((int)handle >> 16) & 0xffff;
    if (g_generations.size() > slot && g_generations[slot] == gen) {
      return g_instances[slot];
    } else {
      return nullptr;
    }
  }
}

std::vector<std::shared_ptr<NativeRenderer>> NativeRenderer::MultiGet(
    const std::vector<NativeRendererHandle>& handles) {
  std::vector<std::shared_ptr<NativeRenderer>> renderers;
  {
    std::lock_guard guard(g_lock);
    for (auto handle : handles) {
      int slot = (int)handle & 0xffff;
      int gen = ((int)handle >> 16) & 0xffff;
      if (g_generations.size() > slot && g_generations[slot] == gen) {
        renderers.push_back(g_instances[slot]);
      } else {
        renderers.push_back(nullptr);
      }
    }
  }
  return std::move(renderers);
}

NativeRenderer::NativeRenderer(PeerConnectionHandle peerHandle)
    : m_peerHandle(peerHandle), m_myHandle(nullptr) {}

NativeRenderer::~NativeRenderer() {}

void NativeRenderer::Shutdown() {
  UnregisterRemoteTextures();
}

void NativeRenderer::RegisterRemoteTextures(VideoKind format,
                                            TextureDesc textDescs[],
                                            int textureCount) {
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
        mrsPeerConnectionRegisterI420RemoteVideoFrameCallback(
            m_peerHandle, NativeRenderer::I420RemoteVideoFrameCallback,
            m_myHandle);
      }
      break;
  }
}

void NativeRenderer::UnregisterRemoteTextures() {
  {
    // Instance lock
    std::lock_guard guard(m_lock);
    m_remoteTextures.clear();
    m_remoteVideoFormat = VideoKind::kNone;
  }
}

void NativeRenderer::I420RemoteVideoFrameCallback(void* user_data,
                                                  const void* yptr,
                                                  const void* uptr,
                                                  const void* vptr,
                                                  const void* aptr,
                                                  const int ystride,
                                                  const int ustride,
                                                  const int vstride,
                                                  const int astride,
                                                  const int frame_width,
                                                  const int frame_height) {
  UNREFERENCED_PARAMETER(aptr);
  UNREFERENCED_PARAMETER(astride);

  if (auto renderer = NativeRenderer::Get(user_data)) {
    // TODO: Do we need to keep a frame queue or is it fine to just render the
    // most recent frame?

    // Copy the video frame.
    {
      // Instance lock
      std::lock_guard guard(renderer->m_lock);
      renderer->m_I420RemoteVideoFrame.CopyFrame(yptr, uptr, vptr, ystride,
                                                 ustride, vstride, frame_width,
                                                 frame_height);
    }
    // Register for the next video update.
    {
      // Global lock
      std::lock_guard guard(g_lock);
      // Queue for texture render, unless already queued.
      int slot = (int)renderer->m_myHandle & 0xffff;
      if (g_videoUpdateQueue.size() <= slot) {
        g_videoUpdateQueue.resize((size_t)slot + 1);
      }
      g_videoUpdateQueue[slot] = renderer->m_myHandle;
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

  // Gather the renderer instances (this is a sparse array).
  auto renderers = NativeRenderer::MultiGet(videoUpdateQueue);

  for (auto renderer : renderers) {
    // TODO: Support ARGB format.
    // TODO: Support local video.
    if (!renderer)
      continue;

    std::vector<TextureDesc> textures;
    I420AVideoFrame remoteI420Frame;
    {
      // Instance lock
      std::lock_guard guard(renderer->m_lock);
      // Copy the remote textures and current video frame.
      textures = renderer->m_remoteTextures;
      remoteI420Frame = renderer->m_I420RemoteVideoFrame;
    }

    {
      int index = 0;
      for (const TextureDesc& textureDesc : textures) {
        VideoDesc videoDesc = {VideoFormat::R8, (uint32_t)textureDesc.width,
                               (uint32_t)textureDesc.height};
        /*
        RenderApi::TextureUpdate update;
        if (g_renderApi->BeginModifyTexture(videoDesc, &update)) {
          int copyPitch = std::min<int>(videoDesc.width, update.rowPitch);
          uint8_t* dst = static_cast<uint8_t*>(update.data);
          const uint8_t* src = remoteI420Frame.GetBuffer(index).data();
          for (int32_t r = 0; r < textureDesc.height; ++r) {
            memcpy(dst, src, copyPitch);
            dst += update.rowPitch;
            src += textureDesc.width;
          }
          g_renderApi->EndModifyTexture(textureDesc.texture, update, videoDesc);
        }
        */
        const std::vector<uint8_t>& src = remoteI420Frame.GetBuffer(index);
        g_renderApi->SimpleUpdateTexture(textureDesc.texture, textureDesc.width,
                                         textureDesc.height, src.data(),
                                         src.size());
        ++index;
      }
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
