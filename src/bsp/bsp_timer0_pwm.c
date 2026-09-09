/**
 * @file bsp_timer0_pwm.c
 * @brief TIMER0 三相六路中心对齐互补 PWM 实现。
 */

#include "bsp/bsp_timer0_pwm.h"

/*
 * TIMER0 的死区发生器使用 8 位分段编码：数值越大，时间步进也越大。
 * CK_TIMER0=48MHz、CKDIV=DIV1 时，最大编码 0xFF 对应 1008 个时钟，
 * 即 21us。下面的换算始终向上取整，避免实际死区短于调用者的要求。
 */
#define BSP_TIMER0_PWM_DEADTIME_MAX_TICKS  1008U
#define BSP_TIMER0_PWM_NS_PER_SECOND       1000000000ULL

/* 软件状态只记录本 BSP 的生命周期；init 后仍处于“已配置、未输出”状态。 */
static uint8_t g_timer0_pwm_initialized;
static uint8_t g_timer0_pwm_running;

static void bsp_timer0_pwm_gpio_hold_pin_low(rcu_periph_enum gpio_rcu,
                                             uint32_t gpio_port,
                                             uint32_t gpio_pin)
{
    /* 先写低电平锁存值，再切为输出，避免模式切换时产生高脉冲。 */
    rcu_periph_clock_enable(gpio_rcu);
    gpio_bit_reset(gpio_port, gpio_pin);
    gpio_mode_set(gpio_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, gpio_pin);
    gpio_output_options_set(gpio_port, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, gpio_pin);
}

static void bsp_timer0_pwm_gpio_connect_pin(uint32_t gpio_port, uint32_t gpio_pin)
{
    /* 此时 TIMER0 已完成配置且 POEN(MOE) 为 0，切到 AF 不会输出 PWM 脉冲。 */
    gpio_af_set(gpio_port, BSP_TIMER0_PWM_GPIO_AF, gpio_pin);
    gpio_mode_set(gpio_port, GPIO_MODE_AF, GPIO_PUPD_NONE, gpio_pin);
}

static void bsp_timer0_pwm_gpio_hold_low(void)
{
    /* 六路逐一使用头文件中的 PORT/PIN/RCU，后续重映射时无需修改本文件。 */
    bsp_timer0_pwm_gpio_hold_pin_low(BSP_TIMER0_PWM_CH0_RCU,
                                     BSP_TIMER0_PWM_CH0_PORT, BSP_TIMER0_PWM_CH0_PIN);
    bsp_timer0_pwm_gpio_hold_pin_low(BSP_TIMER0_PWM_CH1_RCU,
                                     BSP_TIMER0_PWM_CH1_PORT, BSP_TIMER0_PWM_CH1_PIN);
    bsp_timer0_pwm_gpio_hold_pin_low(BSP_TIMER0_PWM_CH2_RCU,
                                     BSP_TIMER0_PWM_CH2_PORT, BSP_TIMER0_PWM_CH2_PIN);
    bsp_timer0_pwm_gpio_hold_pin_low(BSP_TIMER0_PWM_CH0N_RCU,
                                     BSP_TIMER0_PWM_CH0N_PORT, BSP_TIMER0_PWM_CH0N_PIN);
    bsp_timer0_pwm_gpio_hold_pin_low(BSP_TIMER0_PWM_CH1N_RCU,
                                     BSP_TIMER0_PWM_CH1N_PORT, BSP_TIMER0_PWM_CH1N_PIN);
    bsp_timer0_pwm_gpio_hold_pin_low(BSP_TIMER0_PWM_CH2N_RCU,
                                     BSP_TIMER0_PWM_CH2N_PORT, BSP_TIMER0_PWM_CH2N_PIN);
}

static void bsp_timer0_pwm_gpio_connect(void)
{
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH0_PORT, BSP_TIMER0_PWM_CH0_PIN);
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH1_PORT, BSP_TIMER0_PWM_CH1_PIN);
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH2_PORT, BSP_TIMER0_PWM_CH2_PIN);
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH0N_PORT, BSP_TIMER0_PWM_CH0N_PIN);
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH1N_PORT, BSP_TIMER0_PWM_CH1N_PIN);
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH2N_PORT, BSP_TIMER0_PWM_CH2N_PIN);
}

