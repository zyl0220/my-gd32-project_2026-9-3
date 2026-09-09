#include <stdint.h>

#include "bsp/bsp_debug_uart.h"
#include "bsp/bsp_spi.h"
#include "bsp/bsp_timer0_pwm.h"
#include "fake_gd32_state.h"

#define TEST_RESULT_ADDRESS 0x20000000UL

_Static_assert(BSP_DEBUG_UART_TX_PIN == GPIO_PIN_6, "USART0 TX must move to PB6");
_Static_assert(BSP_DEBUG_UART_RX_PIN == GPIO_PIN_7, "USART0 RX must move to PB7");
_Static_assert(BSP_DEBUG_UART_TX_PORT == GPIOB, "USART0 TX must move to GPIOB");
_Static_assert(BSP_DEBUG_UART_RX_PORT == GPIOB, "USART0 RX must move to GPIOB");
_Static_assert(BSP_DEBUG_UART_TX_AF == GPIO_AF_0, "PB6 USART0 TX must use AF0");
_Static_assert(BSP_DEBUG_UART_RX_AF == GPIO_AF_0, "PB7 USART0 RX must use AF0");

_Static_assert(BSP_SPI_SCK_PORT == GPIOB, "SPI0 SCK must remain on PB3");
_Static_assert(BSP_SPI_SCK_PIN == GPIO_PIN_3, "SPI0 SCK must remain on PB3");
_Static_assert(BSP_SPI_MISO_PORT == GPIOA, "SPI0 MISO must remain on PA6");
_Static_assert(BSP_SPI_MISO_PIN == GPIO_PIN_6, "SPI0 MISO must remain on PA6");
_Static_assert(BSP_SPI_MOSI_PORT == GPIOB, "SPI0 MOSI must move off PA7");
_Static_assert(BSP_SPI_MOSI_PIN == GPIO_PIN_5, "SPI0 MOSI must move to PB5");

_Static_assert(BSP_TIMER0_PWM_CH0_PORT == GPIOA, "TIMER0 CH0 must be PA8");
_Static_assert(BSP_TIMER0_PWM_CH0_PIN == GPIO_PIN_8, "TIMER0 CH0 must be PA8");
_Static_assert(BSP_TIMER0_PWM_CH1_PORT == GPIOA, "TIMER0 CH1 must be PA9");
_Static_assert(BSP_TIMER0_PWM_CH1_PIN == GPIO_PIN_9, "TIMER0 CH1 must be PA9");
_Static_assert(BSP_TIMER0_PWM_CH2_PORT == GPIOA, "TIMER0 CH2 must be PA10");
_Static_assert(BSP_TIMER0_PWM_CH2_PIN == GPIO_PIN_10, "TIMER0 CH2 must be PA10");
_Static_assert(BSP_TIMER0_PWM_CH0N_PORT == GPIOA, "TIMER0 CH0N must be PA7");
_Static_assert(BSP_TIMER0_PWM_CH0N_PIN == GPIO_PIN_7, "TIMER0 CH0N must be PA7");
_Static_assert(BSP_TIMER0_PWM_CH1N_PORT == GPIOB, "TIMER0 CH1N must be PB0");
_Static_assert(BSP_TIMER0_PWM_CH1N_PIN == GPIO_PIN_0, "TIMER0 CH1N must be PB0");
_Static_assert(BSP_TIMER0_PWM_CH2N_PORT == GPIOB, "TIMER0 CH2N must be PB1");
_Static_assert(BSP_TIMER0_PWM_CH2N_PIN == GPIO_PIN_1, "TIMER0 CH2N must be PB1");
_Static_assert(BSP_TIMER0_PWM_GPIO_AF == GPIO_AF_2, "TIMER0 PWM pins must use AF2");
_Static_assert(BSP_TIMER0_PWM_FREQUENCY_HZ == 20000U, "default PWM must be 20 kHz");
_Static_assert(BSP_TIMER0_PWM_DEADTIME_NS == 500U, "default dead time must be 500 ns");
_Static_assert(BSP_TIMER0_PWM_PERIOD == 1199U, "20 kHz center-aligned period must be 1199");

static void test_finish(uint32_t result)
{
    *(volatile uint32_t *)TEST_RESULT_ADDRESS = result;
    __asm volatile("bkpt #0");
    for(;;) {
    }
}

