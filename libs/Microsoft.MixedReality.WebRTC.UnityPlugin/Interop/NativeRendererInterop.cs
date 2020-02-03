using System;
using System.Runtime.InteropServices;
using Microsoft.MixedReality.WebRTC.Interop;

namespace Microsoft.MixedReality.WebRTC.UnityPlugin.Interop
{
    internal class NativeRendererInterop
    {
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct TextureDesc
        {
            public IntPtr texture;
            public int width;
            public int height;
        }

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsNativeRendererCreate")]
        public static extern uint NativeRenderer_Create(PeerConnectionHandle peerConnectionHandle, out IntPtr nativeRendererHandlePtr);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsNativeRendererDestroy")]
        public static extern uint NativeRenderer_Destroy(out IntPtr nativeRendererHandlePtr);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsNativeRendererRegisterRemoteTextures")]
        public static extern uint NativeRenderer_RegisterRemoteTextures(IntPtr nativeRendererHandle, VideoKind format, TextureDesc[] textures, int textureCount);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsNativeRendererUnregisterRemoteTextures")]
        public static extern uint NativeRenderer_UnregisterRemoteTextures(IntPtr v);

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsNativeRendererGetVideoUpdateMethod")]
        public static extern IntPtr NativeRenderer_GetVideoUpdateMethod();

        [DllImport(Utils.dllPath, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi,
            EntryPoint = "mrsSetLoggingFunctions")]
        public static extern void NativeRenderer_SetLoggingFunctions(LogCallback logDebugCallback, LogCallback logErrorCallback, LogCallback logWarningCallback);
    }
}
