#ifndef MOTOR_PROFILE_H
#define MOTOR_PROFILE_H

/* Fill from motor vendor specification / validated identification.
 * DO NOT run a compressor with unknown values or default zeros.
 * Phase current conventions must match the FOC (PEAK amperes here).
 */
typedef struct {
    unsigned pole_pairs;
    float rs_phase_ohm_25c;
    float ld_h;
    float lq_h;
    float psi_pm_v_s;        /* phase-peak PM flux linkage, Wb = V*s */
    float max_phase_current_peak_a;
    float min_mechanical_rpm;
    float max_mechanical_rpm;
    float min_vdc_v;
    float max_vdc_v;
} MotorProfile;

/* EXAMPLE ONLY: 0 = deliberately UNCONFIGURED, not safe to run */
static const MotorProfile COMPRESSOR_PROFILE = {
    .pole_pairs = 0,
    .rs_phase_ohm_25c = 0,
    .ld_h = 0,
    .lq_h = 0,
    .psi_pm_v_s = 0,
    .max_phase_current_peak_a = 0,
    .min_mechanical_rpm = 0,
    .max_mechanical_rpm = 0,
    .min_vdc_v = 0,
    .max_vdc_v = 0
};

#endif
