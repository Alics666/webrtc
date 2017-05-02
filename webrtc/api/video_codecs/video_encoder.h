/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_VIDEO_CODECS_VIDEO_ENCODER_H_
#define WEBRTC_API_VIDEO_CODECS_VIDEO_ENCODER_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/checks.h"
#include "webrtc/common_types.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_frame.h"
#include "webrtc/base/optional.h"

namespace webrtc {

class RTPFragmentationHeader;
// TODO(pbos): Expose these through a public (root) header or change these APIs.
struct CodecSpecificInfo;
class VideoCodec;

class EncodedImageCallback {
 public:
  virtual ~EncodedImageCallback() {}

  struct Result {
    enum Error {
      OK,

      // Failed to send the packet.
      ERROR_SEND_FAILED,
    };

    Result(Error error) : error(error) {}
    Result(Error error, uint32_t frame_id) : error(error), frame_id(frame_id) {}

    Error error;

    // Frame ID assigned to the frame. The frame ID should be the same as the ID
    // seen by the receiver for this frame. RTP timestamp of the frame is used
    // as frame ID when RTP is used to send video. Must be used only when
    // error=OK.
    uint32_t frame_id = 0;

    // Tells the encoder that the next frame is should be dropped.
    bool drop_next_frame = false;
  };

  // Callback function which is called when an image has been encoded.
  virtual Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) = 0;

  virtual void OnDroppedFrame() {}
};

class VideoEncoder {
 public:
  enum EncoderType {
    kH264,
    kVp8,
    kVp9,
    kUnsupportedCodec,
  };
  struct QpThresholds {
    QpThresholds(int l, int h) : low(l), high(h) {}
    QpThresholds() : low(-1), high(-1) {}
    int low;
    int high;
  };
  struct ScalingSettings {
    ScalingSettings(bool on, int low, int high)
        : enabled(on),
          thresholds(rtc::Optional<QpThresholds>(QpThresholds(low, high))) {}
    explicit ScalingSettings(bool on) : enabled(on) {}
    const bool enabled;
    const rtc::Optional<QpThresholds> thresholds;
  };
  static VideoEncoder* Create(EncoderType codec_type);
  // Returns true if this type of encoder can be created using
  // VideoEncoder::Create.
  static bool IsSupportedSoftware(EncoderType codec_type);
  static EncoderType CodecToEncoderType(VideoCodecType codec_type);

  static VideoCodecVP8 GetDefaultVp8Settings();
  static VideoCodecVP9 GetDefaultVp9Settings();
  static VideoCodecH264 GetDefaultH264Settings();

  virtual ~VideoEncoder() {}

  // Initialize the encoder with the information from the codecSettings
  //
  // Input:
  //          - codec_settings    : Codec settings
  //          - number_of_cores   : Number of cores available for the encoder
  //          - max_payload_size  : The maximum size each payload is allowed
  //                                to have. Usually MTU - overhead.
  //
  // Return value                  : Set bit rate if OK
  //                                 <0 - Errors:
  //                                  WEBRTC_VIDEO_CODEC_ERR_PARAMETER
  //                                  WEBRTC_VIDEO_CODEC_ERR_SIZE
  //                                  WEBRTC_VIDEO_CODEC_LEVEL_EXCEEDED
  //                                  WEBRTC_VIDEO_CODEC_MEMORY
  //                                  WEBRTC_VIDEO_CODEC_ERROR
  virtual int32_t InitEncode(const VideoCodec* codec_settings,
                             int32_t number_of_cores,
                             size_t max_payload_size) = 0;

  // Register an encode complete callback object.
  //
  // Input:
  //          - callback         : Callback object which handles encoded images.
  //
  // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) = 0;

  // Free encoder memory.
  // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t Release() = 0;

  // Encode an I420 image (as a part of a video stream). The encoded image
  // will be returned to the user through the encode complete callback.
  //
  // Input:
  //          - frame             : Image to be encoded
  //          - frame_types       : Frame type to be generated by the encoder.
  //
  // Return value                 : WEBRTC_VIDEO_CODEC_OK if OK
  //                                <0 - Errors:
  //                                  WEBRTC_VIDEO_CODEC_ERR_PARAMETER
  //                                  WEBRTC_VIDEO_CODEC_MEMORY
  //                                  WEBRTC_VIDEO_CODEC_ERROR
  //                                  WEBRTC_VIDEO_CODEC_TIMEOUT
  virtual int32_t Encode(const VideoFrame& frame,
                         const CodecSpecificInfo* codec_specific_info,
                         const std::vector<FrameType>* frame_types) = 0;

  // Inform the encoder of the new packet loss rate and the round-trip time of
  // the network.
  //
  // Input:
  //          - packet_loss : Fraction lost
  //                          (loss rate in percent = 100 * packetLoss / 255)
  //          - rtt         : Round-trip time in milliseconds
  // Return value           : WEBRTC_VIDEO_CODEC_OK if OK
  //                          <0 - Errors: WEBRTC_VIDEO_CODEC_ERROR
  virtual int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) = 0;

  // Inform the encoder about the new target bit rate.
  //
  // Input:
  //          - bitrate         : New target bit rate
  //          - framerate       : The target frame rate
  //
  // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t SetRates(uint32_t bitrate, uint32_t framerate) {
    RTC_NOTREACHED() << "SetRate(uint32_t, uint32_t) is deprecated.";
    return -1;
  }

  // Default fallback: Just use the sum of bitrates as the single target rate.
  // TODO(sprang): Remove this default implementation when we remove SetRates().
  virtual int32_t SetRateAllocation(const BitrateAllocation& allocation,
                                    uint32_t framerate) {
    return SetRates(allocation.get_sum_kbps(), framerate);
  }

  // Any encoder implementation wishing to use the WebRTC provided
  // quality scaler must implement this method.
  virtual ScalingSettings GetScalingSettings() const {
    return ScalingSettings(false);
  }

  virtual int32_t SetPeriodicKeyFrames(bool enable) { return -1; }
  virtual bool SupportsNativeHandle() const { return false; }
  virtual const char* ImplementationName() const { return "unknown"; }
};

}  // namespace webrtc
#endif  // WEBRTC_API_VIDEO_CODECS_VIDEO_ENCODER_H_
