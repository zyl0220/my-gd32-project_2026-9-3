#include "field_weakening.h"
#include <math.h>
#include <stddef.h>

static float clampf(float x, float lo, float hi) {
    return fmaxf(lo, fminf(hi, x));
}

bool FW_Init(FW_Controller *fw, FW_Config cfg) {
    if (fw == NULL || !isfinite(cfg.kp_a_per_v) || cfg.kp_a_per_v < 0.0f ||
        !isfinite(cfg.ki_a_per_v_s) || cfg.ki_a_per_v_s < 0.0f ||
        !isfinite(cfg.current_limit_a_peak) || cfg.current_limit_a_peak <= 0.0f ||
        !isfinite(cfg.id_min_a_peak) || cfg.id_min_a_peak >= 0.0f ||
        -cfg.id_min_a_peak > cfg.current_limit_a_peak ||
        !isfinite(cfg.voltage_margin) || cfg.voltage_margin <= 0.0f ||
        cfg.voltage_margin > 1.0f || !isfinite(cfg.id_slew_a_per_s) ||
        cfg.id_slew_a_per_s <= 0.0f) return false;
    fw->cfg = cfg;
    fw->integral_a = 0.0f;
    fw->id_ref_a_peak = 0.0f;
    fw->initialized = true;
    return true;
}

void FW_Reset(FW_Controller *fw) {
    if (fw != NULL) {
        fw->integral_a = 0.0f;
        fw->id_ref_a_peak = 0.0f;
    }
}

FW_Output FW_Update(FW_Controller *fw, FW_Input in) {
    FW_Output out = {0};
    if (fw == NULL || !fw->initialized || !isfinite(in.vd_request_v) ||
        !isfinite(in.vq_request_v) || !isfinite(in.vdc_v) || in.vdc_v <= 0.0f ||
        !isfinite(in.dt_s) || in.dt_s <= 0.0f || in.dt_s > 0.1f) {
        if (fw != NULL && fw->initialized) FW_Reset(fw);
        return out;  /* valid=false: caller MUST NOT energize inverter */
    }

    out.voltage_request_v = hypotf(in.vd_request_v, in.vq_request_v);
    /* Linear SVPWM: peak phase voltage-vector max Vdc/sqrt(3). */
    out.voltage_limit_v = (fw->cfg.voltage_margin / sqrtf(3.0f)) * in.vdc_v;
    out.valid = true;

    if (!in.closed_loop_locked) {
        FW_Reset(fw);
        out.iq_abs_limit_a_peak = fw->cfg.current_limit_a_peak;
        return out;
    }

    /* Positive error means the demanded voltage exceeds the allowed envelope. */
    float error_v = out.voltage_request_v - out.voltage_limit_v;
    float integral_candidate = clampf(
        fw->integral_a + fw->cfg.ki_a_per_v_s * error_v * in.dt_s,
        0.0f, -fw->cfg.id_min_a_peak);
    float magnitude_candidate = fw->cfg.kp_a_per_v * error_v + integral_candidate;
    float id_magnitude_max = -fw->cfg.id_min_a_peak;

    /* Conditional integration anti-windup at both ends. */
    if (!((magnitude_candidate >= id_magnitude_max && error_v > 0.0f) ||
          (magnitude_candidate <= 0.0f && error_v < 0.0f))) {
        fw->integral_a = integral_candidate;
    }
    float demand_magnitude = clampf(
        fw->cfg.kp_a_per_v * error_v + fw->integral_a,
        0.0f, id_magnitude_max);
    float target_id = -demand_magnitude;
    float max_step = fw->cfg.id_slew_a_per_s * in.dt_s;
    fw->id_ref_a_peak += clampf(target_id - fw->id_ref_a_peak,
                                -max_step, max_step);
    fw->id_ref_a_peak = clampf(fw->id_ref_a_peak, fw->cfg.id_min_a_peak, 0.0f);

    out.id_ref_a_peak = fw->id_ref_a_peak;
    float remaining = fw->cfg.current_limit_a_peak * fw->cfg.current_limit_a_peak -
                      fw->id_ref_a_peak * fw->id_ref_a_peak;
    out.iq_abs_limit_a_peak = sqrtf(fmaxf(0.0f, remaining));
    return out;
}
