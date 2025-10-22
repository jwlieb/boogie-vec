#!/bin/bash
# test_boogie_vec.sh - End-to-end integration test for Boogie-Vec

set -e  # Exit on any error

echo "Testing Boogie-Vec..."

# Check if test data exists
if [ ! -f "data/vectors.bin" ] || [ ! -f "data/ids.json" ]; then
    echo "Test data not found. Run 'python3 examples/ingest.py 10 384' first."
    exit 1
fi

# Start server in background
echo "Starting server..."
./build/boogie_vec_server 8080 &
SERVER_PID=$!
sleep 2

# Test health
echo "Health check:"
curl -s http://localhost:8080/healthz
echo ""

# Test load
echo "Loading test data:"
PROJECT_ROOT=$(pwd)
curl -s -X POST http://localhost:8080/load \
  -H 'Content-Type: application/json' \
  -d "{\"path\": \"$PROJECT_ROOT/data/vectors.bin\", \"dim\": 384, \"metric\": \"cosine\", \"ids_path\": \"$PROJECT_ROOT/data/ids.json\", \"backend\": \"bruteforce\"}"
echo ""

# Test query (384-dimensional vector)
echo "Testing query:"
# Generate a simple 384-dim vector for testing
VECTOR=$(python3 -c "import json; import random; random.seed(42); v=[random.uniform(-0.1, 0.1) for _ in range(384)]; print(json.dumps({'k': 3, 'vector': v}))")
curl -s -X POST http://localhost:8080/query \
  -H 'Content-Type: application/json' \
  -d "$VECTOR"
echo ""

# Test stats
echo "Stats:"
curl -s http://localhost:8080/stats
echo ""

# Cleanup
echo "Cleaning up..."
kill $SERVER_PID 2>/dev/null || true
echo "All tests passed!"