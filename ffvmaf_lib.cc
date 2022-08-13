#include "ffvmaf_lib.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libvmaf/src/libvmaf.h"
}

#include <string>

InMemoryAVIOContext::InMemoryAVIOContext(const char *buffer, uint64_t buffer_size)
    : buffer_(buffer), buffer_size_(buffer_size), position_(0) {
  output_buffer_size_ = 1<<16;
  output_buffer_ = (uint8_t *) av_malloc(output_buffer_size_);
  if (!output_buffer_) {
    fprintf(stderr, "FATAL ERROR: Could not allocate output buffer.\n");
  }
  avio_ctx_ = avio_alloc_context(output_buffer_,
                                 output_buffer_size_,
                                 0,
                                 this,
                                 &InMemoryAVIOContext::read, NULL, &InMemoryAVIOContext::seek);
}

int InMemoryAVIOContext::read(void *opaque, uint8_t *buf, int buf_size) {
  printf("InMemoryAVIOContext::read\n");
  InMemoryAVIOContext *context = static_cast<InMemoryAVIOContext *>(opaque);
  const int bytes_to_read = std::min((int) (context->buffer_size_ - context->position_), buf_size);
  memcpy(buf, context->buffer_ + context->position_, bytes_to_read);
  context->position_ += bytes_to_read;
  printf("InMemoryAVIOContext::read: %d bytes read\n", bytes_to_read);
  printf("InMemoryAVIOContext::read: %d bytes left\n", (int) (context->buffer_size_ - context->position_));
  return bytes_to_read;
}

int64_t InMemoryAVIOContext::seek(void *opaque, int64_t offset, int whence) {
   printf("seek\n");
   InMemoryAVIOContext *context = static_cast<InMemoryAVIOContext *>(opaque);
   if (offset + whence > context->buffer_size_) {
       context->position_ = context->buffer_size_;
       return -1;
   }
   context->position_ = offset + whence;
   return 0;
}

AVIOContext *InMemoryAVIOContext::GetAVIOContext() {
  return avio_ctx_;
}

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

double ComputeVmafScore(const char *reference_video_buffer,
                        uint64_t reference_video_buffer_size,
                        const char *distorted_video_buffer,
                        uint64_t distorted_video_buffer_size) {

  printf("Computing VMAF score...\n");
  av_register_all();
  InMemoryAVIOContext reference_video_context(reference_video_buffer, reference_video_buffer_size);
  AVFormatContext *pFormatContext_for_reference = avformat_alloc_context();
  if (!pFormatContext_for_reference) {
    printf("ERROR could not allocate memory for Format Context\n");
    return -1;
  }
  pFormatContext_for_reference->pb = reference_video_context.GetAVIOContext();
  pFormatContext_for_reference->flags |= AVFMT_FLAG_CUSTOM_IO;

  InMemoryAVIOContext distorted_video_context(distorted_video_buffer, distorted_video_buffer_size);
  AVFormatContext *pFormatContext_for_distorted = avformat_alloc_context();
  if (!pFormatContext_for_distorted) {
    printf("ERROR could not allocate memory for Format Context\n");
    return -1;
  }
  pFormatContext_for_distorted->pb = distorted_video_context.GetAVIOContext();
  pFormatContext_for_distorted->flags |= AVFMT_FLAG_CUSTOM_IO;

  AVInputFormat *iformat_for_reference = const_cast<AVInputFormat *>(av_find_input_format("mp4"));
  if (!iformat_for_reference) {
    printf("ERROR could not find input format\n");
    return -1;
  }
  int err = avformat_open_input(&pFormatContext_for_reference, "",
                                iformat_for_reference, NULL);
  if (err < 0) {
    fprintf(stderr, "ERROR could not open reference video file. error_code: %s\n", "unknown");
    return -1;
  }
//
//  printf("Opened reference video file.\n");
//
//  if (avformat_open_input(&pFormatContext_for_distorted, "unusedDistUrl",
//                          NULL, NULL) != 0) {
//    printf("ERROR could not open distorted video file from %s\n");
//    return -1;
//  }

  printf("Ready with input files with AVIOContexts.\n");

  if (avformat_find_stream_info(pFormatContext_for_reference, NULL) < 0) {
    printf("ERROR could not find stream information for reference video.\n");
    return -1;
  }

//  if (avformat_find_stream_info(pFormatContext_for_distorted, NULL) < 0) {
//    printf("ERROR could not find stream information for distorted video.\n");
//    return -1;
//  }

  printf("Found stream info for refernce and distorted video. num_streams: %d \n",
         pFormatContext_for_reference->nb_streams);
  AVCodec *pCodec_for_reference = NULL;
  AVCodecParameters *pCodecParameters_for_reference = NULL;
  int reference_video_stream_index = -1;

  AVCodec *pCodec_for_distorted = NULL;
  AVCodecParameters *pCodecParameters_for_distorted = NULL;
  int distorted_video_stream_index = -1;

  // Find the video stream and resolution for the reference video.
  for (int i = 0; i < pFormatContext_for_reference->nb_streams; i++) {
    AVCodecParameters *pLocalCodecParameters = NULL;
    pLocalCodecParameters = pFormatContext_for_reference->streams[i]->codecpar;
    AVCodec *pLocalCodec = NULL;
    pLocalCodec = (AVCodec *) avcodec_find_decoder(pLocalCodecParameters->codec_id);

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

  return 10.0;
}