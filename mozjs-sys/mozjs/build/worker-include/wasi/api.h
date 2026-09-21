/* The wasi-libc headers are useful as a freestanding C header set, but their
 * public API header rejects wasm32-unknown-unknown before defining constants
 * needed by otherwise portable headers such as __seek.h. Scope the marker to
 * this include; Worker C++ sources must not be compiled as WASI. */
#ifndef SERVO_WORKER_WASI_API_H
#define SERVO_WORKER_WASI_API_H
#define __wasi__ 1
#include_next <wasi/api.h>
#undef __wasi__
#endif
