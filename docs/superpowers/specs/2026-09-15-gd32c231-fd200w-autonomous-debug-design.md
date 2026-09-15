# GD32MC-FOC-FD200W C231 自主调试闭环设计

日期：2026-09-15
状态：已完成方案确认，等待文档审阅

## 1. 目标

为 GD32MC-FOC-FD200W C231 冰箱压缩机方案建立一条可重复、可审计的调试闭环：

```text
读代码 → 插入探针 → 构建 → 烧录 → 读取 UART/SWD
       → 分析 → 单点修改 → 再次验证
```

GD32 资源管理器及其配套工具链是工程生成、配置、构建、烧录和调试的唯一工具基础。自动化层复用资源管理器已经配置的 `mcudeva`、CMake、Ninja、ARM GCC/GDB、GD-Link、J-Link 和 OpenOCD，不再引入第二套工程模型。

本设计交付的是自主调试基础设施和安全验证顺序，不凭空补写电机参数、功率板引脚或完整 FOC 算法。准确的 FD200W 原始包、功率板资料和压缩机参数接入后，基础设施应能直接用于实板迭代。

## 2. 当前工程基线

当前仓库具备：

- `GD32C231F8P6TR`、Cortex-M23、64 KB Flash、12 KB SRAM 工程配置；
- CMake + Ninja + ARM GCC 构建链；
- TIMER0 三相互补 PWM、中心对齐、20 kHz 和死区配置；
- USART0、PB6/PB7、115200 8N1 输出通道；
- Debug、Release、FactoryTest 和 FlashEepromDemo 构建预设；
- 资源管理器生成的 SPL/CMSIS、启动文件、链接脚本和 `.project.json`。

当前仓库尚不具备：

- Clarke/Park、SVPWM、电流环、速度环和无位置传感器观测器；
- 相电流、母线电压、功率模块温度等采样链；
- 可用于闭环控制的 FD200W 板级引脚定义；
- 自动串口采集、SWD 快照读取和一次完整调试循环的主机脚本；
- 已连接且经过识别的探针和串口设备。

必须先解决的硬件一致性问题：`.project.json` 的目标为 20 引脚 `GD32C231F8P6TR`，而现有 PWM 示例使用 PA8/PA9/PA10、PA7/PB0/PB1。板级配置确认这些信号全部可达之前，禁止使能功率输出。

当前调试配置也存在明确冲突：活动配置声明 `probe: jlink`，`cfg` 却指向使用 CMSIS-DAP 的 OpenOCD 配置；同时 TIMER0 的 BREAK0/BREAK1 均未启用。前者必须在资源管理器中按实际探针修正，后者必须完成硬件故障链配置和实测，二者都属于带电测试的阻断条件。空的 `elfFile` 不单独视为错误，资源管理器会按活动配置推导 `build/<配置>/<工程名>.elf`，自动化层需要验证推导出的文件确实存在。

## 3. 工具和文件所有权

### 3.1 GD32 资源管理器所有

- `.project.json`；
- `Firmware/`、`gcc_startup/`、`ldscripts/`；
- `src/` 和 `inc/` 根目录的厂商入口及系统文件；
- 全局工具路径配置 `~/.mcudeva/config.json`。

自动化工具默认只读取这些文件。目标芯片、探针和基础工程选项的持久化修改通过 GD32 资源管理器完成，避免脚本静默改写生成器所有的配置。

### 3.2 项目团队所有

- `src/app/`、`inc/app/`；
- `src/bsp/`、`inc/bsp/`；
- `src/diagnostics/`、`inc/diagnostics/`；
- `inc/config/`；
- `tools/motor_debug/`；
- `debug/motor_debug.json`；
- `CMakeLists.txt` 和 `CMakePresets.json` 中明确标记的项目扩展；
- `docs/`。

资源管理器重新生成工程后，只恢复有明确标记的项目钩子和项目自有目录，不用旧厂商文件覆盖新文件。

## 4. 总体架构

