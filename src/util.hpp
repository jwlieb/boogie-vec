#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <algorithm>
#include <cmath>

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

// Thread-safe latency tracker with ring buffer
class LatencyTracker {
public:
    explicit LatencyTracker(size_t buffer_size = 1000) 
        : buffer_size_(buffer_size), samples_(buffer_size), index_(0), count_(0) {}
    
    void record(double latency_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_[index_ % buffer_size_] = latency_ms;
        index_++;
        count_ = std::min(count_ + 1, buffer_size_);
    }
    
    double percentile(double p) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == 0) return 0.0;
        
        std::vector<double> sorted(samples_.begin(), samples_.begin() + count_);
        std::sort(sorted.begin(), sorted.end());
        
        size_t idx = static_cast<size_t>(std::ceil(p * count_ / 100.0)) - 1;
        idx = std::min(idx, count_ - 1);
        return sorted[idx];
    }

private:
    size_t buffer_size_;
    std::vector<double> samples_;
    mutable std::mutex mutex_;
    size_t index_;
    size_t count_;
};

// QPS tracker with 1-minute rolling window
class QPSTracker {
public:
    void record() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Remove entries older than 1 minute
        auto one_min_ago = now - std::chrono::minutes(1);
        while (!timestamps_.empty() && timestamps_.front() < one_min_ago) {
            timestamps_.pop_front();
        }
        
        timestamps_.push_back(now);
    }
    
    double get_qps() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (timestamps_.empty()) return 0.0;
        
        auto now = std::chrono::steady_clock::now();
        auto one_min_ago = now - std::chrono::minutes(1);
        
        size_t count = 0;
        for (const auto& ts : timestamps_) {
            if (ts >= one_min_ago) count++;
        }
        
        auto window_sec = std::chrono::duration<double>(
            now - std::max(one_min_ago, timestamps_.front())).count();
        return window_sec > 0 ? count / window_sec : 0.0;
    }

private:
    mutable std::mutex mutex_;
    std::deque<std::chrono::steady_clock::time_point> timestamps_;
};

// Uptime tracker
class UptimeTracker {
public:
    UptimeTracker() : start_time_(std::chrono::steady_clock::now()) {}
    
    double get_uptime_sec() const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        return duration.count();
    }

private:
    std::chrono::steady_clock::time_point start_time_;
};

// Simple logging function
void log(const std::string& level, const std::string& message);

// Structured logging for queries
void log_query(double latency_ms, int k, size_t count, int dim, 
               const std::string& backend, const std::string& version);

// Convenience macros
#define LOG_INFO(msg) log("INFO", msg)
#define LOG_ERROR(msg) log("ERROR", msg)
#define LOG_DEBUG(msg) log("DEBUG", msg)
