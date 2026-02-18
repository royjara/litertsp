#pragma once

#include <cstdint>
#include <string>

// Abstract inference backend, modeled after GStreamer's element lifecycle:
//
//   set_model()  ↔  g_object_set(element, "model-path", ...)   (configure props)
//   prepare()    ↔  GST_STATE_PLAYING                           (allocate resources)
//   process()    ↔  GstBaseTransform::transform()               (data in → results out)
//   teardown()   ↔  GST_STATE_NULL                              (release resources)
//
// TODO: wrap as a GstBaseTransform subclass so an InferenceBackend can be
//       inserted directly into any GStreamer pipeline between videoconvert
//       and appsink, emitting inference results as downstream metadata.
//
// Concrete backends (same interface, swap at runtime):
//   CpuBackend  — LiteRT, no delegate    (ARM64/x86 baseline, cross-compilable)
//   GpuBackend  — LiteRT GPU delegate    (OpenCL on Linux ARM, Metal on macOS)
//   NpuBackend  — LiteRT NNAPI / vendor delegate (Android, dedicated NPU silicon)

class InferenceBackend {
public:
    virtual ~InferenceBackend() = default;

    // Configure: must be called before prepare().
    virtual bool set_model(const std::string& model_path) = 0;

    // Lifecycle
    virtual bool prepare()  = 0;  // load model, allocate interpreter + tensors
    virtual void teardown() = 0;  // release all resources

    // Run inference on one RGB frame.
    // Layout: width × height × 3 bytes, row-major, uint8.
    // Returns false on error or if not prepared.
    virtual bool process(const uint8_t* rgb_data, int width, int height) = 0;

    // Output tensor access — valid until the next process() call.
    virtual int          output_count()                  const = 0;
    virtual const float* output_data(int tensor_idx = 0) const = 0;
    virtual int          output_size(int tensor_idx = 0) const = 0;  // # of floats
};
