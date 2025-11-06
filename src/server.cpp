#include "server.hpp"
#include "httplib.h"
#include "json.hpp"
#include "snapshot_io.hpp"
#include "util.hpp"

Server::Server(std::unique_ptr<IndexBackend> backend, int port)
    : state_(), latency_tracker_(), qps_tracker_(), uptime_tracker_(), port_(port) {
    if (backend) {
        std::string backend_name = backend->get_backend_name();
        update_state(std::move(backend), backend_name, "cosine");
    }
}

void Server::update_state(std::unique_ptr<IndexBackend> new_index,
                          const std::string& backend_name,
                          const std::string& metric) {
    std::lock_guard<std::mutex> lk(state_.m);
    state_.index = std::move(new_index);
    state_.dim = state_.index->get_dim();
    state_.count = state_.index->get_count();
    state_.backend_name = backend_name;
    state_.metric = metric;
    state_.snapshot_version = "v001";
    state_.loaded.store(true);
}

void Server::send_error(httplib::Response& res, int status, const std::string& code, const std::string& message) const {
    res.status = status;
    nlohmann::json error_response;
    error_response["error"]["code"] = code;
    error_response["error"]["message"] = message;
    res.set_content(error_response.dump(), "application/json");
}

void Server::handle_healthz(const httplib::Request&, httplib::Response& res) {
    res.set_content("ok", "text/plain");
}

void Server::handle_stats(const httplib::Request&, httplib::Response& res) {
    bool loaded = state_.loaded.load();
    int dim;
    size_t count;
    std::string backend;
    std::string version;
    std::string metric;
    
    {
        std::lock_guard<std::mutex> lk(state_.m);
        if (!state_.index) {
            loaded = false;
        } else {
            dim = state_.index->get_dim();
            count = state_.index->get_count();
            backend = state_.backend_name;
            version = state_.snapshot_version;
            metric = state_.metric;
        }
    }
    
    nlohmann::json response;
    response["status"] = loaded ? "ready" : "empty";
    response["count"] = count;
    response["dim"] = dim;
    response["backend"] = backend;
    response["metric"] = metric;
    response["snapshot_version"] = version;
    response["uptime_sec"] = static_cast<int>(uptime_tracker_.get_uptime_sec());
    response["qps_1m"] = qps_tracker_.get_qps();
    response["latency_ms"]["p50"] = latency_tracker_.percentile(50.0);
    response["latency_ms"]["p95"] = latency_tracker_.percentile(95.0);
    response["latency_ms"]["p99"] = latency_tracker_.percentile(99.0);
    
    res.set_content(response.dump(), "application/json");
}

void Server::handle_load(const httplib::Request& req, httplib::Response& res) {
    try {
        nlohmann::json json_req;
        try {
            json_req = nlohmann::json::parse(req.body);
        } catch (const nlohmann::json::parse_error& e) {
            send_error(res, 400, "INVALID_JSON", "Failed to parse JSON: " + std::string(e.what()));
            return;
        }
        
        if (!json_req.contains("path")) {
            send_error(res, 400, "MISSING_FIELD", "Missing required field: path");
            return;
        }
        
        std::string vectors_path = json_req["path"];
        std::string ids_path = json_req.value("ids_path", "");
        std::string metric = json_req.value("metric", "cosine");
        std::string backend = json_req.value("backend", "bruteforce");
        
        if (backend != "bruteforce") {
            send_error(res, 400, "UNSUPPORTED_BACKEND", "Backend '" + backend + "' not yet supported. Use 'bruteforce'.");
            return;
        }
        
        LOG_INFO("Loading snapshot from: " + vectors_path);
        
        SnapshotData snapshot = load_snapshot(vectors_path, ids_path);
        
        if (snapshot.count == 0) {
            send_error(res, 400, "LOAD_FAILED", "Failed to load snapshot from: " + vectors_path);
            return;
        }
        
        auto new_backend = std::make_unique<BruteforceIndex>(std::move(snapshot));
        size_t count = new_backend->get_count();
        int dim = new_backend->get_dim();
        
        update_state(std::move(new_backend), backend, metric);
        
        nlohmann::json response;
        response["ok"] = true;
        response["loaded"]["count"] = count;
        response["loaded"]["dim"] = dim;
        response["loaded"]["backend"] = backend;
        
        res.set_content(response.dump(), "application/json");
        LOG_INFO("Loaded " + std::to_string(count) + " vectors, dim=" + std::to_string(dim));
        
    } catch (const std::exception& e) {
        LOG_ERROR("Load request failed: " + std::string(e.what()));
        send_error(res, 500, "INTERNAL_ERROR", "Internal error: " + std::string(e.what()));
    }
}

