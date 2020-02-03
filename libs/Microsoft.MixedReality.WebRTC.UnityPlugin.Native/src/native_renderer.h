// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <mutex>
#include <vector>

#include "render_api.h"

using mrsI420AVideoFrame = Microsoft::MixedReality::WebRTC::I420AVideoFrame;

struct I420VideoFrame {
  int width{0};
  int height{0};
  int ystride{0};
  int ustride{0};
  int vstride{0};
  std::vector<uint8_t> ybuffer;
  std::vector<uint8_t> ubuffer;
  std::vector<uint8_t> vbuffer;

  void CopyFrame(const mrsI420AVideoFrame& frame);

  const std::vector<uint8_t>& GetBuffer(int i) {
    switch (i) {
      case 0:
        return ybuffer;
      case 1:
        return ubuffer;
      case 2:
        return vbuffer;
      default:
        // TODO: Fix.
        throw "aaah!";
    }
  }
};

class NativeRenderer {
 public:
  static NativeRendererHandle Create(PeerConnectionHandle peerHandle);
  static void Destroy(NativeRendererHandle handle);
  static std::shared_ptr<NativeRenderer> Get(NativeRendererHandle handle);
  static std::vector<std::shared_ptr<NativeRenderer>> MultiGet(
      const std::vector<NativeRendererHandle>& handles);

  ~NativeRenderer();

  NativeRendererHandle GetNativeHandle() const { return m_handle; }
  void RegisterRemoteTextures(VideoKind format,
                              TextureDesc textDescs[],
                              int textureCount);
  void UnregisterRemoteTextures();

  static void MRS_CALL DoVideoUpdate();
  static void OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType,
                                    UnityGfxRenderer deviceType,
                                    IUnityInterfaces* unityInterfaces);

 private:
  NativeRendererHandle m_handle;
  std::mutex m_lock;
  void* m_peerHandle{nullptr};
  std::vector<TextureDesc> m_remoteTextures;
  VideoKind m_remoteVideoFormat{VideoKind::kNone};
  std::shared_ptr<I420VideoFrame> m_nextI420RemoteVideoFrame;
  // REVIEW: Free frames could be kept in a global queue.
  std::vector<std::shared_ptr<I420VideoFrame>> m_freeI420VideoFrames;
  
  NativeRenderer(PeerConnectionHandle peerHandle);
  void Shutdown();

  static void MRS_CALL
  I420ARemoteVideoFrameCallback(void* user_data,
                                const mrsI420AVideoFrame& frame);
};
