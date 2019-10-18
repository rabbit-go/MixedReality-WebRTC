using System;
using System.Runtime.InteropServices;
using Microsoft.MixedReality.WebRTC.Interop;

namespace Microsoft.MixedReality.WebRTC
{
    /// <summary>
    /// Video track sending to the remote peer video frames originating from
    /// a local track source.
    /// </summary>
    public class LocalVideoTrack : IDisposable
    {
        /// <summary>
        /// Peer connection this video track is added to, if any.
        /// This is <c>null</c> after the track has been removed from the peer connection.
        /// </summary>
        public PeerConnection PeerConnection { get; private set; }

        /// <summary>
        /// Track name as specified during creation. This property is immutable.
        /// </summary>
        public string Name { get; }

        /// <summary>
        /// External source for this video track, or <c>null</c> if the source is
        /// some internal video capture device.
        /// </summary>
        public ExternalVideoTrackSource Source { get; } = null;

        /// <summary>
        /// Enabled status of the track. If enabled, send local video frames to the remote peer as
        /// expected. If disabled, send only black frames instead.
        /// </summary>
        /// <remarks>
        /// Reading the value of this property after the track has been disposed is valid, and returns
        /// <c>false</c>. Writing to this property after the track has been disposed throws an exception.
        /// </remarks>
        public bool Enabled
        {
            get
            {
                return (LocalVideoTrackInterop.LocalVideoTrack_IsEnabled(_nativePeerHandle) != 0);
            }
            set
            {
                uint res = LocalVideoTrackInterop.LocalVideoTrack_SetEnabled(_nativePeerHandle, value ? -1 : 0);
                Utils.ThrowOnErrorCode(res);
            }
        }

        /// <summary>
        /// Event that occurs when a video frame has been produced by the underlying source is available.
        /// </summary>
        public event I420VideoFrameDelegate I420VideoFrameReady;

        /// <summary>
        /// Event that occurs when a video frame has been produced by the underlying source is available.
        /// </summary>
        public event ARGBVideoFrameDelegate ARGBVideoFrameReady;

        /// <summary>
        /// Handle to native peer connection C++ object the native track is added to, if any.
        /// </summary>
        protected IntPtr _nativePeerHandle = IntPtr.Zero;

        /// <summary>
        /// Handle to native local video track C++ object.
        /// </summary>
        internal IntPtr _nativeHandle = IntPtr.Zero;

        /// <summary>
        /// Handle to self for interop callbacks. This adds a reference to the current object, preventing
        /// it from being garbage-collected.
        /// </summary>
        private IntPtr _selfHandle = IntPtr.Zero;

        /// <summary>
        /// Callback arguments to ensure delegates registered with the native layer don't go out of scope.
        /// </summary>
        private LocalVideoTrackInterop.InteropCallbackArgs _interopCallbackArgs;

        internal LocalVideoTrack(PeerConnection peer, IntPtr nativePeerHandle, IntPtr nativeHandle, string trackName)
        {
            PeerConnection = peer;
            _nativePeerHandle = nativePeerHandle;
            _nativeHandle = nativeHandle;
            Name = trackName;
            RegisterInteropCallbacks();
        }

        internal LocalVideoTrack(ExternalVideoTrackSource source, PeerConnection peer, IntPtr nativePeerHandle,
            IntPtr nativeHandle, string trackName)
        {
            PeerConnection = peer;
            _nativePeerHandle = nativePeerHandle;
            _nativeHandle = nativeHandle;
            Name = trackName;
            Source = source;
            RegisterInteropCallbacks();
        }

        private void RegisterInteropCallbacks()
        {
            _interopCallbackArgs = new LocalVideoTrackInterop.InteropCallbackArgs()
            {
                Track = this,
                I420FrameCallback = LocalVideoTrackInterop.I420FrameCallback,
                ARGBFrameCallback = LocalVideoTrackInterop.ARGBFrameCallback,
            };
            _selfHandle = Utils.MakeWrapperRef(this);
            LocalVideoTrackInterop.LocalVideoTrack_RegisterI420FrameCallback(
                _nativeHandle, _interopCallbackArgs.I420FrameCallback, _selfHandle);
            LocalVideoTrackInterop.LocalVideoTrack_RegisterARGBFrameCallback(
                _nativeHandle, _interopCallbackArgs.ARGBFrameCallback, _selfHandle);
        }

        #region IDisposable support

        /// <summary>
        /// Dispose of native resources and optionally of managed ones.
        /// </summary>
        /// <param name="disposing"><c>true</c> if this is called from <see cref="Dispose()"/> and managed
        /// resources also need to be diposed, or <c>false</c> if called from the finalizer and onyl native
        /// resources can safely be accessed to be disposed.</param>
        protected virtual void Dispose(bool disposing)
        {
            if (_selfHandle != IntPtr.Zero)
            {
                LocalVideoTrackInterop.LocalVideoTrack_RegisterI420FrameCallback(_nativeHandle, null, IntPtr.Zero);
                LocalVideoTrackInterop.LocalVideoTrack_RegisterARGBFrameCallback(_nativeHandle, null, IntPtr.Zero);
                GCHandle.FromIntPtr(_selfHandle).Free();
                if (disposing)
                {
                    _interopCallbackArgs = null;
                }
                _selfHandle = IntPtr.Zero;
            }

            if (_nativeHandle != IntPtr.Zero)
            {
                // Remove from peer connection. Currently this is equivalent to destroying from
                // the point of view of C# because the native object is kept alive by the peer
                // connection and never lives on its own.
                if (_nativePeerHandle != IntPtr.Zero)
                {
                    PeerConnectionInterop.PeerConnection_RemoveLocalVideoTrack(_nativePeerHandle, _nativeHandle);
                    _nativePeerHandle = IntPtr.Zero;
                }
                if (disposing)
                {
                    PeerConnection = null;
                }
                _nativeHandle = IntPtr.Zero;
            }
        }

        /// <summary>
        /// Finalizer disposing of native resources if <see cref="Dispose()"/> was not called.
        /// </summary>
        ~LocalVideoTrack()
        {
            Dispose(false);
        }

        /// <summary>
        /// Implementation of the <see cref="IDisposable"/> pattern. Release native and
        /// managed resources, and supress the finalizer invoke.
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        #endregion

        internal void OnI420FrameReady(I420AVideoFrame frame)
        {
            I420VideoFrameReady?.Invoke(frame);
        }

        internal void OnARGBFrameReady(ARGBVideoFrame frame)
        {
            ARGBVideoFrameReady?.Invoke(frame);
        }
    }
}
