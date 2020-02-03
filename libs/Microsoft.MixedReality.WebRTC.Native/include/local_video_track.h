// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "callback.h"
#include "interop/interop_api.h"
#include "str.h"
#include "tracked_object.h"
#include "video_frame_observer.h"

namespace rtc {
template <typename T>
class scoped_refptr;
}

namespace webrtc {
class RtpSenderInterface;
class VideoTrackInterface;
}  // namespace webrtc

namespace Microsoft::MixedReality::WebRTC {

class PeerConnection;

/// A local video track is a media track for a peer connection backed by a local
/// source, and transmitted to a remote peer.
///
/// The local nature of the track implies that the local peer has control on it,
/// including enabling or disabling the track, and removing it from the peer
/// connection. This is in contrast with a remote track	reflecting a track sent
/// by the remote peer, for which the local peer has limited control.
///
/// The local video track is backed by a local video track source. This is
/// typically a video capture device (e.g. webcam), but can	also be a source
/// producing programmatically generated frames. The local video track itself
/// has no knowledge about how the source produces the frames.
class LocalVideoTrack : public VideoFrameObserver, public TrackedObject {
 public:
  LocalVideoTrack(PeerConnection& owner,
                  rtc::scoped_refptr<webrtc::VideoTrackInterface> track,
                  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
                  mrsLocalVideoTrackInteropHandle interop_handle) noexcept;
  MRS_API ~LocalVideoTrack() override;

  /// Get the name of the local video track.
  MRS_API std::string GetName() const noexcept override;

  /// Enable or disable the video track. An enabled track streams its content
  /// from its source to the remote peer. A disabled video track only sends
  /// black frames.
  MRS_API void SetEnabled(bool enabled) const noexcept;

  /// Check if the track is enabled.
  /// See |SetEnabled(bool)|.
  MRS_API MRS_NODISCARD bool IsEnabled() const noexcept;

  //
  // Advanced use
  //

  MRS_NODISCARD webrtc::VideoTrackInterface* impl() const;
  MRS_NODISCARD webrtc::RtpSenderInterface* sender() const;

  MRS_NODISCARD mrsLocalVideoTrackInteropHandle GetInteropHandle() const
      noexcept {
    return interop_handle_;
  }

  void RemoveFromPeerConnection(webrtc::PeerConnectionInterface& peer);

 private:
  /// Weak reference to the PeerConnection object owning this track.
  PeerConnection* owner_{};

  /// Underlying core implementation.
  rtc::scoped_refptr<webrtc::VideoTrackInterface> track_;

  /// RTP sender this track is associated with.
  rtc::scoped_refptr<webrtc::RtpSenderInterface> sender_;

  /// Optional interop handle, if associated with an interop wrapper.
  mrsLocalVideoTrackInteropHandle interop_handle_{};
};

}  // namespace Microsoft::MixedReality::WebRTC
