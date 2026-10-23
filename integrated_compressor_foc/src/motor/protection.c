/* 采样级过流/电压保护及 1 kHz 堵转检测。首个故障锁存到显式复位。 */
#include "motor/protection.h"

static int32_t absolute(int32_t x) { return x < 0 ? -x : x; }

void motor_protection_init(motor_protection_t *protection)
{
    protection->fault = MOTOR_FAULT_NONE;
    protection->stall_ms = 0U;
}

void motor_protection_trip(motor_protection_t *protection, motor_fault_t fault)
{
    if (protection->fault == MOTOR_FAULT_NONE && fault != MOTOR_FAULT_NONE)
        protection->fault = fault;
}

motor_fault_t motor_protection_sample(motor_protection_t *protection,
                                      uint16_t ia_raw, uint16_t ib_raw,
                                      uint16_t bus_raw, int16_t ia_q15,
                                      int16_t ib_q15, uint32_t bus_mv,
                                      uint32_t min_bus_mv, uint32_t max_bus_mv,
                                      int16_t current_limit_q15)
{
    if (ia_raw < 8U || ia_raw > 4087U || ib_raw < 8U || ib_raw > 4087U ||
        bus_raw < 8U || bus_raw > 4087U)
        motor_protection_trip(protection, MOTOR_FAULT_ADC);
    else if (absolute(ia_q15) > current_limit_q15 ||
             absolute(ib_q15) > current_limit_q15 ||
             absolute((int32_t)ia_q15 + ib_q15) > current_limit_q15)
        motor_protection_trip(protection, MOTOR_FAULT_OVERCURRENT);
    else if (bus_mv < min_bus_mv)
        motor_protection_trip(protection, MOTOR_FAULT_UNDERVOLTAGE);
    else if (bus_mv > max_bus_mv)
        motor_protection_trip(protection, MOTOR_FAULT_OVERVOLTAGE);
    return protection->fault;
}

motor_fault_t motor_protection_speed(motor_protection_t *protection,
                                     int32_t target_rpm, int32_t measured_rpm,
                                     uint32_t stall_speed_rpm,
                                     uint16_t timeout_ms)
{
    if (target_rpm > (int32_t)stall_speed_rpm &&
        measured_rpm < (int32_t)stall_speed_rpm) {
        if (protection->stall_ms < timeout_ms) protection->stall_ms++;
        if (protection->stall_ms >= timeout_ms)
            motor_protection_trip(protection, MOTOR_FAULT_STALL);
    } else {
        protection->stall_ms = 0U;
    }
    return protection->fault;
}
