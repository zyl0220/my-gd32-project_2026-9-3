/* ADC 码值换算：上电静止时测得零点；±半量程对应 ±Q15 满量程。 */
#include "motor/current_sample.h"

static int16_t adc_current_q15(uint16_t raw, uint16_t offset)
{
    int32_t scaled = ((int32_t)raw - offset) * 65534 / 4095;
    if (scaled > 32767) scaled = 32767;
    if (scaled < -32767) scaled = -32767;
    return (int16_t)scaled;
}

void motor_current_sample_init(motor_current_sample_t *sample,
                               uint16_t ia_offset, uint16_t ib_offset)
{
    sample->ia_offset = ia_offset;
    sample->ib_offset = ib_offset;
    sample->ia_q15 = 0;
    sample->ib_q15 = 0;
    sample->bus_mv = 0U;
}

void motor_current_sample_decode(motor_current_sample_t *sample,
                                 uint16_t ia_raw, uint16_t ib_raw,
                                 uint16_t bus_raw, uint32_t bus_full_scale_mv)
{
    sample->ia_q15 = adc_current_q15(ia_raw, sample->ia_offset);
    sample->ib_q15 = adc_current_q15(ib_raw, sample->ib_offset);
    sample->bus_mv = (uint32_t)bus_raw * bus_full_scale_mv / 4095U;
}
