// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <mutex>
#include <vector>

#include "render_api.h"

struct I420AVideoFrame {
  int width{0};
  int height{0};
  int ystride{0};
  int ustride{0};
  int vstride{0};
  std::vector<uint8_t> ybuffer;
  std::vector<uint8_t> ubuffer;
  std::vector<uint8_t> vbuffer;

  void CopyFrame(const void* yptr,
                 const void* uptr,
                 const void* vptr,
                 const int ystride,
                 const int ustride,
                 const int vstride,
                 const int width,
                 const int height);

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

  NativeRendererHandle GetNativeHandle() const { return m_myHandle; }
  void RegisterRemoteTextures(VideoKind format,
                              TextureDesc textDescs[],
                              int textureCount);
  void UnregisterRemoteTextures();

  static void MRS_CALL DoVideoUpdate();
  static void OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType,
                                    UnityGfxRenderer deviceType,
                                    IUnityInterfaces* unityInterfaces);

 private:
  NativeRendererHandle m_myHandle;
  std::mutex m_lock;
  void* m_peerHandle{nullptr};
  std::vector<TextureDesc> m_remoteTextures;
  VideoKind m_remoteVideoFormat{VideoKind::kNone};
  // REVIEW: Should we keep a frame queue?
  I420AVideoFrame m_I420RemoteVideoFrame{};
  
  NativeRenderer(PeerConnectionHandle peerHandle);
  void Shutdown();

  static void MRS_CALL I420RemoteVideoFrameCallback(void* user_data,
                                                    const void* yptr,
                                                    const void* uptr,
                                                    const void* vptr,
                                                    const void* aptr,
                                                    const int ystride,
                                                    const int ustride,
                                                    const int vstride,
                                                    const int astride,
                                                    const int frame_width,
                                                    const int frame_height);
};
