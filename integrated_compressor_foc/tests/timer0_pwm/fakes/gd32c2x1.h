#ifndef TEST_FAKE_GD32C2X1_H
#define TEST_FAKE_GD32C2X1_H

#include <stdint.h>

typedef uint32_t rcu_periph_enum;
typedef uint32_t exti_line_enum;
typedef int32_t IRQn_Type;

typedef enum {
    DISABLE = 0,
    ENABLE = 1
} ControlStatus;

typedef enum {
    RESET = 0,
    SET = 1
} FlagStatus;

typedef struct {
    uint16_t prescaler;
    uint16_t alignedmode;
    uint16_t counterdirection;
    uint32_t period;
    uint16_t clockdivision;
    uint16_t repetitioncounter;
} timer_parameter_struct;

typedef struct {
    uint16_t runoffstate;
    uint16_t ideloffstate;
    uint16_t deadtime;
    uint16_t outputautostate;
    uint16_t protectmode;
    uint32_t break0state;
    uint32_t break0filter;
    uint32_t break0polarity;
    uint32_t break1state;
    uint32_t break1filter;
    uint32_t break1polarity;
} timer_break_parameter_struct;

typedef struct {
    uint16_t outputstate;
    uint16_t outputnstate;
    uint16_t ocpolarity;
    uint16_t ocnpolarity;
    uint16_t ocidlestate;
    uint16_t ocnidlestate;
} timer_oc_parameter_struct;

#define GPIOA                         0U
#define GPIOB                         1U
#define GPIOC                         2U
#define GPIOD                         3U
#define GPIOF                         4U

#define GPIO_PIN_0                    (1UL << 0U)
#define GPIO_PIN_1                    (1UL << 1U)
#define GPIO_PIN_2                    (1UL << 2U)
#define GPIO_PIN_3                    (1UL << 3U)
#define GPIO_PIN_4                    (1UL << 4U)
#define GPIO_PIN_5                    (1UL << 5U)
#define GPIO_PIN_6                    (1UL << 6U)
#define GPIO_PIN_7                    (1UL << 7U)
#define GPIO_PIN_8                    (1UL << 8U)
#define GPIO_PIN_9                    (1UL << 9U)
#define GPIO_PIN_10                   (1UL << 10U)
#define GPIO_PIN_11                   (1UL << 11U)
#define GPIO_PIN_12                   (1UL << 12U)
#define GPIO_PIN_13                   (1UL << 13U)
#define GPIO_PIN_14                   (1UL << 14U)
#define GPIO_PIN_15                   (1UL << 15U)

#define GPIO_MODE_INPUT               0U
#define GPIO_MODE_OUTPUT              1U
#define GPIO_MODE_AF                  2U
#define GPIO_MODE_ANALOG              3U
#define GPIO_PUPD_NONE                0U
#define GPIO_PUPD_PULLUP              1U
#define GPIO_OTYPE_PP                 0U
#define GPIO_OSPEED_LEVEL_1           1U
#define GPIO_AF_0                     0U
#define GPIO_AF_1                     1U
#define GPIO_AF_2                     2U

#define RCU_GPIOA                     0x10U
#define RCU_GPIOB                     0x11U
#define RCU_GPIOD                     0x13U
#define RCU_TIMER0                    0x20U
#define RCU_USART0                    0x30U
#define RCU_SPI0                      0x40U

#define TIMER0                        0x100U
#define USART0                        0x200U
#define SPI0                          0x300U

#define TIMER_CH_0                    0U
#define TIMER_CH_1                    1U
#define TIMER_CH_2                    2U

#define TIMER_COUNTER_EDGE            0x0000U
#define TIMER_COUNTER_CENTER_BOTH     0x0060U
#define TIMER_COUNTER_UP              0x0000U
#define TIMER_CKDIV_DIV1              0x0000U

#define TIMER_CCX_ENABLE              0x0001U
#define TIMER_CCX_DISABLE             0x0000U
#define TIMER_CCXN_ENABLE             0x0004U
#define TIMER_CCXN_DISABLE            0x0000U
#define TIMER_OC_POLARITY_HIGH        0x0000U
#define TIMER_OCN_POLARITY_HIGH       0x0000U
#define TIMER_OC_IDLE_STATE_LOW       0x0000U
#define TIMER_OCN_IDLE_STATE_LOW      0x0000U
#define TIMER_OC_MODE_PWM0            0x0060U
#define TIMER_OC_SHADOW_ENABLE        0x0008U

#define TIMER_ROS_STATE_ENABLE        0x0800U
#define TIMER_IOS_STATE_ENABLE        0x0400U
#define TIMER_OUTAUTO_DISABLE         0x0000U
#define TIMER_CCHP_PROT_OFF           0x0000U
#define TIMER_BREAK0_DISABLE          0x0000U
#define TIMER_BREAK1_DISABLE          0x0000U
#define TIMER_BREAK0_POLARITY_LOW     0x0000U
#define TIMER_BREAK1_POLARITY_LOW     0x0000U

#define TIMER_EVENT_SRC_UPG           0x0001U

#define EXTI_0                        0U
#define EXTI_4                        4U
#define EXTI_SOURCE_GPIOA             0U
#define EXTI_SOURCE_PIN0              0U
#define EXTI_SOURCE_PIN4              4U
#define EXTI0_IRQn                    0
#define EXTI4_IRQn                    4

#define SPI_CK_PL_LOW_PH_1EDGE        0U
#define SPI_PSC_32                    5U

void rcu_periph_clock_enable(rcu_periph_enum periph);
void gpio_bit_reset(uint32_t gpio_periph, uint32_t pin);
void gpio_mode_set(uint32_t gpio_periph, uint32_t mode, uint32_t pull_up_down, uint32_t pin);
void gpio_output_options_set(uint32_t gpio_periph, uint8_t otype, uint32_t speed, uint32_t pin);
void gpio_af_set(uint32_t gpio_periph, uint32_t alt_func_num, uint32_t pin);

void timer_deinit(uint32_t timer_periph);
void timer_struct_para_init(timer_parameter_struct *initpara);
void timer_init(uint32_t timer_periph, timer_parameter_struct *initpara);
void timer_channel_output_struct_para_init(timer_oc_parameter_struct *ocpara);
void timer_channel_output_config(uint32_t timer_periph, uint16_t channel,
                                 timer_oc_parameter_struct *ocpara);
void timer_channel_output_pulse_value_config(uint32_t timer_periph, uint16_t channel,
                                             uint32_t pulse);
void timer_channel_output_mode_config(uint32_t timer_periph, uint16_t channel,
                                      uint32_t ocmode);
void timer_channel_output_shadow_config(uint32_t timer_periph, uint16_t channel,
                                        uint16_t ocshadow);
void timer_break_struct_para_init(timer_break_parameter_struct *breakpara);
void timer_break_config(uint32_t timer_periph, timer_break_parameter_struct *breakpara);
void timer_primary_output_config(uint32_t timer_periph, ControlStatus newvalue);
void timer_counter_value_config(uint32_t timer_periph, uint16_t counter);
void timer_event_software_generate(uint32_t timer_periph, uint32_t event);
void timer_update_event_enable(uint32_t timer_periph);
void timer_update_event_disable(uint32_t timer_periph);
void timer_enable(uint32_t timer_periph);
void timer_disable(uint32_t timer_periph);

#endif /* TEST_FAKE_GD32C2X1_H */