static bsp_timer0_pwm_status_t bsp_timer0_pwm_deadtime_encode(uint32_t deadtime_ns,
                                                              uint16_t *deadtime_code)
{
    uint32_t ticks;
    uint32_t units;
    uint64_t scaled_ticks;

    /* ticks=ceil(ns*fDTS/1e9)，用 64 位避免纳秒乘时钟频率时溢出。 */
    scaled_ticks = ((uint64_t)deadtime_ns * BSP_TIMER0_PWM_TIMER_CLOCK_HZ) +
                   (BSP_TIMER0_PWM_NS_PER_SECOND - 1ULL);
    ticks = (uint32_t)(scaled_ticks / BSP_TIMER0_PWM_NS_PER_SECOND);

    if(ticks > BSP_TIMER0_PWM_DEADTIME_MAX_TICKS) {
        return BSP_TIMER0_PWM_ERROR_RANGE;
    }

    /*
     * 芯片手册的 DTG[7:0] 分段关系：
     *   0xxxxxxx: DT = code * tDTS
     *   10xxxxxx: DT = (64 + code[5:0]) * 2 * tDTS
     *   110xxxxx: DT = (32 + code[4:0]) * 8 * tDTS
     *   111xxxxx: DT = (32 + code[4:0]) * 16 * tDTS
     */
    if(ticks <= 127U) {
        *deadtime_code = (uint16_t)ticks;
    } else if(ticks <= 254U) {
        units = (ticks + 1U) / 2U;
        *deadtime_code = (uint16_t)(0x80U + (units - 64U));
    } else if(ticks <= 504U) {
        units = (ticks + 7U) / 8U;
        if(units < 32U) {
            units = 32U;
        }
        *deadtime_code = (uint16_t)(0xC0U + (units - 32U));
    } else {
        units = (ticks + 15U) / 16U;
        if(units < 32U) {
            units = 32U;
        }
        *deadtime_code = (uint16_t)(0xE0U + (units - 32U));
    }

    return BSP_TIMER0_PWM_OK;
}

static void bsp_timer0_pwm_break_config(uint16_t deadtime_code)
{
    timer_break_parameter_struct break_parameters;

    timer_break_struct_para_init(&break_parameters);

    /*
     * ROS/IOS 使 POEN 开关前后都采用下面配置的低电平空闲态。
     * 自动恢复关闭：若以后接入 BREAK 保护，必须由软件确认故障后重新启动。
     * 当前板级需求没有指定 BREAK 引脚，因此 BRK0/BRK1 保持关闭。
     */
    break_parameters.runoffstate = TIMER_ROS_STATE_ENABLE;
    break_parameters.ideloffstate = TIMER_IOS_STATE_ENABLE;
    break_parameters.deadtime = deadtime_code;
    break_parameters.outputautostate = TIMER_OUTAUTO_DISABLE;
    break_parameters.protectmode = TIMER_CCHP_PROT_OFF;
    break_parameters.break0state = TIMER_BREAK0_DISABLE;
    break_parameters.break0filter = 0U;
    break_parameters.break0polarity = TIMER_BREAK0_POLARITY_LOW;
    break_parameters.break1state = TIMER_BREAK1_DISABLE;
    break_parameters.break1filter = 0U;
    break_parameters.break1polarity = TIMER_BREAK1_POLARITY_LOW;
    timer_break_config(BSP_TIMER0_PWM_TIMER, &break_parameters);
}

