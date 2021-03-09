
#pragma once
#ifndef EPOLL_SERVER_H
#define EPOLL_SERVER_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <wait.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>

using namespace std;

#define log(fmt, ...)                                                                        \
do {                                                                                         \
    printf("[LOG at pid:%d %s:%d] " #fmt "\n", getpid(), __FILE__, __LINE__, ##__VA_ARGS__); \
    fflush(stdout);                                                                          \
} while (0)

#define MODE_THUNDER_HERD 0
#define MODE_REUSE_PORT 1

class epollserver {
private:
    int _worker_num{ 4 };
    int _mode{ MODE_THUNDER_HERD };
    bool _et;
    bool _loop_accept;
    int _port;
    int _sleep;

public:
    epollserver(int size = 4) { _worker_num = size; }
    ~epollserver() {}

    int init_listen_fd() {
        const char* serv_ip = "0.0.0.0";
        int backlog = 100;
        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0)
            throw runtime_error("init listen socket failed.");
        set_socket_nonblock(listen_fd);
        if (_mode == MODE_REUSE_PORT)
            set_socket_reuseport(listen_fd);
        set_socket_binding(listen_fd, serv_ip, backlog);
        return listen_fd;
    }

    void set_socket_nonblock(int fd) {
        int flag = fcntl(fd, F_GETFL, 0);
        if (flag < 0)
            throw runtime_error("set nonblock socket failed.");
        int ret = fcntl(fd, F_SETFL, flag | O_NONBLOCK);
        if (ret < 0)
            throw runtime_error("set nonblock socket failed.");
    }

    void set_socket_reuseport(int fd) {
        int val = 1;
        int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof val);
        if (ret < 0)
            throw runtime_error("socket reuse port failed.");
    }

    void set_socket_binding(int listen_fd, const char* serv_ip, int backlog) {
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof serv_addr);
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(serv_ip);
        serv_addr.sin_port = htons(_port);
        int ret = bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr));
        if (ret < 0)
            throw runtime_error("bind to serv_ip port failed.");
        ret = listen(listen_fd, backlog);
        if (ret < 0)
            throw runtime_error("listen socket failed.");
    }

    void register_epoll_event(int epfd, int listen_fd, int events, int op) {
        struct epoll_event ev;
        memset(&ev, 0, sizeof ev);
        ev.events = events;
        if (_et)
            ev.events |= EPOLLET;
        ev.data.fd = listen_fd;
        int ret = epoll_ctl(epfd, op, listen_fd, &ev);
        if (ret)
            throw runtime_error("epoll_ctl failed.");
    }

    void send_hello(int conn_fd) {
        char buf[] = "hello from server.\n";
        write(conn_fd, buf, strlen(buf));
    }

    void handle_accept(int listen_fd) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        do {
            int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addrlen);
            if (conn_fd <= 0) {
                if (!_et)
                    log("thunder herd.");
                return;
            }
            log("accept connection from %s:%d", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
            send_hello(conn_fd);
            close(conn_fd);
        } while (_loop_accept);
    }

    void handle_read(int conn_fd) {
        
    }

    void handle_write(int conn_fd) {
        
    }

    void epoll_loop_once(int epfd, int listen_fd, int wait_ms) {
        const int max_events_count = 10;
        struct epoll_event ready_events[max_events_count];
        int nfds = epoll_wait(epfd, ready_events, max_events_count, wait_ms);
        if (nfds > 0 && _sleep)
            sleep(_sleep); // 测试惊群?
        for (auto& event : ready_events) {
            int fd = event.data.fd;
            int events = event.events;
            if (events & (EPOLLIN | EPOLLERR)) {
                if (fd == listen_fd)
                    handle_accept(listen_fd);
                else {
                    log("RECV");
                    handle_read(fd);
                }
            }
            else if (events & EPOLLOUT)
                handle_write(fd);
            else
                throw runtime_error("unknown event.");
        }
    }

    void start_worker(int epfd, int listen_fd) {
        if (_mode != MODE_THUNDER_HERD) {
            listen_fd = init_listen_fd();
            epfd = epoll_create(1);
            register_epoll_event(epfd, listen_fd, EPOLLIN, EPOLL_CTL_ADD);
        }
        int loop_wait_ms = 5000;
        while (1)
            epoll_loop_once(epfd, listen_fd, loop_wait_ms);
        close(listen_fd);
        close(epfd);
    }

    void parse_args(int argc, char** argv) {
        const char* usage = "Usage: ./server --mode 0|1 [--et] [--loop-accept] --port 1234 [--sleep n(s)]";
        if (argc < 5)
            cout << usage << endl;
        int i = 1;
        while (i < argc) {
            if (strcmp("--mode", argv[i]) == 0)
                _mode = atoi(argv[++i]);
            else if (strcmp("--et", argv[i]) == 0)
                _et = true;
            else if (strcmp("--loop-accept", argv[i]) == 0)
                _loop_accept = true;
            else if (strcmp("--port", argv[i]) == 0)
                _port = atoi(argv[++i]);
            else if (strcmp("--sleep", argv[i]) == 0)
                _sleep = atoi(argv[++i]);
            else
                throw runtime_error(usage);
            i++;
        }
    }

    void run(int argc, char** argv) {
        parse_args(argc, argv);
        int epfd = 0, listen_fd = 0;
        if (_mode == MODE_THUNDER_HERD) {
            listen_fd = init_listen_fd();
            epfd = epoll_create(1);
            register_epoll_event(epfd, listen_fd, EPOLLIN, EPOLL_CTL_ADD);
        }
        for (int i = 0; i < _worker_num; ++i) {
            pid_t pid = fork();
            if (pid == 0)
                start_worker(epfd, listen_fd);
        }

        while (wait(nullptr) > 0);
        if (errno == ECHILD)
            throw runtime_error("wait child process failed.");
    }
};

