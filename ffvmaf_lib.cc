#include "ffvmaf_lib.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libvmaf/src/libvmaf.h"
#include "libswscale/swscale.h"
}

#include <string>

static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext,
                         AVFrame *pFrame);

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

int GetNextFrame(AVFormatContext *pFormatContext,
                 AVCodecContext *pCodecContext,
                 AVPacket *pPacket,
                 AVFrame *pFrame,
                 int8_t video_stream_index) {
  while (av_read_frame(pFormatContext, pPacket) >= 0) {
    if (pPacket->stream_index == video_stream_index) {
//      printf("AVPacket->pts %"
//      PRId64, pPacket->pts);
//      printf("\n");
      // decode_packet returns the number of frames decoded, or a negative value on error.
      int response = decode_packet(pPacket, pCodecContext, pFrame);
      if (response == 0) {
        // No error but no frame decoded.
        continue;
      }
      return response;
    }
    av_packet_unref(pPacket);
  }
  return -1;
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
    return -1.0;
  }
  return 0;
}

static int CopyPictureData(AVFrame *src, VmafPicture *dst, unsigned bpc) {
  int err = vmaf_picture_alloc(dst, VMAF_PIX_FMT_YUV420P, bpc,
                               src->width, src->height);
  if (err)
    return AVERROR(ENOMEM);

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

bool IsHDResolution(const AVCodecParameters *codec_parameters) {
  return codec_parameters->height == 1080 && codec_parameters->width == 1920;
}

float ComputeVmafForEachFrame(const std::string &reference_file,
                              const std::string &test_file,
                              AVFormatContext *pFormatContext_reference,
                              AVFormatContext *pFormatContext_test,
                              AVCodecContext *pCodecContext_reference,
                              AVCodecContext *pCodecContext_test,
                              AVFrame *pFrame_reference,
                              AVFrame *pFrame_test,
                              AVPacket *pPacket_reference,
                              AVPacket *pPacket_test,
                              int8_t *video_stream_index_reference,
                              int8_t *video_stream_index_test,
                              std::unordered_map <uint8_t, int64_t> &frame_timestamps,
                              VmafContext *vmaf,
                              VmafModel *model,
                              uintptr_t output_buffer) {

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

  float *output_buffer_ptr = reinterpret_cast<float *>(output_buffer);

  AVCodec *pCodec_reference = NULL;
  AVCodecParameters *pCodecParameters_reference = NULL;
  SwsContext *reference_sws_context = NULL;
  SwsContext *test_sws_context = NULL;
  AVFrame *scaled_pFrame_reference = NULL;
  AVFrame *scaled_pFrame_test = NULL;

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
        int err = AllocateHDFrame(pLocalCodecParameters, reference_sws_context, scaled_pFrame_reference);
        if (err != 0) {
          fprintf(stderr, "Failed to prepare HD frame for scaling.\n");
          return -1.0;
        }
      }
      *video_stream_index_reference = i;
      pCodec_reference = pLocalCodec;
      pCodecParameters_reference = pLocalCodecParameters;
      break;
    }
  }

  AVCodec *pCodec_test = NULL;
  AVCodecParameters *pCodecParameters_test = NULL;

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
        int err = AllocateHDFrame(pLocalCodecParameters, test_sws_context, scaled_pFrame_test);
        if (err != 0) {
          fprintf(stderr, "Failed to prepare HD frame for scaling.\n");
          return -1.0;
        }
      }
      *video_stream_index_test = i;
      pCodec_test = pLocalCodec;
      pCodecParameters_test = pLocalCodecParameters;
    }
  }

  if (*video_stream_index_reference == -1 || *video_stream_index_test == -1) {
    fprintf(stderr, "One of the files does not contain a video stream!\n");
    return -1.0;
  }

  const unsigned num_frames_reference =
      std::round(av_q2d(pFormatContext_reference->streams[*video_stream_index_reference]->r_frame_rate) *
          pFormatContext_reference->duration / AV_TIME_BASE);
  const unsigned
      num_frames_test = std::round(av_q2d(pFormatContext_test->streams[*video_stream_index_test]->r_frame_rate) *
      pFormatContext_test->duration / AV_TIME_BASE);
  const unsigned num_common_frames = std::min(num_frames_reference, num_frames_test);

  output_buffer_ptr[0] = (float) num_common_frames;

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

  unsigned frame_index;
  unsigned frame_index_for_vmaf;
  const unsigned num_frames_to_process = std::min(num_common_frames, (unsigned) 100000);
  float fps = 0;
  const time_t t0 = clock();
  for (frame_index = 0; frame_index < num_frames_to_process; frame_index++) {

    bool reference_frame_decoded =
        (GetNextFrame(pFormatContext_reference, pCodecContext_reference, pPacket_reference, pFrame_reference,
                      *video_stream_index_reference) == 1);

    bool test_frame_decoded = (GetNextFrame(pFormatContext_test, pCodecContext_test, pPacket_test, pFrame_test,
                                            *video_stream_index_test) == 1);

    if (reference_frame_decoded && test_frame_decoded) {

      // Scale the frames to HD if they are not already that resolution.
      ScaleFrameToHD(reference_sws_context, scaled_pFrame_reference, pFrame_reference);
      ScaleFrameToHD(test_sws_context, scaled_pFrame_test, pFrame_test);

      // Copy the frames into VmafPictures and read them into VmafContext.
      VmafPicture reference_vmaf_picture, test_vmaf_picture;
      if (CopyPictureData(scaled_pFrame_reference, &reference_vmaf_picture, 8) != 0 ||
          CopyPictureData(scaled_pFrame_test, &test_vmaf_picture, 8) != 0) {
        fprintf(stderr, "Error allocating vmaf picture\n");
        return -1.0;
      }

      if (vmaf_read_pictures(vmaf, &reference_vmaf_picture, &test_vmaf_picture, frame_index) != 0) {
        fprintf(stderr, "Error reading vmaf pictures.\n");
        return -1.0;
      }

      // Compute the vmaf score at index. The score is computed for the frame_index_for_vmaf'th frame.
      double vmaf_score;
      if (frame_index >= 2) {
        frame_index_for_vmaf = frame_index - 2;
        int err = vmaf_score_at_index(vmaf, model, &vmaf_score, frame_index_for_vmaf);
        if (err != 0) {
          fprintf(stderr, "Error computing vmaf score at index\n");
          return -1.0;
        }
        output_buffer_ptr[3 + frame_index_for_vmaf] = vmaf_score;
        //printf("Frame %d: vmaf score %f\n", frame_index_for_vmaf, vmaf_score);
      }

      // Compute and store FPS.
      if (frame_index != 0 && frame_index % 5 == 0) {
        fps = (frame_index + 1) /
            (((float) clock() - t0) / CLOCKS_PER_SEC);
        printf("Computing at a rate of %f fps\n", fps);
        output_buffer_ptr[2] = fps;
      }

      const unsigned num_frames_processed = frame_index + 1;
      output_buffer_ptr[1] = num_frames_processed;

    } else {
      fprintf(stderr, "Decoding failed before end of files.\n");
      break;
    }
  }

  // Flush the VMAF context and compute the pooled VMAF score.
  if (vmaf_read_pictures(vmaf, NULL, NULL, 0)) {
    fprintf(stderr, "Problem flushing VMAF context.\n");
    return -1.0;
  }
  double pooled_vmaf_score = 0;
  if (vmaf_score_pooled(vmaf, model, VMAF_POOL_METHOD_MEAN, &pooled_vmaf_score, 0, frame_index_for_vmaf + 1)) {
    fprintf(stderr, "Problem computing pooled VMAF score.\n");
    return -1.0;
  }
  printf("Pooled VMAF score: %f", pooled_vmaf_score);
  output_buffer_ptr[3 + num_common_frames] = (float) pooled_vmaf_score;
  return 0.0;
}

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