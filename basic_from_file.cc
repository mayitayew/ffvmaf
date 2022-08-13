#include <inttypes.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "libvmaf/src/libvmaf.h"
}
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include "ffvmaf_lib.h"

#include "libvmaf/src/runfiles_util.h"

static int decode_packet(AVPacket *ref_pPacket,
                         AVCodecContext *ref_pCodecContext,
                         AVFrame *ref_pFrame,
                         AVPacket *dist_pPacket,
                         AVCodecContext *dist_pCodecContext,
                         AVFrame *dist_pFrame,
                         int *frame_index);

/* Declare VMAF specific global variables including pointers to an initialized VMAF context and model. */
VmafConfiguration cfg = {
    .log_level = VMAF_LOG_LEVEL_INFO,
};
VmafContext *vmaf;
VmafModel **model;
VmafModelConfig model_config = {
    .name = "current_model",
};
VmafModelCollection **model_collection;
uint64_t model_collection_count;

int InitializeVmafEntities() {
  int err = vmaf_init(&vmaf, cfg);
  if (err) {
    fprintf(stderr, "Failed to initialize VMAF context. error code: %d\n", err);
    return -1;
  }

  // Prepare the vmaf model object.
  const size_t model_sz = sizeof(*model);
  model = (VmafModel **) malloc(model_sz);
  memset(model, 0, model_sz);

  // Prepare the vmaf model collection object.
  const size_t model_collection_sz = sizeof(*model_collection);
  model_collection = (VmafModelCollection **) malloc(model_sz);
  memset(model_collection, 0, model_collection_sz);
  model_collection_count = 0;
  return 0;
}

