// Boogie-Vec: Tiny vector search service
#include "server.hpp"

int main(int argc, char** argv) {
    int port = 8080;
    if (argc >= 2) {
        try { port = std::stoi(argv[1]); } catch(...) {}
    }

    Server server(nullptr, port);
    server.run();

    return 0;
}