```text
                         ┌──────────────────────────────┐
                         │ debug/motor_debug.json       │
                         │ 板型、后端选择、安全门限      │
                         └──────────────┬───────────────┘
                                        │
┌─────────────────┐     ┌───────────────▼──────────────┐
│ .project.json   │────▶│ tools/motor_debug 主控程序    │
│ CMakePresets    │     │ preflight / cycle / capture  │
│ mcudeva config  │────▶│ analyze / report             │
└─────────────────┘     └───────┬───────────┬──────────┘
                                │           │
                         build/flash     runtime feedback
                                │           │
                         ┌──────▼─────┐  ┌──▼───────────────┐
                         │ mcudeva    │  │ UART 主通道       │
                         │ CMake/GCC  │  │ SWD 故障/停机快照 │
                         └──────┬─────┘  └──┬───────────────┘
                                │           │
                                └─────┬─────┘
                                      ▼
                          artifacts/debug/<run-id>/
```

主机程序只负责确定性操作和证据采集。代码分析与修改由调试代理基于本次产物执行，每轮只做一个有明确假设的修改。

## 5. 主机侧设计

### 5.1 命令接口

主入口设计为：

```text
python -m tools.motor_debug preflight
python -m tools.motor_debug build
python -m tools.motor_debug flash
python -m tools.motor_debug capture
python -m tools.motor_debug swd-snapshot
python -m tools.motor_debug cycle
```

所有命令支持 `--config debug/motor_debug.json` 和 `--dry-run`。`cycle` 顺序执行预检、构建、烧录、启动观测、采集、规则分析和产物汇总；任一步失败即停止，不继续执行可能扩大风险的动作。

### 5.2 配置解析优先级

目标、探针和工具按以下顺序解析：

1. 当前命令行显式参数；
2. `debug/motor_debug.json` 的项目级设置；
3. `.project.json` 的活动调试配置；
4. 仅检测到一个探针时采用该探针。

工具路径来自 `~/.mcudeva/config.json`，不在项目中硬编码用户目录。多个探针同时连接且没有显式选择时直接报错，不能猜测。

### 5.3 预检

`preflight` 检查并输出机器可读报告：

- `.project.json` 可解析，目标芯片与板级配置一致；
- 活动配置、探针类型、调试接口和速度互相兼容；
- CMake、Ninja、ARM GCC/GDB、`mcudeva.exe` 和所选探针后端可执行；
- 构建预设存在，输出 ELF/HEX 的路径可确定；
- GD-Link/J-Link 扫描结果唯一且可访问；
- UART 端口能通过 VID/PID、USB 序列号或显式配置唯一确定；
- 板级引脚映射、电流采样、母线采样、硬件过流和 PWM BREAK 配置满足当前测试阶段；
- 高压测试需要的物理互锁和软件使能均已满足。

预检区分 `PASS`、`BLOCKED` 和 `FAIL`：未连接硬件属于 `BLOCKED`，配置矛盾或安全条件不满足属于 `FAIL`。

### 5.4 构建

- 普通 Debug/Release 构建优先调用资源管理器 CLI：`mcudeva build . --type <type>`；
- 项目增加 `MotorDebug` CMake 预设，用资源管理器已配置的 CMake、Ninja 和 ARM GCC 构建；
- `MotorDebug` 只增加观测能力，默认不使能功率级；
- 构建完成后必须验证 ELF、HEX、MAP、符号表和尺寸信息；
- 超出 Flash/RAM 阈值、缺少观测符号或出现新的编译告警时不得烧录。

### 5.5 烧录

烧录统一走资源管理器 CLI：

```text
mcudeva flash <hex> --probe <type> --device <mcu>
                --interface SWD --speed <kHz>
                --exe-path <resource-manager-tool> --reset
```

烧录前记录固件 SHA-256、Git 提交、工作区差异、编译器版本和探针序列号。烧录失败后不自动擦除整片；Flash EEPROM 保留区是否保留必须由具体烧录后端验证。

### 5.6 UART 采集

UART 是电机运行期间的主反馈通道：

