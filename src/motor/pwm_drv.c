/* TIMER0 PWM 桥接层：复用已有六路互补 PWM BSP，CH3 只做内部 ADC 时标。 */
#include "motor/pwm_drv.h"
#include "bsp/bsp_timer0_pwm.h"

uint8_t motor_pwm_drv_init(void)
{
    timer_oc_parameter_struct channel;
    if (bsp_timer0_pwm_init() != BSP_TIMER0_PWM_OK) return 0U;

    /* CH3 无引脚输出。中心对齐下 PWM0 参考信号仅在向下计数跨过比较值时
     * 产生一次上升沿；TIMER0 TRGO 把载波峰值后 50 tick 的边沿送给 ADC。 */
    timer_channel_output_struct_para_init(&channel);
    channel.outputstate = TIMER_CCX_DISABLE;
    channel.outputnstate = TIMER_CCXN_DISABLE;
    timer_channel_output_config(TIMER0, TIMER_CH_3, &channel);
    timer_channel_output_pulse_value_config(TIMER0, TIMER_CH_3,
                                            BSP_TIMER0_PWM_PERIOD - 50U);
    timer_channel_output_mode_config(TIMER0, TIMER_CH_3, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER0, TIMER_CH_3, TIMER_OC_SHADOW_ENABLE);
    timer_master_output_trigger_source_select(TIMER0, TIMER_TRI_OUT_SRC_O3CPRE);
    timer_event_software_generate(TIMER0, TIMER_EVENT_SRC_UPG);
    return 1U;
}

uint8_t motor_pwm_drv_start(void)
{
    if (bsp_timer0_pwm_set_duty_permille(500U, 500U, 500U) != BSP_TIMER0_PWM_OK)
        return 0U;
    return bsp_timer0_pwm_start() == BSP_TIMER0_PWM_OK;
}

void motor_pwm_drv_stop(void)
{
    bsp_timer0_pwm_stop();
}

uint8_t motor_pwm_drv_set(motor_duty_t duty)
{
    return bsp_timer0_pwm_set_duty_permille(duty.u, duty.v, duty.w) ==
        BSP_TIMER0_PWM_OK;
}
