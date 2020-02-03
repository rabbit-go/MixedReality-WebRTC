// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include "../../Microsoft.MixedReality.WebRTC.Native/include/export.h"
#include "../../Microsoft.MixedReality.WebRTC.Native/include/mrs_errors.h"
#include "../../Microsoft.MixedReality.WebRTC.Native/include/video_frame.h"

extern "C" {

using mrsResult = Microsoft::MixedReality::WebRTC::Result;

/// Opaque handle to a native PeerConnection C++ object.
using PeerConnectionHandle = void*;

//
// Native rendering
//

/// Opaque handle to a native NativeRenderer C++ object.
using NativeRendererHandle = void*;

enum class VideoKind : int32_t {
  kNone = 0,
  kI420 = 1,
  kARGB = 2,
};

struct TextureDesc {
  void* texture{nullptr};
  int width{0};
  int height{0};
};

typedef void(MRS_CALL* VideoRenderMethod)();

/// Create a native renderer and return a handle to it.
MRS_API mrsResult MRS_CALL
mrsNativeRendererCreate(PeerConnectionHandle peerHandle,
                        NativeRendererHandle* handleOut) noexcept;

// Destroy a native renderer.
MRS_API mrsResult MRS_CALL
mrsNativeRendererDestroy(NativeRendererHandle* handlePtr) noexcept;

MRS_API mrsResult MRS_CALL
mrsNativeRendererRegisterRemoteTextures(NativeRendererHandle handle,
                                        VideoKind format,
                                        TextureDesc textures[],
                                        int textureCount) noexcept;

MRS_API mrsResult MRS_CALL
mrsNativeRendererUnregisterRemoteTextures(NativeRendererHandle handle) noexcept;

MRS_API VideoRenderMethod MRS_CALL
mrsNativeRendererGetVideoUpdateMethod() noexcept;

//
// Utils
//

typedef void (*LogFunction)(const char*);

MRS_API void MRS_CALL mrsSetLoggingFunctions(LogFunction logDebugFunc,
                                             LogFunction logErrorFunc,
                                             LogFunction logWarningFunc);
}
