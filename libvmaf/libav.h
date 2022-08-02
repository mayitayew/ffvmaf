#ifndef LIBAV_H_
#define LIBAV_H_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

AVFormatContext libav_avformat_alloc_context() {
  return avformat_alloc_context();
}

int libav_avformat_open_input(AVFormatContext * *ps, const char *url, const AVInputFormat *fmt, AVDictionary * *options) {
  return avformat_open_input(ps, url, fmt, options);
}


#endif  // LIBAV_H_