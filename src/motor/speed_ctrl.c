/* 1 kHz 机械速度环：转速误差按最大转速归一化，输出 Iq_ref(Q15)。 */
#include "motor/speed_ctrl.h"

void motor_speed_ctrl_init(motor_speed_ctrl_t *ctrl, int16_t kp, int16_t ki,
                           int16_t iq_limit_q15)
{
    motor_pi_init(&ctrl->pi, kp, ki, iq_limit_q15);
}

int16_t motor_speed_ctrl_step(motor_speed_ctrl_t *ctrl, int32_t target_rpm,
                              int32_t measured_rpm, uint32_t max_rpm)
{
    int32_t error = target_rpm - measured_rpm;
    if (max_rpm == 0U) return 0;
    if (error > (int32_t)max_rpm) error = (int32_t)max_rpm;
    if (error < -(int32_t)max_rpm) error = -(int32_t)max_rpm;
    return motor_pi_step(&ctrl->pi, (int16_t)(error * 32767 / (int32_t)max_rpm));
}
