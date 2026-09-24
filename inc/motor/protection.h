#ifndef MOTOR_PROTECTION_H
#define MOTOR_PROTECTION_H

#include <stdint.h>

typedef enum {
    MOTOR_FAULT_NONE = 0,
    MOTOR_FAULT_ADC,
    MOTOR_FAULT_OVERCURRENT,
    MOTOR_FAULT_UNDERVOLTAGE,
    MOTOR_FAULT_OVERVOLTAGE,
    MOTOR_FAULT_STALL,
    MOTOR_FAULT_LOST_POSITION,
    MOTOR_FAULT_PWM,
    MOTOR_FAULT_CONFIG
} motor_fault_t;

typedef struct {
    motor_fault_t fault;
    uint16_t stall_ms;
} motor_protection_t;

void motor_protection_init(motor_protection_t *protection);
motor_fault_t motor_protection_sample(motor_protection_t *protection,
                                      uint16_t ia_raw, uint16_t ib_raw,
                                      uint16_t bus_raw, int16_t ia_q15,
                                      int16_t ib_q15, uint32_t bus_mv,
                                      uint32_t min_bus_mv, uint32_t max_bus_mv,
                                      int16_t current_limit_q15);
motor_fault_t motor_protection_speed(motor_protection_t *protection,
                                     int32_t target_rpm, int32_t measured_rpm,
                                     uint32_t stall_speed_rpm,
                                     uint16_t timeout_ms);
void motor_protection_trip(motor_protection_t *protection, motor_fault_t fault);

#endif
