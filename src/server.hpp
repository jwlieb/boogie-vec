#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include "knn_bruteforce.hpp"
#include "util.hpp"

namespace httplib {
struct Request;
struct Response;
class Server;
}

class Server {
public:
    Server(std::unique_ptr<IndexBackend> backend, int port);
    void run();
    
    // Route handlers  
    void handle_healthz(const httplib::Request& req, httplib::Response& res);
    void handle_stats(const httplib::Request& req, httplib::Response& res);
    void handle_load(const httplib::Request& req, httplib::Response& res);
    void handle_query(const httplib::Request& req, httplib::Response& res);
    void handle_root(const httplib::Request& req, httplib::Response& res);

private:
    struct IndexState {
        std::atomic<bool> loaded{false};
        int dim = 0;
        size_t count = 0;
        std::string backend_name = "bruteforce";
        std::string snapshot_version = "v0";
        std::string metric = "cosine";
        std::mutex m;
        std::unique_ptr<IndexBackend> index;
    };
    
    IndexState state_;
    LatencyTracker latency_tracker_;
    QPSTracker qps_tracker_;
    UptimeTracker uptime_tracker_;
    int port_;
    
    void update_state(std::unique_ptr<IndexBackend> new_index, 
                     const std::string& backend_name,
                     const std::string& metric);
};
