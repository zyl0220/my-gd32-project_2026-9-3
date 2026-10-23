/* 压缩包浮点弱磁与现有Q15 FOC之间的单位适配，不直接访问硬件。 */
#include "motor/field_weakening_adapter.h"
#include <math.h>
#include <stddef.h>

bool motor_fw_adapter_init(motor_fw_adapter_t *fw, FW_Config cfg,
                           uint32_t current_full_scale_ma,
                           uint16_t duty_min_permille, uint16_t duty_max_permille)
{
    uint16_t headroom;
    float current_limit_q15;
    if (fw == NULL) return false;
    *fw = (motor_fw_adapter_t){0};
    if (current_full_scale_ma == 0U || duty_min_permille >= 500U ||
        duty_max_permille <= 500U || duty_max_permille > 1000U)
        return false;
    fw->current_full_scale_a = (float)current_full_scale_ma * 0.001f;
    if (!isfinite(cfg.current_limit_a_peak) ||
        cfg.current_limit_a_peak > fw->current_full_scale_a ||
        !FW_Init(&fw->controller, cfg)) return false;
    headroom = 500U - duty_min_permille;
    if (duty_max_permille - 500U < headroom)
        headroom = duty_max_permille - 500U;
    /* 原FW使用Vdc/sqrt(3)。以有效母线折算采样窗口，而请求仍使用真实V。 */
    fw->usable_bus_ratio = (float)(2U * headroom) * 0.001f;
    fw->voltage_limit_q15 = (int16_t)(cfg.voltage_margin * fw->usable_bus_ratio *
                                     (32768.0f / 1.7320508075688772f));
    current_limit_q15 = cfg.current_limit_a_peak * 32767.0f /
                        fw->current_full_scale_a;
    if (fw->voltage_limit_q15 <= 0 || current_limit_q15 < 1.0f ||
        current_limit_q15 > 32767.0f) {
        fw->controller.initialized = false;
        return false;
    }
    fw->current_limit_q15 = (int16_t)current_limit_q15;
    motor_fw_adapter_reset(fw);
    return true;
}

void motor_fw_adapter_reset(motor_fw_adapter_t *fw)
{
    if (fw == NULL) return;
    FW_Reset(&fw->controller);
    fw->id_ref_q15 = 0;
    fw->iq_limit_q15 = fw->current_limit_q15;
}

bool motor_fw_adapter_update(motor_fw_adapter_t *fw, int32_t vd_q15, int32_t vq_q15,
                             uint32_t bus_mv, float dt_s, bool locked)
{
    FW_Input input;
    FW_Output output;
    int32_t remaining;
    if (fw == NULL) return false;
    input.vd_request_v = (float)vd_q15 * ((float)bus_mv / 32768000.0f);
    input.vq_request_v = (float)vq_q15 * ((float)bus_mv / 32768000.0f);
    input.vdc_v = (float)bus_mv * 0.001f * fw->usable_bus_ratio;
    input.dt_s = dt_s;
    input.closed_loop_locked = locked;
    output = FW_Update(&fw->controller, input);
    if (!output.valid || !isfinite(output.id_ref_a_peak) ||
        !isfinite(output.iq_abs_limit_a_peak)) {
        motor_fw_adapter_reset(fw);
        fw->iq_limit_q15 = 0;
        return false;
    }
    /* 向零截断Id、向下取Iq，量化后再次按整数电流圆收紧，避免浮点舍入越界。 */
    fw->id_ref_q15 = (int16_t)(output.id_ref_a_peak * 32767.0f /
                              fw->current_full_scale_a);
    if (fw->id_ref_q15 < -fw->current_limit_q15)
        fw->id_ref_q15 = (int16_t)-fw->current_limit_q15;
    if (fw->id_ref_q15 > 0) fw->id_ref_q15 = 0;
    remaining = (int32_t)fw->current_limit_q15 * fw->current_limit_q15 -
                 (int32_t)fw->id_ref_q15 * fw->id_ref_q15;
    if (remaining < 0) remaining = 0;
    fw->iq_limit_q15 = (int16_t)sqrtf((float)remaining);
    while ((int32_t)fw->iq_limit_q15 * fw->iq_limit_q15 > remaining)
        --fw->iq_limit_q15;
    return true;
}

int16_t motor_fw_adapter_limit_iq(const motor_fw_adapter_t *fw, int16_t iq_q15)
{
    if (iq_q15 > fw->iq_limit_q15) return fw->iq_limit_q15;
    if (iq_q15 < -fw->iq_limit_q15) return (int16_t)-fw->iq_limit_q15;
    return iq_q15;
}
