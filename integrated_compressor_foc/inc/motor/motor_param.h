#ifndef MOTOR_PARAM_H
#define MOTOR_PARAM_H

/* 目标芯片 GD32C231C8T7 / LQFP48；电气参数必须由实际电机板决定。 */
#define MOTOR_MCU_MODEL                 "GD32C231C8T7"
#define MOTOR_PWM_HZ                    20000U
#define MOTOR_SPEED_DIVIDER             20U
#define MOTOR_ADC_VDDA_MV               3300U
/* 暂按两路低侧分流采样预留导通窗口；上板需按拓扑与波形重新确认。 */
#define MOTOR_DUTY_MIN_PERMILLE         250U
#define MOTOR_DUTY_MAX_PERMILLE         750U

/* 尚无原理图和压缩机参数，保持门极输出锁定。 */
#ifndef MOTOR_BOARD_READY
#define MOTOR_BOARD_READY              0U
#endif

/* 新压缩包弱磁模块：默认关闭，参数不得从测试值直接复制。 */
#ifndef MOTOR_ENABLE_FIELD_WEAKENING
#define MOTOR_ENABLE_FIELD_WEAKENING    0U
#endif
#ifndef MOTOR_FW_KP_A_PER_V
#define MOTOR_FW_KP_A_PER_V             0.0f
#endif
#ifndef MOTOR_FW_KI_A_PER_V_S
#define MOTOR_FW_KI_A_PER_V_S           0.0f
#endif
#ifndef MOTOR_FW_ID_MIN_A_PEAK
#define MOTOR_FW_ID_MIN_A_PEAK          0.0f /* 须填负的相峰值A，受退磁边界约束 */
#endif
#ifndef MOTOR_FW_VOLTAGE_MARGIN
#define MOTOR_FW_VOLTAGE_MARGIN         0.95f
#endif
#ifndef MOTOR_FW_ID_SLEW_A_PER_S
#define MOTOR_FW_ID_SLEW_A_PER_S         0.0f
#endif
#if (MOTOR_ENABLE_FIELD_WEAKENING != 0U) && (MOTOR_ENABLE_FIELD_WEAKENING != 1U)
#error "MOTOR_ENABLE_FIELD_WEAKENING must be 0 or 1"
#endif

/* 暂定 ADC 引脚，实际板上启用前必须核对；PA2/PA3 与 USART1 冲突。 */
#define MOTOR_IA_ADC_CHANNEL           1U  /* PA1 / ADC_IN1 */
#define MOTOR_IB_ADC_CHANNEL           2U  /* PA2 / ADC_IN2 */
#define MOTOR_BUS_ADC_CHANNEL          3U  /* PA3 / ADC_IN3 */
#define MOTOR_IA_GPIO_PORT             GPIOA
#define MOTOR_IA_GPIO_PIN              GPIO_PIN_1
#define MOTOR_IA_GPIO_RCU              RCU_GPIOA
#define MOTOR_IB_GPIO_PORT             GPIOA
#define MOTOR_IB_GPIO_PIN              GPIO_PIN_2
#define MOTOR_IB_GPIO_RCU              RCU_GPIOA
#define MOTOR_BUS_GPIO_PORT            GPIOA
#define MOTOR_BUS_GPIO_PIN             GPIO_PIN_3
#define MOTOR_BUS_GPIO_RCU             RCU_GPIOA

/* 填写实际测量/器件参数后，再将 MOTOR_BOARD_READY 设为 1U。 */
#ifndef MOTOR_CURRENT_FULL_SCALE_MA
#define MOTOR_CURRENT_FULL_SCALE_MA    0U  /* signed ADC half-range current */
#endif
#ifndef MOTOR_BUS_FULL_SCALE_MV
#define MOTOR_BUS_FULL_SCALE_MV        0U  /* bus voltage at ADC code 4095 */
#endif
#ifndef MOTOR_POLE_PAIRS
#define MOTOR_POLE_PAIRS               0U
#endif
#ifndef MOTOR_MAX_SPEED_RPM
#define MOTOR_MAX_SPEED_RPM            0U
#endif
#ifndef MOTOR_START_SPEED_RPM
#define MOTOR_START_SPEED_RPM          0U
#endif
#ifndef MOTOR_OIL_SPEED_RPM
#define MOTOR_OIL_SPEED_RPM            0U
#endif
#ifndef MOTOR_MIN_BUS_MV
#define MOTOR_MIN_BUS_MV               0U
#endif
#ifndef MOTOR_MAX_BUS_MV
#define MOTOR_MAX_BUS_MV               0U
#endif
#ifndef MOTOR_MAX_CURRENT_MA
#define MOTOR_MAX_CURRENT_MA           0U
#endif
#ifndef MOTOR_STALL_SPEED_RPM
#define MOTOR_STALL_SPEED_RPM          0U
#endif

