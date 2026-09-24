#ifndef MOTOR_FOC_H
#define MOTOR_FOC_H

#include "motor/foc_math.h"
#include "motor/observer.h"
#include "motor/pi.h"
#include <stdint.h>

typedef struct {
    motor_pi_t d_pi, q_pi;
    motor_observer_t observer;
    motor_ab_t applied_voltage;
    motor_dq_t measured_current;
} motor_foc_t;

void motor_foc_init(motor_foc_t *foc, int16_t current_kp, int16_t current_ki,
                    int16_t observer_r, int16_t observer_l, int16_t min_emf,
                    int32_t max_angle_step);
/* 传入本周期的两相电流、d/q 电流目标；开环启动用 open_angle，闭环用观测角。 */
motor_duty_t motor_foc_step(motor_foc_t *foc, int16_t ia, int16_t ib,
                            int16_t id_ref, int16_t iq_ref, uint16_t open_angle,
                            uint8_t closed_loop);

#endif
