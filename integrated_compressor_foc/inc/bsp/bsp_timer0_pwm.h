/**
 * @file bsp_timer0_pwm.h
 * @brief TIMER0 三相六路互补 PWM：中心对齐、可编程死区和安全启停。
 *
 * 引脚固定为：
 *   CH0/CH1/CH2    = PA8/PA9/PA10 (AF2)
 *   CH0N/CH1N/CH2N = PA7/PB0/PB1  (AF2)
 *
 * bsp_timer0_pwm_init() 只完成配置，六路保持低电平；调用
 * bsp_timer0_pwm_start() 后才允许 TIMER0 驱动功率输出。
 */

#ifndef BSP_TIMER0_PWM_H
#define BSP_TIMER0_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32c2x1.h"
#include <stdint.h>

#define BSP_TIMER0_PWM_TIMER                 TIMER0
#define BSP_TIMER0_PWM_TIMER_RCU             RCU_TIMER0
#define BSP_TIMER0_PWM_TIMER_CLOCK_HZ        48000000U
#define BSP_TIMER0_PWM_FREQUENCY_HZ          20000U
#define BSP_TIMER0_PWM_DEADTIME_NS           500U
#define BSP_TIMER0_PWM_DUTY_FULL             1000U

/* 中心对齐：fPWM = fTIMER / (2 * (PSC+1) * (ARR+1))。 */
#define BSP_TIMER0_PWM_PRESCALER             0U
#define BSP_TIMER0_PWM_PERIOD                \
    ((BSP_TIMER0_PWM_TIMER_CLOCK_HZ / (2U * BSP_TIMER0_PWM_FREQUENCY_HZ)) - 1U)

#define BSP_TIMER0_PWM_GPIO_AF               GPIO_AF_2

#define BSP_TIMER0_PWM_CH0_PORT              GPIOA
#define BSP_TIMER0_PWM_CH0_PIN               GPIO_PIN_8
#define BSP_TIMER0_PWM_CH0_RCU               RCU_GPIOA

#define BSP_TIMER0_PWM_CH1_PORT              GPIOA
#define BSP_TIMER0_PWM_CH1_PIN               GPIO_PIN_9
#define BSP_TIMER0_PWM_CH1_RCU               RCU_GPIOA

#define BSP_TIMER0_PWM_CH2_PORT              GPIOA
#define BSP_TIMER0_PWM_CH2_PIN               GPIO_PIN_10
#define BSP_TIMER0_PWM_CH2_RCU               RCU_GPIOA

#define BSP_TIMER0_PWM_CH0N_PORT             GPIOA
#define BSP_TIMER0_PWM_CH0N_PIN              GPIO_PIN_7
#define BSP_TIMER0_PWM_CH0N_RCU              RCU_GPIOA

#define BSP_TIMER0_PWM_CH1N_PORT             GPIOB
#define BSP_TIMER0_PWM_CH1N_PIN              GPIO_PIN_0
#define BSP_TIMER0_PWM_CH1N_RCU              RCU_GPIOB

#define BSP_TIMER0_PWM_CH2N_PORT             GPIOB
#define BSP_TIMER0_PWM_CH2N_PIN              GPIO_PIN_1
#define BSP_TIMER0_PWM_CH2N_RCU              RCU_GPIOB

typedef enum {
    BSP_TIMER0_PWM_OK = 0,
    BSP_TIMER0_PWM_ERROR_RANGE,
    BSP_TIMER0_PWM_ERROR_STATE
} bsp_timer0_pwm_status_t;

/** 初始化 20kHz 中心对齐互补 PWM，默认死区 500ns；初始化后不启动输出。 */
bsp_timer0_pwm_status_t bsp_timer0_pwm_init(void);

/** 从计数器零点同步启动，并最后开启 TIMER0 主输出。 */
bsp_timer0_pwm_status_t bsp_timer0_pwm_start(void);

/** 先关闭 TIMER0 主输出，再停止计数器；六路回到低电平空闲态。 */
void bsp_timer0_pwm_stop(void);

/**
 * 同步更新 U/V/W 三相占空比，单位为千分比，范围均为 0..1000。
 * CH0/CH1/CH2 分别对应 U/V/W，相应互补通道由 TIMER0 硬件生成。
 */
bsp_timer0_pwm_status_t bsp_timer0_pwm_set_duty_permille(uint16_t phase_u,
                                                         uint16_t phase_v,
                                                         uint16_t phase_w);

/**
 * 修改死区时间。仅允许在输出停止时调用；硬件无法精确表示时向上取整，
 * 保证实际死区不短于请求值。48MHz/DIV1 下有效范围为 0..21000ns。
 */
bsp_timer0_pwm_status_t bsp_timer0_pwm_set_deadtime_ns(uint32_t deadtime_ns);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TIMER0_PWM_H */
