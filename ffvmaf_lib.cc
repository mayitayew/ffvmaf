#include "ffvmaf_lib.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libvmaf/src/libvmaf.h"
}

#include <string>

static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext,
                         AVFrame *pFrame);

int InitalizeVmaf(VmafContext *vmaf,
                  VmafModel **model,
                  VmafModelCollection **model_collection,
                  uint64_t *model_collection_count, const char *model_buffer, uint64_t model_buffer_size) {
  // Attempt to load the model from memory. If this fails, attempt to load it as a model
  // collection. model_name could refer to a model or a model collection.
  VmafModelConfig model_config{
      .name = "default_vmaf_model",
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
      printf("AVPacket->pts %"
      PRId64, pPacket->pts);
      printf("\n");
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

static int copy_picture_data(AVFrame *src, VmafPicture *dst, unsigned bpc) {
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
                              int8_t* video_stream_index_reference,
                              int8_t* video_stream_index_test,
                              std::unordered_map<uint8_t, int64_t>& frame_timestamps,
                              VmafContext *vmaf,
                              VmafModel *model,
                              uintptr_t vmaf_scores_buffer) {
  printf("ComputeVmafForEachFrame...\n");

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

  AVCodec *pCodec_reference = NULL;
  AVCodecParameters *pCodecParameters_reference = NULL;

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
      printf("Reference video Codec: resolution %d x %d\n", pLocalCodecParameters->width,
             pLocalCodecParameters->height);
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
      printf("Test video Codec: resolution %d x %d\n", pLocalCodecParameters->width,
             pLocalCodecParameters->height);
      *video_stream_index_test = i;
      pCodec_test = pLocalCodec;
      pCodecParameters_test = pLocalCodecParameters;
    }
  }

  if (*video_stream_index_reference == -1 || *video_stream_index_test == -1) {
    fprintf(stderr, "Reference file does not contain a video stream!\n");
    return -1.0;
  }

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

  float *vmaf_scores_ptr = reinterpret_cast<float *>(vmaf_scores_buffer);
  for (int frame_index = 0; frame_index < 100; frame_index++) {

    bool reference_frame_decoded =
        (GetNextFrame(pFormatContext_reference, pCodecContext_reference, pPacket_reference, pFrame_reference,
                      *video_stream_index_reference) == 1);

    bool test_frame_decoded = (GetNextFrame(pFormatContext_test, pCodecContext_test, pPacket_test, pFrame_test,
                                            *video_stream_index_test) == 1);

    if (reference_frame_decoded && test_frame_decoded) {

      printf("Reference frame has height %d and width %d\n", pFrame_reference->height, pFrame_reference->width);
      printf("Test frame has height %d and width %d\n", pFrame_test->height, pFrame_test->width);

      frame_timestamps[frame_index] = pFrame_reference->best_effort_timestamp;

      VmafPicture reference_vmaf_picture, test_vmaf_picture;
      if (copy_picture_data(pFrame_reference, &reference_vmaf_picture, 8) != 0 ||
          copy_picture_data(pFrame_test, &test_vmaf_picture, 8) != 0) {
        fprintf(stderr, "Error allocating vmaf picture\n");
        return -1.0;
      }

      if (vmaf_read_pictures(vmaf, &reference_vmaf_picture, &test_vmaf_picture, frame_index) != 0) {
        fprintf(stderr, "Error reading pictures\n");
        return -1.0;
      }

      double vmaf_score;
      if (frame_index >= 5 && vmaf_score_at_index(vmaf, model, &vmaf_score, frame_index - 5) != 0) {
        fprintf(stderr, "Error computing vmaf score at index\n");
        return -1.0;
      }
      vmaf_scores_ptr[frame_index - 5] = vmaf_score;
      printf("Frame %d: vmaf score %f\n", frame_index - 5, vmaf_score);

    } else if (!reference_frame_decoded && !test_frame_decoded) {
      printf("Reached end of stream for both files.\n");
      break;
    } else if (!test_frame_decoded) {
      printf("Reached end of stream for test file.\n");
      break;
    } else if (!reference_frame_decoded) {
      printf("Reached end of stream for reference file.\n");
      break;
    }
  }

  // Get vmaf score for the last few indices.
  double vmaf_score;
  for (int i = 95; i < 100; i++) {
    if (vmaf_score_at_index(vmaf, model, &vmaf_score, i) != 0) {
      fprintf(stderr, "Error computing vmaf score at index\n");
      return -1.0;
    }
    vmaf_scores_ptr[i] = vmaf_score;
    printf("Frame %d: vmaf score %f\n", i, vmaf_score);
  }

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
    printf("Parsed a frame from the packet.\n");
    num_frames++;
  }
  return num_frames;
}