#include "ffvmaf_lib.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libvmaf/src/libvmaf.h"
#include "libswscale/swscale.h"
}

#include <cmath>

// Returns 0 if no frames have been decoded, 1 if a frame has been decoded, and a negative value on error.
static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext,
                         AVFrame *pFrame) {
  int response = avcodec_send_packet(pCodecContext, pPacket);

  if (response < 0) {
    printf("Error while sending a packet to the decoder: ");
    printf("\n");
    return response;
  }

  int num_frames = 0;
  while (response >= 0) {
    // We know that there is exactly one frame per packet. After reading this frame, however, we still need to make another
    // call to receive_frame() to flush the decoder. To retain the decoded frame, we don't pass it in the flush call
    // and instead pass a nullptr.
    auto frame_arg = num_frames == 0 ? pFrame : nullptr;
    response = avcodec_receive_frame(pCodecContext, frame_arg);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      break;
    } else if (response < 0) {
      return response;
    }
    num_frames++;
  }
  return num_frames;
}

int InitializeVmaf(VmafContext *vmaf,
                   VmafModel **model,
                   VmafModelCollection **model_collection,
                   uint64_t *model_collection_count, const char *model_buffer, uint64_t model_buffer_size,
                   bool use_phone_model) {

  enum VmafModelFlags flags = VMAF_MODEL_FLAGS_DEFAULT;
  if (use_phone_model) {
    flags = (VmafModelFlags) (flags | VMAF_MODEL_FLAG_ENABLE_TRANSFORM);
  }
  VmafModelConfig model_config{
      .name = "vmaf",
      .flags = flags,
  };

  if (vmaf_model_load_from_buffer(model, &model_config, model_buffer, model_buffer_size) != 0) {
    printf("Reading json model from buffer failed. Attempting to read it as a model collection.\n");

    if (vmaf_model_collection_load_from_buffer(model,
                                               model_collection,
                                               &model_config,
                                               model_buffer,
                                               model_buffer_size) != 0) {
      fprintf(stderr, "Reading json model collection from buffer failed. Unable to read model.\n");
      return -1;
    }

    if (vmaf_use_features_from_model_collection(vmaf, model_collection[0]) != 0) {
      fprintf(stderr, "Problem registering feature extractors from model collection.\n");
      return -1;
    }

    printf("Incrmenting model collection count.\n");
    *model_collection_count = *model_collection_count + 1;
  }

  // Register the feature extractors required by the model.
  if (vmaf_use_features_from_model(vmaf, model[0]) != 0) {
    fprintf(stderr, "Problem registering feature extractors for the model.\n");
    return -1;
  }

  printf("VMAF Initialized!\n");
  return 0;
}

bool GetNextFrame(AVFormatContext *pFormatContext,
                  AVCodecContext *pCodecContext,
                  AVPacket *pPacket,
                  AVFrame *pFrame,
                  int8_t video_stream_index) {
  while (av_read_frame(pFormatContext, pPacket) >= 0) {
    if (pPacket->stream_index == video_stream_index) {
      // decode_packet returns the number of frames decoded, or a negative value on error.
      int response = decode_packet(pPacket, pCodecContext, pFrame);
      if (response == 0) {
        // No error but no frame decoded.
        continue;
      }
      return (response == 1);
    }
    av_packet_unref(pPacket);
  }
  return false;
}

