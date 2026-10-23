/* Hardware boundary fakes; state/control/PI/FOC/weakening are real production code. */
#include "field_weakening_parameters.h"
#include "../../src/motor/motor_app.c"

#define CHECK(x) do { if (!(x)) finish(__LINE__); } while (0)
static unsigned pwm_writes, pwm_stops;
static uint8_t pwm_running;
uint8_t motor_pwm_drv_init(void) { return 1U; }
uint8_t motor_pwm_drv_start(void) { pwm_running = 1U; return 1U; }
void motor_pwm_drv_stop(void) { pwm_running = 0U; ++pwm_stops; }
uint8_t motor_pwm_drv_set(motor_duty_t duty)
{
    (void)duty;
    if (!pwm_running) return 0U;
    ++pwm_writes;
    return 1U;
}
uint8_t motor_adc_drv_init(uint16_t *a, uint16_t *b) { *a = *b = 2048U; return 1U; }
uint8_t motor_adc_drv_read_bus_once(uint16_t *bus) { *bus = 3071U; return 1U; }
void motor_adc_drv_arm_trigger(void) {}
static void finish(uint32_t result)
{
    *(volatile uint32_t *)0x20000000UL = result;
    __asm volatile("bkpt #0");
    for (;;) {}
}
static void build_weakening(void)
{
    g_state = MOTOR_STATE_RUN;
    g_foc.observer.locked = 1U;
    g_foc.observer.step = 100;
    g_foc.observer.speed_step = 100;
    g_foc.voltage_request_d_q15 = 0;
    g_foc.voltage_request_q_q15 = 16000;
    motor_speed_tick();
    CHECK(g_fw.id_ref_q15 < 0);
}
void test_entry(void)
{
    motor_adc_raw_t sample = {2048U, 2048U, 3071U};
    unsigned before;
    CHECK(motor_app_init() == 1U);
    CHECK(motor_app_request_start(1000U) == 1U);
    motor_app_process_1ms();
    CHECK(motor_app_state() == MOTOR_STATE_ALIGN && pwm_running);
    build_weakening();
    CHECK(g_speed.pi.limit == g_fw.iq_limit_q15);
    /* An immediately lost observer must clear the held negative Id in this ISR. */
    g_foc.observer.locked = 1U;
    g_iq_ref = 32000;
    g_foc.observer.emf = (motor_ab_t){0, 0};
    g_foc.applied_voltage = (motor_ab_t){0, 0};
    motor_app_current_isr(sample);
    CHECK(g_fw.id_ref_q15 == 0);
    CHECK(g_foc.voltage_request_d_q15 >= 0);
    CHECK(g_iq_ref <= g_fw.iq_limit_q15);
    g_iq_ref = -32000;
    motor_app_current_isr(sample);
    CHECK(g_iq_ref >= -g_fw.iq_limit_q15);
    build_weakening();
    motor_app_request_stop();
    motor_app_watchdog_1ms_isr();
    CHECK(motor_app_state() == MOTOR_STATE_STOPPED && !pwm_running);
    CHECK(g_fw.id_ref_q15 == 0);
    CHECK(motor_app_request_start(1000U) == 1U);
    motor_app_process_1ms();
    CHECK(g_fw.id_ref_q15 == 0 && g_foc.voltage_limit_q15 == 8986);
    build_weakening();
    before = pwm_writes;
    sample.bus = 1000U;
    motor_app_current_isr(sample);
    CHECK(motor_app_state() == MOTOR_STATE_FAULT && !pwm_running);
    CHECK(g_fw.id_ref_q15 == 0 && pwm_writes == before && pwm_stops > 0U);
    CHECK(motor_app_clear_fault() == 1U);
    CHECK(motor_app_request_start(1000U) == 1U);
    motor_app_process_1ms();
    build_weakening();
    /* An invalid FW result at the slow tick must stop before the PWM register write. */
    g_fw.controller.initialized = false;
    g_divider = MOTOR_SPEED_DIVIDER - 1U;
    g_foc.observer.locked = 1U;
    g_foc.observer.lock_count = 1000U;
    g_foc.observer.emf = (motor_ab_t){0, 1000};
    g_foc.applied_voltage = (motor_ab_t){0, 1000};
    sample.bus = 3071U;
    before = pwm_writes;
    motor_app_current_isr(sample);
    CHECK(motor_app_state() == MOTOR_STATE_FAULT && !pwm_running);
    CHECK(g_fw.id_ref_q15 == 0 && pwm_writes == before);
    finish(0U);
}
