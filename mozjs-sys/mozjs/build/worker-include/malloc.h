#ifndef SERVO_WORKER_MALLOC_H
#define SERVO_WORKER_MALLOC_H
#include_next <malloc.h>
#ifdef __cplusplus
extern "C" {
#endif
void* memalign(size_t alignment, size_t size);
#ifdef __cplusplus
}
#endif
#endif
