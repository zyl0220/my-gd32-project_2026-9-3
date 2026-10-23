/* 压缩机启动/停机/上油状态机。状态与 PI 仅由 ADC 电流中断更新；
 * 主循环提交转速命令，SysTick 负责失采样的独立看门检查。 */
#include "motor/motor_app.h"
#include "motor/current_sample.h"
#include "motor/foc.h"
#include "motor/motor_param.h"
#include "motor/pwm_drv.h"
#include "motor/speed_ctrl.h"
#if MOTOR_ENABLE_FIELD_WEAKENING
#include "motor/field_weakening_adapter.h"
static motor_fw_adapter_t g_fw;
#endif

static motor_foc_t g_foc;
static motor_speed_ctrl_t g_speed;
static motor_current_sample_t g_sample;
static motor_protection_t g_protection;
static volatile motor_state_t g_state;
static volatile motor_fault_t g_fault;
static volatile uint32_t g_requested_rpm;
static uint8_t g_ready;
static uint8_t g_divider;
static uint32_t g_state_ms, g_oil_ms;
static uint16_t g_open_angle;
static uint16_t g_open_step;
static int16_t g_iq_ref, g_current_limit_q15;
static uint16_t g_lost_lock_ms;
static volatile uint32_t g_adc_ticks;
static uint32_t g_watchdog_seen_ticks;
static uint8_t g_missing_adc_ms;

static void motor_weakening_reset(void)
{
#if MOTOR_ENABLE_FIELD_WEAKENING
    motor_fw_adapter_reset(&g_fw);
#endif
}

static void motor_trip(motor_fault_t fault)
{
    /* 故障先关闭 TIMER0 主输出并锁存，必须显式清故障才可再次启动。 */
    motor_protection_trip(&g_protection, fault);
    g_fault = g_protection.fault;
    g_requested_rpm = 0U;
    motor_pwm_drv_stop();
    motor_weakening_reset();
    g_state = MOTOR_STATE_FAULT;
}

uint8_t motor_app_init(void)
{
    uint16_t ia_offset, ib_offset;
    uint32_t current_scale_ma = MOTOR_CURRENT_FULL_SCALE_MA;
    g_ready = 0U;
    g_state = MOTOR_STATE_FAULT;
    g_fault = MOTOR_FAULT_CONFIG;
    g_requested_rpm = 0U;
    g_adc_ticks = 0U;
    g_watchdog_seen_ticks = 0U;
    g_missing_adc_ms = 0U;
    /* 没有实际原理图与电机参数时，初始化停在这里，门极不会被接管。 */
    if (MOTOR_BOARD_READY == 0U || current_scale_ma == 0U) return 0U;

#if MOTOR_ENABLE_FIELD_WEAKENING
    {
        FW_Config cfg = {
            MOTOR_FW_KP_A_PER_V, MOTOR_FW_KI_A_PER_V_S,
            (float)MOTOR_MAX_CURRENT_MA * 0.001f,
            MOTOR_FW_ID_MIN_A_PEAK, MOTOR_FW_VOLTAGE_MARGIN,
            MOTOR_FW_ID_SLEW_A_PER_S
        };
        if (!motor_fw_adapter_init(&g_fw, cfg, current_scale_ma,
                                    MOTOR_DUTY_MIN_PERMILLE,
                                    MOTOR_DUTY_MAX_PERMILLE)) return 0U;
    }
#endif
    if (!motor_pwm_drv_init()) return 0U;
    if (!motor_adc_drv_init(&ia_offset, &ib_offset)) {
        motor_pwm_drv_stop();
        g_fault = MOTOR_FAULT_ADC;
        return 0U;
    }
    motor_current_sample_init(&g_sample, ia_offset, ib_offset);
    motor_foc_init(&g_foc, MOTOR_CURRENT_KP_Q15, MOTOR_CURRENT_KI_Q15,
                   MOTOR_OBSERVER_R_Q15, MOTOR_OBSERVER_L_Q15,
                   MOTOR_OBSERVER_MIN_EMF_Q15,
                   (int32_t)((int64_t)MOTOR_MAX_SPEED_RPM * MOTOR_POLE_PAIRS *
                             65536 / (60 * MOTOR_PWM_HZ)));
    motor_protection_init(&g_protection);
    g_current_limit_q15 = (int16_t)((int64_t)MOTOR_MAX_CURRENT_MA * 32767 /
                                    current_scale_ma);
    if (g_current_limit_q15 <= 0 ||
        MOTOR_ALIGN_ID_Q15 >= g_current_limit_q15 ||
        MOTOR_START_IQ_Q15 >= g_current_limit_q15) return 0U;
#if MOTOR_ENABLE_FIELD_WEAKENING
    /* 同时满足整数保护阈值，避免浮点配置换算出现一LSB偏差。 */
    if (g_fw.current_limit_q15 > g_current_limit_q15)
        g_fw.current_limit_q15 = g_current_limit_q15;
    motor_weakening_reset();
    g_foc.voltage_limit_q15 = g_fw.voltage_limit_q15;
#endif
    motor_speed_ctrl_init(&g_speed, MOTOR_SPEED_KP_Q15, MOTOR_SPEED_KI_Q15,
                          g_current_limit_q15);
    g_divider = 0U;
    g_state_ms = 0U;
    g_oil_ms = 0U;
    g_lost_lock_ms = 0U;
    g_open_angle = 0U;
    g_open_step = 0U;
    g_iq_ref = 0;
    motor_adc_drv_arm_trigger();
    g_ready = 1U;
    g_fault = MOTOR_FAULT_NONE;
    g_state = MOTOR_STATE_STOPPED;
    return 1U;
}

