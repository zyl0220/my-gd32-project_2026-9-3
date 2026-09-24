# 工程阅读路线

当前工程目标芯片是 GD32C231C8T7。Debug/Release 默认进入电机控制主框架；板级参数未填时 `motor_app_init()` 返回配置故障，不接管任何未知主板引脚。

## 1. 先追踪启动和主循环

```text
复位向量 → gcc_startup/startup_gd32c231_221.S 的 Reset_Handler
          → 准备 C 运行环境，调用 SystemInit 配置时钟
          → src/main.c 的 main
              → systick_config：启动 1ms SysTick
              → app_init：初始化实际使用的模块
              → 循环调用 app_process

SysTick_Handler → app_tick_1ms_isr：累计节拍
                → delay_decrement：服务阻塞式 delay_1ms

app_process → 读取节拍快照 → app_process_1ms
                            → motor_app_process_1ms（电机主框架）

TIMER0 TRGO → ADC 注入序列 → ADC_IRQHandler
                              → motor_app_current_isr（20 kHz）
                              → 每 20 次调用速度环（1 kHz）
```

先看 [main.c](../src/main.c)，接着看 [app.c](../src/app/app.c)，再看 [中断入口](../src/gd32c2x1_it.c)。无需一开始就逐行研究启动汇编或厂商库。

`app_process_1ms()` 运行在主循环。它的名称表示每次处理一个软件节拍，不能把它当成独立定时中断。普通业务阻塞会推迟其他任务；长时间关闭中断还可能丢失 SysTick 次数。

`delay_1ms()` 会忙等 SysTick 更新计数器，既会阻塞主循环，也不能用于 SysTick 无法执行的上下文。应用里的周期动作优先采用累计节拍、条件满足后执行一次的方式。

## 2. 分清现有能力与当前启用功能

| 位置 | 阅读重点 | 当前用途 |
| --- | --- | --- |
| `inc/config/project_config.h` | 周期参数、功能宏、默认值 | 业务参数集中入口 |
| `src/motor/motor_app.c` | 启停、对齐、开环、闭环、上油、故障 | Debug/Release 电机主入口 |
| `src/motor/observer.c` | 反电动势位置和速度估算 | 20 kHz 电流中断 |
| `inc/motor/motor_param.h` | 板级引脚和电机参数 | 待实际 PCB 确认，默认禁止功率输出 |
| `src/bsp/bsp_led.c`、对应头文件 | 逻辑 LED 编号如何映射到引脚与电平 | 仅诊断示例 |
| `src/bsp/bsp_key.c`、对应头文件 | 连续稳定采样、DOWN/UP 事件 | 仅诊断示例 |
| `src/diagnostics/startup_log.c` | USART0 初始化、启动信息 | 仅诊断示例 |
| `src/diagnostics/auto_probe.c` | RAM 探针、外接 EEPROM 自检 | FactoryTest |
| `src/examples/flash_eeprom_demo.c` | 启动计数跨复位保存 | FlashEepromDemo |
| `src/bsp/bsp_flash_eeprom.c` | 片内 Flash 的日志快照 | 供应用主动调用 |
| `inc/bsp/bsp_*.h` | 引脚、单位、返回值、调用约束 | 查阅其他外设接口 |

其他 BSP 包含通用 GPIO、UART、I2C、SPI、PWM、ADC、定时器、EXTI、看门狗。必须根据业务显式调用初始化，文件存在不表示已经运行。引脚分配以 [硬件资源表](HARDWARE_RESOURCES.md) 和实际 PCB 为准；例如 LED4 和 PWM 共用 PB4。

`bsp_timer_callback()` 和 `bsp_exti_callback()` 在中断里调用，即使函数由应用提供，也应只做简短处理。对应 Handler 已定义在 BSP 中，不能再次定义同名中断函数。

注意两组容易混淆的模块：

- `bsp_i2c_eeprom` 访问外接 AT24C02；`bsp_flash_eeprom` 使用 MCU 片内 Flash，两者不是同一个存储区。
- 日志 `printf` 使用 EVAL 的 USART0；通用 `bsp_uart` 使用 USART1。接线和初始化应分别确认。

## 3. 沿着 Flash EEPROM 的数据流阅读

