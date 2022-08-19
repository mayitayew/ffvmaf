#include <emscripten/bind.h>
#include <emscripten/fetch.h>
#include <string>

#include <cstring>
#include <vector>
#include "libvmaf/src/libvmaf.h"
#include "libvmaf/src/model_buffer.h"
#include "ffvmaf_lib.h"

/* Declare VMAF specific global variables including pointers to an initialized VMAF context and model. */
VmafConfiguration cfg = {
    .log_level = VMAF_LOG_LEVEL_INFO,
    .n_threads = 1,
};
VmafContext *vmaf;
VmafModel **model;
VmafModelConfig model_config = {
    .name = "current_model",
};
VmafModelCollection **model_collection;
uint64_t model_collection_count;

/* Declare libav{format, util, codec} specific global variables. */
AVFormatContext *pFormatContext_reference;
AVFormatContext *pFormatContext_test;
AVCodecContext *pCodecContext_reference;
AVCodecContext *pCodecContext_test;
AVFrame *pFrame_reference;
AVFrame *pFrame_test;
AVPacket *pPacket_reference;
AVPacket *pPacket_test;
int8_t *video_stream_index_reference;
int8_t *video_stream_index_test;

// Frame number to timestamp mapping.
std::unordered_map <uint8_t, int64_t> frame_timestamps;

/* Prepare the in-memory buffer and loaders for VMAF models. */
VmafModelBuffer vmaf_model_buffer({"vmaf_v0.6.1neg.json",}, "https://localhost:3000/models/");

void downloadSucceeded(emscripten_fetch_t *fetch) {
  const char *model_name = static_cast<char *>(fetch->userData);
  printf("Finished downloading %llu bytes from %s.\n", fetch->numBytes,
         fetch->url);
  vmaf_model_buffer.SetFetch(model_name, fetch);
  if (vmaf_model_buffer.AllModelsDownloaded()) {
    printf("Finished downloading all models.\n");
    InitalizeVmaf(vmaf, model, model_collection, &model_collection_count,
                  vmaf_model_buffer.GetBuffer("vmaf_v0.6.1neg.json"),
                  vmaf_model_buffer.GetBufferSize("vmaf_v0.6.1neg.json"));
  }
}

void downloadFailed(emscripten_fetch_t *fetch) {
  auto fetch_ptr = static_cast<emscripten_fetch_t *>(fetch->userData);
  printf("Downloading %s failed, HTTP failure status code: %d.\n",
         fetch_ptr->url, fetch_ptr->status);
}

void asyncDownload(const std::string &url, const std::string &model_name) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  strcpy(attr.requestMethod, "GET");
  attr.userData = (void *) model_name.c_str();
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  attr.onsuccess = downloadSucceeded;
  attr.onerror = downloadFailed;
  emscripten_fetch(&attr, url.c_str());
}

int main(int argc, char **argv) {
  // Libav related initializations.
  av_register_all();
  pFormatContext_reference = avformat_alloc_context();
  pFormatContext_test = avformat_alloc_context();
  if (!pFormatContext_reference || !pFormatContext_test) {
    fprintf(stderr, "ERROR could not allocate memory for format contexts\n");
    return -1;
  }

  pFrame_reference = av_frame_alloc();
  pFrame_test = av_frame_alloc();
  if (!pFrame_reference || !pFrame_test) {
    fprintf(stderr, "failed to allocate memory for AVFrame\n");
    return -1;
  }

  pPacket_reference = av_packet_alloc();
  pPacket_test = av_packet_alloc();
  if (!pPacket_reference || !pPacket_test) {
    fprintf(stderr, "failed to allocate memory for AVPacket\n");
    return -1;
  }
  *video_stream_index_reference = -1;
  *video_stream_index_test = -1;
  frame_timestamps.reserve(100);

  // Initailize the VMAF context.
  int err = vmaf_init(&vmaf, cfg);
  if (err) {
    fprintf(stderr, "Failed to initialize VMAF context. error code: %d\n", err);
    return -1;
  }

  // Prepare the vmaf model object.
  const size_t model_sz = sizeof(*model);
  model = (VmafModel **) malloc(model_sz);
  memset(model, 0, model_sz);

  // Prepare the vmaf model collection object.
  const size_t model_collection_sz = sizeof(*model_collection);
  model_collection = (VmafModelCollection **) malloc(model_sz);
  memset(model_collection, 0, model_collection_sz);
  model_collection_count = 0;

  // Download the model files and load them into memory.
  vmaf_model_buffer.DownloadModels();
}

std::string GetVmafVersion() { return std::string(vmaf_version()); }

void ComputeVmaf(const std::string &reference_file, const std::string &test_file, uintptr_t vmaf_scores_buffer) {
  ComputeVmafForEachFrame(reference_file,
                          test_file,
                          pFormatContext_reference,
                          pFormatContext_test,
                          pCodecContext_reference,
                          pCodecContext_test,
                          pFrame_reference,
                          pFrame_test,
                          pPacket_reference,
                          pPacket_test,
                          video_stream_index_reference,
                          video_stream_index_test,
                          frame_timestamps,
                          vmaf,
                          model[0],
                          vmaf_scores_buffer);
}

int GetFramesAtIndex(uint8_t index, uintptr_t reference_frame, uintptr_t test_frame) {
  return GetFrameAtTimestamp(pFormatContext_reference,
                      pCodecContext_reference,
                      pFrame_reference,
                      pPacket_reference,
                      *video_stream_index_reference,
                      frame_timestamps[index],
                      reference_frame);
}

// The functions below are exposed in the wasm module.
EMSCRIPTEN_BINDINGS(module) {
    emscripten::function("computeVmaf", ComputeVmaf, emscripten::allow_raw_pointers());
    emscripten::function("getVmafVersion", GetVmafVersion);
}