#pragma once

#include <vector>
#include <string>
#include <utility>

struct Neighbor {
    std::string id;
    float score;
    
    Neighbor(const std::string& id, float score) : id(id), score(score) {}
};

// Search for k nearest neighbors using brute force cosine similarity
// Returns vector of {id, score} pairs sorted by score (highest first)
std::vector<Neighbor> search_knn(
    const std::vector<float>& query_vector,
    const std::vector<float>& data,
    const std::vector<float>& norms,
    const std::vector<std::string>& ids,
    uint32_t dim,
    uint32_t count,
    int k
);
