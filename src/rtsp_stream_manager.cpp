#include "rtsp_stream_manager.h"
#include "video_renderer.h"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <iostream>

// ---------------------------------------------------------------------------
// RtspStream
// ---------------------------------------------------------------------------

RtspStream::RtspStream(const std::string& url, int slot, VideoRenderer* renderer)
    : url_(url), slot_(slot), renderer_(renderer) {}

RtspStream::~RtspStream() { stop(); }

bool RtspStream::start() {
    if (pipeline_) {
        std::cout << "Stream already started: " << url_ << "\n";
        return true;
    }

    std::string pipeline_str;
    if (renderer_) {
        // GStreamer decodes and converts to RGB; appsink hands us raw frames.
        // max-buffers=2 drop=true keeps the renderer at live speed without backpressure.
        pipeline_str =
            "rtspsrc location=" + url_ +
            " ! decodebin"
            " ! videoconvert"
            " ! video/x-raw,format=RGB"
            " ! appsink name=sink sync=false max-buffers=2 drop=true emit-signals=true";
    } else {
        pipeline_str = "rtspsrc location=" + url_ + " ! decodebin ! autovideosink";
    }

    GError* error = nullptr;
    pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);
    if (error) {
        std::cerr << "Failed to create pipeline: " << error->message << "\n";
        g_error_free(error);
        return false;
    }

    if (renderer_) {
        GstElement* appsink = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
        if (appsink) {
            g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), this);
            gst_object_unref(appsink);
        }
    }

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to start pipeline for: " << url_ << "\n";
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        return false;
    }

    playing_ = true;
    std::cout << "[slot " << slot_ << "] Started: " << url_ << "\n";
    return true;
}

void RtspStream::stop() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    playing_ = false;
}

GstFlowReturn RtspStream::on_new_sample(GstAppSink* appsink, gpointer user_data) {
    auto* self = static_cast<RtspStream*>(user_data);
    if (!self->renderer_) return GST_FLOW_OK;

    GstSample* sample = gst_app_sink_pull_sample(appsink);
    if (!sample) return GST_FLOW_OK;

    GstCaps*      caps = gst_sample_get_caps(sample);
    GstStructure* s    = gst_caps_get_structure(caps, 0);
    int width = 0, height = 0;
    gst_structure_get_int(s, "width",  &width);
    gst_structure_get_int(s, "height", &height);

    if (width > 0 && height > 0) {
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            self->renderer_->push_frame(self->slot_, map.data, width, height);
            gst_buffer_unmap(buffer, &map);
        }
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// ---------------------------------------------------------------------------
// RtspStreamManager
// ---------------------------------------------------------------------------

RtspStreamManager::RtspStreamManager() {}

RtspStreamManager::~RtspStreamManager() { stop_all_streams(); }

int RtspStreamManager::add_stream(const std::string& rtsp_url) {
    int slot = (int)streams_.size();
    auto stream = std::make_unique<RtspStream>(rtsp_url, slot, renderer_);
    stream->start();
    streams_.push_back(std::move(stream));
    return slot;
}

void RtspStreamManager::stop_all_streams() {
    for (auto& s : streams_) s->stop();
    streams_.clear();
    std::cout << "All streams stopped\n";
}
