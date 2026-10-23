#ifndef MOTOR_OBSERVER_H
#define MOTOR_OBSERVER_H

#include "motor/foc_math.h"
#include <stdint.h>

typedef struct {
    /* angle/step 是电角；step 为每个 20 kHz 采样周期的角度增量。 */
    uint16_t angle;
    int32_t step;
    /* PLL 步长低通结果，供机械转速换算。 */
    int32_t speed_step;
    int32_t max_step;
    int16_t r_q15, l_q15, min_emf_q15;
    motor_ab_t prev_current;
    motor_ab_t emf;
    uint16_t lock_count;
    uint8_t locked;
} motor_observer_t;

void motor_observer_init(motor_observer_t *obs, int16_t r_q15, int16_t l_q15,
                         int16_t min_emf_q15, int32_t max_step);
void motor_observer_step(motor_observer_t *obs, motor_ab_t current,
                         motor_ab_t applied_voltage);
/* 未锁定或极对数为 0 时返回 0，防止速度环采纳开环预测值。 */
int32_t motor_observer_speed_rpm(const motor_observer_t *obs, uint32_t pole_pairs);

#endif
