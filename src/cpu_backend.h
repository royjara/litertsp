#pragma once

#include "inference_backend.h"
#include <memory>
#include <string>

// Forward-declare TFLite types to keep LiteRT headers out of this header.
namespace tflite {
class FlatBufferModel;
class Interpreter;
}

// CPU-only LiteRT backend — the cross-platform ARM baseline.
// No delegate is applied explicitly; XNNPACK (built into LiteRT by default)
// provides NEON-optimized kernels on ARM64/ARMv7 without extra configuration.
//
// Swap for GpuBackend / NpuBackend when targeting a platform with a suitable
// delegate; all three share the same InferenceBackend interface.
class CpuBackend : public InferenceBackend {
public:
    CpuBackend();
    ~CpuBackend() override;

    bool set_model(const std::string& model_path) override;
    bool prepare()  override;
    void teardown() override;
    bool process(const uint8_t* rgb_data, int width, int height) override;

    int          output_count()                  const override;
    const float* output_data(int tensor_idx = 0) const override;
    int          output_size(int tensor_idx = 0) const override;

    // Optional: set thread count before prepare(). Default: 1.
    void set_num_threads(int n) { num_threads_ = n; }

private:
    std::string model_path_;
    int         num_threads_ = 1;

    std::unique_ptr<tflite::FlatBufferModel> model_;
    std::unique_ptr<tflite::Interpreter>     interpreter_;
};
