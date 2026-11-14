#ifndef MIM_ERR_H
#define MIM_ERR_H

#include <stdint.h>  // for uint8_t
#include <stddef.h>  // for size_t

#ifdef __cplusplus
extern "C" {
#endif

[[noreturn]] void syserr(const char* fmt, ...);
[[noreturn]] void fatal(const char* fmt, ...);

void error(const char* fmt, ...);
void error_msg(const uint8_t* msg_bytes, size_t msg_len);

#ifdef __cplusplus
}
#endif

#endif // MIM_ERR_H
