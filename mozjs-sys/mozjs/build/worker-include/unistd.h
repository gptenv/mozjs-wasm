/* Minimal freestanding declarations for the Worker SpiderMonkey build. */
#ifndef SERVO_WORKER_UNISTD_H
#define SERVO_WORKER_UNISTD_H

#include <stddef.h>
#include <stdint.h>

typedef int pid_t;

// The C++ sources call the standard functions through std::. Avoid the C
// compatibility macros from the freestanding math headers rewriting them.
#ifdef isnan
#undef isnan
#endif
#ifdef isfinite
#undef isfinite
#endif

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define _SC_CLK_TCK 2
#define _SC_PAGESIZE 30

#ifdef __cplusplus
extern "C" {
#endif

int close(int);
int getpid(void);
long sysconf(int);
long read(int, void*, size_t);
long write(int, const void*, size_t);

#ifdef __cplusplus
}
#endif

#endif