static void bsp_timer0_pwm_channel_config(uint16_t channel,
                                          timer_oc_parameter_struct *output_parameters)
{
    timer_channel_output_config(BSP_TIMER0_PWM_TIMER, channel, output_parameters);

    /* 初始比较值为 0；影子寄存器保证新占空比只在更新事件边界生效。 */
    timer_channel_output_pulse_value_config(BSP_TIMER0_PWM_TIMER, channel, 0U);
    timer_channel_output_mode_config(BSP_TIMER0_PWM_TIMER, channel, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(BSP_TIMER0_PWM_TIMER, channel,
                                       TIMER_OC_SHADOW_ENABLE);
}

bsp_timer0_pwm_status_t bsp_timer0_pwm_init(void)
{
    timer_parameter_struct timer_parameters;
    timer_oc_parameter_struct output_parameters;
    uint16_t deadtime_code;

    /* 先清软件状态；即使中途返回错误，start() 也不能开启功率输出。 */
    g_timer0_pwm_initialized = 0U;
    g_timer0_pwm_running = 0U;

    if(BSP_TIMER0_PWM_OK !=
       bsp_timer0_pwm_deadtime_encode(BSP_TIMER0_PWM_DEADTIME_NS, &deadtime_code)) {
        return BSP_TIMER0_PWM_ERROR_RANGE;
    }

    /* 在 TIMER0 接管引脚以前，先用普通 GPIO 将六路门极信号压低。 */
    bsp_timer0_pwm_gpio_hold_low();

    /* 复位 TIMER0，避免 bootloader 或前一次初始化留下寄存器状态。 */
    rcu_periph_clock_enable(BSP_TIMER0_PWM_TIMER_RCU);
    timer_deinit(BSP_TIMER0_PWM_TIMER);

    timer_struct_para_init(&timer_parameters);
    timer_parameters.prescaler = BSP_TIMER0_PWM_PRESCALER;

    /*
     * 中心对齐模式下计数器执行 0 -> ARR -> 0，一个 PWM 周期包含 2*(ARR+1)
     * 个定时器时钟；48MHz、PSC=0、ARR=1199 得到 20kHz。
     */
    timer_parameters.alignedmode = TIMER_COUNTER_CENTER_BOTH;
    timer_parameters.counterdirection = TIMER_COUNTER_UP;
    timer_parameters.period = BSP_TIMER0_PWM_PERIOD;
    timer_parameters.clockdivision = TIMER_CKDIV_DIV1;
    timer_parameters.repetitioncounter = 0U;
    timer_init(BSP_TIMER0_PWM_TIMER, &timer_parameters);

    timer_channel_output_struct_para_init(&output_parameters);

    /* 三个主通道和三个互补通道均启用，主/互补有效极性都为高。 */
    output_parameters.outputstate = TIMER_CCX_ENABLE;
    output_parameters.outputnstate = TIMER_CCXN_ENABLE;
    output_parameters.ocpolarity = TIMER_OC_POLARITY_HIGH;
    output_parameters.ocnpolarity = TIMER_OCN_POLARITY_HIGH;
    output_parameters.ocidlestate = TIMER_OC_IDLE_STATE_LOW;
    output_parameters.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;

    /* CH0/1/2 分别作为 U/V/W，相应 CHxN 由高级定时器硬件互补生成。 */
    bsp_timer0_pwm_channel_config(TIMER_CH_0, &output_parameters);
    bsp_timer0_pwm_channel_config(TIMER_CH_1, &output_parameters);
    bsp_timer0_pwm_channel_config(TIMER_CH_2, &output_parameters);

    /* 配置死区和低电平空闲态，但保持主输出 POEN(MOE) 关闭。 */
    bsp_timer0_pwm_break_config(deadtime_code);
    timer_primary_output_config(BSP_TIMER0_PWM_TIMER, DISABLE);

    /* 软件更新事件把 PSC/ARR/CHxCV 预装值一次性装入活动寄存器。 */
    timer_event_software_generate(BSP_TIMER0_PWM_TIMER, TIMER_EVENT_SRC_UPG);
    timer_counter_value_config(BSP_TIMER0_PWM_TIMER, 0U);

    /* TIMER 完整配置且主输出关闭后，最后才把六个引脚接到复用功能。 */
    bsp_timer0_pwm_gpio_connect();
    g_timer0_pwm_initialized = 1U;

    return BSP_TIMER0_PWM_OK;
}

bsp_timer0_pwm_status_t bsp_timer0_pwm_start(void)
{
    if(0U == g_timer0_pwm_initialized) {
        return BSP_TIMER0_PWM_ERROR_STATE;
    }
    if(0U != g_timer0_pwm_running) {
        return BSP_TIMER0_PWM_OK;
    }

    /*
     * 每次都从计数器零点重新起步，三相使用同一个 TIMER0，天然同步。
     * 必须先启动计数器、最后打开 POEN，防止静止计数器直接驱动门极。
     */
    timer_event_software_generate(BSP_TIMER0_PWM_TIMER, TIMER_EVENT_SRC_UPG);
    timer_counter_value_config(BSP_TIMER0_PWM_TIMER, 0U);
    timer_enable(BSP_TIMER0_PWM_TIMER);
    timer_primary_output_config(BSP_TIMER0_PWM_TIMER, ENABLE);
    g_timer0_pwm_running = 1U;

    return BSP_TIMER0_PWM_OK;
}

void bsp_timer0_pwm_stop(void)
{
    if(0U == g_timer0_pwm_initialized) {
        return;
    }

    /* 先关总输出再停计数器；反过来会让输出停留在当前比较状态。 */
    timer_primary_output_config(BSP_TIMER0_PWM_TIMER, DISABLE);
    timer_disable(BSP_TIMER0_PWM_TIMER);
    timer_counter_value_config(BSP_TIMER0_PWM_TIMER, 0U);
    g_timer0_pwm_running = 0U;
}

bsp_timer0_pwm_status_t bsp_timer0_pwm_set_duty_permille(uint16_t phase_u,
                                                         uint16_t phase_v,
                                                         uint16_t phase_w)
{
    uint32_t pulse_u;
    uint32_t pulse_v;
    uint32_t pulse_w;

    if(0U == g_timer0_pwm_initialized) {
        return BSP_TIMER0_PWM_ERROR_STATE;
    }
    if((phase_u > BSP_TIMER0_PWM_DUTY_FULL) ||
       (phase_v > BSP_TIMER0_PWM_DUTY_FULL) ||
       (phase_w > BSP_TIMER0_PWM_DUTY_FULL)) {
        return BSP_TIMER0_PWM_ERROR_RANGE;
    }

    /* 千分比换算为比较值；1000 对应 ARR+1，即硬件意义上的 100% 占空比。 */
    pulse_u = ((uint32_t)phase_u * (BSP_TIMER0_PWM_PERIOD + 1U)) /
              BSP_TIMER0_PWM_DUTY_FULL;
    pulse_v = ((uint32_t)phase_v * (BSP_TIMER0_PWM_PERIOD + 1U)) /
              BSP_TIMER0_PWM_DUTY_FULL;
    pulse_w = ((uint32_t)phase_w * (BSP_TIMER0_PWM_PERIOD + 1U)) /
              BSP_TIMER0_PWM_DUTY_FULL;

    /*
     * 临时屏蔽更新事件，依次写入三个影子寄存器，再恢复更新。
     * 这样即使写入过程跨过计数边界，U/V/W 也会在同一个后续边界生效。
     */
    timer_update_event_disable(BSP_TIMER0_PWM_TIMER);
    timer_channel_output_pulse_value_config(BSP_TIMER0_PWM_TIMER, TIMER_CH_0, pulse_u);
    timer_channel_output_pulse_value_config(BSP_TIMER0_PWM_TIMER, TIMER_CH_1, pulse_v);
    timer_channel_output_pulse_value_config(BSP_TIMER0_PWM_TIMER, TIMER_CH_2, pulse_w);
    timer_update_event_enable(BSP_TIMER0_PWM_TIMER);

    return BSP_TIMER0_PWM_OK;
}

bsp_timer0_pwm_status_t bsp_timer0_pwm_set_deadtime_ns(uint32_t deadtime_ns)
{
    uint16_t deadtime_code;
    bsp_timer0_pwm_status_t status;

    /* timer_break_config() 会重写 CCHP（包含 POEN），运行中修改会中断输出。 */
    if((0U == g_timer0_pwm_initialized) || (0U != g_timer0_pwm_running)) {
        return BSP_TIMER0_PWM_ERROR_STATE;
    }

    status = bsp_timer0_pwm_deadtime_encode(deadtime_ns, &deadtime_code);
    if(BSP_TIMER0_PWM_OK != status) {
        return status;
    }

    bsp_timer0_pwm_break_config(deadtime_code);
    return BSP_TIMER0_PWM_OK;
}
