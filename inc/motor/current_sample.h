#ifndef MOTOR_CURRENT_SAMPLE_H
#define MOTOR_CURRENT_SAMPLE_H

#include <stdint.h>

typedef struct {
    uint16_t ia_offset, ib_offset;
    int16_t ia_q15, ib_q15;
    uint32_t bus_mv;
} motor_current_sample_t;

void motor_current_sample_init(motor_current_sample_t *sample,
                               uint16_t ia_offset, uint16_t ib_offset);
void motor_current_sample_decode(motor_current_sample_t *sample,
                                 uint16_t ia_raw, uint16_t ib_raw,
                                 uint16_t bus_raw, uint32_t bus_full_scale_mv);

#endif
