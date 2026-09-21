/* Minimal pthread ABI declarations for libc++ headers. Cloudflare Worker
 * modules are single-threaded; these declarations are only a portability
 * bridge until the engine's mutex surface is mapped to the host runtime. */
#ifndef SERVO_WORKER_PTHREAD_H
#define SERVO_WORKER_PTHREAD_H

#include <sys/types.h>
#include <time.h>

#define PTHREAD_MUTEX_INITIALIZER {0}
#define PTHREAD_COND_INITIALIZER {0}
#define PTHREAD_ONCE_INIT 0
#define PTHREAD_MUTEX_RECURSIVE 1

int pthread_mutexattr_init(pthread_mutexattr_t*);
int pthread_mutexattr_settype(pthread_mutexattr_t*, int);
int pthread_mutexattr_destroy(pthread_mutexattr_t*);
int pthread_mutex_init(pthread_mutex_t*, const pthread_mutexattr_t*);
int pthread_mutex_destroy(pthread_mutex_t*);
int pthread_mutex_lock(pthread_mutex_t*);
int pthread_mutex_trylock(pthread_mutex_t*);
int pthread_mutex_unlock(pthread_mutex_t*);
int pthread_cond_signal(pthread_cond_t*);
int pthread_cond_broadcast(pthread_cond_t*);
int pthread_cond_wait(pthread_cond_t*, pthread_mutex_t*);
int pthread_cond_timedwait(pthread_cond_t*, pthread_mutex_t*, const struct timespec*);
int pthread_cond_destroy(pthread_cond_t*);
int pthread_rwlock_init(pthread_rwlock_t*, const void*);
int pthread_rwlock_destroy(pthread_rwlock_t*);
int pthread_rwlock_rdlock(pthread_rwlock_t*);
int pthread_rwlock_wrlock(pthread_rwlock_t*);
int pthread_rwlock_unlock(pthread_rwlock_t*);
int pthread_once(pthread_once_t*, void (*)(void));
int pthread_key_create(pthread_key_t*, void (*)(void*));
int pthread_setspecific(pthread_key_t, const void*);
void* pthread_getspecific(pthread_key_t);
pthread_t pthread_self(void);
int pthread_create(pthread_t*, const void*, void* (*)(void*), void*);
int pthread_join(pthread_t, void**);
int pthread_detach(pthread_t);

#endif
