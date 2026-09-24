# GD32C231C8T7 电机控制主框架

本工程面向 `GD32C231C8T7`（LQFP48、64 KB Flash、12 KB RAM）。目前搭建的是无主板条件下可编译的电机控制主框架；板级引脚与参数尚未确认，功率输出保持锁定。底层启动、芯片库和基础工程配置以 **GD32 资源管理器**为准；产品代码按应用、板级驱动、诊断和配置分层。

## 第一次修改工程

第一次阅读建议先看 [工程阅读路线](docs/CODE_WALKTHROUGH.md)，按“启动 → 1 ms 调度 → 电机状态机 → ADC 电流中断 → 速度观测器”的顺序追踪代码。关键入口和控制流程已补充中文注释。

| 要修改的内容 | 文件位置 |
| --- | --- |
| 产品业务、状态机、任务调度 | `src/app/app.c` |
| 产品版本、周期参数、功能开关 | `inc/config/project_config.h` |
| LED、按键、串口等硬件引脚 | 对应的 `inc/bsp/bsp_*.h` |
| 板级外设实现 | `src/bsp/bsp_*.c` |
| 工厂自检和自动化探针 | `src/diagnostics/auto_probe.c` |
| 芯片、编译器和下载配置 | GD32 资源管理器 / `.project.json` |

一般业务需求只修改前三项。不要直接修改 `Firmware/`、`gcc_startup/` 和 `ldscripts/`；更换芯片或升级固件库时应通过 GD32 资源管理器重新生成。

## 目录职责

```text
.
├─ src/                     GD32 入口文件及自有实现
│  ├─ app/                 产品业务（主要修改区）
│  ├─ bsp/                 板卡硬件实现
│  ├─ diagnostics/         工厂测试/诊断实现
│  ├─ motor/               压缩机控制、FOC、电流采样与保护
│  └─ examples/            可独立启用的功能例程
├─ inc/
│  ├─ app/                 应用模块接口
│  ├─ bsp/                 板级模块接口及引脚映射
│  ├─ config/              产品集中配置
│  ├─ diagnostics/         诊断模块接口
│  ├─ motor/               电机控制接口与参数
│  └─ examples/            功能例程接口
├─ Firmware/               GD32 厂商库（资源管理器所有）
├─ gcc_startup/            启动文件（资源管理器所有）
├─ ldscripts/              链接脚本（资源管理器所有）
├─ docs/                   维护和量产文档
└─ build/                  构建产物，不提交版本库
```

## 应用入口

`main.c` 只执行三件事：配置 SysTick、调用 `app_init()`、循环调用 `app_process()`。SysTick 中断调用 `app_tick_1ms_isr()` 更新时间并检查 ADC 采样心跳。中断中禁止延时、打印或执行 Flash 操作。

Debug/Release 默认接入电机主框架。尚未取得主板资料，`MOTOR_BOARD_READY=0U`，不会初始化 PWM/ADC，也不接管 LED、按键和日志串口引脚。LED/按键示例仅保留在 FactoryTest、FlashEepromDemo 等独立构建中。后续板级参数集中填入 `inc/motor/motor_param.h`。

## 编译

```powershell
cmake --preset Debug
cmake --build --preset Debug --parallel

cmake --preset Release
cmake --build --preset Release --parallel

# 工厂自检镜像（需要板载 AT24C02 和 USART0 接线）
cmake --preset FactoryTest
cmake --build --preset FactoryTest --parallel

# 片内 Flash 模拟 EEPROM 例程
cmake --preset FlashEepromDemo
cmake --build --preset FlashEepromDemo --parallel

# 单独的电机 Release 预设（与普通 Release 同样接入电机主框架）
cmake --preset MotorControl
cmake --build --preset MotorControl --parallel
```

Debug/Release/MotorControl 在板级参数未确认前都只是不可启动功率输出的主框架，不能作为可运行的压缩机固件交付。Release 使用 `-Os -g1 -DNDEBUG`；电机构建不启用启动日志，诊断例程仍可保留日志。上板前必须完成 `docs/MOTOR_CONTROL.md` 的参数核对和调试步骤。

## 进一步阅读

- `docs/ARCHITECTURE.md`：分层、依赖方向和中断规则
- `docs/CHANGE_GUIDE.md`：常见修改的标准步骤
- `docs/HARDWARE_RESOURCES.md`：引脚占用和已知冲突
- `docs/PRODUCTION_CHECKLIST.md`：量产发布检查项
- `docs/FLASH_EEPROM.md`：片内 Flash 模拟 EEPROM 使用说明
- `docs/MOTOR_CONTROL.md`：电机控制链路、板级参数与调试步骤
