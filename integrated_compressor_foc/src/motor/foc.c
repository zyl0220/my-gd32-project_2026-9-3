/* 20 kHz 电流环：观测角度 → Clarke/Park → d/q PI → 反 Park → SVPWM。 */
#include "motor/foc.h"

static int16_t clip_error(int32_t error)
{
    if (error > 32767) return 32767;
    if (error < -32767) return -32767;
    return (int16_t)error;
}

void motor_foc_init(motor_foc_t *foc, int16_t current_kp, int16_t current_ki,
                    int16_t observer_r, int16_t observer_l, int16_t min_emf,
                    int32_t max_angle_step)
{
    motor_pi_init(&foc->d_pi, current_kp, current_ki, 12000);
    motor_pi_init(&foc->q_pi, current_kp, current_ki, 12000);
    motor_observer_init(&foc->observer, observer_r, observer_l,
                        min_emf, max_angle_step);
    foc->voltage_request_d_q15 = 0;
    foc->voltage_request_q_q15 = 0;
    foc->voltage_limit_q15 = 0;
    foc->applied_voltage = (motor_ab_t){0, 0};
    foc->measured_current = (motor_dq_t){0, 0};
}

motor_duty_t motor_foc_step(motor_foc_t *foc, int16_t ia, int16_t ib,
                            int16_t id_ref, int16_t iq_ref, uint16_t open_angle,
                            uint8_t closed_loop)
{
    motor_ab_t current = motor_clarke(ia, ib);
    motor_dq_t voltage;
    uint16_t angle;

    /* 当前 ADC 样本对应上一 PWM 周期的电压，观测器先用上一周期电压更新。 */
    motor_observer_step(&foc->observer, current, foc->applied_voltage);
    if (foc->voltage_limit_q15 > 0 && closed_loop &&
        !foc->observer.locked && id_ref < 0) id_ref = 0;
    angle = closed_loop ? foc->observer.angle : open_angle;
    foc->measured_current = motor_park(current, angle);
    if (foc->voltage_limit_q15 > 0) {
        int16_t d_error = clip_error((int32_t)id_ref - foc->measured_current.d);
        int16_t q_error = clip_error((int32_t)iq_ref - foc->measured_current.q);
        foc->voltage_request_d_q15 = motor_pi_request(&foc->d_pi, d_error);
        foc->voltage_request_q_q15 = motor_pi_request(&foc->q_pi, q_error);
        voltage = motor_voltage_limit(foc->voltage_request_d_q15,
                                       foc->voltage_request_q_q15,
                                       foc->voltage_limit_q15);
        motor_pi_track(&foc->d_pi, d_error, voltage.d);
        motor_pi_track(&foc->q_pi, q_error, voltage.q);
    } else {
        voltage.d = motor_pi_step(&foc->d_pi,
                                  clip_error((int32_t)id_ref - foc->measured_current.d));
        voltage.q = motor_pi_step(&foc->q_pi,
                                  clip_error((int32_t)iq_ref - foc->measured_current.q));
        foc->voltage_request_d_q15 = voltage.d;
        foc->voltage_request_q_q15 = voltage.q;
    }
    foc->applied_voltage = motor_inv_park(voltage, angle);
    return motor_svpwm(foc->applied_voltage);
}
