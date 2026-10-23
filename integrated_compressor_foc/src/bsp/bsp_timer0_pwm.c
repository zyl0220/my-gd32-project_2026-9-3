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

/*
 * 推荐调用顺序：
 *
 *   bsp_timer0_pwm_init()                 初始化硬件，此时六路不输出 PWM
 *            ↓
 *   bsp_timer0_pwm_set_deadtime_ns()      可选：在停止状态下修改死区
 *            ↓
 *   bsp_timer0_pwm_set_duty_permille()    设置 U/V/W 三相占空比
 *            ↓
 *   bsp_timer0_pwm_start()                启动计数器并开放六路输出
 *            ↓
 *   bsp_timer0_pwm_set_duty_permille()    运行期间可以继续同步更新占空比
 *            ↓
 *   bsp_timer0_pwm_stop()                 关闭六路输出并停止计数器
 */

/* 软件状态只记录本 BSP 的生命周期；init 后仍处于“已配置、未输出”状态。 */
static uint8_t g_timer0_pwm_initialized;
static uint8_t g_timer0_pwm_running;

/*!
    \brief      将一个 PWM 引脚临时配置成普通推挽输出并保持低电平
    \param[in]  gpio_rcu: 该 GPIO 端口对应的 RCU 时钟，例如 RCU_GPIOA
    \param[in]  gpio_port: GPIO 端口，例如 GPIOA
    \param[in]  gpio_pin: GPIO 引脚掩码，例如 GPIO_PIN_8
    \param[out] none
    \retval     none
    \note       在 TIMER0 接管引脚前调用，用来避免初始化期间产生门极高脉冲。
*/
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

/*!
    \brief      将一个已经保持低电平的引脚连接到 TIMER0 复用功能
    \param[in]  gpio_port: GPIO 端口
    \param[in]  gpio_pin: GPIO 引脚掩码
    \param[out] none
    \retval     none
    \note       只能在 TIMER0 配置完成且主输出 POEN(MOE) 关闭时调用。
*/
static void bsp_timer0_pwm_gpio_connect_pin(uint32_t gpio_port, uint32_t gpio_pin)
{
    /* 此时 TIMER0 已完成配置且 POEN(MOE) 为 0，切到 AF 不会输出 PWM 脉冲。 */
    gpio_af_set(gpio_port, BSP_TIMER0_PWM_GPIO_AF, gpio_pin);
    gpio_mode_set(gpio_port, GPIO_MODE_AF, GPIO_PUPD_NONE, gpio_pin);
}

/*!
    \brief      将 U/V/W 三个主通道和三个互补通道对应的六个引脚全部拉低
    \param[in]  none
    \param[out] none
    \retval     none
*/
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

