#include <gst/gst.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "rtsp_stream_manager.h"
#include "video_renderer.h"

static void usage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << " <root_url> <endpoint1> [endpoint2 ...]\n"
        << "  " << prog << " --debug <root_url> <endpoint> <repeat_count>\n"
        << "\n"
        << "Normal mode: connects to root_url + each endpoint simultaneously.\n"
        << "  Example: " << prog << " rtsp://192.168.1.100:554 /ch0 /ch1 /ch2\n"
        << "\n"
        << "Debug mode: spawns repeat_count independent pipelines for one endpoint\n"
        << "  to stress-test hardware codec throughput.\n"
        << "  Example: " << prog << " --debug rtsp://192.168.1.100:554 /ch0 4\n";
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    bool debug_mode = (std::string(argv[1]) == "--debug");

    std::string              root_url;
    std::vector<std::string> full_urls;

    if (debug_mode) {
        // --debug <root_url> <endpoint> <repeat_count>
        if (argc != 5) {
            usage(argv[0]);
            return 1;
        }
        root_url        = argv[2];
        std::string ep  = argv[3];
        int count       = std::atoi(argv[4]);
        if (count < 1) {
            std::cerr << "repeat_count must be >= 1\n";
            return 1;
        }
        std::string url = root_url + ep;
        for (int i = 0; i < count; ++i)
            full_urls.push_back(url);

        std::cout << "Debug mode: " << count << " pipeline(s) → " << url << "\n";
    } else {
        // <root_url> <endpoint1> [endpoint2 ...]
        root_url = argv[1];
        for (int i = 2; i < argc; ++i)
            full_urls.push_back(root_url + std::string(argv[i]));
    }

    int num_streams = (int)full_urls.size();
    std::cout << "Starting " << num_streams << " stream(s)\n";

    VideoRenderer    renderer(num_streams, "RTSP Stream");
    RtspStreamManager manager;
    manager.set_renderer(&renderer);

    for (const auto& url : full_urls)
        manager.add_stream(url);

    // Render loop on the main thread (required by GLFW)
    while (!renderer.should_close())  {
        renderer.render();
        renderer.poll_events();
    }

    manager.stop_all_streams();
    gst_deinit();
    return 0;
}
