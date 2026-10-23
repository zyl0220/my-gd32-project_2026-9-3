#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "field_weakening.h"

#define CHECK(x) do { if (!(x)) finish(__LINE__); } while (0)

static void finish(uint32_t result)
{
    *(volatile uint32_t *)0x20000000UL = result;
    __asm volatile("bkpt #0");
    for (;;) {}
}

static FW_Config bench_config(void)
{
    /* Synthetic values validate the algorithm; these are not motor settings. */
    FW_Config cfg = {0.05f, 0.5f, 5.0f, -2.5f, 0.95f, 20.0f};
    return cfg;
}

static FW_Input bench_input(void)
{
    FW_Input in = {0.0f, 120.0f, 300.0f, 0.001f, true};
    return in;
}

static void check_invalid_config(void)
{
    FW_Controller fw = {0};
    FW_Config cfg = bench_config();
    CHECK(!FW_Init(NULL, cfg));
    cfg.kp_a_per_v = -0.1f;
    CHECK(!FW_Init(&fw, cfg));
    cfg = bench_config(); cfg.ki_a_per_v_s = NAN;
    CHECK(!FW_Init(&fw, cfg));
    cfg = bench_config(); cfg.current_limit_a_peak = 0.0f;
    CHECK(!FW_Init(&fw, cfg));
    cfg = bench_config(); cfg.id_min_a_peak = 0.0f;
    CHECK(!FW_Init(&fw, cfg));
    cfg = bench_config(); cfg.id_min_a_peak = -5.1f;
    CHECK(!FW_Init(&fw, cfg));
    cfg = bench_config(); cfg.voltage_margin = 0.0f;
    CHECK(!FW_Init(&fw, cfg));
    cfg = bench_config(); cfg.voltage_margin = 1.01f;
    CHECK(!FW_Init(&fw, cfg));
    cfg = bench_config(); cfg.id_slew_a_per_s = INFINITY;
    CHECK(!FW_Init(&fw, cfg));
    cfg = bench_config(); cfg.id_slew_a_per_s = 0.0f;
    CHECK(!FW_Init(&fw, cfg));
}

static void check_sign_limits_release(void)
{
    FW_Controller fw = {0};
    FW_Config cfg = bench_config();
    FW_Input in = bench_input();
    FW_Output out;
    uint32_t index;
    float previous_id = 0.0f;
    CHECK(FW_Init(&fw, cfg));
    for (index = 0U; index < 100U; index++) {
        out = FW_Update(&fw, in);
        CHECK(out.valid && fabsf(out.id_ref_a_peak) < 0.000001f);
    }
    CHECK(fabsf(out.voltage_request_v - 120.0f) < 0.001f);
    /* 0.95 * 300 / sqrt(3) = 164.5448267 V phase peak. */
    CHECK(fabsf(out.voltage_limit_v - 164.54483f) < 0.001f);
    in.vq_request_v = 185.0f;
    for (index = 0U; index < 200U; index++) {
        out = FW_Update(&fw, in);
        CHECK(out.valid && out.id_ref_a_peak <= 0.0f);
        CHECK(out.id_ref_a_peak >= -2.500001f);
        CHECK(fabsf(out.id_ref_a_peak - previous_id) <= 0.020001f);
        CHECK(out.iq_abs_limit_a_peak >= 0.0f && out.iq_abs_limit_a_peak <= 5.0f);
        CHECK(out.id_ref_a_peak * out.id_ref_a_peak +
              out.iq_abs_limit_a_peak * out.iq_abs_limit_a_peak <= 25.00001f);
        previous_id = out.id_ref_a_peak;
    }
    CHECK(out.id_ref_a_peak < -0.1f);
    CHECK(out.id_ref_a_peak <= -2.49f);
    CHECK(out.iq_abs_limit_a_peak > 4.32f && out.iq_abs_limit_a_peak < 4.34f);
    /* Both voltage axes and the q-axis sign must contribute to magnitude. */
    FW_Reset(&fw);
    in.vd_request_v = 185.0f; in.vq_request_v = 0.0f;
    out = FW_Update(&fw, in);
    CHECK(out.id_ref_a_peak < 0.0f);
    FW_Reset(&fw);
    in.vd_request_v = 0.0f; in.vq_request_v = -185.0f;
    out = FW_Update(&fw, in);
    CHECK(out.id_ref_a_peak < 0.0f);
    in.vq_request_v = 185.0f;
    for (index = 0U; index < 200U; index++) out = FW_Update(&fw, in);
    in.vq_request_v = 110.0f;
    for (index = 0U; index < 2000U; index++) out = FW_Update(&fw, in);
    CHECK(out.valid && fabsf(out.id_ref_a_peak) < 0.0001f);
    CHECK(fabsf(out.iq_abs_limit_a_peak - 5.0f) < 0.0001f);
}

static void check_reset_and_invalid_input(void)
{
    FW_Controller fw = {0};
    FW_Config cfg = bench_config();
    FW_Input in = bench_input();
    FW_Output out;
    CHECK(!FW_Update(NULL, in).valid);
    CHECK(!FW_Update(&fw, in).valid);
    CHECK(FW_Init(&fw, cfg));
    in.vq_request_v = 185.0f;
    out = FW_Update(&fw, in);
    CHECK(out.id_ref_a_peak < 0.0f);
    in.closed_loop_locked = false;
    out = FW_Update(&fw, in);
    CHECK(out.valid && out.id_ref_a_peak == 0.0f);
    CHECK(out.iq_abs_limit_a_peak == 5.0f);
    CHECK(fw.integral_a == 0.0f && fw.id_ref_a_peak == 0.0f);
    in.closed_loop_locked = true;
    CHECK(FW_Update(&fw, in).id_ref_a_peak < 0.0f);
    FW_Reset(&fw);
    CHECK(fw.integral_a == 0.0f && fw.id_ref_a_peak == 0.0f);
    FW_Reset(NULL);
    CHECK(FW_Update(&fw, in).id_ref_a_peak < 0.0f);
    in.vdc_v = 0.0f;
    CHECK(!FW_Update(&fw, in).valid);
    CHECK(fw.integral_a == 0.0f && fw.id_ref_a_peak == 0.0f);
    in = bench_input(); in.vdc_v = -1.0f;
    CHECK(!FW_Update(&fw, in).valid);
    in = bench_input(); in.vd_request_v = NAN;
    CHECK(!FW_Update(&fw, in).valid);
    in = bench_input(); in.vq_request_v = INFINITY;
    CHECK(!FW_Update(&fw, in).valid);
    in = bench_input(); in.dt_s = 0.0f;
    CHECK(!FW_Update(&fw, in).valid);
    in = bench_input(); in.dt_s = 0.1001f;
    CHECK(!FW_Update(&fw, in).valid);
}

void test_entry(void)
{
    check_invalid_config();
    check_sign_limits_release();
    check_reset_and_invalid_input();
    finish(0U);
}
