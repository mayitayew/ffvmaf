#include <inttypes.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libvmaf/libvmaf.h"
#include "ffvmaf_lib.h"

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
  ComputeVmafScore("ref.y4m", "dist.y4m");
}