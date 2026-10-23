# GD32C231C8T7 电机控制

## 当前状态

工程的芯片型号已改为 GD32C231C8T7。Debug、Release 和 MotorControl 预设都从 `main.c → app.c → motor_app.c` 进入电机主框架。`MOTOR_BOARD_READY` 默认为 `0U`，因此 `motor_app_init()` 返回配置故障且不会初始化 PWM/ADC 或接管门极引脚。收到实际 PCB 和电机参数后，填写 `inc/motor/motor_param.h` 并将其设为 `1U` 才能开始板级调试。未知主板上的 LED、按键和串口不在电机主框架中初始化。

## 控制链路

```text
TIMER0 中心对齐互补 PWM，20 kHz
  └─ CH3 内部比较参考经 TRGO 触发 ADC 注入序列
       └─ Ia、Ib、母线电压各采样一次 → ADC_IRQHandler
            ├─ 电流/电压保护、Clarke/Park、d/q PI、反 Park、SVPWM
            └─ 每 20 次 ADC 中断运行一次速度环，即 1 kHz
```

ADC 时钟配置为 12 MHz，三个通道各使用 7.5 个采样周期和 12.5 个转换周期，理想转换时间合计约 5 µs。CH3 触发边沿位于载波峰值后约 1.04 µs，三相占空比暂限制在 25%–75%，为低侧分流采样预留导通窗口。该点是否落在实际采样有效窗口，必须用示波器结合门极死区、电流放大器建立时间及采样拓扑确认。采用注入序列的独立结果寄存器和完成中断；GD32C231 的 DMA 请求服务于常规序列，本路径不使用 DMA。

`motor_app.c` 负责停止、转子对齐、开环加速、闭环运行、周期上油与故障锁存。`motor_app_request_start(rpm)` 提交目标速度，`motor_app_request_stop()` 停止，`motor_app_clear_fault()` 只在停止命令后清故障。当前没有把 WAKEUP 键自动映射为压缩机启动命令，避免在目标转速未知时意外启动。SysTick 每 1 ms 检查 ADC 心跳，连续 3 ms 没有采样会关闭 PWM。

## 必须填写的板级参数

1. 核对 PA8/PA9/PA10、PA7/PB0/PB1 与门极驱动的连接、极性和 500 ns 死区。若不同，修改 `inc/bsp/bsp_timer0_pwm.h`。
2. 核对 Ia、Ib 和母线电压的 ADC 通道与 GPIO。当前 PA1/PA2/PA3 仅是暂定映射；PA2/PA3 与工程中的 USART1 功能冲突。修改 `motor_param.h` 中三个通道和 GPIO 定义。
3. `MOTOR_CURRENT_FULL_SCALE_MA` 是电流 ADC 相对零点偏移半量程（约 2048 码）对应的电流，须按分流电阻、放大倍数和 VDDA 计算。`MOTOR_BUS_FULL_SCALE_MV` 是 ADC 4095 码对应的真实母线电压，须按分压电阻计算。
4. 填写极对数、最大/启动/上油转速、电流和母线保护限值。对齐电流与启动电流用 Q15 表示，32767 对应 `MOTOR_CURRENT_FULL_SCALE_MA`。
5. 在限流电源和无负载台架上调电流 PI、速度 PI、反电动势估算器的 R/L 系数及最低有效反电动势。R 系数约为 `Rs × 电流满量程 / 标称母线电压 × 32768`；L 系数约为 `L × 电流满量程 × 20000 / 标称母线电压 × 32768`。这些公式只是初值，采样极性、相序、延迟和量纲须在实测中验证。

## 上板验证

- 首先保持 `MOTOR_BOARD_READY=0`，核对构建、芯片识别和门极全低。
- 填入参数后检查 ADC 零点校准、母线电压读数及三相 PWM 波形；确认 TRGO 在每个 PWM 周期只产生一次 ADC 中断。
- 在低压限流条件下依次验证过流、欠压、过压、缺失 ADC 中断、停机和故障锁存，再调试对齐、开环和位置估算器。压缩机上油的时间与转速是暂定软件参数，须按压缩机规格确认。
- 软件过流检测发生在 ADC 转换和 ISR 之后；功率级仍须配置独立的硬件快速过流关断路径。上板时测量电流 ISR 的最长执行时间，必须小于 50 µs 的控制周期并留出 ADC 与其他中断余量。

芯片型号、封装及引脚见 [GD32C231xx 数据手册](https://www.gd32mcu.com/data/documents/datasheet/GD32C231xx_Datasheet_Rev1.6.pdf)；ADC 触发、转换和 DMA 范围见 [GD32C2x1 用户手册](https://www.gd32mcu.com/data/documents/userManual/GD323C2x1_User_Manual_Rev1.1.pdf)；ADC EOC 读取延迟见 [GD32C231 芯片使用限制](https://www.gd32mcu.com/data/documents/errataSheet/Device%C2%A0limitations%C2%A0of%C2%A0GD32C231_Rev1.2.pdf)。
