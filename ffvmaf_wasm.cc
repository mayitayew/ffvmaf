#include <emscripten/bind.h>
#include <emscripten/fetch.h>
#include <string>

#include <cstring>
#include "libvmaf/src/libvmaf.h"
#include "libvmaf/src/model_buffer.h"
#include "ffvmaf_lib.h"

/* Declare VMAF specific global variables including pointers to an initialized VMAF context and model. */
VmafConfiguration cfg = {
    .log_level = VMAF_LOG_LEVEL_INFO,
};
VmafContext *vmaf;
VmafModel **model;
VmafModelConfig model_config = {
    .name = "current_model",
};
VmafModelCollection **model_collection;
uint64_t model_collection_count;

/* Prepare the in-memory buffer and loaders for VMAF models. */
VmafModelBuffer vmaf_model_buffer({"vmaf_v0.6.1neg.json"}, "models/");

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
  if (vmaf_model_buffer.HasModel("reference_video")) {
    printf("Finished downloading reference_video! Computing VMAF!!\n");
    ComputeVmafScore(vmaf_model_buffer.GetBuffer("reference_video"),
                     vmaf_model_buffer.GetBufferSize("reference_video"),
                     vmaf_model_buffer.GetBuffer("reference_video"),
                     vmaf_model_buffer.GetBufferSize("reference_video"));
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

//double ComputeVmaf(uintptr_t reference_video_data, uint64_t reference_video_size,
//                  uintptr_t distorted_video_data, uint64_t distorted_video_size) {
//  const char* reference_video_buffer = reinterpret_cast<const char*>(reference_video_data);
//  const char* distorted_video_buffer = reinterpret_cast<const char*>(distorted_video_data);


double ComputeVmaf(const std::string &reference_video_url, const std::string &distorted_video_url) {
  asyncDownload(reference_video_url, "reference_video");
  return 10.0;
}

std::string GetVmafVersion() { return std::string(vmaf_version()); }

// The functions below are exposed in the wasm module.
EMSCRIPTEN_BINDINGS(module) {
    emscripten::function("computeVmaf", ComputeVmaf);
    emscripten::function("getVmafVersion", GetVmafVersion);
}