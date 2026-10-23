#include <stdint.h>
#include "motor/foc_math.h"
#include "motor/pi.h"
#include "motor/protection.h"
#include "motor/speed_ctrl.h"
#include "motor/observer.h"

#define CHECK(x) do { if (!(x)) finish(__LINE__); } while (0)

static void finish(uint32_t result)
{
    *(volatile uint32_t *)0x20000000UL = result;
    __asm volatile("bkpt #0");
    for (;;) {}
}

static int32_t abs32(int32_t value)
{
    return value < 0 ? -value : value;
}

void test_entry(void)
{
    int16_t s, c;
    motor_ab_t ab;
    motor_dq_t dq;
    motor_duty_t duty;
    motor_pi_t pi;
    motor_protection_t protection;
    motor_speed_ctrl_t speed;
    motor_observer_t observer;
    uint16_t real_angle = 0U;
    uint32_t sample_index;

    motor_sincos(0U, &s, &c);
    CHECK(abs32(s) < 2 && abs32((int32_t)c - 32767) < 2);
    motor_sincos(16384U, &s, &c);
    CHECK(abs32((int32_t)s - 32767) < 2 && abs32(c) < 2);
    motor_sincos(32768U, &s, &c);
    CHECK(abs32(s) < 2 && abs32((int32_t)c + 32767) < 2);

    ab = motor_clarke(10000, -5000);
    CHECK(abs32((int32_t)ab.alpha - 10000) < 2);
    CHECK(abs32(ab.beta) < 3);
    dq = motor_park(ab, 0U);
    CHECK(abs32((int32_t)dq.d - 10000) < 3 && abs32(dq.q) < 3);
    dq = motor_park(ab, 16384U);
    CHECK(abs32(dq.d) < 3 && abs32((int32_t)dq.q + 10000) < 3);

    duty = motor_svpwm((motor_ab_t){0, 0});
    CHECK(duty.u == 500U && duty.v == 500U && duty.w == 500U);
    duty = motor_svpwm((motor_ab_t){5000, 0});
    CHECK(duty.u > duty.v && duty.v == duty.w);
    CHECK(duty.u <= 750U && duty.v >= 250U);

    motor_pi_init(&pi, 20000, 4000, 8000);
    CHECK(motor_pi_step(&pi, 20000) == 8000);
    CHECK(pi.integral == 0);
    CHECK(motor_pi_step(&pi, -20000) == -8000);

    motor_speed_ctrl_init(&speed, 12000, 1000, 4000);
    CHECK(motor_speed_ctrl_step(&speed, 1000, 0, 5000) > 0);
    CHECK(motor_speed_ctrl_step(&speed, 1000, 2000, 5000) < 0);
    CHECK(motor_speed_ctrl_step(&speed, 5000, 0, 5000) == 4000);
    CHECK(speed.pi.integral <= 4000);

    motor_protection_init(&protection);
    CHECK(motor_protection_sample(&protection, 2048, 2048, 2000,
                                  0, 0, 3300, 2000, 5000, 10000) ==
          MOTOR_FAULT_NONE);
    CHECK(motor_protection_sample(&protection, 2048, 2048, 2000,
                                  15000, 0, 3300, 2000, 5000, 10000) ==
          MOTOR_FAULT_OVERCURRENT);
    CHECK(motor_protection_sample(&protection, 2048, 2048, 2000,
                                  0, 0, 3300, 2000, 5000, 10000) ==
          MOTOR_FAULT_OVERCURRENT);
    motor_protection_init(&protection);
    CHECK(motor_protection_sample(&protection, 2048, 2048, 2000,
                                  0, 0, 1000, 2000, 5000, 10000) ==
          MOTOR_FAULT_UNDERVOLTAGE);
    motor_protection_init(&protection);
    CHECK(motor_protection_sample(&protection, 2048, 2048, 2000,
                                  0, 0, 6000, 2000, 5000, 10000) ==
          MOTOR_FAULT_OVERVOLTAGE);
    motor_protection_init(&protection);
    CHECK(motor_protection_speed(&protection, 1000, 0, 200, 3) ==
          MOTOR_FAULT_NONE);
    CHECK(motor_protection_speed(&protection, 1000, 0, 200, 3) ==
          MOTOR_FAULT_NONE);
    CHECK(motor_protection_speed(&protection, 1000, 0, 200, 3) ==
          MOTOR_FAULT_STALL);

    /* 合成正转反电动势，验证角度锁定与 PLL 速度换算。 */
    motor_observer_init(&observer, 0, 0, 500, 500);
    observer.step = 100;
    for (sample_index = 0U; sample_index < 1300U; sample_index++) {
        motor_ab_t emf;
        motor_sincos(real_angle, &s, &c);
        emf.alpha = (int16_t)(-(int32_t)s * 4000 / 32767);
        emf.beta = (int16_t)((int32_t)c * 4000 / 32767);
        motor_observer_step(&observer, (motor_ab_t){0, 0}, emf);
        real_angle = (uint16_t)(real_angle + 100U);
    }
    CHECK(observer.locked != 0U);
    CHECK(abs32(observer.speed_step - 100) < 30);
    CHECK(motor_observer_speed_rpm(&observer, 4U) > 300);

    finish(0U);
}
