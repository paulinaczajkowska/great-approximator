#ifndef MESSAGES_H
#define MESSAGES_H

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
#include <string>

#include "err.h"
#include "common.h"

using namespace std;

struct ClientInfo;
class ServerArgs;
struct Function;
class ClientArgs;


struct OutgoingMessage {
    string type;
    string message;
    time_t send_at;
    uint64_t nr;
};

struct MessageCompare {
    bool operator()(const OutgoingMessage& a, const OutgoingMessage& b) const {
        return a.send_at > b.send_at;
    }
};

void send_scoring(vector<ClientInfo>& clients);
void print_bad_message(const ClientInfo& client, const string message);
void check_message(ClientInfo& client, vector<pollfd>& fds, vector<ClientInfo>& clients, size_t ix, ifstream& coeff_file, ServerArgs& args, uint32_t& count_M);
bool type_put(string& message);
bool validate_put(const string& message, int K);
void send_penalty(ClientInfo& client, string message2, vector<pollfd>& fds);
void send_bad_put(ClientInfo& client, string message2, vector<pollfd>& fds);
void send_coeff(ClientInfo& client, ifstream& coeff_file, vector<pollfd>& fds, string message2);
void send_state(ClientInfo& client, vector<pollfd>& fds);
void read_hello(ClientInfo& client, string message, vector<pollfd>& fds, vector<ClientInfo>& clients, size_t ix, ifstream& coeff_file);
void read_put(ClientInfo& client, string message, vector<pollfd>& fds, ServerArgs& args, uint32_t& count_M);
void disconnect_unresponsive_clients(vector<pollfd>& fds, vector<ClientInfo>& clients);

void send_put(double point, double value, priority_queue<OutgoingMessage, vector<OutgoingMessage>, MessageCompare>& outgoing_messages);
void check_message2(string& client_buffer, bool strategy_a, Function& f, priority_queue<OutgoingMessage, vector<OutgoingMessage>, MessageCompare>& outgoing_messages, bool& is_state, ClientArgs& args);
bool validate_bad_put(const string& message);
bool validate_coeff(const string& message, vector<double>& coeff_vector);
bool validate_state(const string& message, int& K);
bool validate_penalty(const string& message);
bool validate_scoring(const string& message);
void send_hello(string& player_id, priority_queue<OutgoingMessage, vector<OutgoingMessage>, MessageCompare>& outgoing_messages);

#endif