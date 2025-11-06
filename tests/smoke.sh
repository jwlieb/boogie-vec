#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
SERVER="$BUILD_DIR/boogie_vec_server"
DATA_DIR="$PROJECT_ROOT/data"

echo "=== Boogie-Vec Smoke Test ==="

# Build
echo "Building..."
cd "$PROJECT_ROOT"
mkdir -p build
cd build
cmake .. > /dev/null 2>&1
make -j4 > /dev/null 2>&1

if [ ! -f "$SERVER" ]; then
    echo "ERROR: Server binary not found at $SERVER"
    exit 1
fi

echo "Build successful!"

# Generate test data if needed
if [ ! -f "$DATA_DIR/vectors.bin" ] || [ ! -f "$DATA_DIR/ids.json" ]; then
    echo "Generating test data..."
    python3 "$PROJECT_ROOT/examples/ingest.py" 50 384
fi

VECTORS_PATH="$DATA_DIR/vectors.bin"
IDS_PATH="$DATA_DIR/ids.json"

# Start server in background
echo "Starting server..."
PORT=8080
"$SERVER" $PORT > /tmp/boogie_vec_smoke.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 2

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"
    cat /tmp/boogie_vec_smoke.log
    exit 1
fi

# Function to cleanup
cleanup() {
    echo "Cleaning up..."
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
}
trap cleanup EXIT

# Test 1: Health check
echo "Test 1: Health check..."
RESPONSE=$(curl -s http://127.0.0.1:$PORT/healthz)
if [ "$RESPONSE" != "ok" ]; then
    echo "ERROR: Health check failed. Got: $RESPONSE"
    exit 1
fi
echo "✓ Health check passed"

# Test 2: Load snapshot
echo "Test 2: Load snapshot..."
LOAD_RESPONSE=$(curl -s -X POST http://127.0.0.1:$PORT/load \
    -H 'Content-Type: application/json' \
    -d "{
        \"path\": \"$VECTORS_PATH\",
        \"dim\": 384,
        \"metric\": \"cosine\",
        \"ids_path\": \"$IDS_PATH\",
        \"backend\": \"bruteforce\"
    }")

if echo "$LOAD_RESPONSE" | grep -q '"ok":true'; then
    echo "✓ Load successful"
else
    echo "ERROR: Load failed. Response: $LOAD_RESPONSE"
    exit 1
fi

# Test 3: Query
echo "Test 3: Query..."
QUERY_VECTOR=$(python3 -c "import json; print(json.dumps([0.1] * 384))")
QUERY_RESPONSE=$(curl -s -X POST http://127.0.0.1:$PORT/query \
    -H 'Content-Type: application/json' \
    -d "{
        \"k\": 5,
        \"vector\": $QUERY_VECTOR
    }")

if echo "$QUERY_RESPONSE" | grep -q '"neighbors"'; then
    NEIGHBOR_COUNT=$(echo "$QUERY_RESPONSE" | python3 -c "import sys, json; print(len(json.load(sys.stdin)['neighbors']))")
    if [ "$NEIGHBOR_COUNT" -gt 0 ]; then
        echo "✓ Query successful (got $NEIGHBOR_COUNT neighbors)"
    else
        echo "ERROR: Query returned no neighbors"
        exit 1
    fi
else
    echo "ERROR: Query failed. Response: $QUERY_RESPONSE"
    exit 1
fi

# Test 4: Stats
echo "Test 4: Stats..."
STATS_RESPONSE=$(curl -s http://127.0.0.1:$PORT/stats)
if echo "$STATS_RESPONSE" | grep -q '"status":"ready"'; then
    echo "✓ Stats endpoint working"
else
    echo "ERROR: Stats check failed. Response: $STATS_RESPONSE"
    exit 1
fi

echo ""
echo "=== All tests passed! ==="

