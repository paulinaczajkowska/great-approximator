#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fstream>
#include <queue>
#include <regex>
#include <sstream>

#include "err.h"
#include "common.h"
#include "data.h"
#include "messages.h"

using namespace std;

// Functions responsible for handling events on socket file descriptors (fd),
// such as accepting new client connections or processing incoming/outgoing data.
void handle_main_sockets(vector<pollfd>& fds, vector<ClientInfo>& clients, ServerArgs& args);
void handle_client_sockets(vector<pollfd>& fds, vector<ClientInfo>& clients, char* buffer, ifstream& coeff_file, ServerArgs& args, uint32_t& count_M);

// Functions responsible for proper socket setup and teardown:
// creating, configuring, binding, listening, and closing sockets.
int set_nonblocking(int fd);
int create_listening_socket(uint16_t port, int family);
void close_all_fds(struct pollfd* fds, size_t nfds);
void close_client_fds(struct pollfd* fds, size_t nfds);


int main(int argc, char* argv[]) {
    ServerArgs args;
    args.read(argc, argv);

    int sock_v4 = create_listening_socket(args.port, AF_INET);
    int sock_v6 = create_listening_socket(args.port, AF_INET6);

    vector<struct pollfd> fds;
    vector<ClientInfo> clients;
    char buffer[BUFFER_SIZE];

    ifstream coeff_file(args.file);
    if (!coeff_file.is_open()) {
        syserr("open");
    }

    while(true) {
        uint32_t count_M = 0;
        vector<struct pollfd> fds;
        vector<ClientInfo> clients;

        fds.push_back({ sock_v4, POLLIN, 0 });
        fds.push_back({ sock_v6, POLLIN, 0 });

        while (count_M < args.m) {
            int ready = poll(fds.data(), fds.size(), 1000);
            if (ready < 0) {
                syserr("poll");
            }
            if (ready >= 0) {
                disconnect_unresponsive_clients(fds, clients);
            }
            if (ready > 0) {
                handle_main_sockets(fds, clients, args);
                handle_client_sockets(fds, clients, buffer, coeff_file, args, count_M);
            }
        }
        send_scoring(clients);
        close_client_fds(fds.data(), fds.size());

    }

    close_all_fds(fds.data(), fds.size());
    return 0;
}


int create_listening_socket(uint16_t port, int family) {
    int socket_fd = socket(family, SOCK_STREAM, 0);
    if (socket_fd < 0)
        syserr("socket");

    int optval = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
        syserr("setsockopt SO_REUSEADDR");

    if (family == AF_INET6) {
        int v6only = 1;
        if (setsockopt(socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0)
            syserr("setsockopt IPV6_V6ONLY");
    }

    if (family == AF_INET) {
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            syserr("bind IPv4");
    } else if (family == AF_INET6) {
        sockaddr_in6 addr {};
        addr.sin6_family = AF_INET6;
        addr.sin6_addr = in6addr_any;
        addr.sin6_port = htons(port);

        if (bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            syserr("bind IPv6");
    } else {
        cerr << "wrong family\n";
        close(socket_fd);
        exit(1);
    }

    if (set_nonblocking(socket_fd) < 0)
        syserr("set_nonblocking");

    if (listen(socket_fd, QUEUE_LENGTH) < 0)
        syserr("listen");

    return socket_fd;
}

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void close_all_fds(struct pollfd* fds, size_t nfds) {
    for (size_t i = 0; i < nfds; ++i) {
        if (fds[i].fd >= 0) {
            close(fds[i].fd);
            fds[i].fd = -1;
        }
    }
}

void close_client_fds(struct pollfd* fds, size_t nfds) {
    for (size_t i = 2; i < nfds; ++i) {
        if (fds[i].fd >= 0) {
            close(fds[i].fd);
            fds[i].fd = -1;
        }
    }
}

void handle_main_sockets(vector<pollfd>& fds, vector<ClientInfo>& clients, ServerArgs& args) {
    for (size_t i = 0; i < 2; ++i) {
        if (fds[i].revents & POLLIN) {
            fds[i].revents = 0;

            sockaddr_storage client_addr;
            socklen_t client_addr_len = sizeof(client_addr);

            int client_fd = accept(fds[i].fd, (sockaddr*)&client_addr, &client_addr_len);
            if (client_fd < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                    syserr("accept");
            }
            else {
                set_nonblocking(client_fd);

                char ip_str[INET6_ADDRSTRLEN];
                uint16_t port = 0;

                if (client_addr.ss_family == AF_INET) {
                    sockaddr_in* addr_in = (sockaddr_in*)&client_addr;
                    inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, sizeof(ip_str));
                    port = ntohs(addr_in->sin_port);
                } else if (client_addr.ss_family == AF_INET6) {
                    sockaddr_in6* addr_in6 = (sockaddr_in6*)&client_addr;
                    inet_ntop(AF_INET6, &(addr_in6->sin6_addr), ip_str, sizeof(ip_str));
                    port = ntohs(addr_in6->sin6_port);
                } else {
                    strcpy(ip_str, "UNKNOWN");
                    port = 0;
                }

                fds.push_back({ client_fd, POLLIN, 0 });
                clients.emplace_back(client_fd, string(ip_str), port, args.k);

                cout << "New client " << " [" << ip_str << "]:" << port << '\n';
            }
        }
    }
}


void handle_client_sockets(vector<pollfd>& fds, vector<ClientInfo>& clients, char* buffer, ifstream& coeff_file, ServerArgs& args, uint32_t& count_M) {
    for (size_t i = fds.size() - 1; i >= 2; --i) {
        auto& client = clients[i - 2];

        if (fds[i].revents & POLLOUT) {
            while (client.has_pending_message()) {
                OutgoingMessage msg = client.next_message();

                if (msg.send_at <= time(nullptr)) {
                    client.pop_message(fds);

                    ssize_t sent = send(client.fd, msg.message.c_str(), msg.message.size(), 0);
                    if (sent < 0) {
                        syserr("send");
                    }

                    cout << "Sending " << msg.message.substr(0, msg.message.length() - 2)
                         << " to [" << client.ip << "]:" << client.port << ", " << client.player_id << "\n";

                    client.last_put_response = max(client.last_put_response, msg.nr);
                }
                else {
                    break;
                }
            }
        }

        if (fds[i].revents & POLLIN) {
                ssize_t bytes = read(fds[i].fd, buffer, BUFFER_SIZE);
                if (bytes < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }
                    else {
                        syserr("read");
                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        clients.erase(clients.begin() + (i - 2));
                    }
                }
                else if (bytes == 0) {
                    cout << "Client disconnected:"
                         << ", [" << client.ip << "]:" << client.port
                         << ", player_id=" << (client.hello ? client.player_id : "UNKNOWN") << "\n";

                    count_M -= client.put_count;
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    clients.erase(clients.begin() + (i - 2));
                }
                else {
                    client.buffer.append(buffer, bytes);
                    check_message(client, fds, clients, i, coeff_file, args, count_M);
                    if (count_M >= args.m) {
                        break;
                    }
                }
        }
    }
}