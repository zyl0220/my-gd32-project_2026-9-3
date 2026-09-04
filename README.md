# GD32C231 量产工程模板

本工程面向 `GD32C231F8P6TR`。底层启动、芯片库和基础工程配置以 **GD32 资源管理器**为准；产品代码按应用、板级驱动、诊断和配置分层，避免业务继续堆积到 `main.c`。

## 第一次修改工程

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
│  └─ examples/            可独立启用的功能例程
├─ inc/
│  ├─ app/                 应用模块接口
│  ├─ bsp/                 板级模块接口及引脚映射
│  ├─ config/              产品集中配置
│  ├─ diagnostics/         诊断模块接口
│  └─ examples/            功能例程接口
├─ Firmware/               GD32 厂商库（资源管理器所有）
├─ gcc_startup/            启动文件（资源管理器所有）
├─ ldscripts/              链接脚本（资源管理器所有）
├─ docs/                   维护和量产文档
└─ build/                  构建产物，不提交版本库
```

## 应用入口

`main.c` 只执行三件事：配置 SysTick、调用 `app_init()`、循环调用 `app_process()`。SysTick 中断只调用 `app_tick_1ms_isr()` 更新时间，禁止在中断中延时、打印或执行复杂业务。

当前示例行为：LED1 每 500 ms 翻转；WAKEUP 按键经非阻塞消抖后切换 LED2。后续产品逻辑从 `src/app/app.c` 替换。

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
```

交付使用 `build/Release/*.hex` 或 `*.bin`，不要把 FactoryTest 或 FlashEepromDemo 镜像作为正式固件。Release 默认使用 `-Os -g1 -DNDEBUG` 并关闭启动日志；Debug 和例程构建保留日志。发版前还必须完成 `docs/PRODUCTION_CHECKLIST.md`。

## 进一步阅读

- `docs/ARCHITECTURE.md`：分层、依赖方向和中断规则
- `docs/CHANGE_GUIDE.md`：常见修改的标准步骤
- `docs/HARDWARE_RESOURCES.md`：引脚占用和已知冲突
- `docs/PRODUCTION_CHECKLIST.md`：量产发布检查项
- `docs/FLASH_EEPROM.md`：片内 Flash 模拟 EEPROM 使用说明