void Server::handle_query(const httplib::Request& req, httplib::Response& res) {
    if (!state_.loaded.load()) {
        send_error(res, 400, "NO_INDEX", "No index loaded. Call /load first.");
        return;
    }
    
    try {
        nlohmann::json json_req;
        try {
            json_req = nlohmann::json::parse(req.body);
        } catch (const nlohmann::json::parse_error& e) {
            send_error(res, 400, "INVALID_JSON", "Failed to parse JSON: " + std::string(e.what()));
            return;
        }
        
        if (!json_req.contains("k")) {
            send_error(res, 400, "MISSING_FIELD", "Missing required field: k");
            return;
        }
        if (!json_req.contains("vector")) {
            send_error(res, 400, "MISSING_FIELD", "Missing required field: vector");
            return;
        }
        
        int k = json_req["k"];
        if (k <= 0) {
            send_error(res, 400, "INVALID_VALUE", "k must be greater than 0, got: " + std::to_string(k));
            return;
        }
        
        std::vector<float> query_vector;
        try {
            query_vector = json_req["vector"].get<std::vector<float>>();
        } catch (const std::exception& e) {
            send_error(res, 400, "INVALID_FIELD", "Invalid vector format: " + std::string(e.what()));
            return;
        }
        
        IndexBackend* index;
        int dim;
        size_t count;
        std::string backend;
        std::string version;
        
        {
            std::lock_guard<std::mutex> lk(state_.m);
            if (!state_.index) {
                send_error(res, 400, "NO_INDEX", "No index loaded. Call /load first.");
                return;
            }
            index = state_.index.get();
            dim = state_.index->get_dim();
            count = state_.index->get_count();
            backend = state_.backend_name;
            version = state_.snapshot_version;
        }
        
        // Validate dimensions
        if (static_cast<int>(query_vector.size()) != dim) {
            send_error(res, 400, "DIMENSION_MISMATCH", 
                      "Query vector dimension mismatch: expected " + std::to_string(dim) + 
                      ", got " + std::to_string(query_vector.size()));
            return;
        }
        
        // Cap k to available vectors
        if (static_cast<size_t>(k) > count) {
            k = static_cast<int>(count);
        }
        
        Timer timer;
        
        // Perform KNN search
        std::vector<Neighbor> neighbors = index->search_knn(query_vector, k);
        
        double latency = timer.elapsed_ms();
        
        // Record metrics
        latency_tracker_.record(latency);
        qps_tracker_.record();
        
        // Build response
        nlohmann::json response;
        nlohmann::json neighbors_json = nlohmann::json::array();
        
        for (const auto& neighbor : neighbors) {
            nlohmann::json neighbor_obj;
            neighbor_obj["id"] = neighbor.id;
            neighbor_obj["score"] = neighbor.score;
            neighbors_json.push_back(neighbor_obj);
        }
        
        response["neighbors"] = neighbors_json;
        response["latency_ms"] = latency;
        response["backend"] = backend;
        
        res.set_content(response.dump(), "application/json");
        
        // Structured logging
        log_query(latency, k, count, dim, backend, version);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Query request failed: " + std::string(e.what()));
        send_error(res, 500, "INTERNAL_ERROR", "Internal error: " + std::string(e.what()));
    }
}

void Server::handle_root(const httplib::Request&, httplib::Response& res) {
    std::string html = "<html><body><h2>Boogie-Vec</h2>"
                       "<p>Endpoints: <code>/healthz</code>, <code>/stats</code>, <code>/load</code>, <code>/query</code></p>"
                       "</body></html>";
    res.set_content(html, "text/html");
}

void Server::run() {
    httplib::Server svr;
    
    svr.Get("/healthz", [this](const httplib::Request& req, httplib::Response& res) {
        handle_healthz(req, res);
    });
    
    svr.Get("/stats", [this](const httplib::Request& req, httplib::Response& res) {
        handle_stats(req, res);
    });
    
    svr.Post("/load", [this](const httplib::Request& req, httplib::Response& res) {
        handle_load(req, res);
    });
    
    svr.Post("/query", [this](const httplib::Request& req, httplib::Response& res) {
        handle_query(req, res);
    });
    
    svr.Get("/", [this](const httplib::Request& req, httplib::Response& res) {
        handle_root(req, res);
    });
    
    std::cout << "[boogie-vec] starting server on 0.0.0.0:" << port_ << std::endl;
    svr.listen("0.0.0.0", port_);
}
