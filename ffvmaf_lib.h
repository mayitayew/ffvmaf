#ifndef FFVMAF_LIB_H
#define FFVMAF_LIB_H

#include <string>
extern "C" {
#include "libvmaf/src/libvmaf.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

#include <unordered_map>

int InitializeVmaf(VmafContext *vmaf,
                   VmafModel **model,
                   VmafModelCollection **model_collection,
                   uint64_t *model_collection_count,
                   const char *model_buffer,
                   uint64_t model_buffer_size,
                   bool use_phone_model);

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
                              VmafModel *model, uintptr_t output_buffer);

int GetFrameAtTimestamp(AVFormatContext *pFormatContext,
                        AVCodecContext *pCodecContext,
                        AVFrame *pFrame,
                        AVPacket *pPacket,
                        int8_t video_stream_index,
                        int64_t timestamp,
                        uintptr_t frame);

#endif // FFVMAF_LIB_H