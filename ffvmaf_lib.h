#ifndef FFVMAF_LIB_H
#define FFVMAF_LIB_H

#include <string>
extern "C" {
#include "libvmaf/libvmaf.h"
}

int InitalizeVmaf(VmafContext *vmaf,
                  VmafModel **model,
                  VmafModelCollection **model_collection,
                  uint64_t *model_collection_count, const char* model_buffer, uint64_t model_buffer_size);

double ComputeVmafScore(const std::string& ref_video_url,
                   const std::string& dist_video_url);

#endif // FFVMAF_LIB_H