#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

struct StreamInfo {
    std::string rtsp_url;
    std::string device_name;
    std::string device_ip;
    std::chrono::steady_clock::time_point last_seen;
    bool is_active;

    StreamInfo(const std::string& url, const std::string& name, const std::string& ip)
        : rtsp_url(url), device_name(name), device_ip(ip),
          last_seen(std::chrono::steady_clock::now()), is_active(true) {}
};

class StreamDiscovery {
public:
    StreamDiscovery();
    ~StreamDiscovery();

    void start_discovery();
    void stop_discovery();
    void print_discovered_streams() const;
    std::vector<StreamInfo> get_active_streams() const;

private:
    void discovery_worker();
    void scan_network_range(const std::string& base_ip, int start_host, int end_host);
    bool probe_rtsp_endpoint(const std::string& ip, int port = 554);
    void cleanup_stale_streams();

    mutable std::mutex streams_mutex_;
    std::vector<StreamInfo> discovered_streams_;
    std::thread discovery_thread_;
    std::atomic<bool> running_;

    static const int DISCOVERY_INTERVAL_MS = 30000; // 30 seconds
    static const int STREAM_TIMEOUT_MS = 60000;     // 1 minute
};