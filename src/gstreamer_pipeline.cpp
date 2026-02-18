#include <gst/gst.h>
#include <iostream>

// This file provides utility functions for GStreamer pipeline management
// Currently implemented as part of RtspStream class in rtsp_stream_manager.cpp

namespace GStreamerUtils {

void print_gstreamer_info() {
    std::cout << "GStreamer version: " << gst_version_string() << std::endl;

    // Print available plugins
    GstRegistry* registry = gst_registry_get();
    GList* plugins = gst_registry_get_plugin_list(registry);

    std::cout << "Available GStreamer plugins:" << std::endl;
    for (GList* l = plugins; l; l = l->next) {
        GstPlugin* plugin = GST_PLUGIN(l->data);
        std::cout << "  - " << gst_plugin_get_name(plugin) << std::endl;
    }

    gst_plugin_list_free(plugins);
}

bool check_required_plugins() {
    const char* required_plugins[] = {
        "rtspsrc",
        "decodebin",
        "autovideosink",
        "videoconvert",
        "videoscale",
        nullptr
    };

    GstRegistry* registry = gst_registry_get();
    bool all_found = true;

    for (int i = 0; required_plugins[i]; ++i) {
        GstPluginFeature* feature = gst_registry_lookup_feature(registry, required_plugins[i]);
        if (!feature) {
            std::cerr << "Required plugin not found: " << required_plugins[i] << std::endl;
            all_found = false;
        } else {
            gst_object_unref(feature);
        }
    }

    return all_found;
}

} // namespace GStreamerUtils