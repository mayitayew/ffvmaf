#ifndef FFVMAF_LIB_H
#define FFVMAF_LIB_H

#include <string>
extern "C" {
#include "libvmaf/src/libvmaf.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

enum class VmafComputeStatus {
  INITIALIZATION_ERROR = -1,
  SUCCESS,
  INPUT_VIDEO_ERROR,
  CANCELLED,
  VMAF_ERROR_COPYING_FRAMES,
  VMAF_ERROR_READING_FRAMES,
  VMAF_ERROR_COMPUTING_AT_INDEX,
  VMAF_ERROR_FLUSHING_CONTEXT,
  VMAF_ERROR_COMPUTING_POOLED,  // 7
};

int InitializeVmaf(VmafContext *vmaf,
                   VmafModel **model,
                   VmafModelCollection **model_collection,
                   uint64_t *model_collection_count,
                   const char *model_buffer,
                   uint64_t model_buffer_size,
                   bool use_phone_model);

VmafComputeStatus ComputeVmafForEachFrame(const std::string &reference_file,
                              const std::string &test_file,
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

#endif // FFVMAF_LIB_H