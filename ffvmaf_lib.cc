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
    fprintf(stderr, "Reading json model from buffer failed. Attempting to read it as a model collection.\n");

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

float ComputeVmafForEachFrame(const std::string &reference_file, const std::string &test_file) {
  printf("Computing VMAF...\n");

  av_register_all();
  AVFormatContext *pFormatContext_reference = avformat_alloc_context();
  AVFormatContext *pFormatContext_test = avformat_alloc_context();
  if (!pFormatContext_reference || !pFormatContext_test) {
    fprintf(stderr, "ERROR could not allocate memory for format contexts\n");
    return -1.0;
  }

  if (avformat_open_input(&pFormatContext_reference, reference_file.c_str(), NULL, NULL) != 0
      || avformat_open_input(&pFormatContext_test, test_file.c_str(), NULL, NULL) != 0) {
    fprintf(stderr, "ERROR could not open file.\n");
    return -1.0;
  }

  printf("Reference file format %s, duration %lld us, bit_rate %lld\n",
         pFormatContext_reference->iformat->name, pFormatContext_reference->duration,
         pFormatContext_reference->bit_rate);

  printf("Test file format %s, duration %lld us, bit_rate %lld\n",
         pFormatContext_test->iformat->name, pFormatContext_test->duration,
         pFormatContext_test->bit_rate);

  if (avformat_find_stream_info(pFormatContext_reference, NULL) < 0
      || avformat_find_stream_info(pFormatContext_test, NULL) < 0) {
    printf("ERROR could not get the stream info\n");
    return -1.0;
  }

  AVCodec *pCodec_reference = NULL;
  AVCodecParameters *pCodecParameters_reference = NULL;
  int reference_video_stream_index = -1;

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
      if (reference_video_stream_index == -1) {
        reference_video_stream_index = i;
        pCodec_reference = pLocalCodec;
        pCodecParameters_reference = pLocalCodecParameters;
      }

      printf("Reference video Codec: resolution %d x %d\n", pLocalCodecParameters->width,
             pLocalCodecParameters->height);
    }
  }

  AVCodec *pCodec_test = NULL;
  AVCodecParameters *pCodecParameters_test = NULL;
  int test_video_stream_index = -1;

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
      if (test_video_stream_index == -1) {
        test_video_stream_index = i;
        pCodec_test = pLocalCodec;
        pCodecParameters_test = pLocalCodecParameters;
      }

      printf("Test video Codec: resolution %d x %d\n", pLocalCodecParameters->width,
             pLocalCodecParameters->height);
    }
  }

  if (reference_video_stream_index == -1 || test_video_stream_index == -1) {
    fprintf(stderr, "Reference file does not contain a video stream!\n");
    return -1.0;
  }

  AVCodecContext *pCodecContext_reference = avcodec_alloc_context3(pCodec_reference);
  AVCodecContext *pCodecContext_test = avcodec_alloc_context3(pCodec_test);
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

  AVFrame *pFrame_reference = av_frame_alloc();
  AVFrame *pFrame_test = av_frame_alloc();
  if (!pFrame_reference || !pFrame_test) {
    fprintf(stderr, "failed to allocate memory for AVFrame\n");
    return -1.0;
  }

  AVPacket *pPacket_reference = av_packet_alloc();
  AVPacket *pPacket_test = av_packet_alloc();
  if (!pPacket_reference || !pPacket_test) {
    fprintf(stderr, "failed to allocate memory for AVPacket\n");
    return -1.0;
  }

  bool reference_frame_decoded = false;
  bool test_frame_decoded = false;
  for (int frame_index = 0; frame_index <= 100; frame_index++) {

    // Demux packet from reference file and read the current frame.
    while (av_read_frame(pFormatContext_reference, pPacket_reference) >= 0) {
      if (pPacket_reference->stream_index == reference_video_stream_index) {
        printf("AVPacket->pts %" PRId64, pPacket_reference->pts);
        printf("\n");
        if (decode_packet(pPacket_reference, pCodecContext_reference, pFrame_reference) < 0) {
          break;
        }
        reference_frame_decoded = true;
        break;
      }
      av_packet_unref(pPacket_reference);
    }

    // Demux packet from test file and read the current frame.
    while (av_read_frame(pFormatContext_test, pPacket_test) >= 0) {
      if (pPacket_test->stream_index == test_video_stream_index) {
        if (decode_packet(pPacket_test, pCodecContext_test, pFrame_test) < 0) {
          break;
        }
        test_frame_decoded = true;
        break;
      }
      av_packet_unref(pPacket_test);
    }

    if (reference_frame_decoded && test_frame_decoded) {
      printf("Decoded frame %d from reference and test files.\n", frame_index);
      reference_frame_decoded = false;
      test_frame_decoded = false;
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
}

static int decode_packet(AVPacket* pPacket, AVCodecContext* pCodecContext,
                         AVFrame* pFrame) {
  int response = avcodec_send_packet(pCodecContext, pPacket);

  if (response < 0) {
    printf("Error while sending a packet to the decoder: ");
    printf("\n");
    return response;
  }

  while (response >= 0) {
    response = avcodec_receive_frame(pCodecContext, pFrame);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      //printf("Done receiving frame.\n");
      break;
    } else if (response < 0) {
//      printf("Error while receiving a frame from the decoder: ");
//      printf("\n");
      return response;
    }
  }
  return 0;
}