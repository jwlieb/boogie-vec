#include "snapshot_io.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <sstream>
#include <filesystem>

SnapshotData load_snapshot(const std::string& vectors_path, const std::string& ids_path) {
    SnapshotData snapshot;
    
    // Check if vectors file exists
    if (!std::filesystem::exists(vectors_path)) {
        std::cerr << "Vectors file does not exist: " << vectors_path << std::endl;
        return snapshot;
    }
    
    // Check file size (must be at least header size)
    auto file_size = std::filesystem::file_size(vectors_path);
    const size_t header_size = 2 * sizeof(uint32_t);
    if (file_size < header_size) {
        std::cerr << "Vectors file too small: " << vectors_path << " (size: " << file_size << ")" << std::endl;
        return snapshot;
    }
    
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
    
    // Validate header values
    if (dim == 0 || dim > 100000) {
        std::cerr << "Invalid dimension in header: " << dim << std::endl;
        return snapshot;
    }
    if (count == 0 || count > 100000000) {
        std::cerr << "Invalid count in header: " << count << std::endl;
        return snapshot;
    }
    
    // Validate file size matches expected data size
    size_t total_floats = static_cast<size_t>(count) * static_cast<size_t>(dim);
    size_t expected_size = header_size + total_floats * sizeof(float);
    if (file_size < expected_size) {
        std::cerr << "Vectors file smaller than expected: expected " << expected_size 
                  << " bytes, got " << file_size << std::endl;
        return snapshot;
    }
    
    // Allocate data array
    snapshot.data.resize(total_floats);
    snapshot.norms.resize(count);
    snapshot.ids.resize(count);
    
    // Read vector data
    file.read(reinterpret_cast<char*>(snapshot.data.data()), total_floats * sizeof(float));
    if (file.fail()) {
        std::cerr << "Failed to read vector data" << std::endl;
        return snapshot;
    }
    
    // Verify we read exactly what we expected
    if (file.gcount() != static_cast<std::streamsize>(total_floats * sizeof(float))) {
        std::cerr << "Read incomplete vector data" << std::endl;
        return snapshot;
    }
    
    // Precompute norms for cosine similarity
    for (uint32_t i = 0; i < count; i++) {
        const float* vector = &snapshot.data[i * dim];
        snapshot.norms[i] = compute_norm(vector, dim);
    }
    
    // Load IDs if provided
    if (!ids_path.empty()) {
        if (!std::filesystem::exists(ids_path)) {
            std::cerr << "Warning: IDs file does not exist: " << ids_path << ", generating sequential IDs" << std::endl;
        } else {
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
