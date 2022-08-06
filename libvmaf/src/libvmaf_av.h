#ifndef LIBVMAF_AV_H_
#define LIBVMAF_AV_H_

#include <string>

#include "libvmaf.h"

int InitializeVmaf(VmafContext* vmaf, VmafModel** model,
                   VmafModelCollection** model_collection,
                   uint64_t *model_collection_count,
                   const char* model_path);

double ComputeVmafScore(const std::string& ref_video_url,
                       const std::string& dist_video_url);

#endif  // LIBVMAF_AV_H_