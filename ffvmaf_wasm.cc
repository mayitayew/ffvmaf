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
uint32_t frame_index;

VmafPicture reference_picture, distorted_picture;

/* Prepare the in-memory buffer and loaders for VMAF models. */
VmafModelBuffer vmaf_model_buffer({"vmaf_v0.6.1neg.json",}, "models/");

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

extern "C" {
extern void renderVmafScore();
};

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

  if (vmaf_picture_alloc(&reference_picture, VmafPixelFormat::VMAF_PIX_FMT_YUV420P, 8, 720, 480)
      < 0) {
    fprintf(stderr, "Failed to allocate vmaf_picture for reference picture.\n");
    return -1;
  }

  printf("Reference picture after init has width %d.\n",
         reference_picture.w[0]);
  if (vmaf_picture_alloc(&distorted_picture, VmafPixelFormat::VMAF_PIX_FMT_YUV420P, 8, 720, 480)
      < 0) {
    fprintf(stderr, "Failed to allocate vmaf_picture for distorted picture.\n");
    return -1;
  }
  printf("Distorted picture after init has width %d.\n",
         distorted_picture.w[0]);

  model_collection_count++;
  frame_index = 0;
  // Download the model files and load them into memory.
  vmaf_model_buffer.DownloadModels();
  renderVmafScore();
}

std::vector<uint32_t> offsets{0, 345600, 432000};
std::vector<uint32_t> strides{720, 360, 360};

void ReadForVmaf(uintptr_t reference_frame, uintptr_t distorted_frame) {
  printf("Reference picture at vmaf has width %d.\n",
         reference_picture.w[0]);
  printf("Distorted picture at vmaf has width %d.\n",
         distorted_picture.w[0]);
  printf("Model collection count has value %d at vmaf.\n", model_collection_count);

  // Copy the frame data into the VmafPictures.
  for (unsigned i = 0; i < 3; i++) {
    uint8_t *reference_frame_data = reinterpret_cast<uint8_t *>(reference_frame);
    uint8_t *distorted_frame_data = reinterpret_cast<uint8_t *>(distorted_frame);

    uint8_t *reference_vmaf_picture = static_cast<uint8_t *>(reference_picture.data[i]);
    uint8_t *distorted_vmaf_picture = static_cast<uint8_t *>(distorted_picture.data[i]);

    for (unsigned j = 0; j < reference_picture.h[i]; j++) {
      // For reference frame to reference picture.
      memcpy(reference_vmaf_picture, &reference_frame_data[offsets[i]], sizeof(uint8_t) * reference_picture.w[i]);
      reference_frame_data += strides[i];
      reference_vmaf_picture += reference_picture.stride[i];

      // For distorted frame to distorted picture.
      memcpy(distorted_vmaf_picture, &distorted_frame_data[offsets[i]], sizeof(uint8_t) * distorted_picture.w[i]);
      distorted_frame_data += strides[i];
      distorted_vmaf_picture += distorted_picture.stride[i];
    }
  }

  if (vmaf_read_pictures(vmaf, &reference_picture, &distorted_picture, frame_index++) < 0) {
    fprintf(stderr, "Failed to read pictures.\n");
    return;
  }

  if (frame_index >= 200) {
    double score;
    if (vmaf_score_pooled(vmaf, model[0], VMAF_POOL_METHOD_MEAN,
                          &score, 0, frame_index - 2) < 0) {
      fprintf(stderr, "Failed to get score.\n");
    }
    printf("Score: %f\n", score);
  }

  return;
}

std::string GetVmafVersion() { return std::string(vmaf_version()); }

void ReadFile(const std::string& filename) { ReadInputFromFile(filename); }

// The functions below are exposed in the wasm module.
EMSCRIPTEN_BINDINGS(module) {
    emscripten::function("readFile", ReadFile);
    emscripten::function("getVmafVersion", GetVmafVersion);
    emscripten::function("renderScore", renderVmafScore);
}