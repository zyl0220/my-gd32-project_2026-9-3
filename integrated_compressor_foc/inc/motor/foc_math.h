#ifndef MOTOR_FOC_MATH_H
#define MOTOR_FOC_MATH_H

#include <stdint.h>

/* Currents and voltages are signed Q15; angle 0..65535 maps to 0..2pi. */
typedef struct { int16_t alpha, beta; } motor_ab_t;
typedef struct { int16_t d, q; } motor_dq_t;
typedef struct { uint16_t u, v, w; } motor_duty_t;

void motor_sincos(uint16_t angle, int16_t *sin_q15, int16_t *cos_q15);
motor_ab_t motor_clarke(int16_t ia, int16_t ib);
motor_dq_t motor_park(motor_ab_t ab, uint16_t angle);
motor_ab_t motor_inv_park(motor_dq_t dq, uint16_t angle);
motor_duty_t motor_svpwm(motor_ab_t voltage);

/* dq请求可超过int16范围；最终矢量模长不大于limit_q15。 */
motor_dq_t motor_voltage_limit(int32_t d, int32_t q, int16_t limit_q15);

#endif
