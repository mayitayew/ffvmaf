#ifndef FFVMAF_LIB_H
#define FFVMAF_LIB_H

#include <string>
extern "C" {
#include "libvmaf/src/libvmaf.h"
#include "libavformat/avformat.h"
}

int InitalizeVmaf(VmafContext *vmaf,
                  VmafModel **model,
                  VmafModelCollection **model_collection,
                  uint64_t *model_collection_count, const char *model_buffer, uint64_t model_buffer_size);

void ReadInputFromFile(const std::string& filename);

#endif // FFVMAF_LIB_H