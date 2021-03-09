
#include "epollserver.hpp"

using namespace std;

namespace epollservertest {
// the epollserver
void testepollserver(int argc, char** argv) {
    epollserver server;
    server.run(argc, argv);
}

// the Server
void testepollserver() {
    Server server(2333);
    server.Bind();
    server.Listen();
    server.Run();
}
}

int main(int argc, char** argv) {
    // epollservertest::testepollserver(argc, argv);
    epollservertest::testepollserver();
    return 0;
}

