#ifndef MOTOR_SPEED_CTRL_H
#define MOTOR_SPEED_CTRL_H

#include "motor/pi.h"
#include <stdint.h>

typedef struct { motor_pi_t pi; } motor_speed_ctrl_t;
/* iq_limit_q15 同时限制 PI 积分与最终电流目标。 */
void motor_speed_ctrl_init(motor_speed_ctrl_t *ctrl, int16_t kp, int16_t ki,
                           int16_t iq_limit_q15);
int16_t motor_speed_ctrl_step(motor_speed_ctrl_t *ctrl, int32_t target_rpm,
                              int32_t measured_rpm, uint32_t max_rpm);

#endif
