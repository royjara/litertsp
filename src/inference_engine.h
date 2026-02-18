#pragma once

#include "inference_backend.h"
#include <memory>
#include <string>

enum class Accelerator {
    CPU,
    // GPU,  // LiteRT GPU delegate (OpenCL/Metal) — add when needed
    // NPU,  // LiteRT NNAPI / vendor delegate     — add when needed
};

// Owns and manages a single InferenceBackend strategy.
// On construction: selects the backend for the requested accelerator,
// loads the model file, and warms up the interpreter so the first
// process() call hits cached weights.
class InferenceEngine {
public:
    explicit InferenceEngine(const std::string& model_path,
                             Accelerator accel = Accelerator::CPU);
    ~InferenceEngine();

    // True if construction succeeded (model loaded + backend prepared).
    bool ready() const { return ready_; }

    Accelerator accelerator() const { return accel_; }

    // Run one frame through the model.
    // rgb_data: width × height × 3 bytes, row-major uint8.
    bool process(const uint8_t* rgb_data, int width, int height);

    // Output tensor access — valid until the next process() call.
    int          output_count()                  const;
    const float* output_data(int tensor_idx = 0) const;
    int          output_size(int tensor_idx = 0) const;

private:
    Accelerator                       accel_;
    bool                              ready_   = false;
    std::unique_ptr<InferenceBackend> backend_;
};
