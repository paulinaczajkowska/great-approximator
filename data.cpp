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
#include <unistd.h>


#include "err.h"
#include "common.h"
#include "messages.h"
#include "data.h"

using namespace std;



void ServerArgs::read(int argc, char* argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "p:k:n:m:f:")) != -1) {
        switch (opt) {
            case 'p': {
                uint16_t new_port = read_port(optarg);
                if (port != DEFAULT_PORT && port != new_port)
                    fatal("conflicting -p values");
                if (new_port < MIN_PORT || new_port > MAX_PORT)
                    fatal("invalid port number");
                port = new_port;
                break;
            }
            case 'k': {
                uint16_t new_k = static_cast<uint16_t>(read_number(optarg));
                if (k != DEFAULT_K && k != new_k)
                    fatal("conflicting -k values");
                if (new_k < MIN_K || new_k > MAX_K)
                    fatal("invalid -k");
                k = new_k;
                break;
            }
            case 'n': {
                uint8_t new_n = static_cast<uint8_t>(read_number(optarg));
                if (n != DEFAULT_N && n != new_n)
                    fatal("conflicting -n values");
                if (new_n < MIN_N || new_n > MAX_N)
                    fatal("invalid -n");
                n = new_n;
                break;
            }
            case 'm': {
                uint32_t new_m = static_cast<uint32_t>(read_number(optarg));
                if (m != DEFAULT_M && m != new_m)
                    fatal("conflicting -m values");
                if (new_m < MIN_M || new_m > MAX_M)
                    fatal("invalid -m");
                m = new_m;
                break;
            }
            case 'f': {
                if (!file.empty() && file != optarg)
                    fatal("conflicting -f values");
                file = optarg;
                break;
            }
            default:
                fatal("Usage: %s -f file [-p port] [-k value] [-n value] [-m value]", argv[0]);
        }
    }

    if (file.empty()) {
        fatal("Usage: %s -f file [-p port] [-k value] [-n value] [-m value]", argv[0]);
    }
}

void ServerArgs::print() const {
    cout << "Argumenty serwera:\n"
         << "  Plik: " << file << '\n'
         << "  Port: " << port << '\n'
         << "  K: " << k << '\n'
         << "  N: " << static_cast<uint32_t>(n) << '\n'
         << "  M: " << m << '\n';
}

void Function::print_func() {
    cout << "COEFF" << "\n";
    for (size_t i = 0; i < values.size(); ++i) {
        cout << values[i] << ' ';
    }
    cout << "\n" << "\n";

    cout << "ESTIMATED_VALUES" << "\n";
    for (size_t i = 0; i < estimated_values.size(); ++i) {
        cout << estimated_values[i] << ' ';
    }
    cout << "\n" << "\n";

    cout << "VALUES" << "\n";
    for (size_t i = 0; i < values.size(); ++i) {
        cout << values[i] << ' ';
    }
}

void Function::add(int point, double value) {
    estimated_values[point] += value;
}

int Function::len() {
    return values.size();
}

double Function::estimated_value(int point) {
    return estimated_values[point];
}

double Function::value(int point) {
    return values[point];
}

void Function::set_coeffs(vector<double>& coeff_) {
    coeff = coeff_;
}

void Function::calculate_values() {
    int degree = coeff.size() - 1;
    int K = values.size() - 1;

    for (int x = 0; x <= K; ++x) {
        double fx = 0.0;
        double x_pow = 1.0;  // x^0 na początek

        for (int i = 0; i <= degree; ++i) {
            fx += coeff[i] * x_pow;
            x_pow *= x;  // przechodzimy do x^{i+1}
        }

        values[x] = fx;
    }
}

double Function::calculate_error() {
    double error = 0.0;
    int K = values.size() - 1;

    for (int x = 0; x <= K; ++x) {
        double diff = estimated_values[x] - values[x];
        error += diff * diff;
    }

    return error;
}

void Function::init(int K) {
    values = vector<double>(K + 1, 0);
    estimated_values = vector<double>(K + 1, 0);
}

void ClientArgs::read(int argc, char* argv[]) {
    int opt;

    while ((opt = getopt(argc, argv, "u:s:p:46a")) != -1) {
        switch (opt) {
            case 'u':
                if (!player_id.empty() && player_id != optarg) {
                    fatal("conflicting -u values");
                }
                player_id = optarg;
                break;

            case 's':
                if (!server.empty() && server != optarg) {
                    fatal("conflicting -s values");
                }
                server = optarg;
                break;

            case 'p': {
                uint16_t new_port = read_port(optarg);
                if (port != 0 && port != new_port) {
                    fatal("conflicting -p values");
                }
                if (new_port < MIN_PORT || new_port > MAX_PORT) {
                    fatal("invalid port number");
                }
                port = new_port;
                break;
            }

            case '4':
                use_ipv4 = true;
                break;

            case '6':
                use_ipv6 = true;
                break;

            case 'a':
                strategy_a = true;
                break;

            default:
                fatal("Usage: %s -u player_id -s server -p port [-4] [-6] [-a]", argv[0]);
        }
    }

    if (player_id.empty() || server.empty() || port == 0) {
        fatal("Missing required arguments: -u, -s, -p");
    }

    if (!validate_player_id(player_id)) {
        fatal("Invalid player_id: must contain only [a-zA-Z0-9]");
    }

    if (use_ipv4 && use_ipv6) {
        cerr << "Both -4 and -6 provided. Using address family from getaddrinfo.\n";
    }
}

void ClientArgs::print() {
    cout << "Player ID   : " << player_id << '\n'
            << "Server      : " << server << '\n'
            << "Port        : " << port << '\n'
            << "IPv4 Forced : " << (use_ipv4 ? "Yes" : "No") << '\n'
            << "IPv6 Forced : " << (use_ipv6 ? "Yes" : "No") << '\n'
            << "Strategy A  : " << (strategy_a ? "Yes" : "No") << '\n';
}


bool ClientArgs::validate_player_id(const string& id) {
    for (char c : id) {
        if (!isalnum(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

void ClientInfo::set_player_id(const string& id) {
    player_id = id;
    lowercase_count = 0;
    for (char c : player_id) {
        if (islower(static_cast<unsigned char>(c))) {
            lowercase_count++;
        }
    }
}

void ClientInfo::queue_message(const string& type, const string& msg, time_t send_time, vector<pollfd>& fds, uint64_t nr) {
    outgoing_messages.push({type, msg, send_time, nr});
    for (auto& pollfd : fds) {
        if (pollfd.fd == fd) {
            pollfd.events |= POLLOUT;
            break;
        }
    }
}

bool ClientInfo::has_pending_message() {
    return !outgoing_messages.empty();
}

OutgoingMessage ClientInfo::next_message() {
    if (outgoing_messages.empty()) {
        throw runtime_error("No messages to send");
    }
    return outgoing_messages.top();
}

void ClientInfo::pop_message(vector<pollfd>& fds) {
    if (!outgoing_messages.empty()) {
        outgoing_messages.pop();
    }
    if (outgoing_messages.empty()) {
        for (auto& pollfd : fds) {
        if (pollfd.fd == fd) {
            pollfd.events = POLLIN;
            break;
        }
    }
    }
}