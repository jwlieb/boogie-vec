#!/usr/bin/env python3
"""
Test data generator for Boogie-Vec.
Generates random embeddings and saves them in the expected binary format.
"""

import numpy as np
import json
import os
import struct
import sys

def generate_test_data(num_vectors=100, dim=384, output_dir="data"):
    """Generate test vectors and IDs."""
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Generate random vectors (normalized for cosine similarity)
    print(f"Generating {num_vectors} random {dim}-dimensional vectors...")
    vectors = np.random.randn(num_vectors, dim).astype(np.float32)
    
    # Normalize vectors for cosine similarity
    norms = np.linalg.norm(vectors, axis=1, keepdims=True)
    vectors = vectors / norms
    
    # Generate IDs
    ids = [f"track_{i:06d}" for i in range(num_vectors)]
    
    # Write vectors.bin (little-endian binary format)
    vectors_path = os.path.join(output_dir, "vectors.bin")
    print(f"Writing vectors to: {vectors_path}")
    
    with open(vectors_path, 'wb') as f:
        # Write header: [uint32 dim][uint32 count]
        f.write(struct.pack('<I', dim))      # dim (little-endian uint32)
        f.write(struct.pack('<I', num_vectors))  # count (little-endian uint32)
        
        # Write vector data (row-major: v0[0..dim-1], v1[0..dim-1], ...)
        vectors_flat = vectors.flatten()
        f.write(vectors_flat.tobytes())
    
    # Write ids.json
    ids_path = os.path.join(output_dir, "ids.json")
    print(f"Writing IDs to: {ids_path}")
    
    with open(ids_path, 'w') as f:
        json.dump(ids, f, indent=2)
    
    print(f"\nGenerated test data:")
    print(f"  Vectors: {vectors_path}")
    print(f"  IDs: {ids_path}")
    print(f"  Count: {num_vectors}")
    print(f"  Dimension: {dim}")
    
    # Print example curl commands
    abs_vectors_path = os.path.abspath(vectors_path)
    abs_ids_path = os.path.abspath(ids_path)
    
    print(f"\nExample curl commands:")
    print(f"# Load the snapshot:")
    print(f'curl -X POST http://localhost:8080/load \\')
    print(f'  -H "Content-Type: application/json" \\')
    print(f'  -d \'{{"path": "{abs_vectors_path}", "dim": {dim}, "metric": "cosine", "ids_path": "{abs_ids_path}", "backend": "bruteforce"}}\'')
    
    print(f"\n# Query with a random vector:")
    query_vector = np.random.randn(dim).astype(np.float32)
    query_vector = query_vector / np.linalg.norm(query_vector)  # Normalize
    query_json = json.dumps({"k": 5, "vector": query_vector.tolist()})
    print(f'curl -X POST http://localhost:8080/query \\')
    print(f'  -H "Content-Type: application/json" \\')
    print(f'  -d \'{query_json}\'')

if __name__ == "__main__":
    # Parse command line arguments
    num_vectors = 100
    dim = 384
    
    if len(sys.argv) > 1:
        num_vectors = int(sys.argv[1])
    if len(sys.argv) > 2:
        dim = int(sys.argv[2])
    
    generate_test_data(num_vectors, dim)
