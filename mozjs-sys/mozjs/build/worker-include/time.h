#ifndef SERVO_WORKER_TIME_H
#define SERVO_WORKER_TIME_H

#include_next <time.h>

#ifdef __cplusplus
extern "C" {
#endif
void tzset(void);
extern char* tzname[2];
#ifdef __cplusplus
}
#endif

#endif
