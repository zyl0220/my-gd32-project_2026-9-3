# 硬件资源表

以下资源来自当前 BSP 配置，量产前必须与实际 PCB 原理图逐项核对。

| 模块 | 外设/引脚 | 用途 |
| --- | --- | --- |
| LED1 | PA15 | 状态灯 |
| LED2 | PD1 | 用户指示灯 |
| LED3 | PD3 | 预留指示灯 |
| LED4 | PB4 | 预留指示灯 |
| WAKEUP 按键 | PA0 | 高电平按下、下拉输入 |
| USER 按键 | PA4 | 高电平按下、下拉输入 |
| 调试串口 | USART0：PA9/PA10 | 启动日志和工厂自检，115200 8N1 |
| 通用串口 | USART1：PA2/PA3 | `bsp_uart`，115200 8N1 |
| 通用 I²C | I2C0：PB8/PB9 | 400 kHz 寄存器读写 |
| EEPROM | I2C1：PA11/PA12 | AT24C02，地址 `0xA0` |
| SPI | SPI0：PB3/PA6/PA7，CS=PA5 | 8 位主机、软件片选 |
| PWM | TIMER2_CH0：PB4 | 默认约 1 kHz、0–100% 占空比 |
| 周期定时器 | TIMER15 / TIMER15_IRQn | 微秒周期更新中断 |
| 外部中断 | PB2 / EXTI2_IRQn | 上升沿触发 |
| ADC | ADC13/ADC14 | 内部温度传感器、VREFINT |
| 看门狗 | FWDGT / IRC40K | 独立看门狗 |
| Flash EEPROM | 页 62、63：`0x0800F800–0x0800FFFF` | 保留 2 KB，提供 64 字节模拟 EEPROM |

## 已知冲突

- LED4 与 PWM 共用 PB4，不能同时作为 LED GPIO 和 TIMER2_CH0 使用。
- `bsp_exti.c` 自带 `EXTI2_IRQHandler`，不要在 `gd32c2x1_it.c` 重复定义。
- `bsp_timer.c` 自带 `TIMER15_IRQHandler`，不要在其他文件重复定义。
- 调试串口 USART0 与通用串口 USART1 是两套不同引脚，接线时不要混淆。
- 当前表以 GD32C231C-EVAL 映射为基础；自研板必须重新核对封装可用引脚和复用功能。
- 整片擦除会清除 Flash EEPROM；下载器、Bootloader 和升级流程必须明确保留或迁移这两个页。