#define CHECK(condition)                  \
    do {                                  \
        if(!(condition)) {                \
            test_finish((uint32_t)__LINE__); \
        }                                 \
    } while(0)

static void check_pwm_pin(uint32_t port, uint32_t pin_index)
{
    CHECK(GPIO_MODE_AF == g_fake_gd32.gpio_mode[port][pin_index]);
    CHECK(GPIO_PUPD_NONE == g_fake_gd32.gpio_pull[port][pin_index]);
    CHECK(GPIO_AF_2 == g_fake_gd32.gpio_af[port][pin_index]);
}

void test_entry(void)
{
    uint32_t previous_deadtime;

    fake_gd32_reset();

    CHECK(BSP_TIMER0_PWM_ERROR_STATE == bsp_timer0_pwm_start());
    CHECK(BSP_TIMER0_PWM_ERROR_STATE == bsp_timer0_pwm_set_duty_permille(0U, 0U, 0U));
    CHECK(BSP_TIMER0_PWM_ERROR_STATE == bsp_timer0_pwm_set_deadtime_ns(500U));
    CHECK(0U == g_fake_gd32.timer_enabled);
    CHECK(0U == g_fake_gd32.primary_output_enabled);

    CHECK(BSP_TIMER0_PWM_OK == bsp_timer0_pwm_init());
    CHECK(1U == g_fake_gd32.gpio_clock_a_enabled);
    CHECK(1U == g_fake_gd32.gpio_clock_b_enabled);
    CHECK(1U == g_fake_gd32.timer_clock_enabled);
    CHECK(0U == g_fake_gd32.unsafe_gpio_transition);

    check_pwm_pin(GPIOA, 8U);
    check_pwm_pin(GPIOA, 9U);
    check_pwm_pin(GPIOA, 10U);
    check_pwm_pin(GPIOA, 7U);
    check_pwm_pin(GPIOB, 0U);
    check_pwm_pin(GPIOB, 1U);

    CHECK(0U == g_fake_gd32.timer_parameters.prescaler);
    CHECK(TIMER_COUNTER_CENTER_BOTH == g_fake_gd32.timer_parameters.alignedmode);
    CHECK(TIMER_COUNTER_UP == g_fake_gd32.timer_parameters.counterdirection);
    CHECK(1199U == g_fake_gd32.timer_parameters.period);
    CHECK(TIMER_CKDIV_DIV1 == g_fake_gd32.timer_parameters.clockdivision);
    CHECK(0U == g_fake_gd32.timer_parameters.repetitioncounter);

    CHECK(0x7U == g_fake_gd32.channel_configured_mask);
    CHECK(TIMER_CCX_ENABLE == g_fake_gd32.channel_parameters[0].outputstate);
    CHECK(TIMER_CCXN_ENABLE == g_fake_gd32.channel_parameters[0].outputnstate);
    CHECK(TIMER_CCX_ENABLE == g_fake_gd32.channel_parameters[1].outputstate);
    CHECK(TIMER_CCXN_ENABLE == g_fake_gd32.channel_parameters[1].outputnstate);
    CHECK(TIMER_CCX_ENABLE == g_fake_gd32.channel_parameters[2].outputstate);
    CHECK(TIMER_CCXN_ENABLE == g_fake_gd32.channel_parameters[2].outputnstate);
    CHECK(TIMER_OC_IDLE_STATE_LOW == g_fake_gd32.channel_parameters[0].ocidlestate);
    CHECK(TIMER_OCN_IDLE_STATE_LOW == g_fake_gd32.channel_parameters[0].ocnidlestate);
    CHECK(TIMER_OC_MODE_PWM0 == g_fake_gd32.channel_mode[0]);
    CHECK(TIMER_OC_MODE_PWM0 == g_fake_gd32.channel_mode[1]);
    CHECK(TIMER_OC_MODE_PWM0 == g_fake_gd32.channel_mode[2]);
    CHECK(TIMER_OC_SHADOW_ENABLE == g_fake_gd32.channel_shadow[0]);
    CHECK(TIMER_OC_SHADOW_ENABLE == g_fake_gd32.channel_shadow[1]);
    CHECK(TIMER_OC_SHADOW_ENABLE == g_fake_gd32.channel_shadow[2]);
    CHECK(0U == g_fake_gd32.channel_pulse[0]);
    CHECK(0U == g_fake_gd32.channel_pulse[1]);
    CHECK(0U == g_fake_gd32.channel_pulse[2]);

    CHECK(TIMER_ROS_STATE_ENABLE == g_fake_gd32.break_parameters.runoffstate);
    CHECK(TIMER_IOS_STATE_ENABLE == g_fake_gd32.break_parameters.ideloffstate);
    CHECK(24U == g_fake_gd32.break_parameters.deadtime);
    CHECK(TIMER_OUTAUTO_DISABLE == g_fake_gd32.break_parameters.outputautostate);
    CHECK(TIMER_CCHP_PROT_OFF == g_fake_gd32.break_parameters.protectmode);
    CHECK(TIMER_BREAK0_DISABLE == g_fake_gd32.break_parameters.break0state);
    CHECK(TIMER_BREAK1_DISABLE == g_fake_gd32.break_parameters.break1state);
    CHECK(0U == g_fake_gd32.timer_enabled);
    CHECK(0U == g_fake_gd32.primary_output_enabled);

    CHECK(BSP_TIMER0_PWM_OK == bsp_timer0_pwm_set_duty_permille(250U, 500U, 1000U));
    CHECK(300U == g_fake_gd32.channel_pulse[0]);
    CHECK(600U == g_fake_gd32.channel_pulse[1]);
    CHECK(1200U == g_fake_gd32.channel_pulse[2]);
    CHECK(1U == g_fake_gd32.update_disable_count);
    CHECK(1U == g_fake_gd32.update_enable_count);
    CHECK(1U == g_fake_gd32.update_events_enabled);

    CHECK(BSP_TIMER0_PWM_ERROR_RANGE == bsp_timer0_pwm_set_duty_permille(250U, 1001U, 500U));
    CHECK(300U == g_fake_gd32.channel_pulse[0]);
    CHECK(600U == g_fake_gd32.channel_pulse[1]);
    CHECK(1200U == g_fake_gd32.channel_pulse[2]);
    CHECK(1U == g_fake_gd32.update_disable_count);
    CHECK(1U == g_fake_gd32.update_enable_count);

    CHECK(BSP_TIMER0_PWM_OK == bsp_timer0_pwm_set_deadtime_ns(2645U));
    CHECK(0x7FU == g_fake_gd32.break_parameters.deadtime);
    CHECK(BSP_TIMER0_PWM_OK == bsp_timer0_pwm_set_deadtime_ns(2646U));
    CHECK(0x80U == g_fake_gd32.break_parameters.deadtime);
    CHECK(BSP_TIMER0_PWM_OK == bsp_timer0_pwm_set_deadtime_ns(21000U));
    CHECK(0xFFU == g_fake_gd32.break_parameters.deadtime);
    previous_deadtime = g_fake_gd32.break_parameters.deadtime;
    CHECK(BSP_TIMER0_PWM_ERROR_RANGE == bsp_timer0_pwm_set_deadtime_ns(21001U));
    CHECK(previous_deadtime == g_fake_gd32.break_parameters.deadtime);

    CHECK(BSP_TIMER0_PWM_OK == bsp_timer0_pwm_start());
    CHECK(1U == g_fake_gd32.timer_enabled);
    CHECK(1U == g_fake_gd32.primary_output_enabled);
    CHECK(0U == g_fake_gd32.unsafe_start_order);
    CHECK(g_fake_gd32.timer_enable_sequence < g_fake_gd32.primary_enable_sequence);
    previous_deadtime = g_fake_gd32.break_parameters.deadtime;
    CHECK(BSP_TIMER0_PWM_ERROR_STATE == bsp_timer0_pwm_set_deadtime_ns(500U));
    CHECK(previous_deadtime == g_fake_gd32.break_parameters.deadtime);

    bsp_timer0_pwm_stop();
    CHECK(0U == g_fake_gd32.primary_output_enabled);
    CHECK(0U == g_fake_gd32.timer_enabled);
    CHECK(0U == g_fake_gd32.counter);
    CHECK(0U == g_fake_gd32.unsafe_stop_order);
    CHECK(g_fake_gd32.primary_disable_sequence < g_fake_gd32.timer_disable_sequence);

    test_finish(0U);
}
