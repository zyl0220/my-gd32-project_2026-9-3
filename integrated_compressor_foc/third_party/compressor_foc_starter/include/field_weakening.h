#ifndef FIELD_WEAKENING_H
#define FIELD_WEAKENING_H

#include <stdbool.h>

typedef struct {
    float kp_a_per_v;              /* Voltage-error P gain: A/V */
    float ki_a_per_v_s;            /* Voltage-error I gain: A/(V*s) */
    float current_limit_a_peak;    /* |Is| peak, NOT RMS */
    float id_min_a_peak;           /* Negative limit, e.g. -2.5 A */
    float voltage_margin;          /* (0,1], e.g. 0.95 of linear SVPWM */
    float id_slew_a_per_s;         /* Maximum Id reference ramp rate */
} FW_Config;

typedef struct {
    FW_Config cfg;
    float integral_a;
    float id_ref_a_peak;
    bool initialized;
} FW_Controller;

typedef struct {
    float vd_request_v;            /* UNCLIPPED dq current-PI voltage request */
    float vq_request_v;            /* Same scaling as vd */
    float vdc_v;
    float dt_s;
    bool closed_loop_locked;       /* Only enable in verified closed loop */
} FW_Input;

typedef struct {
    float id_ref_a_peak;
    float iq_abs_limit_a_peak;
    float voltage_request_v;
    float voltage_limit_v;
    bool valid;
} FW_Output;

bool FW_Init(FW_Controller *fw, FW_Config cfg);
void FW_Reset(FW_Controller *fw);
FW_Output FW_Update(FW_Controller *fw, FW_Input in);

#endif