int AllocateAndOpenCodecContexts(AVCodecContext *&pCodecContext_reference,
                                 AVCodecContext *&pCodecContext_test,
                                 const AVCodecParameters *pCodecParameters_reference,
                                 const AVCodecParameters *pCodecParameters_test,
                                 const AVCodec *pCodec_reference,
                                 const AVCodec *pCodec_test) {
  pCodecContext_reference = avcodec_alloc_context3(pCodec_reference);
  pCodecContext_test = avcodec_alloc_context3(pCodec_test);
  if (!pCodecContext_reference || !pCodecContext_test) {
    fprintf(stderr, "failed to allocated memory for AVCodecContext\n");
    return -1.0;
  }

  if (avcodec_parameters_to_context(pCodecContext_reference, pCodecParameters_reference) < 0
      || avcodec_parameters_to_context(pCodecContext_test, pCodecParameters_test) < 0) {
    fprintf(stderr, "failed to copy codec params to codec context\n");
    return -1.0;
  }

  if (avcodec_open2(pCodecContext_reference, pCodec_reference, NULL) < 0
      || avcodec_open2(pCodecContext_test, pCodec_test, NULL) < 0) {
    fprintf(stderr, "failed to open codec through avcodec_open2\n");
    return -1.0;
  }
  return 0;
}

static int ScaleFrame(SwsContext *sws_context, AVFrame *dst, AVFrame *src) {
  sws_scale(sws_context,
            (const uint8_t *const *) src->data,
            src->linesize,
            0,
            src->height,
            dst->data,
            dst->linesize);
  return 0;
}

static int ScaleFrameToHD(SwsContext *sws_context, AVFrame *&dst, AVFrame *&src) {
  if (sws_context != NULL) {
    ScaleFrame(sws_context, dst, src);
  } else {
    dst = src;
  }
  return 0;
}

static int AllocateHDFrame(const AVCodecParameters *pCodec_parameters,
                           SwsContext *&sws_context,
                           AVFrame *&scaled_pFrame) {
  sws_context = sws_getContext(pCodec_parameters->width,
                               pCodec_parameters->height,
                               AV_PIX_FMT_YUV420P,
                               1920,
                               1080,
                               AV_PIX_FMT_YUV420P,
                               SWS_BICUBIC,
                               NULL,
                               NULL,
                               NULL);

  scaled_pFrame = av_frame_alloc();
  if (!scaled_pFrame) {
    fprintf(stderr, "Failed to allocate new frame.");
    return -1.0;
  }
  scaled_pFrame->height = 1080;
  scaled_pFrame->width = 1920;
  scaled_pFrame->format = AV_PIX_FMT_YUV420P;
  if (av_frame_get_buffer(scaled_pFrame, 32)) {
    fprintf(stderr, "Failed to allocate data buffers for new frame.\n");
    av_frame_free(&scaled_pFrame);
    return -1.0;
  }
  return 0;
}

static int CopyPictureData(AVFrame *src, VmafPicture *dst, unsigned bpc) {
  int err = vmaf_picture_alloc(dst, VMAF_PIX_FMT_YUV420P, bpc,
                               src->width, src->height);
  if (err) {
    fprintf(stderr, "Error allocating vmaf picture: %d.\n", err);
    return AVERROR(ENOMEM);
  }

  for (unsigned i = 0; i < 3; i++) {
    uint8_t *src_data = (uint8_t *) src->data[i];
    uint8_t *dst_data = (uint8_t *) dst->data[i];
    for (unsigned j = 0; j < dst->h[i]; j++) {
      memcpy(dst_data, src_data, sizeof(*dst_data) * dst->w[i]);
      src_data += src->linesize[i];
      dst_data += dst->stride[i];
    }
  }

  return 0;
}

static int CopyFrameToBuffer(AVFrame *frame, uint8_t *buffer) {
  for (unsigned i = 0; i < 3; i++) {
    uint8_t *frame_data = (uint8_t *) frame->data[i];
    for (unsigned j = 0; j < frame->height; j++) {
      memcpy(buffer, frame_data, sizeof(*buffer) * frame->linesize[i]);
      frame_data += frame->linesize[i];
      buffer += frame->linesize[i];
    }
  }
  return 0;
}

bool IsHDResolution(const AVCodecParameters *codec_parameters) {
  return codec_parameters->height == 1080 && codec_parameters->width == 1920;
}

