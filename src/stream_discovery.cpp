#include "stream_discovery.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ifaddrs.h>

StreamDiscovery::StreamDiscovery() : running_(false) {
}

StreamDiscovery::~StreamDiscovery() {
    stop_discovery();
}

void StreamDiscovery::start_discovery() {
    if (running_) {
        std::cout << "Discovery already running" << std::endl;
        return;
    }

    running_ = true;
    discovery_thread_ = std::thread(&StreamDiscovery::discovery_worker, this);
    std::cout << "Started stream discovery service" << std::endl;
}

void StreamDiscovery::stop_discovery() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (discovery_thread_.joinable()) {
        discovery_thread_.join();
    }
    std::cout << "Stopped stream discovery service" << std::endl;
}

void StreamDiscovery::print_discovered_streams() const {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    std::cout << "\n=== Discovered RTSP Streams ===" << std::endl;
    if (discovered_streams_.empty()) {
        std::cout << "No streams discovered yet." << std::endl;
        return;
    }

    for (const auto& stream : discovered_streams_) {
        if (stream.is_active) {
            std::cout << "Device: " << stream.device_name << std::endl;
            std::cout << "  IP: " << stream.device_ip << std::endl;
            std::cout << "  RTSP URL: " << stream.rtsp_url << std::endl;
            std::cout << "  Last seen: " << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - stream.last_seen).count() << "s ago" << std::endl;
            std::cout << std::endl;
        }
    }
}

std::vector<StreamInfo> StreamDiscovery::get_active_streams() const {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    std::vector<StreamInfo> active;

    for (const auto& stream : discovered_streams_) {
        if (stream.is_active) {
            active.push_back(stream);
        }
    }

    return active;
}

void StreamDiscovery::discovery_worker() {
    while (running_) {
        std::cout << "Scanning for RTSP streams..." << std::endl;

        // Get local network interfaces
        struct ifaddrs* ifaddrs_ptr;
        if (getifaddrs(&ifaddrs_ptr) == 0) {
            for (struct ifaddrs* ifa = ifaddrs_ptr; ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                    struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                    struct sockaddr_in* netmask_in = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_netmask);

                    if (addr_in && netmask_in) {
                        uint32_t ip = ntohl(addr_in->sin_addr.s_addr);
                        uint32_t mask = ntohl(netmask_in->sin_addr.s_addr);

                        // Skip loopback
                        if ((ip & 0xFF000000) == 0x7F000000) continue;

                        uint32_t network = ip & mask;
                        uint32_t broadcast = network | (~mask);

                        // Scan network range
                        std::string base_ip = inet_ntoa(addr_in->sin_addr);
                        size_t last_dot = base_ip.find_last_of('.');
                        if (last_dot != std::string::npos) {
                            std::string subnet = base_ip.substr(0, last_dot);
                            scan_network_range(subnet, 1, 254);
                        }
                    }
                }
            }
            freeifaddrs(ifaddrs_ptr);
        }

        cleanup_stale_streams();

        // Wait before next discovery cycle
        for (int i = 0; i < DISCOVERY_INTERVAL_MS / 100 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void StreamDiscovery::scan_network_range(const std::string& base_ip, int start_host, int end_host) {
    for (int host = start_host; host <= end_host && running_; ++host) {
        std::string ip = base_ip + "." + std::to_string(host);

        if (probe_rtsp_endpoint(ip)) {
            std::lock_guard<std::mutex> lock(streams_mutex_);

            // Check if we already know about this stream
            auto it = std::find_if(discovered_streams_.begin(), discovered_streams_.end(),
                [&ip](const StreamInfo& info) { return info.device_ip == ip; });

            if (it != discovered_streams_.end()) {
                // Update existing stream
                it->last_seen = std::chrono::steady_clock::now();
                it->is_active = true;
            } else {
                // Add new stream
                std::string rtsp_url = "rtsp://" + ip + ":554/";
                std::string device_name = "RTSP Device (" + ip + ")";
                discovered_streams_.emplace_back(rtsp_url, device_name, ip);
                std::cout << "Discovered new RTSP stream: " << rtsp_url << std::endl;
            }
        }
    }
}

bool StreamDiscovery::probe_rtsp_endpoint(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    bool result = (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0);

    close(sock);
    return result;
}

void StreamDiscovery::cleanup_stale_streams() {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto now = std::chrono::steady_clock::now();

    for (auto& stream : discovered_streams_) {
        if (stream.is_active) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - stream.last_seen);
            if (elapsed.count() > STREAM_TIMEOUT_MS) {
                stream.is_active = false;
                std::cout << "Stream timeout: " << stream.rtsp_url << std::endl;
            }
        }
    }
}