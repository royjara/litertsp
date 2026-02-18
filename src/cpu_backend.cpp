#include "cpu_backend.h"

// LiteRT headers — available via the transitive include path from tensorflow-lite target.
// Header paths follow the upstream tensorflow/lite/ layout.
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model_builder.h"

#include <algorithm>
#include <cstring>
#include <iostream>

CpuBackend::CpuBackend()  = default;
CpuBackend::~CpuBackend() { teardown(); }

bool CpuBackend::set_model(const std::string& model_path) {
    model_path_ = model_path;
    return true;
}

bool CpuBackend::prepare() {
    model_ = tflite::FlatBufferModel::BuildFromFile(model_path_.c_str());
    if (!model_) {
        std::cerr << "[CpuBackend] Failed to load model: " << model_path_ << "\n";
        return false;
    }

    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder builder(*model_, resolver);
    builder(&interpreter_);
    if (!interpreter_) {
        std::cerr << "[CpuBackend] Failed to build interpreter\n";
        return false;
    }

    interpreter_->SetNumThreads(num_threads_);
    if (interpreter_->AllocateTensors() != kTfLiteOk) {
        std::cerr << "[CpuBackend] AllocateTensors failed\n";
        return false;
    }

    std::cout << "[CpuBackend] Ready — model: " << model_path_
              << ", threads: " << num_threads_ << "\n";
    return true;
}

void CpuBackend::teardown() {
    interpreter_.reset();
    model_.reset();
}

bool CpuBackend::process(const uint8_t* rgb_data, int width, int height) {
    if (!interpreter_) return false;

    // Copy raw RGB bytes into input tensor[0].
    // The caller is responsible for ensuring the frame dimensions match the
    // model's expected input shape (resize/crop before calling if needed).
    TfLiteTensor* in = interpreter_->input_tensor(0);
    if (!in) return false;

    const int copy_bytes = std::min<int>(static_cast<int>(in->bytes), width * height * 3);
    std::memcpy(in->data.raw, rgb_data, copy_bytes);

    if (interpreter_->Invoke() != kTfLiteOk) {
        std::cerr << "[CpuBackend] Invoke failed\n";
        return false;
    }
    return true;
}

int CpuBackend::output_count() const {
    if (!interpreter_) return 0;
    return static_cast<int>(interpreter_->outputs().size());
}

const float* CpuBackend::output_data(int idx) const {
    if (!interpreter_ || idx >= output_count()) return nullptr;
    return interpreter_->typed_output_tensor<float>(idx);
}

int CpuBackend::output_size(int idx) const {
    if (!interpreter_ || idx >= output_count()) return 0;
    const TfLiteTensor* t = interpreter_->output_tensor(idx);
    return t ? static_cast<int>(t->bytes / sizeof(float)) : 0;
}