/* 以下系数为有符号 Q15，须在限流台架上整定。 */
#ifndef MOTOR_CURRENT_KP_Q15
#define MOTOR_CURRENT_KP_Q15           0
#endif
#ifndef MOTOR_CURRENT_KI_Q15
#define MOTOR_CURRENT_KI_Q15           0
#endif
#ifndef MOTOR_SPEED_KP_Q15
#define MOTOR_SPEED_KP_Q15             0
#endif
#ifndef MOTOR_SPEED_KI_Q15
#define MOTOR_SPEED_KI_Q15             0
#endif
#ifndef MOTOR_OBSERVER_R_Q15
#define MOTOR_OBSERVER_R_Q15           0
#endif
#ifndef MOTOR_OBSERVER_L_Q15
#define MOTOR_OBSERVER_L_Q15           0
#endif
#ifndef MOTOR_OBSERVER_MIN_EMF_Q15
#define MOTOR_OBSERVER_MIN_EMF_Q15     0
#endif
#ifndef MOTOR_ALIGN_ID_Q15
#define MOTOR_ALIGN_ID_Q15             0
#endif
#ifndef MOTOR_START_IQ_Q15
#define MOTOR_START_IQ_Q15             0
#endif

#define MOTOR_ALIGN_MS                 500U
#define MOTOR_OPEN_LOOP_MS             2000U
#define MOTOR_OIL_INTERVAL_MS          3600000U
#define MOTOR_OIL_DURATION_MS          30000U
#define MOTOR_STALL_TIMEOUT_MS         1000U

#if (MOTOR_BOARD_READY != 0U) && (MOTOR_BOARD_READY != 1U)
#error "MOTOR_BOARD_READY must be 0 or 1"
#endif
#if (MOTOR_DUTY_MIN_PERMILLE >= MOTOR_DUTY_MAX_PERMILLE) || \
    (MOTOR_DUTY_MAX_PERMILLE > 1000U)
#error "Invalid motor duty window"
#endif
#if MOTOR_BOARD_READY
#if (MOTOR_CURRENT_FULL_SCALE_MA == 0U) || (MOTOR_BUS_FULL_SCALE_MV == 0U) || \
    (MOTOR_POLE_PAIRS == 0U) || (MOTOR_MAX_SPEED_RPM == 0U) || \
    (MOTOR_MIN_BUS_MV == 0U) || (MOTOR_MAX_BUS_MV == 0U) || \
    (MOTOR_MAX_CURRENT_MA == 0U) || (MOTOR_START_SPEED_RPM == 0U) || \
    (MOTOR_OIL_SPEED_RPM == 0U) || (MOTOR_STALL_SPEED_RPM == 0U) || \
    (MOTOR_CURRENT_KP_Q15 == 0) || (MOTOR_CURRENT_KI_Q15 == 0) || \
    (MOTOR_SPEED_KP_Q15 == 0) || (MOTOR_SPEED_KI_Q15 == 0) || \
    (MOTOR_OBSERVER_R_Q15 == 0) || (MOTOR_OBSERVER_L_Q15 == 0) || \
    (MOTOR_OBSERVER_MIN_EMF_Q15 == 0) || (MOTOR_ALIGN_ID_Q15 == 0) || \
    (MOTOR_START_IQ_Q15 == 0)
#error "Set all motor PCB, motor, protection and loop parameters before enabling"
#endif
#if (MOTOR_MAX_CURRENT_MA >= MOTOR_CURRENT_FULL_SCALE_MA) || \
    (MOTOR_START_SPEED_RPM > MOTOR_MAX_SPEED_RPM) || \
    (MOTOR_OIL_SPEED_RPM > MOTOR_MAX_SPEED_RPM) || \
    (MOTOR_MIN_BUS_MV >= MOTOR_MAX_BUS_MV) || \
    (MOTOR_MAX_BUS_MV >= MOTOR_BUS_FULL_SCALE_MV) || \
    (MOTOR_BUS_FULL_SCALE_MV > 1000000U)
#error "Motor protection and speed limits are inconsistent"
#endif
#if (MOTOR_CURRENT_KP_Q15 < 1) || (MOTOR_CURRENT_KP_Q15 > 32767) || \
    (MOTOR_CURRENT_KI_Q15 < 1) || (MOTOR_CURRENT_KI_Q15 > 32767) || \
    (MOTOR_SPEED_KP_Q15 < 1) || (MOTOR_SPEED_KP_Q15 > 32767) || \
    (MOTOR_SPEED_KI_Q15 < 1) || (MOTOR_SPEED_KI_Q15 > 32767) || \
    (MOTOR_OBSERVER_R_Q15 < 1) || (MOTOR_OBSERVER_R_Q15 > 32767) || \
    (MOTOR_OBSERVER_L_Q15 < 1) || (MOTOR_OBSERVER_L_Q15 > 32767) || \
    (MOTOR_OBSERVER_MIN_EMF_Q15 < 1) || \
    (MOTOR_OBSERVER_MIN_EMF_Q15 > 32767) || \
    (MOTOR_ALIGN_ID_Q15 < 1) || (MOTOR_ALIGN_ID_Q15 > 32767) || \
    (MOTOR_START_IQ_Q15 < 1) || (MOTOR_START_IQ_Q15 > 32767)
#error "Motor Q15 gains and current references must be positive and fit int16"
#endif
#if (MOTOR_IA_ADC_CHANNEL == MOTOR_IB_ADC_CHANNEL) || \
    (MOTOR_IA_ADC_CHANNEL == MOTOR_BUS_ADC_CHANNEL) || \
    (MOTOR_IB_ADC_CHANNEL == MOTOR_BUS_ADC_CHANNEL)
#error "Motor ADC channels must be distinct"
#endif
#endif

#endif