uint8_t motor_app_request_start(uint32_t speed_rpm)
{
    if (!g_ready || g_state == MOTOR_STATE_FAULT || speed_rpm == 0U ||
        speed_rpm > MOTOR_MAX_SPEED_RPM) return 0U;
    g_requested_rpm = speed_rpm;
    return 1U;
}

void motor_app_request_stop(void)
{
    g_requested_rpm = 0U;
}

void motor_app_watchdog_1ms_isr(void)
{
    /* 与 ADC ISR 独立；即使 ADC 触发停止，也要在 3 ms 内关断 PWM。 */
    if (g_state == MOTOR_STATE_STOPPED || g_state == MOTOR_STATE_FAULT) {
        g_missing_adc_ms = 0U;
        g_watchdog_seen_ticks = g_adc_ticks;
        return;
    }
    if (g_requested_rpm == 0U) {
        motor_pwm_drv_stop();
        motor_weakening_reset();
        g_state = MOTOR_STATE_STOPPED;
        return;
    }
    if (g_adc_ticks == g_watchdog_seen_ticks) {
        if (++g_missing_adc_ms >= 3U) motor_trip(MOTOR_FAULT_ADC);
    } else {
        g_missing_adc_ms = 0U;
        g_watchdog_seen_ticks = g_adc_ticks;
    }
}

uint8_t motor_app_clear_fault(void)
{
    if (!g_ready || g_requested_rpm != 0U || g_state != MOTOR_STATE_FAULT)
        return 0U;
    motor_protection_init(&g_protection);
    g_fault = MOTOR_FAULT_NONE;
    g_state = MOTOR_STATE_STOPPED;
    return 1U;
}

motor_state_t motor_app_state(void) { return g_state; }
motor_fault_t motor_app_fault(void) { return g_fault; }

