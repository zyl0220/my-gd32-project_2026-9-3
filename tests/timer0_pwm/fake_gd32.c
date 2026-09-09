#include "fake_gd32_state.h"

fake_gd32_state_t g_fake_gd32;

static void fake_zero_state(void)
{
    volatile uint8_t *bytes = (volatile uint8_t *)&g_fake_gd32;
    uint32_t i;

    for(i = 0U; i < (uint32_t)sizeof(g_fake_gd32); i++) {
        bytes[i] = 0U;
    }
}

void fake_gd32_reset(void)
{
    uint32_t port;
    uint32_t pin;

    fake_zero_state();
    for(port = 0U; port < FAKE_GPIO_PORT_COUNT; port++) {
        for(pin = 0U; pin < FAKE_GPIO_PIN_COUNT; pin++) {
            g_fake_gd32.gpio_mode[port][pin] = GPIO_MODE_ANALOG;
        }
    }
    g_fake_gd32.update_events_enabled = 1U;
}

void rcu_periph_clock_enable(rcu_periph_enum periph)
{
    if(RCU_GPIOA == periph) {
        g_fake_gd32.gpio_clock_a_enabled = 1U;
    } else if(RCU_GPIOB == periph) {
        g_fake_gd32.gpio_clock_b_enabled = 1U;
    } else if(RCU_TIMER0 == periph) {
        g_fake_gd32.timer_clock_enabled = 1U;
    }
}

void gpio_bit_reset(uint32_t gpio_periph, uint32_t pin)
{
    g_fake_gd32.gpio_output_latch[gpio_periph] &= ~pin;
}

void gpio_mode_set(uint32_t gpio_periph, uint32_t mode, uint32_t pull_up_down, uint32_t pin)
{
    uint32_t index;

    if((GPIO_MODE_OUTPUT == mode) &&
       (0U != (g_fake_gd32.gpio_output_latch[gpio_periph] & pin))) {
        g_fake_gd32.unsafe_gpio_transition = 1U;
    }
    if(GPIO_MODE_AF == mode) {
        if((0U == g_fake_gd32.timer_initialized) ||
           (0x7U != g_fake_gd32.channel_configured_mask) ||
           (0U == g_fake_gd32.break_configured) ||
           (0U != g_fake_gd32.primary_output_enabled)) {
            g_fake_gd32.unsafe_gpio_transition = 1U;
        }
    }

    for(index = 0U; index < FAKE_GPIO_PIN_COUNT; index++) {
        if(0U != (pin & (1UL << index))) {
            g_fake_gd32.gpio_mode[gpio_periph][index] = mode;
            g_fake_gd32.gpio_pull[gpio_periph][index] = pull_up_down;
        }
    }
}

void gpio_output_options_set(uint32_t gpio_periph, uint8_t otype, uint32_t speed, uint32_t pin)
{
    (void)gpio_periph;
    (void)otype;
    (void)speed;
    (void)pin;
}

void gpio_af_set(uint32_t gpio_periph, uint32_t alt_func_num, uint32_t pin)
{
    uint32_t index;

    for(index = 0U; index < FAKE_GPIO_PIN_COUNT; index++) {
        if(0U != (pin & (1UL << index))) {
            g_fake_gd32.gpio_af[gpio_periph][index] = alt_func_num;
        }
    }
}

void timer_deinit(uint32_t timer_periph)
{
    (void)timer_periph;
    g_fake_gd32.timer_enabled = 0U;
    g_fake_gd32.primary_output_enabled = 0U;
}

void timer_struct_para_init(timer_parameter_struct *initpara)
{
    volatile uint8_t *bytes = (volatile uint8_t *)initpara;
    uint32_t i;

    for(i = 0U; i < (uint32_t)sizeof(*initpara); i++) {
        bytes[i] = 0U;
    }
}

void timer_init(uint32_t timer_periph, timer_parameter_struct *initpara)
{
    (void)timer_periph;
    g_fake_gd32.timer_parameters = *initpara;
    g_fake_gd32.timer_initialized = 1U;
}

void timer_channel_output_struct_para_init(timer_oc_parameter_struct *ocpara)
{
    ocpara->outputstate = TIMER_CCX_DISABLE;
    ocpara->outputnstate = TIMER_CCXN_DISABLE;
    ocpara->ocpolarity = TIMER_OC_POLARITY_HIGH;
    ocpara->ocnpolarity = TIMER_OCN_POLARITY_HIGH;
    ocpara->ocidlestate = TIMER_OC_IDLE_STATE_LOW;
    ocpara->ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;
}

