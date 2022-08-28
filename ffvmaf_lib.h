#ifndef FFVMAF_LIB_H
#define FFVMAF_LIB_H

#include <string>
extern "C" {
#include "libvmaf/src/libvmaf.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

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
                              uintptr_t min_score_test_frame_buffer, uintptr_t output_buffer);

int GetFrameAtTimestamp(AVFormatContext *pFormatContext,
                        AVCodecContext *pCodecContext,
                        AVFrame *pFrame,
                        AVPacket *pPacket,
                        int8_t video_stream_index,
                        int64_t timestamp,
                        uintptr_t frame);

#endif // FFVMAF_LIB_H