- USART0，115200 8N1 作为当前默认值，由板级配置覆盖；
- 不能选择“第一个 COM 口”，必须唯一匹配设备身份；
- 固件 ISR 只更新内存快照，不调用 `printf()` 或阻塞发送；
- 主循环以低频率把快照送入非阻塞发送队列；
- 队列满时丢弃调试帧并累计丢帧计数，不能拖慢 FOC 中断；
- 固件发送紧凑的定长整数帧，包含协议版本、长度、序号和 CRC；
- 主机保存原始二进制，同时解码为 JSON Lines 供分析。

UART 超时不会立刻判定固件死亡：主机转入安全的 SWD 故障快照流程，区分串口链路异常、固件停机和 MCU 复位。

### 5.7 SWD 采集

SWD 用于启动失败、Fault、复位循环和停机后的状态读取：

- 使用资源管理器配置的 GD-Link/J-Link/OpenOCD GDB Server；
- 使用资源管理器配置的 `arm-none-eabi-gdb`；
- 从 ELF 符号解析观测块地址，不把 RAM 地址硬编码到主机脚本；
- 读取观测块前后校验序号，避免把更新一半的数据当成完整快照；
- 保存寄存器、调用栈、Fault 快照和指定变量；
- 默认只在功率输出关闭、故障锁存或 CPU 已停机时进行可能导致停顿的读取。

电机带载运行时，未经实际测量证明不会扰动实时控制，不允许周期性 halt/read/resume。此时只使用 UART；SWD 作为故障后的取证通道。

## 6. 固件观测层

新增独立的 diagnostics 模块，暴露一个稳定、带版本的观测结构：

```text
magic / abi_version / struct_size
sequence / heartbeat / uptime_ms / reset_cause
motor_state / requested_state / fault_bits
phase_current_raw[] / phase_current_mA[] / dc_bus_mV
power_stage_temp_cdeg / speed_target_rpm / speed_est_rpm
electrical_angle_q31 / duty_q15[] / control_loop_cycles
uart_drop_count / last_event / last_event_argument
fault_stack_frame / available_fault_status_registers
```

设计规则：

- 控制量使用明确量纲的整数或定点数，不在观测 ABI 中暴露编译器相关的浮点布局；
- `sequence` 在更新前变奇数、更新后变偶数，主机只接受两次读取一致的偶数快照；
- 观测块由链接器分配并导出符号，不占用 OpenOCD work area 的固定地址；
- 需要跨软件复位保留的 Fault 摘要放入单独的 `NOLOAD` 区，并带 CRC 与启动计数；
- HardFault 路径只复制已入栈的核心寄存器和芯片实际提供的故障状态寄存器，不执行格式化输出；
- 所有探针代码受 `PROJECT_ENABLE_MOTOR_DEBUG` 编译开关控制，Release 默认关闭。

## 7. 安全状态机

自主循环采用以下状态：

```text
IDLE
  → PREFLIGHT
  → BUILD_ONLY
  → FLASH_SAFE_IMAGE
  → OBSERVE_NO_POWER
  → READY_FOR_POWER_TEST
  → RUN_LIMITED_TEST
  → CAPTURE_AND_DISABLE
  → ANALYZE
  → IDLE

任意状态 → FAULT_LATCHED → PWM_OFF → CAPTURE_FAULT
```

安全约束：

1. 新构建和首次烧录一律为功率级关闭镜像。
2. TIMER0 BREAK、门极驱动故障输入、过流阈值、母线采样和电流零偏未验证前，不进入 `READY_FOR_POWER_TEST`。
3. 高压母线、压缩机和功率板接入后，需要物理互锁信号与软件一次性授权同时成立。
4. 心跳丢失、UART 控制超时、采样越界、观测器失锁或调试器异常均立即关闭 PWM 并锁存故障。
5. 自动化不会自行提高电流、速度、母线电压或测试时长；这些上限由已审阅的板级配置给出。
6. 每轮只允许一个代码假设和一个对应修改，修改前后使用相同测试条件比较。

## 8. 分析与迭代

规则分析器先处理确定性问题：

