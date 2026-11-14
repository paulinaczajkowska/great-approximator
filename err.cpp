#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <iostream>

#include "err.h"

[[noreturn]] void syserr(const char* fmt, ...) {
    va_list fmt_args;
    int org_errno = errno;

    std::cerr << "ERROR: ";

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    std::cerr << " (" << org_errno << "; " << strerror(org_errno) << ")" << std::endl;
    std::exit(1);
}

[[noreturn]] void fatal(const char* fmt, ...) {
    va_list fmt_args;

    std::cerr << "ERROR: ";

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    std::cerr << std::endl;
    std::exit(1);
}

void error(const char* fmt, ...) {
    va_list fmt_args;
    int org_errno = errno;

    std::cerr << "ERROR: ";

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    if (org_errno != 0) {
        std::cerr << " (" << org_errno << "; " << strerror(org_errno) << ")";
    }
    std::cerr << std::endl;
}

void error_msg(const uint8_t* msg_bytes, size_t msg_len) {
    std::cerr << "ERROR MSG ";
    size_t len = msg_len < 10 ? msg_len : 10;
    for (size_t i = 0; i < len; ++i) {
        std::cerr << std::hex << std::uppercase
                  << ((msg_bytes[i] >> 4) & 0xF)
                  << (msg_bytes[i] & 0xF);
    }
    std::cerr << std::dec << std::endl;
}