int main(int argc, const char *argv[]) {

  if (InitializeVmafEntities() < 0) {
    return -1;
  }

  // Read model into buffer and initialize VMAF.
  char buffer[2500000];
  const std::string model_filepath =
      tools::GetModelRunfilesPath(argv[0]) + "vmaf_v0.6.1neg.json";
  FILE *f1 = fopen(model_filepath.c_str(), "rb");
  uint32_t bytes = fread(buffer, sizeof(char), 250000, f1);
  InitalizeVmaf(vmaf, model, model_collection, &model_collection_count, buffer, bytes);

  printf("Initializing AV side of things - containers, codecs and protocols.\n");
  av_register_all();
  AVFormatContext *ref_pFormatContext = avformat_alloc_context();
  if (!ref_pFormatContext) {
    printf("ERROR could not allocate memory for Format Context\n");
    return -1;
  }

  AVFormatContext *dist_pFormatContext = avformat_alloc_context();
  if (!dist_pFormatContext) {
    printf("ERROR could not allocate memory for Format Context\n");
    return -1;
  }

  const std::string ref_filepath = "file:" + tools::GetModelRunfilesPath(argv[0]) + "mux.mp4";
  if (avformat_open_input(&ref_pFormatContext, ref_filepath.c_str(), NULL, NULL) != 0) {
    printf("ERROR could not open the file\n");
    return -1;
  }

  if (avformat_find_stream_info(ref_pFormatContext, NULL) < 0) {
    printf("ERROR could not get the stream info\n");
    return -1;
  }

  const std::string dist_filepath = "file:" + tools::GetModelRunfilesPath(argv[0]) + "mux_modified.mp4";
  if (avformat_open_input(&dist_pFormatContext, dist_filepath.c_str(), NULL, NULL) != 0) {
    printf("ERROR could not open the file\n");
    return -1;
  }

  if (avformat_find_stream_info(dist_pFormatContext, NULL) < 0) {
    printf("ERROR could not get the stream info\n");
    return -1;
  }

  AVCodec *ref_pCodec = NULL;
  AVCodec *dist_pCodec = NULL;
  AVCodecParameters *ref_pCodecParameters = NULL;
  AVCodecParameters *dist_pCodecParameters = NULL;
  int ref_video_stream_index = -1;
  int dist_video_stream_index = -1;

  for (int i = 0; i < ref_pFormatContext->nb_streams; i++) {
    AVCodecParameters *pLocalCodecParameters = NULL;
    pLocalCodecParameters = ref_pFormatContext->streams[i]->codecpar;
    AVCodec *pLocalCodec = NULL;
    pLocalCodec = (AVCodec *) avcodec_find_decoder(pLocalCodecParameters->codec_id);
    if (pLocalCodec == NULL) {
      printf("ERROR unsupported codec!\n");
      continue;
    }
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (ref_video_stream_index == -1) {
        ref_video_stream_index = i;
        printf("Reference video stream index is %d\n", ref_video_stream_index);
        ref_pCodec = pLocalCodec;
        ref_pCodecParameters = pLocalCodecParameters;
        break;
      }
    }
  }

  for (int i = 0; i < dist_pFormatContext->nb_streams; i++) {
    AVCodecParameters *pLocalCodecParameters = NULL;
    pLocalCodecParameters = dist_pFormatContext->streams[i]->codecpar;
    AVCodec *pLocalCodec = NULL;
    pLocalCodec = (AVCodec *) avcodec_find_decoder(pLocalCodecParameters->codec_id);
    if (pLocalCodec == NULL) {
      printf("ERROR unsupported codec!\n");
      continue;
    }
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (dist_video_stream_index == -1) {
        dist_video_stream_index = i;
        printf("Distorted video stream index is %d\n", dist_video_stream_index);
        dist_pCodec = pLocalCodec;
        dist_pCodecParameters = pLocalCodecParameters;
        break;
      }
    }
  }

  // Once video stream is found, initialize codec context, frame and packet.
  if (ref_video_stream_index == -1 || dist_video_stream_index == -1) {
    printf("File %s does not contain a video stream!\n", "sample.mp4");
    return -1;
  }

  AVCodecContext *ref_pCodecContext = avcodec_alloc_context3(ref_pCodec);
  AVCodecContext *dist_pCodecContext = avcodec_alloc_context3(dist_pCodec);
  if (!ref_pCodecContext || !dist_pCodecContext) {
    printf("failed to allocated memory for AVCodecContext\n");
    return -1;
  }
  printf("Finished initializing codec contexts.\n");
  if (avcodec_parameters_to_context(ref_pCodecContext, ref_pCodecParameters) < 0
      || avcodec_parameters_to_context(dist_pCodecContext, dist_pCodecParameters) < 0) {
    printf("failed to copy codec params to codec context\n");
    return -1;
  }

  if (avcodec_open2(ref_pCodecContext, ref_pCodec, NULL) < 0
      || avcodec_open2(dist_pCodecContext, dist_pCodec, NULL) < 0) {
    printf("failed to open codec through avcodec_open2\n");
    return -1;
  }

  AVFrame *ref_pFrame = av_frame_alloc();
  AVFrame *dist_pFrame = av_frame_alloc();
  if (!ref_pFrame || !dist_pFrame) {
    printf("failed to allocate memory for AVFrame\n");
    return -1;
  }
  AVPacket *ref_pPacket = av_packet_alloc();
  AVPacket *dist_pPacket = av_packet_alloc();
  if (!ref_pPacket || !dist_pPacket) {
    printf("failed to allocate memory for AVPacket\n");
    return -1;
  }

  int response = 0;
  int how_many_packets_to_process = 100;
  printf("Ready to start processing frames\n");
  int vmaf_picture_index = 0;
  while (av_read_frame(ref_pFormatContext, ref_pPacket) >= 0) {

  } av_read_frame(dist_pFormatContext, ref_pPacket) >= 0) {
    // if it's the video stream
    if (ref_pPacket->stream_index == ref_video_stream_index) {
      response = decode_packet(ref_pPacket,
                               ref_pCodecContext,
                               ref_pFrame,
                               dist_pPacket,
                               dist_pCodecContext,
                               dist_pFrame,
                               &vmaf_picture_index);
      if (response < 0) break;

      if (--how_many_packets_to_process <= 0) break;
    }
    av_packet_unref(ref_pPacket);
  }

  // Compute vmaf score.
  double score;
  int err = vmaf_score_pooled(vmaf, model[0], VMAF_POOL_METHOD_MEAN,
                              &score, 0, 90);
  printf("THE COMPUTED VMAF SCORE IS at index %d is %f\n", 90, score);



  // Flush vmaf context.
  vmaf_read_pictures(vmaf, NULL, NULL, 0);
  printf("Releasing VMAF resources.\n");
  vmaf_model_destroy(model[0]);
  vmaf_model_collection_destroy(model_collection[0]);
  vmaf_close(vmaf);
  printf("releasing all AV the resources\n");
  avformat_close_input(&ref_pFormatContext);
  avformat_close_input(&dist_pFormatContext);
  av_packet_free(&ref_pPacket);
  av_frame_free(&ref_pFrame);

  av_packet_free(&dist_pPacket);
  av_frame_free(&dist_pFrame);

  avcodec_free_context(&ref_pCodecContext);
  avcodec_free_context(&dist_pCodecContext);
  return 0;
}

