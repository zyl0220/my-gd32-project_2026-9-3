#ifndef MOTOR_PI_H
#define MOTOR_PI_H

#include <stdint.h>

typedef struct {
    int16_t kp;
    int16_t ki;
    int16_t limit;
    int32_t integral;
} motor_pi_t;

void motor_pi_init(motor_pi_t *pi, int16_t kp, int16_t ki, int16_t limit);
int16_t motor_pi_step(motor_pi_t *pi, int16_t error);

/* 矢量限幅路径：先预览未饱和请求，再按实际施加电压提交条件积分。 */
int32_t motor_pi_request(const motor_pi_t *pi, int16_t error);
void motor_pi_track(motor_pi_t *pi, int16_t error, int16_t applied);

#endif
