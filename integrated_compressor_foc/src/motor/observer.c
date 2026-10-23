/* 无感位置/速度观测器：由上一周期施加的电压、两相电流及电流变化量
 * 估计反电动势，再以相位误差驱动 PLL。全部使用定点数以适应 48 MHz M23。 */
#include "motor/observer.h"
#include "motor/motor_param.h"

static int32_t absolute(int32_t x) { return x < 0 ? -x : x; }
static int16_t clip(int32_t x)
{
    if (x > 32767) return 32767;
    if (x < -32767) return -32767;
    return (int16_t)x;
}

void motor_observer_init(motor_observer_t *obs, int16_t r_q15, int16_t l_q15,
                         int16_t min_emf_q15, int32_t max_step)
{
    obs->angle = 0U;
    obs->step = 0;
    obs->speed_step = 0;
    obs->max_step = max_step;
    obs->r_q15 = r_q15;
    obs->l_q15 = l_q15;
    obs->min_emf_q15 = min_emf_q15;
    obs->prev_current = (motor_ab_t){0, 0};
    obs->emf = (motor_ab_t){0, 0};
    obs->lock_count = 0U;
    obs->locked = 0U;
}

void motor_observer_step(motor_observer_t *obs, motor_ab_t current,
                         motor_ab_t applied_voltage)
{
    int32_t ea = (int32_t)applied_voltage.alpha -
        (((int32_t)obs->r_q15 * current.alpha) >> 15) -
        (((int32_t)obs->l_q15 * (current.alpha - obs->prev_current.alpha)) >> 15);
    int32_t eb = (int32_t)applied_voltage.beta -
        (((int32_t)obs->r_q15 * current.beta) >> 15) -
        (((int32_t)obs->l_q15 * (current.beta - obs->prev_current.beta)) >> 15);
    int32_t magnitude, error;
    int16_t s, c;
    /* e = v - R*i - L*di/dt；系数已在 motor_param.h 中归一化。 */
    obs->prev_current = current;
    obs->emf.alpha = clip((int32_t)obs->emf.alpha + (ea - obs->emf.alpha) / 8);
    obs->emf.beta = clip((int32_t)obs->emf.beta + (eb - obs->emf.beta) / 8);
    magnitude = absolute(obs->emf.alpha) + absolute(obs->emf.beta);
    if (magnitude < obs->min_emf_q15 || magnitude == 0) {
        obs->locked = 0U;
        obs->lock_count = 0U;
        obs->angle = (uint16_t)(obs->angle + obs->step);
        obs->speed_step += (obs->step - obs->speed_step) / 16;
        return;
    }
    motor_sincos(obs->angle, &s, &c);
    /* e = ω[-sinθ, cosθ]，径向分量的符号对应 θ估计 - θ实际。 */
    error = (((int32_t)obs->emf.alpha * c + (int32_t)obs->emf.beta * s) >> 15);
    error = error * 32767 / magnitude;
    obs->step -= error / 256;
    if (obs->step > obs->max_step) obs->step = obs->max_step;
    if (obs->step < -obs->max_step) obs->step = -obs->max_step;
    /* 速度估算来自 PLL 的电角步长，低通后供 1 kHz 速度环使用。 */
    obs->speed_step += (obs->step - obs->speed_step) / 16;
    obs->angle = (uint16_t)(obs->angle + obs->step - error / 16);
    if (absolute(error) < 4000 && obs->step > 0) {
        if (obs->lock_count < 1000U) obs->lock_count++;
        if (obs->lock_count == 1000U) obs->locked = 1U;
    } else {
        obs->lock_count = 0U;
        obs->locked = 0U;
    }
}

int32_t motor_observer_speed_rpm(const motor_observer_t *obs, uint32_t pole_pairs)
{
    if (pole_pairs == 0U || !obs->locked) return 0;
    /* 每次电流环的电角增量 -> 电角频率 -> 机械转速。 */
    return (int32_t)((int64_t)obs->speed_step * MOTOR_PWM_HZ * 60 /
                     (65536 * (int64_t)pole_pairs));
}
