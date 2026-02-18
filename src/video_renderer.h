#pragma once

#include <cstdint>
#include <memory>
#include <string>

// All GLFW/OpenGL types are hidden behind PIMPL so that including this header
// never requires GLFW to be in the include path.
struct VideoRendererImpl;

class VideoRenderer {
public:
    // num_streams determines the grid layout (1→full, 4→2×2, 9→3×3, etc.)
    explicit VideoRenderer(int num_streams, const std::string& title = "RTSP Stream");
    ~VideoRenderer();

    // Thread-safe: slot ∈ [0, num_streams). Called from GStreamer streaming thread.
    void push_frame(int slot, const uint8_t* data, int width, int height);

    // Main-thread only
    bool should_close() const;
    void render();       // upload dirty textures, draw grid
    void poll_events();

private:
    std::unique_ptr<VideoRendererImpl> impl_;
};