void motor_app_process_1ms(void)
{
    motor_adc_raw_t raw;
    if (!g_ready || g_state != MOTOR_STATE_STOPPED || g_requested_rpm == 0U)
        return;
    /* 输出关闭时先测母线，欠压/过压不允许进入对齐阶段。 */
    if (!motor_adc_drv_read_bus_once(&raw.bus)) {
        motor_trip(MOTOR_FAULT_ADC);
        return;
    }
    raw.ia = g_sample.ia_offset;
    raw.ib = g_sample.ib_offset;
    motor_current_sample_decode(&g_sample, raw.ia, raw.ib, raw.bus,
                                MOTOR_BUS_FULL_SCALE_MV);
    if (motor_protection_sample(&g_protection, raw.ia, raw.ib, raw.bus,
                                g_sample.ia_q15, g_sample.ib_q15,
                                g_sample.bus_mv, MOTOR_MIN_BUS_MV,
                                MOTOR_MAX_BUS_MV, g_current_limit_q15) !=
        MOTOR_FAULT_NONE) {
        motor_trip(g_protection.fault);
        return;
    }
    motor_foc_init(&g_foc, MOTOR_CURRENT_KP_Q15, MOTOR_CURRENT_KI_Q15,
                   MOTOR_OBSERVER_R_Q15, MOTOR_OBSERVER_L_Q15,
                   MOTOR_OBSERVER_MIN_EMF_Q15,
                   (int32_t)((int64_t)MOTOR_MAX_SPEED_RPM * MOTOR_POLE_PAIRS *
                             65536 / (60 * MOTOR_PWM_HZ)));
#if MOTOR_ENABLE_FIELD_WEAKENING
    /* 重启重新初始化FOC会清空电压圆，必须恢复同一已验证配置。 */
    motor_weakening_reset();
    g_foc.voltage_limit_q15 = g_fw.voltage_limit_q15;
#endif
    motor_speed_ctrl_init(&g_speed, MOTOR_SPEED_KP_Q15, MOTOR_SPEED_KI_Q15,
                          g_current_limit_q15);
    g_divider = 0U;
    g_missing_adc_ms = 0U;
    g_watchdog_seen_ticks = g_adc_ticks;
    g_state_ms = 0U;
    g_oil_ms = 0U;
    g_lost_lock_ms = 0U;
    g_open_angle = 0U;
    g_open_step = 0U;
    g_iq_ref = 0;
    g_state = MOTOR_STATE_ALIGN;
    if (!motor_pwm_drv_start()) motor_trip(MOTOR_FAULT_PWM);
}

static void motor_speed_tick(void)
{
    /* 每 20 个采样周期进入一次：20 kHz / 20 = 1 kHz。 */
    int32_t estimated_rpm;
    uint32_t demand_rpm;
    uint32_t oil_speed_rpm = MOTOR_OIL_SPEED_RPM;
    g_state_ms++;
    if (g_state == MOTOR_STATE_ALIGN && g_state_ms >= MOTOR_ALIGN_MS) {
        g_state = MOTOR_STATE_OPEN_LOOP;
        g_state_ms = 0U;
        g_foc.observer.angle = g_open_angle;
        g_foc.observer.locked = 0U;
        g_foc.observer.lock_count = 0U;
    } else if (g_state == MOTOR_STATE_OPEN_LOOP) {
        /* 先用开环电角速度拉起转子，反电动势充分后才切闭环。 */
        uint32_t ramp_rpm = MOTOR_START_SPEED_RPM * g_state_ms /
                            MOTOR_OPEN_LOOP_MS;
        if (ramp_rpm < 50U) ramp_rpm = 50U;
        g_open_step = (uint16_t)((uint64_t)ramp_rpm * MOTOR_POLE_PAIRS *
                                 65536U / (60U * MOTOR_PWM_HZ));
        if (!g_foc.observer.locked) g_foc.observer.step = g_open_step;
        if (g_state_ms >= MOTOR_OPEN_LOOP_MS) {
            if (!g_foc.observer.locked) {
                motor_trip(MOTOR_FAULT_LOST_POSITION);
                return;
            }
            g_state = MOTOR_STATE_RUN;
            g_state_ms = 0U;
        }
    } else if (g_state == MOTOR_STATE_RUN || g_state == MOTOR_STATE_OILING) {
        /* 上油阶段使用更高的最低转速，结束后恢复外部目标。 */
        if (!g_foc.observer.locked) {
            if (++g_lost_lock_ms >= 20U) {
                motor_trip(MOTOR_FAULT_LOST_POSITION);
                return;
            }
        } else {
            g_lost_lock_ms = 0U;
        }
#if MOTOR_ENABLE_FIELD_WEAKENING
        /* 使用刚完成电流环的未饱和请求，新的Id在下一电流周期生效。 */
        if (!motor_fw_adapter_update(&g_fw, g_foc.voltage_request_d_q15,
                                      g_foc.voltage_request_q_q15,
                                      g_sample.bus_mv,
                                      (float)MOTOR_SPEED_DIVIDER / MOTOR_PWM_HZ,
                                      g_foc.observer.locked != 0U)) {
            motor_trip(MOTOR_FAULT_CONFIG);
            return;
        }
        g_speed.pi.limit = g_fw.iq_limit_q15;
        if (g_speed.pi.integral > g_speed.pi.limit)
            g_speed.pi.integral = g_speed.pi.limit;
        if (g_speed.pi.integral < -g_speed.pi.limit)
            g_speed.pi.integral = -g_speed.pi.limit;
#endif
        estimated_rpm = motor_observer_speed_rpm(&g_foc.observer,
                                                 MOTOR_POLE_PAIRS);
        demand_rpm = g_requested_rpm;
        if (g_state == MOTOR_STATE_OILING && demand_rpm < oil_speed_rpm)
            demand_rpm = oil_speed_rpm;
        g_iq_ref = motor_speed_ctrl_step(&g_speed, (int32_t)demand_rpm,
                                          estimated_rpm, MOTOR_MAX_SPEED_RPM);
        if (motor_protection_speed(&g_protection, (int32_t)demand_rpm,
                                   estimated_rpm, MOTOR_STALL_SPEED_RPM,
                                   MOTOR_STALL_TIMEOUT_MS) != MOTOR_FAULT_NONE) {
            motor_trip(g_protection.fault);
            return;
        }
        if (g_state == MOTOR_STATE_RUN && ++g_oil_ms >= MOTOR_OIL_INTERVAL_MS) {
            g_state = MOTOR_STATE_OILING;
            g_state_ms = 0U;
        } else if (g_state == MOTOR_STATE_OILING &&
                   g_state_ms >= MOTOR_OIL_DURATION_MS) {
            g_state = MOTOR_STATE_RUN;
            g_state_ms = 0U;
            g_oil_ms = 0U;
        }
    }
}

