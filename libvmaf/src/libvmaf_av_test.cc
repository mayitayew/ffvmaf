#include "libvmaf_av.h"

#include "gmock/gmock.h"
#include "libvmaf.h"

#include "runfiles_util.h"

class LibvmafAVTest : public testing::Test {
 protected:
  LibvmafAVTest() {
    VmafConfiguration config = {
        .log_level = VMAF_LOG_LEVEL_INFO,
    };
    int err = vmaf_init(&vmaf_, config);
    if (err) {
      fprintf(stderr, "Failed to initialize VMAF context. error code: %d\n",
              err);
      exit(EXIT_FAILURE);
    }

    // Prepare the vmaf model object.
    const size_t model_sz = sizeof(*model_);
    model_ = (VmafModel**)malloc(model_sz);
    memset(model_, 0, model_sz);

    // Prepare the vmaf model collection object.
    const size_t model_collection_sz = sizeof(*model_collection_);
    model_collection_ = (VmafModelCollection**)malloc(model_sz);
    memset(model_collection_, 0, model_collection_sz);
    model_collection_count_ = 0;

    model_path =
        (tools::GetModelRunfilesPathForTest() + "vmaf_v0.6.1neg.json").c_str();
  }

  VmafContext* vmaf_;
  VmafModel** model_;
  VmafModelCollection** model_collection_;
  uint64_t model_collection_count_;
  const char* model_path;

};

TEST_F(LibvmafAVTest, Basic) {
  fprintf(stderr, "Ready to initialize vmaf\n");
  int err = InitializeVmaf(vmaf_, model_, model_collection_,
                           &model_collection_count_,
                           model_path);
  EXPECT_EQ(err, 0) << "InitailizeVmaf call failed. error_code: " << err;

  const std::string file_path =
      tools::GetModelRunfilesPathForTest() + "720p.mp4";

  double vmaf_score = ComputeVmafScore(file_path, file_path);
  EXPECT_GT(vmaf_score, 0.0) << "ComputeVmafScore call failed.";
  printf("Computed a vmaf score of %f\n", vmaf_score);
}
