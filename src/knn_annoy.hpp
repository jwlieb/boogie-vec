#pragma once

#include "knn_bruteforce.hpp"
#include "snapshot_io.hpp"
#include <string>

// AnnoyIndex - Approximate nearest neighbor backend using Annoy
// Note: Annoy requires pre-built index file. Use build_annoy_index.py to create index.
class AnnoyIndex : public IndexBackend {
public:
    // Load from pre-built Annoy index file
    static std::unique_ptr<AnnoyIndex> load(const std::string& index_path, 
                                            const std::string& ids_path,
                                            int n_trees = 50);
    
    std::vector<Neighbor> search_knn(const std::vector<float>& query_vector, int k) override;
    size_t get_count() const override;
    int get_dim() const override;
    std::string get_backend_name() const override { return "annoy"; }

private:
    std::vector<std::string> ids_;
    int dim_ = 0;
    size_t count_ = 0;
    // TODO: Add Annoy index data structure
};

