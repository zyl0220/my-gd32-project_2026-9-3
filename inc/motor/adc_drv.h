#ifndef MOTOR_ADC_DRV_H
#define MOTOR_ADC_DRV_H

#include <stdint.h>

typedef struct { uint16_t ia, ib, bus; } motor_adc_raw_t;

/* 电机模式独占 ADC，不可同时调用已有 bsp_adc_init()。 */
uint8_t motor_adc_drv_init(uint16_t *ia_offset, uint16_t *ib_offset);
/* 注入序列尚未切到 TIMER0 时，用于上电零点校准。 */
uint8_t motor_adc_drv_read_once(motor_adc_raw_t *sample);
/* 停机时常规单通道软件触发，先验证母线电压再启动 PWM。 */
uint8_t motor_adc_drv_read_bus_once(uint16_t *bus_raw);
void motor_adc_drv_arm_trigger(void);

#endif
