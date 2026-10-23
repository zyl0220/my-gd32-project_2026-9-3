#include "field_weakening.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    FW_Controller controller = {0};
    /* Arbitrary bench-test values only; NOT compressor motor settings. */
    FW_Config cfg = {
        .kp_a_per_v = 0.05f,
        .ki_a_per_v_s = 0.5f,
        .current_limit_a_peak = 5.0f,
        .id_min_a_peak = -2.5f,
        .voltage_margin = 0.95f,
        .id_slew_a_per_s = 20.0f
    };
    assert(FW_Init(&controller, cfg));
    FW_Input in = {.vd_request_v = 0, .vq_request_v = 120,
                   .vdc_v = 300, .dt_s = 0.001f, .closed_loop_locked = true};
    FW_Output out;
    for (int i = 0; i < 100; ++i) out = FW_Update(&controller, in);
    assert(out.valid && fabsf(out.id_ref_a_peak) < 1e-6f);

    /* Beyond usable voltage: FW must command negative d-axis current. */
    in.vq_request_v = 185;
    for (int i = 0; i < 200; ++i) out = FW_Update(&controller, in);
    assert(out.valid && out.id_ref_a_peak < -0.1f);
    assert(out.id_ref_a_peak >= cfg.id_min_a_peak);
    assert(out.iq_abs_limit_a_peak <= cfg.current_limit_a_peak);
    assert(out.iq_abs_limit_a_peak >= 0.0f);
    printf("over_voltage: Id_ref=%.3f Apeak, |Iq|max=%.3f Apeak\n",
           out.id_ref_a_peak, out.iq_abs_limit_a_peak);

    /* Voltage headroom regained: FW should unwind to Id=0. */
    in.vq_request_v = 110;
    for (int i = 0; i < 2000; ++i) out = FW_Update(&controller, in);
    assert(out.valid && fabsf(out.id_ref_a_peak) < 1e-4f);

    /* Sensorless observer loss: disable FW immediately; main protection
       state machine must independently fault/stop the inverter. */
    in.closed_loop_locked = false;
    out = FW_Update(&controller, in);
    assert(out.valid && out.id_ref_a_peak == 0.0f);

    in.vdc_v = 0;
    out = FW_Update(&controller, in);
    assert(!out.valid);
    printf("PASS: FW sign, limits, release, and invalid-input tests\n");
    return 0;
}
