#ifndef MOTOR_FIELD_WEAKENING_ADAPTER_H
#define MOTOR_FIELD_WEAKENING_ADAPTER_H

#include "motor/field_weakening.h"
#include <stdint.h>

typedef struct {
    FW_Controller controller;
    float current_full_scale_a;
    float usable_bus_ratio;
    int16_t current_limit_q15;
    int16_t id_ref_q15;
    int16_t iq_limit_q15;
    int16_t voltage_limit_q15;
} motor_fw_adapter_t;

/* 电流为相峰值；电压Q15以真实Vdc为基准。占空窗按500‰中心保守折算。 */
bool motor_fw_adapter_init(motor_fw_adapter_t *fw, FW_Config cfg,
                           uint32_t current_full_scale_ma,
                           uint16_t duty_min_permille, uint16_t duty_max_permille);
void motor_fw_adapter_reset(motor_fw_adapter_t *fw);
/* 仅1kHz调用。vd/vq为限幅前PI请求；false时调用方必须关断功率输出。 */
bool motor_fw_adapter_update(motor_fw_adapter_t *fw, int32_t vd_q15, int32_t vq_q15,
                             uint32_t bus_mv, float dt_s, bool locked);
/* 20kHz路径只做定点夹限，兼容正负Iq。 */
int16_t motor_fw_adapter_limit_iq(const motor_fw_adapter_t *fw, int16_t iq_q15);

#endif
