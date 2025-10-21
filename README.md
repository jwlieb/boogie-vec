# boogie-vec
Boogie-Vec is a small C++ service that loads an embedding snapshot and answers KNN queries over HTTP. It’s designed to be simple to run, easy to swap backends (brute-force → Annoy → FAISS), and fast enough for 10–50k vectors out of the box.
