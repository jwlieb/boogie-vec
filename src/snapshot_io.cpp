#include "snapshot_io.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <sstream>

SnapshotData load_snapshot(const std::string& vectors_path, const std::string& ids_path) {
    SnapshotData snapshot;
    
    // Open vectors.bin file
    std::ifstream file(vectors_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open vectors file: " << vectors_path << std::endl;
        return snapshot;
    }
    
    // Read header: [uint32 dim][uint32 count]
    uint32_t dim, count;
    file.read(reinterpret_cast<char*>(&dim), sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    
    if (file.fail()) {
        std::cerr << "Failed to read header from vectors file" << std::endl;
        return snapshot;
    }
    
    // Allocate data array
    size_t total_floats = static_cast<size_t>(count) * static_cast<size_t>(dim);
    snapshot.data.resize(total_floats);
    snapshot.norms.resize(count);
    snapshot.ids.resize(count);
    
    // Read vector data
    file.read(reinterpret_cast<char*>(snapshot.data.data()), total_floats * sizeof(float));
    if (file.fail()) {
        std::cerr << "Failed to read vector data" << std::endl;
        return snapshot;
    }
    
    // Precompute norms for cosine similarity
    for (uint32_t i = 0; i < count; i++) {
        const float* vector = &snapshot.data[i * dim];
        snapshot.norms[i] = compute_norm(vector, dim);
    }
    
    // Load IDs if provided
    if (!ids_path.empty()) {
        std::ifstream ids_file(ids_path);
        if (ids_file.is_open()) {
            std::string line;
            std::getline(ids_file, line);
            
            // Simple JSON array parsing for ["id1","id2",...]
            if (line.front() == '[' && line.back() == ']') {
                line = line.substr(1, line.length() - 2); // Remove [ and ]
                std::stringstream ss(line);
                std::string id;
                uint32_t idx = 0;
                
                while (std::getline(ss, id, ',') && idx < count) {
                    // Remove quotes and trim
                    if (id.front() == '"' && id.back() == '"') {
                        id = id.substr(1, id.length() - 2);
                    }
                    // Remove leading/trailing whitespace
                    id.erase(0, id.find_first_not_of(" \t"));
                    id.erase(id.find_last_not_of(" \t") + 1);
                    snapshot.ids[idx] = id;
                    idx++;
                }
            }
        }
    }
    
    // Fill in sequential IDs for any missing ones
    for (uint32_t i = 0; i < count; i++) {
        if (snapshot.ids[i].empty()) {
            snapshot.ids[i] = "vector_" + std::to_string(i);
        }
    }
    
    snapshot.dim = dim;
    snapshot.count = count;
    
    std::cout << "Loaded snapshot: " << count << " vectors, dim=" << dim << std::endl;
    return snapshot;
}

float compute_norm(const float* vector, uint32_t dim) {
    float sum_squares = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum_squares += vector[i] * vector[i];
    }
    return std::sqrt(sum_squares);
}
