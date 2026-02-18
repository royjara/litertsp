#include "inference_engine.h"
#include "cpu_backend.h"

#include <iostream>

InferenceEngine::InferenceEngine(const std::string& model_path, Accelerator accel)
    : accel_(accel)
{
    switch (accel_) {
        case Accelerator::CPU:
            backend_ = std::make_unique<CpuBackend>();
            break;
        // case Accelerator::GPU: backend_ = std::make_unique<GpuBackend>(); break;
        // case Accelerator::NPU: backend_ = std::make_unique<NpuBackend>(); break;
    }

    if (!backend_->set_model(model_path)) {
        std::cerr << "[InferenceEngine] set_model failed: " << model_path << "\n";
        return;
    }
    if (!backend_->prepare()) {
        std::cerr << "[InferenceEngine] prepare failed\n";
        return;
    }
    ready_ = true;
}

InferenceEngine::~InferenceEngine() {
    if (backend_) backend_->teardown();
}

bool InferenceEngine::process(const uint8_t* rgb_data, int width, int height) {
    if (!ready_) return false;
    return backend_->process(rgb_data, width, height);
}

int InferenceEngine::output_count() const {
    return backend_ ? backend_->output_count() : 0;
}

const float* InferenceEngine::output_data(int idx) const {
    return backend_ ? backend_->output_data(idx) : nullptr;
}

int InferenceEngine::output_size(int idx) const {
    return backend_ ? backend_->output_size(idx) : 0;
}
