/* Q15 PI：条件积分 anti-windup，输出饱和且误差继续推向饱和时冻结积分。 */
#include "motor/pi.h"

void motor_pi_init(motor_pi_t *pi, int16_t kp, int16_t ki, int16_t limit)
{
    pi->kp = kp;
    pi->ki = ki;
    pi->limit = limit;
    pi->integral = 0;
}

int16_t motor_pi_step(motor_pi_t *pi, int16_t error)
{
    int32_t proportional = ((int32_t)pi->kp * error) >> 15;
    int32_t increment = ((int32_t)pi->ki * error) >> 15;
    int32_t integral = pi->integral + increment;
    int32_t output = proportional + integral;
    /* 反向误差仍允许积分，因而能从饱和状态迅速退出。 */
    if ((output > pi->limit && increment > 0) ||
        (output < -pi->limit && increment < 0)) {
        integral = pi->integral;
        output = proportional + integral;
    }
    if (integral > pi->limit) integral = pi->limit;
    if (integral < -pi->limit) integral = -pi->limit;
    pi->integral = integral;
    if (output > pi->limit) output = pi->limit;
    if (output < -pi->limit) output = -pi->limit;
    return (int16_t)output;
}
