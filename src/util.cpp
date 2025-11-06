#include "util.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

void log(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "]";
    ss << " [" << level << "] " << message;
    
    std::cout << ss.str() << std::endl;
}

void log_query(double latency_ms, int k, size_t count, int dim, 
               const std::string& backend, const std::string& version) {
    std::stringstream ss;
    ss << "{"
       << "\"lat_ms\":" << std::fixed << std::setprecision(2) << latency_ms << ","
       << "\"k\":" << k << ","
       << "\"count\":" << count << ","
       << "\"dim\":" << dim << ","
       << "\"backend\":\"" << backend << "\","
       << "\"version\":\"" << version << "\""
       << "}";
    log("QUERY", ss.str());
}
