#ifndef MOTOR_APP_H
#define MOTOR_APP_H

#include "motor/adc_drv.h"
#include "motor/protection.h"
#include <stdint.h>

typedef enum {
    MOTOR_STATE_STOPPED = 0,
    MOTOR_STATE_ALIGN,
    MOTOR_STATE_OPEN_LOOP,
    MOTOR_STATE_RUN,
    MOTOR_STATE_OILING,
    MOTOR_STATE_FAULT
} motor_state_t;

/* 初始化时保持门极关闭；板级参数未填或 ADC 校准失败时返回 0。 */
uint8_t motor_app_init(void);
/* 主循环每个真实 1 ms 节拍调用，用于停机状态的母线预检查和启动。 */
void motor_app_process_1ms(void);
/* ADC 注入序列完成中断调用：20 kHz 电流环，分频 20 次为 1 kHz 速度环。 */
void motor_app_current_isr(motor_adc_raw_t raw);
/* SysTick 独立检测 ADC 心跳和停机命令。 */
void motor_app_watchdog_1ms_isr(void);
/* 正转目标转速；故障锁存时返回 0，不会自动重启。 */
uint8_t motor_app_request_start(uint32_t speed_rpm);
void motor_app_request_stop(void);
uint8_t motor_app_clear_fault(void);
motor_state_t motor_app_state(void);
motor_fault_t motor_app_fault(void);

#endif
