using System;
using System.Linq;
using Microsoft.MixedReality.WebRTC.Unity.NativeRendering.Wrapper.Interop;

namespace Microsoft.MixedReality.WebRTC.Unity.NativeRendering.Wrapper
{
    public enum VideoKind : int
    {
        None = 0,
        I420 = 1,
        ARGB = 2
    }

    public class TextureDesc
    {
        public IntPtr texture;
        public int width;
        public int height;
    }

    public class NativeRenderer : IDisposable
    {
        private IntPtr _nativeHandle = IntPtr.Zero;

        public NativeRenderer(PeerConnection peerConnection)
        {
            uint res = NativeRendererInterop.NativeRenderer_Create(peerConnection.NativeHandle, out IntPtr nativeHandle);
            WebRTC.Interop.Utils.ThrowOnErrorCode(res);
            _nativeHandle = nativeHandle;
        }

        public void RegisterRemoteTextures(VideoKind format, TextureDesc[] textures)
        {
            var interopTextures = textures.Select(item => new NativeRendererInterop.TextureDesc
            {
                texture = item.texture,
                width = item.width,
                height = item.height
            }).ToArray();
            uint res = NativeRendererInterop.NativeRenderer_RegisterRemoteTextures(_nativeHandle, format, interopTextures, interopTextures.Length);
            WebRTC.Interop.Utils.ThrowOnErrorCode(res);
        }

        public void UnregisterRemoteTextures()
        {
            uint res = NativeRendererInterop.NativeRenderer_UnregisterRemoteTextures(_nativeHandle);
            WebRTC.Interop.Utils.ThrowOnErrorCode(res);
        }

        public static IntPtr GetVideoUpdateMethod()
        {
            return NativeRendererInterop.NativeRenderer_GetVideoUpdateMethod();
        }

        public void Dispose()
        {
            NativeRendererInterop.NativeRenderer_Destroy(out _nativeHandle);
        }
    }
}