#define BUFFER_SIZE 1024
#define EPOLLSIZE 100

struct PACKET_HEAD {
    int length;
};

class Server {
private:
    struct sockaddr_in _server_addr;
    socklen_t _server_addr_len;
    int _listen_fd;
    int _epfd;
    struct epoll_event _events[EPOLLSIZE];

public:
    Server(int port) {
        memset(&_server_addr, 0, sizeof(_server_addr));
        _server_addr.sin_family = AF_INET;
        // _server_addr.sin_addr.s_addr = "0.0.0.0";
        _server_addr.sin_addr.s_addr = htons(INADDR_ANY);
        _server_addr.sin_port = htons(port);
        // create socket to listen
        _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_listen_fd < 0) {
            cout << "create socket failed.";
            exit(-1);
        }
        int opt = 1;
        setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    ~Server() {
        close(_epfd);
    }

    void Bind() {
        if (-1 == bind(_listen_fd, (struct sockaddr*)&_server_addr, sizeof(_server_addr))) {
            cout << "server bind failed.";
            exit(-1);
        }
        cout << "bind successfully." << endl;
    }

    void Listen(int queue_len = 20) {
        if (-1 == listen(_listen_fd, queue_len)) {
            cout << "server listen failed.";
            exit(-1);
        }
        cout << "listen successfully." << endl;
    }

    void Accept() {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int conn_fd = accept(_listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (conn_fd < 0) {
            cout << "server accept failed.";
            exit(-1);
        }
        cout << "new connection was accepted." << endl;

        // register in epoll
        struct epoll_event ev;
        ev.data.fd = conn_fd;
        ev.events = EPOLLIN;

        epoll_ctl(_epfd, EPOLL_CTL_ADD, conn_fd, &ev);
    }

    void Recv(int fd) {
        bool close_conn = false;

        PACKET_HEAD head;
        // 先接受包头，即数据总长度
        recv(fd, &head, sizeof(head), 0);

        char* buffer = new char[head.length];
        memset(buffer, 0, head.length);
        int tot = 0;
        while (tot < head.length) {
            int len = recv(fd, buffer + tot, head.length - tot, 0);
            if (len < 0) {
                cout << "recv() error." << endl;
                close_conn = true;
                break;
            }
            tot += len;
        }

        // echo back to client
        if (tot == head.length) {
            int ret1 = send(fd, &head, sizeof(head), 0);
            int ret2 = send(fd, buffer, head.length, 0);
            if (ret1 < 1 || ret2 < 1) {
                cout << "send() error." << endl;
                close_conn = true;
            }
        }

        delete buffer;

        if (close_conn) {
            close(fd);
            struct epoll_event ev;
            ev.data.fd = fd;
            ev.events = EPOLLIN;
            epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, &ev); // delete fd
        }
    }

    void Run() {
        // create the epoll fd
        _epfd = epoll_create(1);
        
        struct epoll_event ev;
        ev.data.fd = _listen_fd;
        ev.events = EPOLLIN;
        epoll_ctl(_epfd, EPOLL_CTL_ADD, _listen_fd, &ev);

        while (1) {
            // -1 means wait forever
            int nfds = epoll_wait(_epfd, _events, EPOLLSIZE, -1);
            if (nfds < 0) {
                cout << "poll() error.";
                exit(-1);
            }

            if (nfds == 0)
                continue;
            
            for (int i = 0; i < nfds; ++i) {
                int fd = _events[i].data.fd;
                if ((fd == _listen_fd) && (_events[i].events & EPOLLIN))
                    Accept(); // 有新的请求
                else
                    Recv(fd); // 读数据
            }
        }
    }
};

#endif