void FreeResources(SwsContext *reference_sws_context,
                   SwsContext *test_sws_context,
                   AVFrame *scaled_pFrame_reference,
                   AVFrame *scaled_pFrame_test,
                   AVCodecContext *pCodecContext_reference = NULL,
                   AVCodecContext *pCodecContext_test = NULL) {
  printf("Freeing resources...\n");
  sws_freeContext(reference_sws_context);
  sws_freeContext(test_sws_context);

  if (scaled_pFrame_reference != NULL)
    av_frame_free(&scaled_pFrame_reference);

  if (scaled_pFrame_test != NULL)
    av_frame_free(&scaled_pFrame_test);

  if (pCodecContext_reference != NULL)
    avcodec_free_context(&pCodecContext_reference);

  if (pCodecContext_test != NULL)
    avcodec_free_context(&pCodecContext_test);
}

unsigned GetNumCommonFrames(const AVFormatContext *pFormatContext_reference,
                            const AVFormatContext *pFormatContext_test,
                            const int8_t video_stream_index_reference,
                            const int8_t video_stream_index_test) {
  const unsigned num_frames_reference =
      std::round(av_q2d(pFormatContext_reference->streams[video_stream_index_reference]->r_frame_rate) *
          pFormatContext_reference->duration / AV_TIME_BASE);
  const unsigned
      num_frames_test = std::round(av_q2d(pFormatContext_test->streams[video_stream_index_test]->r_frame_rate) *
      pFormatContext_test->duration / AV_TIME_BASE);
  return std::min(num_frames_reference, num_frames_test);
}

// Wrapper around a buffer shared with the frontend to share the data being output.
class OutputBuffer {
 public:
  OutputBuffer(uintptr_t output_buffer) : buffer_(reinterpret_cast<float *>(output_buffer)) {}

  void SetNumFramesToProcess(const unsigned num_frames_to_process) {
    buffer_[0] = num_frames_to_process;
  }

  void SetNumFramesProcessed(const unsigned num_frames_processed) {
    buffer_[1] = num_frames_processed;
  }

  void SetFPS(const float fps) {
    buffer_[2] = fps;
  }

  bool IsTerminationBitSet() {
    return buffer_[3] == -999.0;
  }

  void ClearTerminationBit() {
    buffer_[3] = 0.0;
  }

  bool SetVmafScore(const unsigned vmaf_index, const double vmaf_score) {
    return buffer_[4 + vmaf_index] = vmaf_score;
  }

  bool SetPooledVmafScore(const double pooled_vmaf_score) {
    const unsigned num_frames_to_process = (unsigned) buffer_[0];
    return buffer_[4 + num_frames_to_process] = pooled_vmaf_score;
  }

  bool SetMaxVmafScore(const double max_vmaf_score) {
    const unsigned num_frames_to_process = (unsigned) buffer_[0];
    return buffer_[5 + num_frames_to_process] = max_vmaf_score;
  }

  bool SetMinVmafScore(const double min_vmaf_score) {
    const unsigned num_frames_to_process = (unsigned) buffer_[0];
    return buffer_[6 + num_frames_to_process] = min_vmaf_score;
  }

 private:
  float *buffer_;
};