void timer_channel_output_config(uint32_t timer_periph, uint16_t channel,
                                 timer_oc_parameter_struct *ocpara)
{
    (void)timer_periph;
    g_fake_gd32.channel_parameters[channel].outputstate = ocpara->outputstate;
    g_fake_gd32.channel_parameters[channel].outputnstate = ocpara->outputnstate;
    g_fake_gd32.channel_parameters[channel].ocpolarity = ocpara->ocpolarity;
    g_fake_gd32.channel_parameters[channel].ocnpolarity = ocpara->ocnpolarity;
    g_fake_gd32.channel_parameters[channel].ocidlestate = ocpara->ocidlestate;
    g_fake_gd32.channel_parameters[channel].ocnidlestate = ocpara->ocnidlestate;
    g_fake_gd32.channel_configured_mask |= (1UL << channel);
}

void timer_channel_output_pulse_value_config(uint32_t timer_periph, uint16_t channel,
                                             uint32_t pulse)
{
    (void)timer_periph;
    g_fake_gd32.channel_pulse[channel] = pulse;
}

void timer_channel_output_mode_config(uint32_t timer_periph, uint16_t channel,
                                      uint32_t ocmode)
{
    (void)timer_periph;
    g_fake_gd32.channel_mode[channel] = ocmode;
}

void timer_channel_output_shadow_config(uint32_t timer_periph, uint16_t channel,
                                        uint16_t ocshadow)
{
    (void)timer_periph;
    g_fake_gd32.channel_shadow[channel] = ocshadow;
}

void timer_break_struct_para_init(timer_break_parameter_struct *breakpara)
{
    volatile uint8_t *bytes = (volatile uint8_t *)breakpara;
    uint32_t i;

    for(i = 0U; i < (uint32_t)sizeof(*breakpara); i++) {
        bytes[i] = 0U;
    }
}

void timer_break_config(uint32_t timer_periph, timer_break_parameter_struct *breakpara)
{
    (void)timer_periph;
    g_fake_gd32.break_parameters = *breakpara;
    g_fake_gd32.break_configured = 1U;
    g_fake_gd32.primary_output_enabled = 0U;
}

void timer_primary_output_config(uint32_t timer_periph, ControlStatus newvalue)
{
    (void)timer_periph;
    g_fake_gd32.sequence++;
    if(ENABLE == newvalue) {
        if(0U == g_fake_gd32.timer_enabled) {
            g_fake_gd32.unsafe_start_order = 1U;
        }
        g_fake_gd32.primary_output_enabled = 1U;
        g_fake_gd32.primary_enable_sequence = g_fake_gd32.sequence;
    } else {
        g_fake_gd32.primary_output_enabled = 0U;
        g_fake_gd32.primary_disable_sequence = g_fake_gd32.sequence;
    }
}

void timer_counter_value_config(uint32_t timer_periph, uint16_t counter)
{
    (void)timer_periph;
    g_fake_gd32.counter = counter;
}

void timer_event_software_generate(uint32_t timer_periph, uint32_t event)
{
    (void)timer_periph;
    if(0U != (event & TIMER_EVENT_SRC_UPG)) {
        g_fake_gd32.update_generation_count++;
    }
}

void timer_update_event_enable(uint32_t timer_periph)
{
    (void)timer_periph;
    g_fake_gd32.update_events_enabled = 1U;
    g_fake_gd32.update_enable_count++;
}

void timer_update_event_disable(uint32_t timer_periph)
{
    (void)timer_periph;
    g_fake_gd32.update_events_enabled = 0U;
    g_fake_gd32.update_disable_count++;
}

void timer_enable(uint32_t timer_periph)
{
    (void)timer_periph;
    g_fake_gd32.sequence++;
    g_fake_gd32.timer_enabled = 1U;
    g_fake_gd32.timer_enable_sequence = g_fake_gd32.sequence;
}

void timer_disable(uint32_t timer_periph)
{
    (void)timer_periph;
    g_fake_gd32.sequence++;
    if(0U != g_fake_gd32.primary_output_enabled) {
        g_fake_gd32.unsafe_stop_order = 1U;
    }
    g_fake_gd32.timer_enabled = 0U;
    g_fake_gd32.timer_disable_sequence = g_fake_gd32.sequence;
}
