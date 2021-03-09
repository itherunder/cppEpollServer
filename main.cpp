
#include "epollserver.hpp"

using namespace std;

namespace epollservertest {
void testepollserver(int argc, char** argv) {
    epollserver server;
    server.run(argc, argv);
}
}

int main(int argc, char** argv) {
    epollservertest::testepollserver(argc, argv);
    return 0;
}

