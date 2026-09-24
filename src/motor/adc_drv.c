/* GD32C231 单 ADC：三通道注入序列由 TIMER0 TRGO 定时触发；
 * EOIC 中断读取 Ia/Ib/母线。常规单通道用于停机时的母线预检查。 */
#include "motor/adc_drv.h"
#include "motor/motor_param.h"
#include "motor/motor_app.h"
#include "gd32c2x1.h"
#include "systick.h"

static uint8_t g_trigger_armed;

uint8_t motor_adc_drv_read_once(motor_adc_raw_t *sample)
{
    uint32_t timeout = 48000U;
    if (g_trigger_armed) return 0U;
    adc_flag_clear(ADC_FLAG_EOIC);
    adc_software_trigger_enable(ADC_INSERTED_CHANNEL);
    while (adc_flag_get(ADC_FLAG_EOIC) == RESET && --timeout != 0U) {}
    if (timeout == 0U) return 0U;
    sample->ia = adc_inserted_data_read(ADC_INSERTED_CHANNEL_0);
    sample->ib = adc_inserted_data_read(ADC_INSERTED_CHANNEL_1);
    sample->bus = adc_inserted_data_read(ADC_INSERTED_CHANNEL_2);
    adc_flag_clear(ADC_FLAG_EOIC);
    return 1U;
}

uint8_t motor_adc_drv_init(uint16_t *ia_offset, uint16_t *ib_offset)
{
    uint32_t sum_ia = 0U, sum_ib = 0U;
    motor_adc_raw_t raw;
    uint32_t n;
    g_trigger_armed = 0U;
    rcu_periph_clock_enable(MOTOR_IA_GPIO_RCU);
    rcu_periph_clock_enable(MOTOR_IB_GPIO_RCU);
    rcu_periph_clock_enable(MOTOR_BUS_GPIO_RCU);
    gpio_mode_set(MOTOR_IA_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  MOTOR_IA_GPIO_PIN);
    gpio_mode_set(MOTOR_IB_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  MOTOR_IB_GPIO_PIN);
    gpio_mode_set(MOTOR_BUS_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  MOTOR_BUS_GPIO_PIN);

    rcu_periph_clock_enable(RCU_ADC);
    rcu_adc_clock_config(RCU_ADCSRC_CKSYS, RCU_ADCCK_DIV4);
    adc_deinit();
    adc_resolution_config(ADC_RESOLUTION_12B);
    adc_special_function_config(ADC_SCAN_MODE, ENABLE);
    adc_special_function_config(ADC_CONTINUOUS_MODE, DISABLE);
    adc_data_alignment_config(ADC_DATAALIGN_RIGHT);
    adc_channel_length_config(ADC_INSERTED_CHANNEL, 3U);
    adc_channel_length_config(ADC_ROUTINE_CHANNEL, 1U);
    adc_routine_channel_config(0U, MOTOR_BUS_ADC_CHANNEL,
                               ADC_SAMPLETIME_19POINT5);
    adc_external_trigger_source_config(ADC_ROUTINE_CHANNEL,
                                       ADC_EXTTRIG_ROUTINE_NONE);
    adc_external_trigger_config(ADC_ROUTINE_CHANNEL, ENABLE);
    adc_inserted_channel_config(0U, MOTOR_IA_ADC_CHANNEL, ADC_SAMPLETIME_7POINT5);
    adc_inserted_channel_config(1U, MOTOR_IB_ADC_CHANNEL, ADC_SAMPLETIME_7POINT5);
    adc_inserted_channel_config(2U, MOTOR_BUS_ADC_CHANNEL, ADC_SAMPLETIME_7POINT5);
    adc_external_trigger_source_config(ADC_INSERTED_CHANNEL, ADC_EXTTRIG_INSERTED_NONE);
    adc_external_trigger_config(ADC_INSERTED_CHANNEL, ENABLE);
    adc_enable();
    delay_1ms(1U);

    /* MOE 关闭时累计 128 次零电流样本。双极性放大器零点应在码值中间，
     * 偏离过大按传感器断线或饱和处理。 */
    for (n = 0U; n < 128U; n++) {
        if (!motor_adc_drv_read_once(&raw)) return 0U;
        sum_ia += raw.ia;
        sum_ib += raw.ib;
    }
    *ia_offset = (uint16_t)(sum_ia / 128U);
    *ib_offset = (uint16_t)(sum_ib / 128U);
    if (*ia_offset < 1000U || *ia_offset > 3000U ||
        *ib_offset < 1000U || *ib_offset > 3000U) return 0U;
    return 1U;
}

uint8_t motor_adc_drv_read_bus_once(uint16_t *bus_raw)
{
    uint32_t timeout = 48000U;
    /* 停机状态没有 TIMER0 注入触发。常规序列只采一个通道，临时关闭扫描，
     * 避免 GD32 对常规扫描序列要求 DMA 的限制。返回前恢复注入序列扫描。 */
    adc_special_function_config(ADC_SCAN_MODE, DISABLE);
    adc_flag_clear(ADC_FLAG_EOC);
    adc_software_trigger_enable(ADC_ROUTINE_CHANNEL);
    while (adc_flag_get(ADC_FLAG_EOC) == RESET && --timeout != 0U) {}
    if (timeout == 0U) {
        adc_special_function_config(ADC_SCAN_MODE, ENABLE);
        return 0U;
    }
    /* GD32C231 勘误：EOC 后至少等待两个 ADC 时钟再读 RDATA。 */
    for (volatile uint32_t n = 0U; n < 16U; n++) { __NOP(); }
    *bus_raw = adc_routine_data_read();
    adc_flag_clear(ADC_FLAG_EOC);
    adc_special_function_config(ADC_SCAN_MODE, ENABLE);
    return 1U;
}

void motor_adc_drv_arm_trigger(void)
{
    /* 勘误：触发使能期间切换源可能额外启动一次转换，故先关闭触发。 */
    adc_external_trigger_config(ADC_INSERTED_CHANNEL, DISABLE);
    adc_external_trigger_source_config(ADC_INSERTED_CHANNEL,
                                       ADC_EXTTRIG_INSERTED_T0_TRGO);
    adc_flag_clear(ADC_FLAG_EOIC);
    adc_interrupt_enable(ADC_INT_EOIC);
    nvic_irq_enable(ADC_IRQn, 0U);
    g_trigger_armed = 1U;
    adc_external_trigger_config(ADC_INSERTED_CHANNEL, ENABLE);
}

void ADC_IRQHandler(void)
{
    motor_adc_raw_t raw;
    if (adc_interrupt_flag_get(ADC_INT_FLAG_EOIC) == RESET) return;
    raw.ia = adc_inserted_data_read(ADC_INSERTED_CHANNEL_0);
    raw.ib = adc_inserted_data_read(ADC_INSERTED_CHANNEL_1);
    raw.bus = adc_inserted_data_read(ADC_INSERTED_CHANNEL_2);
    adc_interrupt_flag_clear(ADC_INT_FLAG_EOIC);
    motor_app_current_isr(raw);
}