static int decode_packet(AVPacket *ref_pPacket, AVCodecContext *ref_pCodecContext,
                         AVFrame *ref_pFrame, AVPacket *dist_pPacket, AVCodecContext *dist_pCodecContext,
                         AVFrame *dist_pFrame, int *frame_index) {
  int response = avcodec_send_packet(ref_pCodecContext, ref_pPacket);
  if (response < 0) {
    printf("Error while sending a ref packet to the decoder: ");
    printf("\n");
    return response;
  }

  while (response >= 0) {
    response = avcodec_receive_frame(ref_pCodecContext, ref_pFrame);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      printf("Done receiving frame.\n");
      break;
    } else if (response < 0) {
      printf("Error while receiving a frame from the decoder: ");
      printf("\n");
      return response;
    }

    if (response >= 0) {
      // At this point ref_pFrame has been received.
      break;
    }
  }

  int dist_response = avcodec_send_packet(dist_pCodecContext, dist_pPacket);
  if (dist_response < 0) {
    printf("Error while sending a ref packet to the decoder: ");
    printf("\n");
    return dist_response;
  }

  while (dist_response >= 0) {
    dist_response = avcodec_receive_frame(dist_pCodecContext, dist_pFrame);
    if (dist_response == AVERROR(EAGAIN) || dist_response == AVERROR_EOF) {
      printf("Done receiving frame.\n");
      break;
    } else if (dist_response < 0) {
      printf("Error while receiving a frame from the decoder: ");
      printf("\n");
      return dist_response;
    }

    if (dist_response >= 0) {
      // At this point dist_pFrame has been received.
      break;
    }
  }


  // Convert AVFrames to VMAFPictures.
  VmafPicture ref_picture, dist_picture;
  if (vmaf_picture_alloc(&ref_picture, VmafPixelFormat::VMAF_PIX_FMT_YUV420P, 8, ref_pFrame->width, ref_pFrame->height)
      < 0) {
    fprintf(stderr, "Failed to allocate vmaf_picture for reference picture.\n");
    return 0;
  }

  if (vmaf_picture_alloc(&dist_picture,
                         VmafPixelFormat::VMAF_PIX_FMT_YUV420P,
                         8,
                         dist_pFrame->width,
                         dist_pFrame->height)
      < 0) {
    fprintf(stderr, "Failed to allocate vmaf_picture for distorted picture.\n");
    return 0;
  }

  printf("Ref pframe has pts %d and Dist pFrame has pts %d\n", ref_pFrame->pts, dist_pFrame->pts);

  // Copy from the AVFrames to the VmafPictures.
  for (unsigned i = 0; i < 3; i++) {
    uint8_t *ref_frame_data = ref_pFrame->data[i];
    uint8_t *ref_picture_data = static_cast<uint8_t *>(ref_picture.data[i]);
    for (unsigned j = 0; j < ref_picture.h[i]; j++) {
      memcpy(ref_picture_data, ref_frame_data, sizeof(*ref_picture_data) * ref_picture.w[i]);
      ref_frame_data += dist_pFrame->linesize[i];
      ref_picture_data += ref_picture.stride[i];
    }
  }

  for (unsigned i = 0; i < 3; i++) {
    uint8_t *dist_frame_data = dist_pFrame->data[i];
    uint8_t *dist_picture_data = static_cast<uint8_t *>(dist_picture.data[i]);
    for (unsigned j = 0; j < ref_picture.h[i]; j++) {
      memcpy(dist_picture_data, dist_frame_data, sizeof(*dist_picture_data) * dist_picture.w[i]);
      dist_frame_data += dist_pFrame->linesize[i];
      dist_picture_data += dist_picture.stride[i];
    }
  }

  int err = vmaf_read_pictures(vmaf, &ref_picture, &dist_picture, *frame_index);
  *frame_index = *frame_index + 1;
  if (err != 0) {
    fprintf(stderr, "\nproblem reading pictures. error_code: %d\n", err);
  }

  return 0;
}
