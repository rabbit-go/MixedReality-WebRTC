// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

// This is a precompiled header, it must be on its own, followed by a blank
// line, to prevent clang-format from reordering it with other headers.
#include "pch.h"

#include "api.h"
#include "native_renderer.h"
#include "log_helpers.h"

// Globals
static IUnityInterfaces* s_UnityInterfaces = nullptr;
static IUnityGraphics* s_Graphics = nullptr;

//
// Unity
//

void UNITY_INTERFACE_API
OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
  NativeRenderer::OnGraphicsDeviceEvent(eventType, s_Graphics->GetRenderer(),
                                        s_UnityInterfaces);
}

void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
  s_UnityInterfaces = unityInterfaces;
  s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
  s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
  OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

void UNITY_INTERFACE_API UnityPluginUnload() {
  s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

//
// NativeRenderer
//

mrsResult MRS_CALL
mrsNativeRendererCreate(PeerConnectionHandle peerHandle,
                        NativeRendererHandle* handleOut) noexcept {
  // REVIEW: Should we use std::shared_ptr here?
  *handleOut = nullptr;
  *handleOut = NativeRenderer::Create(peerHandle);
  return MRS_SUCCESS;
}

mrsResult MRS_CALL
mrsNativeRendererDestroy(NativeRendererHandle* handlePtr) noexcept {
  NativeRenderer::Destroy(*handlePtr);
  *handlePtr = nullptr;
  _CrtDumpMemoryLeaks();
  return MRS_SUCCESS;
}

mrsResult MRS_CALL
mrsNativeRendererRegisterRemoteTextures(NativeRendererHandle handle,
                                        VideoKind format,
                                        TextureDesc textures[],
                                        int textureCount) noexcept {
  if (auto renderer = NativeRenderer::Get(handle)) {
    renderer->RegisterRemoteTextures(format, textures, textureCount);
  }
  return MRS_SUCCESS;
}

mrsResult MRS_CALL mrsNativeRendererUnregisterRemoteTextures(
    NativeRendererHandle handle) noexcept {
  if (auto renderer = NativeRenderer::Get(handle)) {
    renderer->UnregisterRemoteTextures();
  }
  return MRS_SUCCESS;
}

VideoRenderMethod MRS_CALL mrsNativeRendererGetVideoUpdateMethod() noexcept {
  return NativeRenderer::DoVideoUpdate;
}

void MRS_CALL mrsSetLoggingFunctions(LogFunction logDebugFunc,
                                     LogFunction logErrorFunc,
                                     LogFunction logWarningFunc) {
  UnityLogger::SetLoggingFunctions(logDebugFunc,
                                   logErrorFunc,
                                   logWarningFunc);
}
