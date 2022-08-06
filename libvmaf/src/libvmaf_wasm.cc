#include <emscripten/bind.h>
#include <emscripten/fetch.h>
#include "libvmaf.h"
#include "model_buffer.h"

VmafModelBuffer vmaf_model_buffer(
    {
        "vmaf_v0.6.1neg.json",
    },
    "model/");

void downloadSucceeded(emscripten_fetch_t* fetch) {
  const char* model_name = static_cast<char*>(fetch->userData);
  printf("Finished downloading %llu bytes from %s.\n", fetch->numBytes,
         fetch->url);
  vmaf_model_buffer.SetFetch(model_name, fetch);
  // Compute VMAF score for the videos.
  if (vmaf_model_buffer.AllModelsDownloaded()) {
    printf("Finished downloading all models.\n");
    vmaf_model_buffer.Summarize();
  }
}

void downloadFailed(emscripten_fetch_t* fetch) {
  auto fetch_ptr = static_cast<emscripten_fetch_t*>(fetch->userData);
  printf("Downloading %s failed, HTTP failure status code: %d.\n",
         fetch_ptr->url, fetch_ptr->status);
}

void asyncDownload(const std::string& url, const std::string& model_name) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  strcpy(attr.requestMethod, "GET");
  attr.userData = (void*)model_name.c_str();
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  attr.onsuccess = downloadSucceeded;
  attr.onerror = downloadFailed;
  emscripten_fetch(&attr, url.c_str());
}

// VMAF specific globals.
VmafConfiguration cfg = {
    .log_level = VMAF_LOG_LEVEL_INFO,
};
VmafContext* vmaf;
VmafModel** model;
VmafModelConfig model_config = {
    .name = "current_model",
};
VmafModelCollection** model_collection;
uint64_t model_collection_count;

int main(int argc, char** argv) {
  // Download the model files and load them into memory.
  vmaf_model_buffer.DownloadModels();

  // Initailize the VMAF context.
  int err = vmaf_init(&vmaf, cfg);
  if (err) {
    fprintf(stderr, "Failed to initialize VMAF context. error code: %d\n", err);
    return -1;
  }

  // Prepare the vmaf model object.
  const size_t model_sz = sizeof(*model);
  model = (VmafModel**)malloc(model_sz);
  memset(model, 0, model_sz);

  // Prepare the vmaf model collection object.
  const size_t model_collection_sz = sizeof(*model_collection);
  model_collection = (VmafModelCollection**)malloc(model_sz);
  memset(model_collection, 0, model_collection_sz);
  model_collection_count = 0;
}

std::string GetVmafVersion() { return std::string(vmaf_version()); }

EMSCRIPTEN_BINDINGS(module) {
    emscripten::function("getVmafVersion", GetVmafVersion);
}