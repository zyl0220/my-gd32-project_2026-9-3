#ifndef TEST_FAKE_GD32_STATE_H
#define TEST_FAKE_GD32_STATE_H

#include "gd32c2x1.h"

#define FAKE_GPIO_PORT_COUNT 5U
#define FAKE_GPIO_PIN_COUNT  16U
#define FAKE_TIMER_CHANNELS  3U

typedef struct {
    uint32_t gpio_mode[FAKE_GPIO_PORT_COUNT][FAKE_GPIO_PIN_COUNT];
    uint32_t gpio_pull[FAKE_GPIO_PORT_COUNT][FAKE_GPIO_PIN_COUNT];
    uint32_t gpio_af[FAKE_GPIO_PORT_COUNT][FAKE_GPIO_PIN_COUNT];
    uint32_t gpio_output_latch[FAKE_GPIO_PORT_COUNT];
    uint32_t gpio_clock_a_enabled;
    uint32_t gpio_clock_b_enabled;
    uint32_t timer_clock_enabled;

    timer_parameter_struct timer_parameters;
    timer_oc_parameter_struct channel_parameters[FAKE_TIMER_CHANNELS];
    timer_break_parameter_struct break_parameters;
    uint32_t channel_configured_mask;
    uint32_t channel_pulse[FAKE_TIMER_CHANNELS];
    uint32_t channel_mode[FAKE_TIMER_CHANNELS];
    uint32_t channel_shadow[FAKE_TIMER_CHANNELS];
    uint32_t timer_initialized;
    uint32_t break_configured;
    uint32_t timer_enabled;
    uint32_t primary_output_enabled;
    uint32_t counter;
    uint32_t update_events_enabled;
    uint32_t update_disable_count;
    uint32_t update_enable_count;
    uint32_t update_generation_count;

    uint32_t sequence;
    uint32_t timer_enable_sequence;
    uint32_t primary_enable_sequence;
    uint32_t primary_disable_sequence;
    uint32_t timer_disable_sequence;
    uint32_t unsafe_gpio_transition;
    uint32_t unsafe_start_order;
    uint32_t unsafe_stop_order;
} fake_gd32_state_t;

extern fake_gd32_state_t g_fake_gd32;

void fake_gd32_reset(void);

#endif /* TEST_FAKE_GD32_STATE_H */