先读 [接口说明](../inc/bsp/bsp_flash_eeprom.h)，再看 [最小例程](../src/examples/flash_eeprom_demo.c)，最后进入 [驱动实现](../src/bsp/bsp_flash_eeprom.c)。

驱动里的术语：

| 名称 | 含义 |
| --- | --- |
| `offset` | 应用看到的 64 字节空间中的字节偏移，不是 Flash 地址 |
| `page` | 驱动逻辑页 0/1，对应芯片物理页 62/63 |
| `slot` | 页内第几条记录，当前为 0～10 |
| `payload` | 一次保存的完整 64 字节应用数据 |
| `sequence` | 快照序号，启动扫描用它判断先后 |
| `crc32` | 序号和 payload 的校验值 |
| `commit` | 最后写入的提交标记 |
| `g_flash_eeprom_cache` | 最近恢复或成功写入的快照在 RAM 中的副本 |

建议按以下顺序查看函数：

1. `flash_eeprom_record_t`：88 字节的记录布局；每页容纳 11 条。
2. `bsp_flash_eeprom_init()`：扫描两页全部记录，校验后选择最新记录，复制到 RAM。找不到有效记录返回 EMPTY；它不擦除 Flash。
3. `bsp_flash_eeprom_read()`：检查参数后复制 RAM 缓存，没有再次访问 Flash 验证持久性。
4. `bsp_flash_eeprom_write()`：跳过未变化数据；合并局部修改成完整快照，选择空槽或切换页。
5. `flash_eeprom_program_record()`：先写记录主体并回读比对，再写 commit，最后完整校验。
6. `flash_eeprom_erase_page()`：通过 FMC 按物理页号擦除，并检查擦除结果。

正常从空白存储连续更新时，页0写序号1～11，随后擦页1并写12～22，再擦页0继续。回收目标页时，最新记录仍在另一个页；成功写入后才更新活动页和 RAM 缓存。

该设计用于拒绝未完成的记录，并不保证突然断电能保存最后一次修改。还没调用 write 的 RAM 数据不会自动持久化，真实低压/中断擦除行为需要实板验证。`format()` 是主动清空两个页，不要在每次保存或开机时调用。

应用格式和驱动格式也要区分：例程结构体里的 magic/version 属于 payload；驱动记录另有自己的 magic/version。前者检查“是不是本应用认识的数据”，后者检查“是不是驱动认识的记录”。

## 4. 了解构建配置和文件产物

配置链是 `CMakePresets.json → CMakeLists.txt → 编译宏 → project_config.h/app.c`。

| 预设 | 默认行为 |
| --- | --- |
| Debug | 电机主框架、功率输出锁定、关闭启动日志 |
| Release | 电机主框架、功率输出锁定、关闭启动日志 |
| MotorControl | 独立目录中的电机 Release 构建 |
| FactoryTest | Debug 基础上启用外接 EEPROM 自检和周期诊断 |
| FlashEepromDemo | Debug 基础上启用片内 Flash 启动计数 |

构建时 `src` 下的 C 文件会被递归收集，链接时未引用的节可以被剔除。`*.elf` 用于调试，`*.hex/*.bin` 用于烧录，`*.map` 用于查看内存分配，`*.lst` 用于查看源码与反汇编。

链接脚本将应用 Flash 限制在前 62KB，末尾 2KB 留给模拟 EEPROM。这个限制防止正常链接把应用放进去，但不能阻止烧录器整片擦除。详见 [Flash EEPROM 使用说明](FLASH_EEPROM.md)。

## 5. 主板到位后接入的位置

先核对三相 PWM、两路电流采样、母线采样和硬件过流关断引脚，再填写 `inc/motor/motor_param.h`。主控命令可调用 `motor_app_request_start(rpm)`、`motor_app_request_stop()` 和 `motor_app_clear_fault()`；未知的通信、按键或温度传感器应在 `src/bsp/` 增加板级接口，由 `src/app/` 在主循环调度。不要在 ADC 中断里等待通信或写 Flash。

学习主框架时先在 Debug 中观察 `g_app_tick_ms`、`g_processed_tick_ms`、`g_state` 和 `g_fault`。板级引脚和电机参数都未确认前，`g_fault` 保持配置故障是预期行为。
