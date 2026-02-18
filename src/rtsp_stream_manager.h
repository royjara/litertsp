#pragma once

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <string>
#include <vector>
#include <memory>

class VideoRenderer;

class RtspStream {
public:
    RtspStream(const std::string& url, int slot, VideoRenderer* renderer);
    ~RtspStream();

    bool start();
    void stop();
    bool is_playing() const { return playing_; }
    const std::string& get_url() const { return url_; }
    int get_slot() const { return slot_; }

private:
    static GstFlowReturn on_new_sample(GstAppSink* sink, gpointer user_data);

    std::string    url_;
    int            slot_     = 0;
    GstElement*    pipeline_ = nullptr;
    bool           playing_  = false;
    VideoRenderer* renderer_ = nullptr;
};

class RtspStreamManager {
public:
    RtspStreamManager();
    ~RtspStreamManager();

    void set_renderer(VideoRenderer* renderer) { renderer_ = renderer; }

    // Adds a stream and assigns it the next available slot. Returns the slot index.
    int add_stream(const std::string& rtsp_url);
    void stop_all_streams();

private:
    std::vector<std::unique_ptr<RtspStream>> streams_;
    VideoRenderer* renderer_ = nullptr;
};
