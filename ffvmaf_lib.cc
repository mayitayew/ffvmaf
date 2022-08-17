#include "ffvmaf_lib.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libvmaf/src/libvmaf.h"
}

#include <string>

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

void ReadInputFromFile(const std::string& filename) {
  printf("Reading input from file.\n");

  av_register_all();
  AVFormatContext* pFormatContext = avformat_alloc_context();
  if (!pFormatContext) {
    printf("ERROR could not allocate memory for Format Context\n");
    return;
  }

  AVInputFormat *iformat_for_reference = const_cast<AVInputFormat *>(av_find_input_format("mp4"));
  if (!iformat_for_reference) {
    printf("*******ERROR could not find input format*******\n");
  } else {
    printf("*******SUCCESS found input format*******\n");
  }

  fprintf(stderr, "filepath: %s\n", filename.c_str());
  if (avformat_open_input(&pFormatContext, filename.c_str(), NULL, NULL) != 0) {
    printf("ERROR could not open the file\n");
    return;
  }

  printf("format %s, duration %lld us, bit_rate %lld\n",
         pFormatContext->iformat->name, pFormatContext->duration,
         pFormatContext->bit_rate);

  printf("finding stream info from format\n");

  if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
    printf("ERROR could not get the stream info\n");
    return;
  }

  AVCodec* pCodec = NULL;
  AVCodecParameters* pCodecParameters = NULL;
  int video_stream_index = -1;

  // loop though all the streams and print its main information
  for (int i = 0; i < pFormatContext->nb_streams; i++) {
    AVCodecParameters* pLocalCodecParameters = NULL;
    pLocalCodecParameters = pFormatContext->streams[i]->codecpar;
    AVCodec* pLocalCodec = NULL;
    pLocalCodec = (AVCodec*) avcodec_find_decoder(pLocalCodecParameters->codec_id);

    if (pLocalCodec == NULL) {
      printf("ERROR unsupported codec!\n");
      // In this example if the codec is not found we just skip it
      continue;
    }

    // when the stream is a video we store its index, codec parameters and codec
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (video_stream_index == -1) {
        video_stream_index = i;
        pCodec = pLocalCodec;
        pCodecParameters = pLocalCodecParameters;
      }

      printf("Video Codec: resolution %d x %d\n", pLocalCodecParameters->width,
             pLocalCodecParameters->height);
    }
  }

  if (video_stream_index == -1) {
    printf("File %s does not contain a video stream!\n", "sample.mp4");
  }
}