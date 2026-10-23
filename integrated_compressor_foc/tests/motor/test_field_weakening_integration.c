#include <stdint.h>
#include <limits.h>
#include "motor/field_weakening_adapter.h"
#include "motor/foc.h"

#define CHECK(x) do { if (!(x)) finish(__LINE__); } while (0)
static void finish(uint32_t result)
{
    *(volatile uint32_t *)0x20000000UL = result;
    __asm volatile("bkpt #0");
    for (;;) {}
}
static int64_t norm2(int32_t d, int32_t q)
{
    return (int64_t)d * d + (int64_t)q * q;
}
void test_entry(void)
{
    motor_fw_adapter_t fw;
    motor_foc_t foc;
    motor_pi_t pi;
    motor_dq_t v;
    motor_duty_t duty;
    FW_Config cfg = {0.05f, 0.5f, 5.0f, -2.5f, 0.95f, 20.0f};
    CHECK(motor_fw_adapter_init(&fw, cfg, 10000U, 250U, 750U));
    CHECK(fw.voltage_limit_q15 == 8986);
    CHECK(fw.iq_limit_q15 == 16383);
    CHECK(motor_fw_adapter_update(&fw, 0, 8000, 300000U, 0.001f, true));
    CHECK(fw.id_ref_q15 == 0);
    /* 10000 Q15 is below Vdc/sqrt(3), but above our 25..75% PWM envelope. */
    CHECK(motor_fw_adapter_update(&fw, 0, 10000, 300000U, 0.001f, true));
    CHECK(fw.id_ref_q15 == -65);
    for (unsigned i = 0; i < 200; ++i)
        CHECK(motor_fw_adapter_update(&fw, 0, 16000, 300000U, 0.001f, true));
    CHECK(fw.id_ref_q15 == -8191);
    CHECK(fw.iq_limit_q15 > 14000 && fw.iq_limit_q15 < 14200);
    CHECK(norm2(fw.id_ref_q15, fw.iq_limit_q15) <= (int64_t)16383 * 16383);
    CHECK(motor_fw_adapter_limit_iq(&fw, 20000) == fw.iq_limit_q15);
    CHECK(motor_fw_adapter_limit_iq(&fw, -20000) == -fw.iq_limit_q15);
    for (unsigned i = 0; i < 1000; ++i)
        CHECK(motor_fw_adapter_update(&fw, 0, 5000, 300000U, 0.001f, true));
    CHECK(fw.id_ref_q15 == 0);
    CHECK(motor_fw_adapter_update(&fw, 0, 16000, 300000U, 0.001f, true));
    CHECK(fw.id_ref_q15 < 0);
    CHECK(motor_fw_adapter_update(&fw, 0, 16000, 300000U, 0.001f, false));
    CHECK(fw.id_ref_q15 == 0 && fw.iq_limit_q15 == 16383);
    CHECK(!motor_fw_adapter_update(&fw, 0, 16000, 0U, 0.001f, true));
    CHECK(fw.id_ref_q15 == 0 && fw.iq_limit_q15 == 0);
    motor_fw_adapter_reset(&fw);
    CHECK(fw.id_ref_q15 == 0 && fw.iq_limit_q15 == 16383);
    CHECK(!motor_fw_adapter_update(&fw, 0, 0, 300000U, 0.0f, true));
    CHECK(!motor_fw_adapter_init(&fw, cfg, 0U, 250U, 750U));
    CHECK(!motor_fw_adapter_init(&fw, cfg, 10000U, 500U, 750U));
    CHECK(motor_fw_adapter_init(&fw, cfg, 10000U, 250U, 600U));
    CHECK(fw.voltage_limit_q15 >= 3593 && fw.voltage_limit_q15 <= 3595);
    cfg.current_limit_a_peak = 11.0f;
    CHECK(!motor_fw_adapter_init(&fw, cfg, 10000U, 250U, 750U));

    /* Float-to-Q15 may round one LSB above the integer protection radius. */
    cfg.current_limit_a_peak = (float)269U * 0.001f;
    cfg.id_min_a_peak = -cfg.current_limit_a_peak;
    cfg.id_slew_a_per_s = 1000.0f;
    CHECK(motor_fw_adapter_init(&fw, cfg, 318U, 250U, 750U));
    fw.current_limit_q15 = 27717; /* floor(269 * 32767 / 318), as in motor_app */
    CHECK(motor_fw_adapter_update(&fw, 0, 16000, 300000U, 0.001f, true));
    CHECK(fw.id_ref_q15 >= -27717);
    CHECK(norm2(fw.id_ref_q15, fw.iq_limit_q15) <= (int64_t)27717 * 27717);

    v = motor_voltage_limit(30000, 40000, 10000);
    CHECK(v.d == 6000 && v.q == 8000);
    v = motor_voltage_limit(-30000, -40000, 10000);
    CHECK(v.d == -6000 && v.q == -8000);
    v = motor_voltage_limit(INT32_MAX, INT32_MIN, 10000);
    CHECK(v.d > 7000 && v.q < -7000 && norm2(v.d, v.q) <= 100000000);
    for (uint32_t a = 0U; a < 65536U; a += 211U) {
        v = motor_voltage_limit(20000, 25000, 8986);
        duty = motor_svpwm(motor_inv_park(v, (uint16_t)a));
        CHECK(duty.u > 250U && duty.u < 750U);
        CHECK(duty.v > 250U && duty.v < 750U);
        CHECK(duty.w > 250U && duty.w < 750U);
    }
    motor_pi_init(&pi, 20000, 4000, 8000);
    CHECK(motor_pi_request(&pi, 20000) > 12000);
    motor_pi_track(&pi, 20000, 8000);
    CHECK(pi.integral == 0);
    pi.integral = 6000;
    motor_pi_track(&pi, -1000, 3000);
    CHECK(pi.integral == 5877);
    motor_foc_init(&foc, 32767, 1000, 0, 0, 500, 100);
    foc.voltage_limit_q15 = 8986;
    (void)motor_foc_step(&foc, 0, 0, 20000, 20000, 0U, 0U);
    CHECK(foc.voltage_request_d_q15 > 12000 && foc.voltage_request_q_q15 > 12000);
    CHECK(norm2(foc.applied_voltage.alpha, foc.applied_voltage.beta) <= (int64_t)8986 * 8986);
    CHECK(foc.d_pi.integral == 0 && foc.q_pi.integral == 0);
    finish(0U);
}