/*!
    \brief      将六个 PWM 引脚全部切换到 TIMER0 的复用功能
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void bsp_timer0_pwm_gpio_connect(void)
{
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH0_PORT, BSP_TIMER0_PWM_CH0_PIN);
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH1_PORT, BSP_TIMER0_PWM_CH1_PIN);
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH2_PORT, BSP_TIMER0_PWM_CH2_PIN);
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH0N_PORT, BSP_TIMER0_PWM_CH0N_PIN);
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH1N_PORT, BSP_TIMER0_PWM_CH1N_PIN);
    bsp_timer0_pwm_gpio_connect_pin(BSP_TIMER0_PWM_CH2N_PORT, BSP_TIMER0_PWM_CH2N_PIN);
}

/*!
    \brief      把用户输入的死区时间（ns）转换成 TIMER0 的 8 位 DTG 编码
    \param[in]  deadtime_ns: 期望死区时间，单位 ns，当前有效范围为 0..21000
    \param[out] deadtime_code: 转换后的 DTG 编码，成功时范围为 0x00..0xFF
    \retval     BSP_TIMER0_PWM_OK: 转换成功
    \retval     BSP_TIMER0_PWM_ERROR_RANGE: 请求时间超过硬件可表示范围
    \note       硬件无法精确表示时向上取整，保证实际死区不短于请求值。
*/
static bsp_timer0_pwm_status_t bsp_timer0_pwm_deadtime_encode(uint32_t deadtime_ns,
                                                              uint16_t *deadtime_code)
{
    /*
     * ticks：用户要求的死区相当于多少个 tDTS 时钟周期。
     * units：进入分段编码后，暂存按 2、8 或 16 个 tDTS 为一步的单位数。
     * scaled_ticks：计算中间值。使用 64 位是因为“纳秒数 × 48MHz”可能超过 32 位。
     */
    uint32_t ticks;
    uint32_t units;
    uint64_t scaled_ticks;

    /*
     * 把纳秒换算成 tDTS 个数：
     *
     *   ticks = ceil(deadtime_ns × TIMER时钟频率 / 1,000,000,000)
     *
     * 整数除法默认向下取整，所以在除法前加“除数 - 1”，实现向上取整：
     *
     *   ceil(a / b) = (a + b - 1) / b
     *
     * 必须向上取整，否则实际死区可能比调用者要求的时间更短。
     * 当前 48MHz、CKDIV=DIV1，因此 1 个 tDTS 约等于 20.833ns。
     */
    scaled_ticks = ((uint64_t)deadtime_ns * BSP_TIMER0_PWM_TIMER_CLOCK_HZ) +
                   (BSP_TIMER0_PWM_NS_PER_SECOND - 1ULL);
    ticks = (uint32_t)(scaled_ticks / BSP_TIMER0_PWM_NS_PER_SECOND);

    /* DTG=0xFF 最多只能表示 1008 个 tDTS，即当前配置下的 21us。 */
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
        /*
         * 第一段，DTG=0x00..0x7F。
         * 每增加 1，死区增加 1 个 tDTS，因此 ticks 可以直接作为寄存器编码。
         * 例如 500ns -> 24 个 tDTS -> DTG=24=0x18。
         */
        *deadtime_code = (uint16_t)ticks;
    } else if(ticks <= 254U) {
        /*
         * 第二段，DTG=0x80..0xBF，每一级代表 2 个 tDTS。
         * ceil(ticks/2) 保证奇数 ticks 也向上选择一个不短于要求的死区。
         * 该段公式中自带基数 64，所以减去 64 后才是 DTG 的低 6 位。
         */
        units = (ticks + 1U) / 2U;
        *deadtime_code = (uint16_t)(0x80U + (units - 64U));
    } else if(ticks <= 504U) {
        /*
         * 第三段，DTG=0xC0..0xDF，每一级代表 8 个 tDTS。
         * 先做 ceil(ticks/8)，再减去公式中的基数 32，得到 DTG 的低 5 位。
         */
        units = (ticks + 7U) / 8U;

        /*
         * 255 个 tDTS 在硬件中没有精确编码；第三段最小值只能是
         * 32×8=256 个 tDTS。因此这里钳位到 32，保证结果不小于请求值。
         */
        if(units < 32U) {
            units = 32U;
        }
        *deadtime_code = (uint16_t)(0xC0U + (units - 32U));
    } else {
        /*
         * 第四段，DTG=0xE0..0xFF，每一级代表 16 个 tDTS。
         * 先做 ceil(ticks/16)，再减去基数 32，得到 DTG 的低 5 位。
         */
        units = (ticks + 15U) / 16U;

        /*
         * 505..511 个 tDTS 同样没有精确编码；第四段最小值是
         * 32×16=512 个 tDTS，所以向上钳位到 32。
         */
        if(units < 32U) {
            units = 32U;
        }
        *deadtime_code = (uint16_t)(0xE0U + (units - 32U));
    }

    /* 编码成功，deadtime_code 已得到可直接写入 TIMER_CCHP.DTCFG 的值。 */
    return BSP_TIMER0_PWM_OK;
}

/*!
    \brief      配置高级定时器的死区、关闭态电平和 BREAK 相关参数
    \param[in]  deadtime_code: 已编码的 8 位 DTG 数值
    \param[out] none
    \retval     none
    \note       timer_break_config() 会整体重写 CCHP，因此只能在输出停止时调用。
*/
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

/*!
    \brief      将一个 TIMER0 通道配置成带影子寄存器的 PWM0 输出
    \param[in]  channel: TIMER_CH_0、TIMER_CH_1 或 TIMER_CH_2
    \param[in]  output_parameters: 主输出、互补输出、极性及空闲电平配置
    \param[out] none
    \retval     none
*/
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

