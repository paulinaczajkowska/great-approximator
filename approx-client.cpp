#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>
#include <cctype>
#include <vector>
#include <regex>
#include <queue>

#include "err.h"
#include "common.h"
#include "messages.h"
#include "data.h"

using namespace std;

bool is_valid_input_line(const string& line, double& point, double& value);


int main(int argc, char* argv[]) {
    ClientArgs args;
    args.read(argc, argv);

    struct sockaddr_storage server_address = get_server_address2(
        args.server.c_str(),
        args.port,
        args.use_ipv4,
        args.use_ipv6
    );

    int family = server_address.ss_family;
    int socket_fd = socket(family, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syserr("socket");
    }

    if (connect(socket_fd, reinterpret_cast<struct sockaddr*>(&server_address),
                static_cast<socklen_t>(sizeof(server_address))) < 0) {
        syserr("connect");
    }


    priority_queue<OutgoingMessage, vector<OutgoingMessage>, MessageCompare> outgoing_messages;
    char buffer[BUFFER_SIZE];
    string client_buffer = "";

    Function f;
    bool is_state = false;

    vector<struct pollfd> fds;
    fds.push_back({ socket_fd, POLLIN | POLLOUT, 0 });

    send_hello(args.player_id, outgoing_messages);

    if (!args.strategy_a) {
        fds.push_back({ STDIN_FILENO, POLLIN, 0 });
    }

    while (true) {
        int ready = poll(fds.data(), fds.size(), -1);
        if (ready < 0) {
            syserr("poll");
        }

        if (!args.strategy_a) {
            if (fds[1].revents & POLLIN) {
                fds[1].revents = 0;

                string line;
                if (!getline(cin, line)) {
                    syserr("getline");
                }

                double point;
                double value;
                if (!is_valid_input_line(line, point, value)) {
                    cerr << "ERROR: invalid input line " << line << "\n";
                }
                else {
                    send_put(point, value, outgoing_messages);
                }

            }
        }

        if (fds[0].revents & POLLIN) {

            ssize_t bytes = read(fds[0].fd, buffer, BUFFER_SIZE);
            if (bytes < 0) {
                if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
                    syserr("read");
                }
            }
            else if (bytes == 0) {
                cout << "Server disconnected\n";
                break;
            }
            else {
                client_buffer.append(buffer, bytes);
                check_message2(client_buffer, args.strategy_a, f, outgoing_messages, is_state, args);
            }

        }

        if (fds[0].revents & POLLOUT) {
            while (!outgoing_messages.empty()) {
                OutgoingMessage msg = outgoing_messages.top();
                outgoing_messages.pop();


                ssize_t sent = send(fds[0].fd, msg.message.c_str(), msg.message.size(), 0);
                if (sent < 0) {
                    syserr("send");
                }

                cout << "Sending " << msg.message.substr(0, msg.message.length() - 2) << "\n";
            }
        }
    }


    close(fds[0].fd);
    if (!args.strategy_a) {
        close(fds[1].fd);
    }
    return 0;
}

bool is_valid_input_line(const string& line, double& point, double& value) {
    static const regex pattern(
        R"((-?\d+(?:\.\d{0,7})?)\s+(-?\d+(?:\.\d{0,7})?)\s*$)"
    );

    smatch match;
    if (!regex_match(line, match, pattern)) {
        return false;
    }

    try {
        point = stod(match[1].str());
        value = stod(match[2].str());
    } catch (...) {
        return false;
    }

    return true;
}