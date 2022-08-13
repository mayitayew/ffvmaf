#ifndef FFVMAF_LIB_H
#define FFVMAF_LIB_H

#include <string>
extern "C" {
#include "libvmaf/src/libvmaf.h"
#include "libavformat/avformat.h"
}

class InMemoryAVIOContext {
 public:
  InMemoryAVIOContext(const char *buffer, uint64_t buffer_size);

  ~InMemoryAVIOContext() {
    av_free((void*) buffer_);
  }

  static int read(void *opaque, uint8_t *buf, int buf_size);

  static int64_t seek(void *opaque, int64_t offset, int whence);

  AVIOContext* GetAVIOContext();

 private:
  AVIOContext *avio_ctx_;

  // The in-memory buffer managed by this context.
  const char *buffer_;
  uint64_t buffer_size_;
  int position_;

  // Output buffer for the AVIOContext.
  uint8_t *output_buffer_;
  int output_buffer_size_;
};

int InitalizeVmaf(VmafContext *vmaf,
                  VmafModel **model,
                  VmafModelCollection **model_collection,
                  uint64_t *model_collection_count, const char *model_buffer, uint64_t model_buffer_size);

double ComputeVmafScore(const char *reference_video_buffer,
                        uint64_t reference_video_buffer_size,
                        const char *distorted_video_buffer,
                        uint64_t distorted_video_buffer_size);

double ComputeVmaf(uintptr_t reference_frame, uintptr_t distorted_frame);

#endif // FFVMAF_LIB_H