/*!
    \brief      初始化 TIMER0 三相六路中心对齐互补 PWM
    \param[in]  none
    \param[out] none
    \retval     BSP_TIMER0_PWM_OK: GPIO、TIMER0、通道和默认死区配置完成
    \retval     BSP_TIMER0_PWM_ERROR_RANGE: 默认死区超出硬件可表示范围
    \note       初始化完成后 TIMER0 不计数、POEN 关闭，必须调用 start() 才有 PWM 输出。
*/
bsp_timer0_pwm_status_t bsp_timer0_pwm_init(void)
{
    timer_parameter_struct timer_parameters;
    timer_oc_parameter_struct output_parameters;
    uint16_t deadtime_code;

    /* 第1步：清软件状态；即使中途返回错误，start() 也不能开启功率输出。 */
    g_timer0_pwm_initialized = 0U;
    g_timer0_pwm_running = 0U;

    /* 第2步：先计算默认 500ns 死区对应的 DTG 寄存器编码。 */
    if(BSP_TIMER0_PWM_OK !=
       bsp_timer0_pwm_deadtime_encode(BSP_TIMER0_PWM_DEADTIME_NS, &deadtime_code)) {
        return BSP_TIMER0_PWM_ERROR_RANGE;
    }

    /* 第3步：在 TIMER0 接管引脚以前，先用普通 GPIO 将六路门极信号压低。 */
    bsp_timer0_pwm_gpio_hold_low();

    /* 第4步：使能并复位 TIMER0，避免 bootloader 或前一次初始化留下状态。 */
    rcu_periph_clock_enable(BSP_TIMER0_PWM_TIMER_RCU);
    timer_deinit(BSP_TIMER0_PWM_TIMER);

    /* 第5步：配置 TIMER0 的计数方式、预分频、周期和死区采样时钟。 */
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

    /* 第6步：准备三个主通道及三个互补通道共用的输出参数。 */
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

    /* 第7步：配置死区和低电平空闲态，但保持主输出 POEN(MOE) 关闭。 */
    bsp_timer0_pwm_break_config(deadtime_code);
    timer_primary_output_config(BSP_TIMER0_PWM_TIMER, DISABLE);

    /* 第8步：用更新事件把 PSC、ARR、CHxCV 预装值一次性装入活动寄存器。 */
    timer_event_software_generate(BSP_TIMER0_PWM_TIMER, TIMER_EVENT_SRC_UPG);
    timer_counter_value_config(BSP_TIMER0_PWM_TIMER, 0U);

    /* 第9步：TIMER 配置完成且 POEN 关闭后，最后才将六个引脚切换到 AF。 */
    bsp_timer0_pwm_gpio_connect();

    /* 第10步：置初始化完成标志；此时仍未启动输出。 */
    g_timer0_pwm_initialized = 1U;

    return BSP_TIMER0_PWM_OK;
}

/*!
    \brief      从计数器零点启动三相 PWM，并开放六路功率输出
    \param[in]  none
    \param[out] none
    \retval     BSP_TIMER0_PWM_OK: 启动成功，或原本已经处于运行状态
    \retval     BSP_TIMER0_PWM_ERROR_STATE: 尚未调用 bsp_timer0_pwm_init()
*/
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

/*!
    \brief      安全关闭六路 PWM 输出并停止 TIMER0
    \param[in]  none
    \param[out] none
    \retval     none
    \note       可以重复调用；尚未初始化时直接返回。
*/
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

/*!
    \brief      同步设置 U/V/W 三相的 PWM 占空比
    \param[in]  phase_u: U 相占空比，千分比 0..1000
    \param[in]  phase_v: V 相占空比，千分比 0..1000
    \param[in]  phase_w: W 相占空比，千分比 0..1000
    \param[out] none
    \retval     BSP_TIMER0_PWM_OK: 三相比较值已写入影子寄存器
    \retval     BSP_TIMER0_PWM_ERROR_RANGE: 任意一相大于 1000
    \retval     BSP_TIMER0_PWM_ERROR_STATE: 尚未初始化
    \note       例如参数 250、500、750 分别表示 25%、50%、75%。
*/
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

/*!
    \brief      修改主通道与互补通道之间的死区时间
    \param[in]  deadtime_ns: 期望死区时间，单位 ns，当前有效范围为 0..21000
    \param[out] none
    \retval     BSP_TIMER0_PWM_OK: 新死区已经写入 TIMER0
    \retval     BSP_TIMER0_PWM_ERROR_RANGE: 请求时间超出硬件范围
    \retval     BSP_TIMER0_PWM_ERROR_STATE: 尚未初始化，或 PWM 正在运行
    \note       必须先调用 bsp_timer0_pwm_stop()，再修改死区。
*/
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
