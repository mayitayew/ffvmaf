#include "libvmaf_av.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}
#include <string>

#include "libvmaf.h"
#include "read_json_model.h"

int InitializeVmaf(VmafContext* vmaf, VmafModel** model,
                   VmafModelCollection** model_collection,
                   uint64_t* model_collection_count,
                   const char* model_path) {
  // Load the model from memory. If this fails, attempt to load it as a model
  // collection. Since model name could refer to a model or a model collection.
  VmafModelConfig model_config{
    .name = "default_vmaf_model",
  };
  fprintf(stderr, "ready to read model from buffer\n");
  if (vmaf_read_json_model_from_path(&model[0], &model_config,
                                       model_path)) {
    int err = vmaf_read_json_model_collection_from_path(
        &model[0], &model_collection[*model_collection_count], &model_config,
        model_path);

    if (err) {
      fprintf(stderr, "Problem loading model. error_code: %d\n", err);
      return -1;
    }

    err = vmaf_use_features_from_model_collection(
        vmaf, model_collection[*model_collection_count]);
    if (err) {
      fprintf(stderr,
              "Problem loading feature extractors from "
              "model collection. error_code: %d\n",
              err);
      return -1;
    }

    *model_collection_count = (*model_collection_count + 1);
  }

  // Register the feature extractors required by the model.
  if (vmaf_use_features_from_model(vmaf, model[0])) {
    fprintf(stderr, "Problem registering feature extractors for the model.\n");
    return -1;
  }

  return 0;
}

double ComputeVmafScore(const std::string& ref_video_url,
                        const std::string& dist_video_url) {
  AVFormatContext* pFormatContext_for_reference = avformat_alloc_context();
  if (!pFormatContext_for_reference) {
    printf("ERROR could not allocate memory for Format Context\n");
    return -1;
  }

  AVFormatContext* pFormatContext_for_distorted = avformat_alloc_context();
  if (!pFormatContext_for_distorted) {
    printf("ERROR could not allocate memory for Format Context\n");
    return -1;
  }

  if (avformat_open_input(&pFormatContext_for_reference, ref_video_url.c_str(),
                          NULL, NULL) != 0) {
    printf("ERROR could not open reference video file from %s\n",
           ref_video_url.c_str());
    return -1;
  }

  if (avformat_open_input(&pFormatContext_for_distorted, dist_video_url.c_str(),
                          NULL, NULL) != 0) {
    printf("ERROR could not open distorted video file from %s\n",
           dist_video_url.c_str());
    return -1;
  }

  if (avformat_find_stream_info(pFormatContext_for_reference, NULL) < 0) {
    printf("ERROR could not find stream information for reference video.\n");
    return -1;
  }

  if (avformat_find_stream_info(pFormatContext_for_distorted, NULL) < 0) {
    printf("ERROR could not find stream information for distorted video.\n");
    return -1;
  }

  AVCodec* pCodec_for_reference = NULL;
  AVCodecParameters* pCodecParameters_for_reference = NULL;
  int reference_video_stream_index = -1;

  AVCodec* pCodec_for_distorted = NULL;
  AVCodecParameters* pCodecParameters_for_distorted = NULL;
  int distorted_video_stream_index = -1;

  // Find the video stream and resolution for the reference video.
  for (int i = 0; i < pFormatContext_for_reference->nb_streams; i++) {
    AVCodecParameters* pLocalCodecParameters = NULL;
    pLocalCodecParameters = pFormatContext_for_reference->streams[i]->codecpar;
    AVCodec* pLocalCodec = NULL;
    pLocalCodec = (AVCodec*) avcodec_find_decoder(pLocalCodecParameters->codec_id);

    if (pLocalCodec == NULL) {
      fprintf(stderr, "ERROR unsupported codec!\n");
      return -1;
    }

    // when the stream is a video we store its index, codec parameters and codec
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (reference_video_stream_index == -1) {
        reference_video_stream_index = i;
        pCodec_for_reference = pLocalCodec;
        pCodecParameters_for_reference = pLocalCodecParameters;
      }

      printf("Reference video Codec: resolution %d x %d\n",
             pCodecParameters_for_reference->width,
             pCodecParameters_for_reference->height);
      break;
    }
  }

  if (reference_video_stream_index == -1) {
    fprintf(stderr, "ERROR could not find video stream in reference video.\n");
    return -1;
  }

  // Find the video stream and resolution for the distorted video.
  for (int i = 0; i < pFormatContext_for_distorted->nb_streams; i++) {
    AVCodecParameters* pLocalCodecParameters = NULL;
    pLocalCodecParameters = pFormatContext_for_distorted->streams[i]->codecpar;
    AVCodec* pLocalCodec = NULL;
    pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);

    if (pLocalCodec == NULL) {
      fprintf(stderr, "ERROR unsupported codec!\n");
      return -1;
    }

    // when the stream is a video we store its index, codec parameters and codec
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (reference_video_stream_index == -1) {
        reference_video_stream_index = i;
        pCodec_for_distorted = pLocalCodec;
        pCodecParameters_for_distorted = pLocalCodecParameters;
      }

      printf("Distorted video Codec: resolution %d x %d\n",
             pCodecParameters_for_distorted->width,
             pCodecParameters_for_distorted->height);
      break;
    }
  }

  if (distorted_video_stream_index == -1) {
    fprintf(stderr, "ERROR could not find video stream in distorted video.\n");
    return -1;
  }

  return 10.0;
}