void motor_app_current_isr(motor_adc_raw_t raw)
{
    motor_duty_t duty;
    int16_t id_ref = 0;
    if (g_state == MOTOR_STATE_STOPPED || g_state == MOTOR_STATE_FAULT) return;
    g_adc_ticks++;
    if (g_requested_rpm == 0U) {
        motor_pwm_drv_stop();
        motor_weakening_reset();
        g_state = MOTOR_STATE_STOPPED;
        return;
    }
    motor_current_sample_decode(&g_sample, raw.ia, raw.ib, raw.bus,
                                MOTOR_BUS_FULL_SCALE_MV);
    /* 保护先于 FOC 和比较寄存器更新，故障周期不再写入新占空比。 */
    if (motor_protection_sample(&g_protection, raw.ia, raw.ib, raw.bus,
                                g_sample.ia_q15, g_sample.ib_q15,
                                g_sample.bus_mv, MOTOR_MIN_BUS_MV,
                                MOTOR_MAX_BUS_MV, g_current_limit_q15) !=
        MOTOR_FAULT_NONE) {
        motor_trip(g_protection.fault);
        return;
    }
    if (g_state == MOTOR_STATE_ALIGN) {
        id_ref = MOTOR_ALIGN_ID_Q15;
    } else if (g_state == MOTOR_STATE_OPEN_LOOP) {
        g_open_angle = (uint16_t)(g_open_angle + g_open_step);
        g_iq_ref = MOTOR_START_IQ_Q15;
    }
#if MOTOR_ENABLE_FIELD_WEAKENING
    if (g_state == MOTOR_STATE_RUN || g_state == MOTOR_STATE_OILING) {
        if (!g_foc.observer.locked) motor_weakening_reset();
        id_ref = g_fw.id_ref_q15;
        g_iq_ref = motor_fw_adapter_limit_iq(&g_fw, g_iq_ref);
    }
#endif
    duty = motor_foc_step(&g_foc, g_sample.ia_q15, g_sample.ib_q15,
                           id_ref, g_iq_ref, g_open_angle,
                           g_state == MOTOR_STATE_RUN ||
                           g_state == MOTOR_STATE_OILING);
#if MOTOR_ENABLE_FIELD_WEAKENING
    /* 观测器可能刚在FOC内部失锁；本周期不保留负Id供下一周期使用。 */
    if ((g_state == MOTOR_STATE_RUN || g_state == MOTOR_STATE_OILING) &&
        !g_foc.observer.locked) motor_weakening_reset();
#endif
    if (++g_divider == MOTOR_SPEED_DIVIDER) {
        g_divider = 0U;
        motor_speed_tick();
    }
    /* 慢环或高优先级看门狗若已关断，故障周期不得写入新PWM。 */
    if (g_state == MOTOR_STATE_STOPPED || g_state == MOTOR_STATE_FAULT) return;
    if (!motor_pwm_drv_set(duty)) {
        motor_trip(MOTOR_FAULT_PWM);
        return;
    }
}
