// Boogie-Vec: Tiny vector search service
#include <iostream>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>

#include "httplib.h"
#include "json.hpp"
#include "snapshot_io.hpp"
#include "knn_bruteforce.hpp"
#include "util.hpp"

using namespace std::chrono_literals;

struct IndexState {
  std::atomic<bool> loaded{false};
  int dim = 0;
  size_t count = 0;
  std::string backend = "bruteforce";
  std::string snapshot_version = "v0";
  std::string metric = "cosine";
  std::mutex m;
  
  // Actual loaded data
  SnapshotData snapshot;
} g_index;


int main(int argc, char** argv) {
  int port = 8080;
  if (argc >= 2) {
    try { port = std::stoi(argv[1]); } catch(...) {}
  }

  httplib::Server svr;

  // healthz
  svr.Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
    res.set_content("ok", "text/plain");
  });

  // stats
  svr.Get("/stats", [](const httplib::Request&, httplib::Response& res) {
    bool loaded = g_index.loaded.load();
    int dim;
    size_t count;
    std::string backend;
    std::string version;
    std::string metric;
    {
      std::lock_guard<std::mutex> lk(g_index.m);
      dim = g_index.dim;
      count = g_index.count;
      backend = g_index.backend;
      version = g_index.snapshot_version;
      metric = g_index.metric;
    }
    
    nlohmann::json response;
    response["status"] = loaded ? "ready" : "empty";
    response["count"] = count;
    response["dim"] = dim;
    response["backend"] = backend;
    response["metric"] = metric;
    response["snapshot_version"] = version;
    response["uptime_sec"] = 0;  // TODO: implement uptime tracking
    response["qps_1m"] = 0.0;    // TODO: implement QPS tracking
    response["latency_ms"]["p50"] = 0.0;  // TODO: implement latency tracking
    response["latency_ms"]["p95"] = 0.0;
    response["latency_ms"]["p99"] = 0.0;
    
    res.set_content(response.dump(), "application/json");
  });

  // load - parse JSON and load actual snapshot
  svr.Post("/load", [](const httplib::Request& req, httplib::Response& res) {
    try {
      auto json_req = nlohmann::json::parse(req.body);
      
      std::string vectors_path = json_req["path"];
      std::string ids_path = json_req.value("ids_path", "");
      std::string metric = json_req.value("metric", "cosine");
      std::string backend = json_req.value("backend", "bruteforce");
      
      LOG_INFO("Loading snapshot from: " + vectors_path);
      
      // Load the snapshot
      SnapshotData snapshot = load_snapshot(vectors_path, ids_path);
      
      if (snapshot.count == 0) {
        res.status = 400;
        res.set_content("{ \"ok\": false, \"error\": \"Failed to load snapshot\" }", "application/json");
        return;
      }
      
      // Update global state
      {
        std::lock_guard<std::mutex> lk(g_index.m);
        g_index.snapshot = std::move(snapshot);
        g_index.loaded.store(true);
        g_index.dim = g_index.snapshot.dim;
        g_index.count = g_index.snapshot.count;
        g_index.backend = backend;
        g_index.metric = metric;
        g_index.snapshot_version = "v001";
      }
      
      // Return success response
      nlohmann::json response;
      response["ok"] = true;
      response["loaded"]["count"] = g_index.count;
      response["loaded"]["dim"] = g_index.dim;
      response["loaded"]["backend"] = g_index.backend;
      
      res.set_content(response.dump(), "application/json");
      LOG_INFO("Loaded " + std::to_string(g_index.count) + " vectors, dim=" + std::to_string(g_index.dim));
      
    } catch (const std::exception& e) {
      LOG_ERROR("Load request failed: " + std::string(e.what()));
      res.status = 400;
      res.set_content("{ \"ok\": false, \"error\": \"Invalid JSON or missing fields\" }", "application/json");
    }
  });

  // query - real KNN search
  svr.Post("/query", [](const httplib::Request& req, httplib::Response& res) {
    if (!g_index.loaded.load()) {
      res.status = 400;
      res.set_content("{ \"error\": \"No index loaded. Call /load first.\" }", "application/json");
      return;
    }
    
    try {
      auto json_req = nlohmann::json::parse(req.body);
      
      int k = json_req["k"];
      std::vector<float> query_vector = json_req["vector"];
      
      // Validate dimensions
      if (static_cast<int>(query_vector.size()) != g_index.dim) {
        res.status = 400;
        res.set_content("{ \"error\": \"Query vector dimension mismatch\" }", "application/json");
        return;
      }
      
      Timer timer;
      
      // Perform KNN search
      std::vector<Neighbor> neighbors = search_knn(
        query_vector,
        g_index.snapshot.data,
        g_index.snapshot.norms,
        g_index.snapshot.ids,
        g_index.snapshot.dim,
        g_index.snapshot.count,
        k
      );
      
      double latency = timer.elapsed_ms();
      
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
      response["backend"] = g_index.backend;
      
      res.set_content(response.dump(), "application/json");
      
      LOG_INFO("Query: k=" + std::to_string(k) + ", latency=" + std::to_string(latency) + "ms");
      
    } catch (const std::exception& e) {
      LOG_ERROR("Query request failed: " + std::string(e.what()));
      res.status = 400;
      res.set_content("{ \"error\": \"Invalid JSON or missing fields\" }", "application/json");
    }
  });

  // Simple static root to explain endpoints
  svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
    std::string html = "<html><body><h2>Boogie-Vec (stub)</h2>"
                       "<p>Endpoints: <code>/healthz</code>, <code>/stats</code>, <code>/load</code>, <code>/query</code></p>"
                       "</body></html>";
    res.set_content(html, "text/html");
  });

  std::cout << "[boogie-vec] starting server on 0.0.0.0:" << port << " (stub endpoints)\n";
  svr.listen("0.0.0.0", port);

  return 0;
}