#pragma once

#include <vector>
#include <string>
#include <memory>
#include "snapshot_io.hpp"

struct Neighbor {
    std::string id;
    float score;
    
    Neighbor(const std::string& id, float score) : id(id), score(score) {}
};

// Abstract backend interface
class IndexBackend {
public:
    virtual ~IndexBackend() = default;
    virtual std::vector<Neighbor> search_knn(const std::vector<float>& query_vector, int k) = 0;
    virtual size_t get_count() const = 0;
    virtual int get_dim() const = 0;
    virtual std::string get_backend_name() const = 0;
};

// Brute-force cosine similarity implementation
class BruteforceIndex : public IndexBackend {
public:
    explicit BruteforceIndex(SnapshotData snapshot) 
        : snapshot_(std::move(snapshot)) {}
    
    std::vector<Neighbor> search_knn(const std::vector<float>& query_vector, int k) override;
    size_t get_count() const override { return snapshot_.count; }
    int get_dim() const override { return static_cast<int>(snapshot_.dim); }
    std::string get_backend_name() const override { return "bruteforce"; }

private:
    SnapshotData snapshot_;
};