float ComputeVmafForEachFrame(const std::string &reference_file,
                              const std::string &test_file,
                              AVFormatContext *pFormatContext_reference,
                              AVFormatContext *pFormatContext_test,
                              AVFrame *pFrame_reference,
                              AVFrame *pFrame_test,
                              AVPacket *pPacket_reference,
                              AVPacket *pPacket_test,
                              SwsContext *display_frame_sws_context,
                              AVFrame *max_score_ref_frame,
                              AVFrame *max_score_test_frame,
                              AVFrame *min_score_ref_frame,
                              AVFrame *min_score_test_frame,
                              VmafContext *vmaf,
                              VmafModel *model,
                              uintptr_t max_score_ref_frame_buffer,
                              uintptr_t max_score_test_frame_buffer,
                              uintptr_t min_score_ref_frame_buffer,
                              uintptr_t min_score_test_frame_buffer,
                              uintptr_t output_buffer) {

  // Open and initialize AVFormatContexts.
  if (avformat_open_input(&pFormatContext_reference, reference_file.c_str(), NULL, NULL) != 0
      || avformat_open_input(&pFormatContext_test, test_file.c_str(), NULL, NULL) != 0) {
    fprintf(stderr, "ERROR could not open file.\n");
    return -1.0;
  }

  if (avformat_find_stream_info(pFormatContext_reference, NULL) < 0
      || avformat_find_stream_info(pFormatContext_test, NULL) < 0) {
    printf("ERROR could not get the stream info\n");
    return -1.0;
  }

  // Declare the various local variables. Those allocated on heap must be free before this function returns.
  int8_t video_stream_index_reference = -1;
  int8_t video_stream_index_test = -1;
  SwsContext *reference_sws_context = NULL;
  SwsContext *test_sws_context = NULL;
  AVFrame *scaled_pFrame_reference = NULL;
  AVFrame *scaled_pFrame_test = NULL;
  AVCodecContext *pCodecContext_reference = NULL;
  AVCodecContext *pCodecContext_test = NULL;
  AVCodec *pCodec_reference = NULL;
  AVCodecParameters *pCodecParameters_reference = NULL;
  AVCodec *pCodec_test = NULL;
  AVCodecParameters *pCodecParameters_test = NULL;
  OutputBuffer output(output_buffer);

  // Find the video stream for the reference video, and allocate an HD resolution frame and a
  // scaling context if necessary.
  for (int i = 0; i < pFormatContext_reference->nb_streams; i++) {
    AVCodecParameters *pLocalCodecParameters = NULL;
    pLocalCodecParameters = pFormatContext_reference->streams[i]->codecpar;
    AVCodec *pLocalCodec = NULL;
    pLocalCodec = (AVCodec *) avcodec_find_decoder(pLocalCodecParameters->codec_id);

    if (pLocalCodec == NULL) {
      printf("ERROR unsupported codec!\n");
      // In this example if the codec is not found we just skip it
      continue;
    }

    // when the stream is a video we store its index, codec parameters and codec
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (!IsHDResolution(pLocalCodecParameters)) {
        if (AllocateHDFrame(pLocalCodecParameters, reference_sws_context, scaled_pFrame_reference) != 0) {
          return -1.0;
        }
      }
      video_stream_index_reference = i;
      pCodec_reference = pLocalCodec;
      pCodecParameters_reference = pLocalCodecParameters;
      break;
    }
  }

  // Find the video stream for the test video, and allocate an HD resolution frame and a scaling context if
  // necessary.
  for (int i = 0; i < pFormatContext_test->nb_streams; i++) {
    AVCodecParameters *pLocalCodecParameters = NULL;
    pLocalCodecParameters = pFormatContext_test->streams[i]->codecpar;
    AVCodec *pLocalCodec = NULL;
    pLocalCodec = (AVCodec *) avcodec_find_decoder(pLocalCodecParameters->codec_id);

    if (pLocalCodec == NULL) {
      printf("ERROR unsupported codec!\n");
      // In this example if the codec is not found we just skip it
      continue;
    }

    // when the stream is a video we store its index, codec parameters and codec
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (!IsHDResolution(pLocalCodecParameters)) {
        if (AllocateHDFrame(pLocalCodecParameters, test_sws_context, scaled_pFrame_test) != 0) {
          return -1.0;
        }
      }
      video_stream_index_test = i;
      pCodec_test = pLocalCodec;
      pCodecParameters_test = pLocalCodecParameters;
    }
  }

  // Return if a video stream was not found for one or both videos. Free the initialized resources before returning.
  if (video_stream_index_reference == -1 || video_stream_index_test == -1) {
    fprintf(stderr, "One of the files does not contain a video stream!\n");
    FreeResources(reference_sws_context, test_sws_context, scaled_pFrame_reference, scaled_pFrame_test);
    return -1.0;
  }

  // Prepare the codec contexts for decoding the videos. Return and free resources if there is an error.
  if (AllocateAndOpenCodecContexts(pCodecContext_reference,
                                   pCodecContext_test,
                                   pCodecParameters_reference,
                                   pCodecParameters_test,
                                   pCodec_reference,
                                   pCodec_test) != 0) {
    FreeResources(reference_sws_context,
                  test_sws_context,
                  scaled_pFrame_reference,
                  scaled_pFrame_test,
                  pCodecContext_reference,
                  pCodecContext_test);
    return -1.0;
  }

  const unsigned num_common_frames = GetNumCommonFrames(pFormatContext_reference,
                                                        pFormatContext_test,
                                                        video_stream_index_reference,
                                                        video_stream_index_test);
  // Process at most 10^5 frames for now.
  const unsigned num_frames_to_process = std::min(num_common_frames, (unsigned) 100000);
  output.SetNumFramesToProcess(num_frames_to_process);

  // For computing the processing rate in FPS and finding the min and max vmaf scores.
  float fps = 0;
  const time_t t0 = clock();
  double max_vmaf_score = 0.0;
  double min_vmaf_score = 100.0;

  unsigned frame_index;
  for (frame_index = 0; frame_index < num_frames_to_process; frame_index++) {

    if (output.IsTerminationBitSet()) {
      printf("Cancelling compute...\n");
      output.ClearTerminationBit();
      FreeResources(reference_sws_context,
                    test_sws_context,
                    scaled_pFrame_reference,
                    scaled_pFrame_test,
                    pCodecContext_reference,
                    pCodecContext_test);
      return -1.0;
    }

    bool reference_frame_decoded =
        GetNextFrame(pFormatContext_reference, pCodecContext_reference, pPacket_reference, pFrame_reference,
                     video_stream_index_reference);

    bool test_frame_decoded = GetNextFrame(pFormatContext_test, pCodecContext_test, pPacket_test, pFrame_test,
                                           video_stream_index_test);

    if (reference_frame_decoded && test_frame_decoded) {

      // Scale the frames to HD if they are not already that resolution.
      ScaleFrameToHD(reference_sws_context, scaled_pFrame_reference, pFrame_reference);
      ScaleFrameToHD(test_sws_context, scaled_pFrame_test, pFrame_test);

      // Copy the frames into VmafPictures and read them into VmafContext.
      VmafPicture reference_vmaf_picture, test_vmaf_picture;
      if (CopyPictureData(scaled_pFrame_reference, &reference_vmaf_picture, 8) != 0 ||
          CopyPictureData(scaled_pFrame_test, &test_vmaf_picture, 8) != 0) {
        FreeResources(reference_sws_context,
                      test_sws_context,
                      scaled_pFrame_reference,
                      scaled_pFrame_test,
                      pCodecContext_reference,
                      pCodecContext_test);
        return -1.0;
      }

      if (vmaf_read_pictures(vmaf, &reference_vmaf_picture, &test_vmaf_picture, frame_index) != 0) {
        fprintf(stderr, "Error reading vmaf pictures.\n");
        FreeResources(reference_sws_context,
                      test_sws_context,
                      scaled_pFrame_reference,
                      scaled_pFrame_test,
                      pCodecContext_reference,
                      pCodecContext_test);
        return -1.0;
      }

      // Compute the vmaf score at index - 2.
      double vmaf_score = -1.0;
      if (frame_index >= 2) {
        const unsigned frame_index_for_vmaf = frame_index - 2;
        int err = vmaf_score_at_index(vmaf, model, &vmaf_score, frame_index_for_vmaf);
        if (err != 0) {
          fprintf(stderr, "Error computing vmaf score at index\n");
          FreeResources(reference_sws_context,
                        test_sws_context,
                        scaled_pFrame_reference,
                        scaled_pFrame_test,
                        pCodecContext_reference,
                        pCodecContext_test);
          return -1.0;
        }
        output.SetVmafScore(frame_index_for_vmaf, vmaf_score);
      }

      // Compute and store FPS.
      if (frame_index != 0 && frame_index % 5 == 0) {
        fps = (frame_index + 1) /
            (((float) clock() - t0) / CLOCKS_PER_SEC);
        printf("Computing at a rate of %f fps\n", fps);
        output.SetFPS(fps);
      }

      // If the frame's vmaf score is max seen so far, copy the frame into buffer for display.
      if (vmaf_score > max_vmaf_score) {
        ScaleFrame(display_frame_sws_context, max_score_ref_frame, scaled_pFrame_reference);
        uint8_t *buffer_ptr = reinterpret_cast<uint8_t *>(max_score_ref_frame_buffer);
        CopyFrameToBuffer(max_score_ref_frame, buffer_ptr);

        ScaleFrame(display_frame_sws_context, max_score_test_frame, scaled_pFrame_test);
        buffer_ptr = reinterpret_cast<uint8_t *>(max_score_test_frame_buffer);
        CopyFrameToBuffer(max_score_test_frame, buffer_ptr);

        max_vmaf_score = vmaf_score;
      }

      // If the frame's vmaf score is the min seen so far, copy the frame into buffer for display.
      if (vmaf_score >= 0.0 && vmaf_score < min_vmaf_score) {
        ScaleFrame(display_frame_sws_context, min_score_ref_frame, scaled_pFrame_reference);
        uint8_t *buffer_ptr = reinterpret_cast<uint8_t *>(min_score_ref_frame_buffer);
        CopyFrameToBuffer(min_score_ref_frame, buffer_ptr);

        ScaleFrame(display_frame_sws_context, min_score_test_frame, scaled_pFrame_test);
        buffer_ptr = reinterpret_cast<uint8_t *>(min_score_test_frame_buffer);
        CopyFrameToBuffer(min_score_test_frame, buffer_ptr);

        min_vmaf_score = vmaf_score;
      }

      const unsigned num_frames_processed = frame_index + 1;
      output.SetNumFramesProcessed(num_frames_processed);

    } else if (!reference_frame_decoded && !test_frame_decoded) {
      printf("Decoding the next frame failed for both test and ref where frame index is %d.\n", frame_index);
      break;
    } else if (!reference_frame_decoded) {
      printf("Decoding the next frame failed for ref where frame index is %d.\n", frame_index);
      break;
    } else {
      printf("Decoding the next frame failed for test where frame index is %d.\n", frame_index);
      break;
    }
  }

  output.SetMaxVmafScore(max_vmaf_score);
  output.SetMinVmafScore(min_vmaf_score);

  // Flush the VMAF context and compute the pooled VMAF score.
  double pooled_vmaf_score = 0;
  if (vmaf_read_pictures(vmaf, NULL, NULL, 0) != 0
      || vmaf_score_pooled(vmaf, model, VMAF_POOL_METHOD_MEAN, &pooled_vmaf_score, 0, frame_index - 2) != 0) {
    fprintf(stderr, "Problem flushing VMAF context.\n");
    FreeResources(reference_sws_context,
                  test_sws_context,
                  scaled_pFrame_reference,
                  scaled_pFrame_test,
                  pCodecContext_reference,
                  pCodecContext_test);
    return -1.0;
  }

  printf("Pooled VMAF score: %f\n", pooled_vmaf_score);
  output.SetPooledVmafScore(pooled_vmaf_score);
  FreeResources(reference_sws_context,
                test_sws_context,
                scaled_pFrame_reference,
                scaled_pFrame_test,
                pCodecContext_reference,
                pCodecContext_test);
  return 0.0;
}