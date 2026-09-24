#ifndef MOTOR_PI_H
#define MOTOR_PI_H

#include <stdint.h>

typedef struct {
    int16_t kp;
    int16_t ki;
    int16_t limit;
    int32_t integral;
} motor_pi_t;

void motor_pi_init(motor_pi_t *pi, int16_t kp, int16_t ki, int16_t limit);
int16_t motor_pi_step(motor_pi_t *pi, int16_t error);

#endif
