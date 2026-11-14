#ifndef DATA_H
#define DATA_H

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <string>
#include <vector>
#include <queue>

#include "err.h"
#include "common.h"
#include "messages.h"

#include <time.h>
#include <bits/poll.h>
#include <sys/poll.h>
#include <stdexcept>

using namespace std;


constexpr uint16_t DEFAULT_PORT = 0;
constexpr uint16_t DEFAULT_K = 100;
constexpr uint8_t DEFAULT_N = 4;
constexpr uint32_t DEFAULT_M = 131;

constexpr uint16_t MIN_PORT = 0;
constexpr uint16_t MAX_PORT = 65535;
constexpr uint16_t MIN_K = 1;
constexpr uint16_t MAX_K = 10000;
constexpr uint8_t MIN_N = 1;
constexpr uint8_t MAX_N = 8;
constexpr uint32_t MIN_M = 1;
constexpr uint32_t MAX_M = 12341234;

constexpr size_t QUEUE_LENGTH = 1000;
constexpr uint8_t PENALTY = 20;
constexpr uint8_t BAD_PUT = 10;
constexpr size_t BUFFER_SIZE = 65536;


class ServerArgs {
public:
    uint16_t port;
    uint16_t k;
    uint8_t n;
    uint32_t m;
    string file;

    ServerArgs()
        : port(DEFAULT_PORT), k(DEFAULT_K), n(DEFAULT_N), m(DEFAULT_M), file() {}

    void read(int argc, char* argv[]);
    void print() const;
};

struct Function {
    vector<double> values;
    vector<double> estimated_values;
    vector<double> coeff;

    Function() {}
    Function(int K) : values(K + 1, 0), estimated_values(K + 1, 0) {}

    void init(int K);
    void add(int point, double value);
    int len();
    double value(int point);
    double estimated_value(int point);
    void calculate_values();
    double calculate_error();
    void print_func();
    void set_coeffs(vector<double>& coeff_);
};

struct ClientInfo {
    string player_id;
    int fd = -1;
    time_t connected_at = 0;
    bool hello = false;
    string ip;
    uint16_t port;
    sockaddr_storage addr;
    socklen_t addr_len = 0;
    int penalty = 0;
    string buffer = "";
    int lowercase_count = 0;
    priority_queue<OutgoingMessage, vector<OutgoingMessage>, MessageCompare> outgoing_messages;
    Function f;
    uint64_t put_count = 0;
    uint64_t last_put_response = 0;
    bool last_response = true;
    uint64_t all_put_count = 0;


    ClientInfo(int fd_, string ip_, uint16_t port_, uint16_t K)
        : fd(fd_), connected_at(time(nullptr)), hello(false), ip(ip_), port(port_), penalty(0), buffer(""), lowercase_count(0), f(K), put_count(0), last_put_response(0), last_response(true), all_put_count(0) {}

    void set_player_id(const string& id);
    void queue_message(const string& type, const string& msg, time_t send_time, vector<pollfd>& fds, uint64_t nr);
    bool has_pending_message();
    OutgoingMessage next_message();
    void pop_message(vector<pollfd>& fds);
};

class ClientArgs {
public:
    string player_id;
    string server;
    uint16_t port = 0;
    bool use_ipv4 = false;
    bool use_ipv6 = false;
    bool strategy_a = false;

    void read(int argc, char* argv[]);
    void print();

private:
    static bool validate_player_id(const string& id);
};

#endif