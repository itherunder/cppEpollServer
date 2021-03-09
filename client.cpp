
#include <netinet/in.h>   // sockaddr_in
#include <sys/types.h>    // socket
#include <sys/socket.h>   // socket
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <iostream>
#include <string>

using namespace std;
#define BUFFER_SIZE 1024

struct PACKET_HEAD {
    int length;
};

class Client {
private:
    struct sockaddr_in _server_addr;
    socklen_t _server_addr_len;
    int _fd;

public:
    Client(const char* ip, int port) {
        memset(&_server_addr, 0, sizeof(_server_addr));
        _server_addr.sin_family = AF_INET;
        if (inet_pton(AF_INET, ip, &_server_addr.sin_addr) == 0) {
            cout << "server ip address error.";
            exit(-1);
        }
        _server_addr.sin_port = htons(port);
        _server_addr_len = sizeof(_server_addr);
        // create socket
        _fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_fd < 0) {
            cout << "create socket failed.";
            exit(-1);
        }
    }

    ~Client() { close(_fd); }

    void Connect() {
        cout << "connecting..." << endl;
        if (connect(_fd, (struct sockaddr*)&_server_addr, _server_addr_len) < 0) {
            cout << "can not connect to server ip.";
            exit(-1);
        }
        cout << "connect to server successfully." << endl;
    }

    void Send(string str) {
        PACKET_HEAD head;
        head.length = str.size() + 1;
        int ret1 = send(_fd, &head, sizeof(head), 0);
        int ret2 = send(_fd, str.c_str(), head.length, 0);
        if (ret1 < 0 || ret2 < 0) {
            cout << "send message failed.";
            exit(-1);
        }
    }

    string Recv() {
        PACKET_HEAD head;
        recv(_fd, &head, sizeof(head), 0);
        
        char* buffer = new char[head.length];

        int tot = 0;
        while (tot < head.length) {
            int len = recv(_fd, buffer + tot, head.length - tot, 0);
            if (len < 0) {
                cout << "recv() error." << endl;
                break;
            }
            tot += len;
        }
        string ret(buffer);
        delete buffer;
        return ret;
    }
};

int main(int argc, char** argv) {
    Client client("127.0.0.1", 2333);
    client.Connect();
    while (1) {
        string msg;
        getline(cin, msg);
        if (msg == "exit")
            break;
        client.Send(msg);
        cout << "the echo from server: " << client.Recv() << endl;
    }
    return 0;
}
