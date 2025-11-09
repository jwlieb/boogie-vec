#include "backend_factory.hpp"
#include "knn_annoy.hpp"

std::unique_ptr<IndexBackend> create_backend(
    const std::string& backend_name,
    SnapshotData snapshot) {
    
    if (backend_name == "bruteforce") {
        return std::make_unique<BruteforceIndex>(std::move(snapshot));
    }
    
    if (backend_name == "annoy") {
        // Annoy requires pre-built index file, not raw snapshot
        // This will be handled differently - see handle_load for annoy path
        return nullptr;
    }
    
    return nullptr;
}

