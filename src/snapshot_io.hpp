#pragma once

#include <vector>
#include <string>

struct SnapshotData {
    std::vector<float> data;        // Contiguous float array: [v0[0..dim-1], v1[0..dim-1], ...]
    std::vector<float> norms;       // Precomputed L2 norms for each vector
    std::vector<std::string> ids;   // String IDs for each vector
    uint32_t dim;                   // Vector dimension
    uint32_t count;                 // Number of vectors
};

// Load a snapshot from vectors.bin and optional ids.json
// Returns empty SnapshotData on failure
SnapshotData load_snapshot(const std::string& vectors_path, const std::string& ids_path = "");

// Helper to compute L2 norm of a vector
float compute_norm(const float* vector, uint32_t dim);
