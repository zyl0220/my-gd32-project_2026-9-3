#ifndef MOTOR_PWM_DRV_H
#define MOTOR_PWM_DRV_H

#include "motor/foc_math.h"
#include <stdint.h>

uint8_t motor_pwm_drv_init(void);
uint8_t motor_pwm_drv_start(void);
void motor_pwm_drv_stop(void);
uint8_t motor_pwm_drv_set(motor_duty_t duty);

#endif
