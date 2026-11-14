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
#include <cmath>

#include "err.h"
#include "common.h"
#include "data.h"
#include "messages.h"

void disconnect_unresponsive_clients(vector<pollfd>& fds, vector<ClientInfo>& clients) {
    for (size_t i = fds.size() - 1; i >= 2; --i) {
        auto& client = clients[i - 2];

        if (!client.hello && time(nullptr) - client.connected_at > 3) {
            cout << "Client fd=" << fds[i].fd << " did not send HELLO – disconnecting\n";
            close(fds[i].fd);
            fds.erase(fds.begin() + i);
            clients.erase(clients.begin() + (i - 2));
        }
    }
}

void print_bad_message(const ClientInfo& client, const string message) {
    const string& player_id = client.hello ? client.player_id : "UNKNOWN";

    cerr << "ERROR: bad message from [" << client.ip << "]:" << client.port
         << ", " << player_id << ": " << message << "\n";
}


void send_scoring(vector<ClientInfo>& clients) {
    vector<pair<string, double>> scores;

    for (auto& client : clients) {
        double result = client.f.calculate_error() + client.penalty;
        scores.emplace_back(client.player_id, result);
    }

    sort(scores.begin(), scores.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    string message = "SCORING";

    for (const auto& entry : scores) {
        message += " " + entry.first + " " + to_string(entry.second);
    }

    message += "\r\n";

    for (auto& client : clients) {
        if (client.fd >= 0) {
            ssize_t sent = send(client.fd, message.c_str(), message.length(), 0);
            if (sent < 0) {
                syserr("send");
            } else {
                cout << "Sending scoring to [" << client.ip << "]:" << client.port
                     << ", " << client.player_id << ": "
                     << message.substr(0, message.length() - 2) << "\n";
            }
        }
    }
}



void check_message(ClientInfo& client, vector<pollfd>& fds, vector<ClientInfo>& clients, size_t ix, ifstream& coeff_file, ServerArgs& args, uint32_t& count_M) {
    while (client.buffer.find("\r\n") != string::npos) {
        if (count_M >= args.m) {
            break;
        }

        size_t pos = client.buffer.find("\r\n");
        string message = client.buffer.substr(0, pos + 2);
        client.buffer.erase(0, pos + 2);

        
        if (message.compare(0, 6, "HELLO ") == 0)
            read_hello(client, message, fds, clients, ix, coeff_file);
        else if (message.compare(0, 4, "PUT ") == 0)
            read_put(client, message, fds, args, count_M);
        else 
            print_bad_message(client, message);
    }
}

bool type_put(string& message) {
    if (message.compare(0, 3, "PUT") == 0) {
        return true;
    }
    return false;
}

bool validate_put(const string& message, int K) {
    regex pattern(R"(PUT (\d+) (-?\d+(?:\.\d{0,7})?)\r\n)");

    smatch match;
    if (regex_match(message, match, pattern)) {
        int point = stoi(match[1].str());
        if (point < 0 || point > K) return false;

        double value = stod(match[2].str());
        if (value < -5.0 || value > 5.0) return false;

        return true;
    }
    return false;
}

void send_penalty(ClientInfo& client, string message2, vector<pollfd>& fds) {
    message2.erase(0, 3);
    message2 = "PENALTY" + message2;

    client.queue_message("PENALTY", message2, time(nullptr), fds, client.all_put_count);
    client.penalty += PENALTY;
}

void send_bad_put(ClientInfo& client, string message2, vector<pollfd>& fds) {
    message2.erase(0, 3);
    message2 = "BAD_PUT" + message2;

    client.queue_message("BAD_PUT", message2, time(nullptr) + 1, fds, client.all_put_count);
    client.penalty += BAD_PUT;
}

void send_coeff(ClientInfo& client, ifstream& coeff_file, vector<pollfd>& fds) {
    string message;
    getline(coeff_file, message);
    message += "\r\n";

    client.queue_message("COEFF", message, time(nullptr), fds, client.all_put_count);

    vector<double> coeff_vector;
    if (validate_coeff(message, coeff_vector)) {
        client.f.set_coeffs(coeff_vector);
        client.f.calculate_values();
    } else {
        fatal("coeff");
    }
}

void send_state(ClientInfo& client, vector<pollfd>& fds) {
    string message = "STATE";
    for (int point = 0; point < client.f.len(); ++point) {
        double value = client.f.estimated_value(point);
        message += " " + to_string(value);
    }
    message += "\r\n";

    client.queue_message("STATE", message, time(nullptr) + client.lowercase_count, fds, client.all_put_count);
}

void read_hello(ClientInfo& client, string message, vector<pollfd>& fds, vector<ClientInfo>& clients, size_t ix, ifstream& coeff_file) {
    if (client.hello) {
        print_bad_message(client, message);
        return;
    }

    string player_id = message.substr(6, message.size() - 8); 

    bool valid = true;
    for (char c : player_id) {
        if (!isalnum(static_cast<unsigned char>(c))) {
            valid = false;
            break;
        }
    }

    if (valid) {
        client.set_player_id(player_id);
        client.hello = true;

        cout << "Reveived HELLO, [" << client.ip << "]:" << client.port << " is known as " << client.player_id << "\n";
        send_coeff(client, coeff_file, fds);
    }
    else {
        print_bad_message(client, message);
        close(client.fd);
        fds.erase(fds.begin() + ix);
        clients.erase(clients.begin() + (ix - 2));
    }
}

void read_put(ClientInfo& client, string message, vector<pollfd>& fds, ServerArgs& args, uint32_t& count_M) {

    if (client.last_put_response != client.all_put_count) {
        send_penalty(client, message, fds);
    }

    regex pattern(R"(PUT (-?\d+(?:\.\d{0,7})?) (-?\d+(?:\.\d{0,7})?)\r\n)");

    smatch match;
    int point = 0;
    double value = 0;

    if (regex_match(message, match, pattern)) {
        client.all_put_count++;
        double point_double = stod(match[1].str());
        point = static_cast<int>(point_double);

        if (fabs(point_double - point) > 1e-6) {
            send_bad_put(client, message, fds);
            cout << "Received bad put: " << message.substr(0, message.length() - 2)
                 << " from " << client.ip << "]:" << client.port << ", " << client.player_id << "\n";
            return;
        }

        if (point < 0 || point > args.k) {
            send_bad_put(client, message, fds);
            cout << "Received bad put: " << message.substr(0, message.length() - 2)
                 << " from " << client.ip << "]:" << client.port << ", " << client.player_id << "\n";
            return;
        }

        value = stod(match[2].str());
        if (value < -5.0 || value > 5.0) {
            send_bad_put(client, message, fds);
            cout << "Received bad put: " << message.substr(0, message.length() - 2)
                 << " from " << client.ip << "]:" << client.port << ", " << client.player_id << "\n";
            return;
        }

        cout << "Received put: " << message.substr(0, message.length() - 2)
             << " from [" << client.ip << "]:" << client.port << ", " << client.player_id << "\n";
        client.f.add(point, value);
        send_state(client, fds);
        count_M++;
        client.put_count++;
    }
    else {
        print_bad_message(client, message);
        return;
    }
}


void send_put(double point, double value, priority_queue<OutgoingMessage, vector<OutgoingMessage>, MessageCompare>& outgoing_messages) {
    string msg = "PUT " + to_string(point) + " " + to_string(value) + "\r\n";
    outgoing_messages.push({ "PUT", msg, time(nullptr), 0});
}

bool validate_bad_put(const string& message) {
    regex pattern(R"(BAD_PUT (-?\d+(?:\.\d{0,7})?) (-?\d+(?:\.\d{0,7})?)\r\n)");

    smatch match;
    if (regex_match(message, match, pattern)) {
        return true;
    }
    return false;
}

bool validate_coeff(const string& message, vector<double>& coeff_vector) {
    regex pattern(R"(COEFF( -?\d+(?:\.\d{0,7})?)+\r\n)");

    smatch match;
    if (!regex_match(message, match, pattern)) {
        return false;
    }

    coeff_vector.clear();

    size_t pos = 6;
    while (pos < message.size()) {
        while (pos < message.size() && message[pos] == ' ') pos++;
        if (message[pos] == '\r') break;

        size_t end_pos = pos;
        while (end_pos < message.size() && message[end_pos] != ' ' && message[end_pos] != '\r') {
            end_pos++;
        }

        string number_str = message.substr(pos, end_pos - pos);
        double number = stod(number_str);

        if (number < -100.0 || number > 100.0) {
            return false;
        }

        coeff_vector.push_back(number);

        pos = end_pos;
    }

    return true;
}



bool validate_state(const string& message, int& K) {
    regex pattern(R"(STATE( -?\d+(?:\.\d{0,7})?)+\r\n)");

    smatch match;
    if (!regex_match(message, match, pattern)) {
        return false;
    }

    size_t pos = 5;
    int count = 0;

    while (pos < message.size()) {
        while (pos < message.size() && message[pos] == ' ') pos++;

        if (message[pos] == '\r') break;

        size_t end_pos = pos;
        while (end_pos < message.size() && message[end_pos] != ' ' && message[end_pos] != '\r') {
            end_pos++;
        }

        count++;
        pos = end_pos;
    }

    K = count - 1;

    return true;
}


bool validate_penalty(const string& message) {
    regex pattern(R"(PENALTY (-?\d+(?:\.\d{0,7})?) (-?\d+(?:\.\d{0,7})?)\r\n)");

    smatch match;
    if (regex_match(message, match, pattern)) {
        return true;
    }
    return false;
}

bool validate_scoring(const string& message) {
    regex pattern(R"(SCORING( [A-Za-z0-9]+ -?\d+(?:\.\d{0,7})?)+\r\n)");

    smatch match;
    if (regex_match(message, match, pattern)) {
        return true;
    }
    return false;
}

void calculate_next_put(Function& f, int& point, double& value) {

    double max_error = -1.0;
    int best_point = -1;
    double best_value = 0.0;

    int K = f.len() - 1;
    for (int x = 0; x <= K; ++x) {
        double error = fabs(f.estimated_value(x) - f.value(x));

        if (error > max_error) {
            max_error = error;
            best_point = x;
            best_value = f.value(x);
        }
    }

    point = best_point;
    value = max(-5.0, min(5.0, best_value));

    f.estimated_values[point] += value;
}


void check_message2(string& client_buffer, bool strategy_a, Function& f, priority_queue<OutgoingMessage, vector<OutgoingMessage>, MessageCompare>& outgoing_messages, bool& is_state, ClientArgs& args) {
    while (client_buffer.find("\r\n") != string::npos) {
        size_t pos = client_buffer.find("\r\n");
        string msg = client_buffer.substr(0, pos + 2);
        client_buffer.erase(0, pos + 2);
        int point;
        double value;

        if (msg.compare(0, 8, "BAD_PUT ") == 0) {
            if (validate_bad_put(msg)) {
                cout << "Received " << msg.substr(0, msg.length() - 2) << "\n";
                if (strategy_a) {
                    calculate_next_put(f, point, value);
                    send_put(point, value, outgoing_messages);
                }
            }
            else {
                cerr << "ERROR: bad message from [" << args.server << "]:" << args.port << ", " << args.player_id << msg << "\n";
            }
        }
        else if (msg.compare(0, 6, "COEFF ") == 0) {
            vector<double> coeff;
            if (validate_coeff(msg, coeff)) {
                // dodaj zapisanie coeff
                cout << "Received " << msg.substr(0, msg.length() - 2) << "\n";
                f.set_coeffs(coeff);

                if (strategy_a) {
                    send_put(0, 0, outgoing_messages);
                }
            }
            else {
                cerr << "ERROR: bad message from [" << args.server << "]:" << args.port << ", " << args.player_id << msg << "\n";
            }
        }
        else if (msg.compare(0, 6, "STATE ") == 0) {
            int K;
            if (validate_state(msg, K)) {
                cout << "Received " << msg.substr(0, msg.length() - 2) << "\n";
                if (strategy_a) {
                    if (!is_state) {
                        f.init(K);
                        f.calculate_values();
                        is_state = true;
                    }
                    calculate_next_put(f, point, value);
                    send_put(point, value, outgoing_messages);
                }
            }
            else {
                cerr << "ERROR: bad message from [" << args.server << "]:" << args.port << ", " << args.player_id << msg << "\n";
            }
        }
        else if (msg.compare(0, 8, "PENALTY ") == 0) {
            if (validate_penalty(msg)) {
                cout << "Received " << msg.substr(0, msg.length() - 2) << "\n";
            }
            else {
                cerr << "ERROR: bad message from [" << args.server << "]:" << args.port << ", " << args.player_id << msg << "\n";
            }
        }
        else if (msg.compare(0, 8, "SCORING ") == 0) {
            if (validate_scoring(msg)) {
                cout << "Received " << msg.substr(0, msg.length() - 2) << "\n";
            }
            else {
                cerr << "ERROR: bad message from [" << args.server << "]:" << args.port << ", " << args.player_id << msg << "\n";
            }
        }
        else {
            cerr << "ERROR: bad message from [" << args.server << "]:" << args.port << ", " << args.player_id << msg << "\n";
        }
    }
}

void send_hello(string& player_id, priority_queue<OutgoingMessage, vector<OutgoingMessage>, MessageCompare>& outgoing_messages) {
    string msg = "HELLO " + player_id + "\r\n";
    outgoing_messages.push({ "HELLO", msg, time(nullptr), 0 });
}