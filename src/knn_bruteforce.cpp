#include "knn_bruteforce.hpp"
#include <algorithm>
#include <queue>
#include <cmath>
#include <iostream>

std::vector<Neighbor> BruteforceIndex::search_knn(
    const std::vector<float>& query_vector, int k) {
    std::vector<Neighbor> results;
    
    uint32_t dim = snapshot_.dim;
    uint32_t count = snapshot_.count;
    
    if (query_vector.size() != dim) {
        std::cerr << "Query vector dimension mismatch: expected " << dim << ", got " << query_vector.size() << std::endl;
        return results;
    }
    
    if (k <= 0 || static_cast<uint32_t>(k) > count) {
        k = std::min(static_cast<int>(count), k);
    }
    
    // Compute query norm
    float query_norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        query_norm += query_vector[i] * query_vector[i];
    }
    query_norm = std::sqrt(query_norm);
    
    if (query_norm == 0.0f) {
        std::cerr << "Query vector has zero norm" << std::endl;
        return results;
    }
    
    // Use a min-heap to maintain top-k results
    std::priority_queue<std::pair<float, uint32_t>, 
                       std::vector<std::pair<float, uint32_t>>, 
                       std::greater<std::pair<float, uint32_t>>> heap;
    
    // Compute cosine similarity for all vectors
    for (uint32_t i = 0; i < count; i++) {
        const float* vector = &snapshot_.data[i * dim];
        float norm = snapshot_.norms[i];
        
        if (norm == 0.0f) {
            continue;
        }
        
        // Compute dot product
        float dot_product = 0.0f;
        for (uint32_t j = 0; j < dim; j++) {
            dot_product += query_vector[j] * vector[j];
        }
        
        // Cosine similarity = dot_product / (query_norm * vector_norm)
        float similarity = dot_product / (query_norm * norm);
        
        // Add to heap
        if (static_cast<int>(heap.size()) < k) {
            heap.push({similarity, i});
        } else if (similarity > heap.top().first) {
            heap.pop();
            heap.push({similarity, i});
        }
    }
    
    // Extract results from heap
    std::vector<std::pair<float, uint32_t>> temp_results;
    while (!heap.empty()) {
        temp_results.push_back(heap.top());
        heap.pop();
    }
    
    // Sort by similarity (highest first)
    std::sort(temp_results.begin(), temp_results.end(), 
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Convert to Neighbor objects
    for (const auto& result : temp_results) {
        results.emplace_back(snapshot_.ids[result.second], result.first);
    }
    
    return results;
}
