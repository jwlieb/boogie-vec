#pragma once

#include <chrono>
#include <string>

class Timer {
public:
    Timer() : start_time_(std::chrono::steady_clock::now()) {}
    
    // Get elapsed time in milliseconds
    double elapsed_ms() const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_);
        return duration.count() / 1000.0;
    }
    
    // Reset the timer
    void reset() {
        start_time_ = std::chrono::steady_clock::now();
    }

private:
    std::chrono::steady_clock::time_point start_time_;
};

// Simple logging function
void log(const std::string& level, const std::string& message);

// Convenience macros
#define LOG_INFO(msg) log("INFO", msg)
#define LOG_ERROR(msg) log("ERROR", msg)
#define LOG_DEBUG(msg) log("DEBUG", msg)
