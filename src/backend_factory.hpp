#pragma once

#include "knn_bruteforce.hpp"
#include <memory>
#include <string>

// Factory function to create IndexBackend instances
std::unique_ptr<IndexBackend> create_backend(
    const std::string& backend_name,
    SnapshotData snapshot);

