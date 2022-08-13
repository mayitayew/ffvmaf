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
#include "ffvmaf_lib.h"
#include "libvmaf/src/runfiles_util.h"

int main(int argc, const char* argv[]) {
  printf("initializing all the containers, codecs and protocols.\n");

  // AVFormatContext holds the header information from the format (Container)
  // Allocating memory for this component
  // http://ffmpeg.org/doxygen/trunk/structAVFormatContext.html
  AVFormatContext *pFormatContext = avformat_alloc_context();
  if (!pFormatContext) {
    printf("ERROR could not allocate memory for Format Context\n");
    return -1;
  }
  printf("FFmpeg Format Context allocated and VMAF's version is %s!.\n", vmaf_version());

  char buffer[2500000];
  const std::string model =
      tools::GetModelRunfilesPath(argv[0]) + "720p.mp4";
  FILE* f1 = fopen(model.c_str(), "rb");
  uint32_t bytes = fread(buffer, sizeof(char), 2500000, f1);

  ComputeVmafScore(buffer, bytes, buffer, bytes);
}