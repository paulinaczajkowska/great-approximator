#ifndef MIM_COMMON_H
#define MIM_COMMON_H

uint16_t read_port(char const *string);
unsigned long read_number(char const *string);
size_t read_size(char const *string);

struct sockaddr_in get_server_address(char const *host, uint16_t port);
struct sockaddr_storage get_server_address2(const char *host, uint16_t port, bool force_ipv4, bool force_ipv6);

#endif
