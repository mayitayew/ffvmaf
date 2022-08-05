#ifndef VMAF_MODEL_BUFFER_H_
#define VMAF_MODEL_BUFFER_H_

#include <emscripten/fetch.h>
#include <unordered_map>

void asyncDownload(const std::string& url, const std::string& model_name);

class VmafModelBuffer {
 public:
  VmafModelBuffer(const std::vector<std::string>& model_names,
                  const std::string& base_url)
      : base_url_(base_url) {
    fetches_by_name_.reserve(model_names.size());
    for (const auto& model_name : model_names) {
      fetches_by_name_[model_name] = nullptr;
    }
  }

  ~VmafModelBuffer() {
    for (const auto& [model_name, fetch] : fetches_by_name_) {
      if (fetch != nullptr) {
        emscripten_fetch_close(fetch);
      }
    }
  }

  void DownloadModels() {
    for (const auto& [model_name, fetch] : fetches_by_name_) {
      if (fetch == nullptr) {
        asyncDownload(ModelUrl(model_name), model_name);
      }
    }
  }

  bool AllModelsDownloaded() {
    return download_count_ == fetches_by_name_.size();
  }

  void Summarize() {
    fprintf(stdout, "Summarizing the models...\n");
    for (const auto& [model_name, fetch] : fetches_by_name_) {
      if (fetch != nullptr) {
        fprintf(stdout, "Model %s has been downloaded and has size %llu.\n",
                model_name.c_str(), fetch->numBytes);
      }
    }
  }

  void SetFetch(const std::string& model_name,
                emscripten_fetch_t* fetch) const {
    fetches_by_name_[model_name] = fetch;
    download_count_++;
  }

  bool HasModel(const std::string& model_name) const {
    return fetches_by_name_.find(model_name) != fetches_by_name_.end();
  }

  uint64_t GetBufferSize(const std::string& model_name) const {
    emscripten_fetch_t* fetch =
        static_cast<emscripten_fetch_t*>(fetches_by_name_[model_name]);
    return fetch->numBytes;
  }

  const char* GetBuffer(const std::string& model_name) const {
    emscripten_fetch_t* fetch =
        static_cast<emscripten_fetch_t*>(fetches_by_name_[model_name]);
    return fetch->data;
  }

 private:
  std::string ModelUrl(const std::string& model_name) const {
    return base_url_ + model_name;
  }

  mutable std::unordered_map<std::string, emscripten_fetch_t*> fetches_by_name_;
  const std::string base_url_;
  mutable std::atomic<int> download_count_ = 0;
};

#endif // VMAF_MODEL_BUFFER_H_