- 无心跳、持续复位、Fault PC/LR、栈水位异常；
- ADC 饱和、零偏漂移、相序或采样窗口异常；
- PWM 占空比越界、死区或 BREAK 未就绪；
- 电流环饱和、速度估算发散、观测器失锁；
- 串口丢帧和控制周期超时。

每轮生成一个假设，例如“V 相电流极性相反”，只修改对应的板级极性配置，再运行相同用例。若证据不能区分多个原因，分析结果应明确标记 `INCONCLUSIVE`，不能随机选择修改。

## 9. 运行产物

每次循环写入：

```text
artifacts/debug/<UTC时间>-<run-id>/
  manifest.json
  preflight.json
  build.log
  firmware.sha256
  flash.log
  uart.raw
  uart.jsonl
  swd-snapshot.json
  analysis.json
  source.diff
  summary.md
```

`manifest.json` 记录工具版本、板型、MCU、探针、固件、Git 状态、测试阶段和安全门限。产物目录默认不提交 Git，但报告必须能关联到源代码提交和工作区差异。

## 10. 错误处理

- 缺少工具：停止在预检并给出资源管理器配置字段；
- 无探针或无串口：报告 `BLOCKED`，不进入烧录或带电步骤；
- 配置冲突：列出冲突来源，不自动改写 `.project.json`；
- 构建失败：保留完整编译日志，不烧录旧固件；
- 烧录失败：不自动整片擦除，不启动串口测试；
- UART 无数据：先关闭功率输出，再尝试 SWD 故障快照；
- SWD 无法连接：保留 UART 和探针日志，禁止重复高速连接影响目标；
- 数据 CRC、版本或序号不一致：丢弃该帧并报告采集质量；
- 任意安全条件失败：锁定为 `PWM_OFF`，只能在原因清除后开始新一轮。

## 11. 验证策略

### 11.1 无硬件验证

- 用 mock 子进程覆盖构建、扫描、烧录和 GDB Server 的成功与失败路径；
- 用虚拟 UART 数据覆盖正常帧、CRC 错误、乱序、超时和复位循环；
- 用固定 ELF/符号夹具验证观测块定位；
- 验证每个失败路径都不会继续执行烧录或功率使能；
- 运行现有固件构建和主机侧单元测试。

### 11.2 无高压实板验证

- 资源管理器 CLI 扫描唯一探针；
- 构建并烧录安全镜像；
- UART 获得版本、心跳和复位原因；
- PWM 输出保持关闭；
- 停机状态下 SWD 读到与 UART 一致的观测序号和关键字段；
- 人为触发软件 Fault，验证复位后故障摘要仍可读取。

### 11.3 功率板与压缩机验证

只有在以下资料进入已审阅的板级配置后才能开始：

- FD200W 原始参考工程或与当前硬件版本对应的源代码包；
- 功率板原理图、板号、MCU 完整料号及全部引脚映射；
- 电流采样拓扑、采样电阻、放大倍数、ADC 基准和极性；
- 母线电压、温度和硬件过流/门极故障链参数；
- 压缩机型号、极对数、相电阻、相电感、反电动势或磁链、额定/峰值电流和允许速度范围；
- 供电、限流、隔离、急停和负载条件。

验证顺序固定为：静态 IO → ADC 零偏 → PWM 空载 → 低压限流开环 → 电流环 → 观测器 → 速度环 → 受限压缩机测试。不能跳级。

## 12. 完成标准

自主调试基础设施满足以下条件才算完成：

1. 单条 `cycle` 命令能从预检运行到报告生成；
2. 构建、烧录、UART 和 SWD 都使用 GD32 资源管理器已配置的工具；
3. 未接硬件时能通过 mock 测试，并准确报告真实硬件为 `BLOCKED`；
4. 接入探针和串口后能自动识别设备、烧录安全镜像并读取心跳；
5. UART 与停机 SWD 快照字段一致，Fault 可跨复位取证；
6. 配置冲突、构建失败、烧录失败和反馈丢失均不会误启功率级；
7. 每轮调试产物完整，可追溯到固件哈希、源代码状态和唯一测试条件；
8. 资源管理器重新生成工程后，项目自有调试层可按明确钩子恢复且测试仍通过。
