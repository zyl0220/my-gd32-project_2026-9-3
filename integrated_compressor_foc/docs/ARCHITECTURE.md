# 工程架构与职责

## 依赖方向

```text
main / IRQ  →  app  →  bsp          →  GD32 SPL/CMSIS
               ↘ motor → bsp/SPL   →  GD32 SPL/CMSIS
                    ↘ diagnostics   →  GD32 EVAL/SPL
```

依赖只能从左向右。BSP 和 diagnostics 不得反向调用应用层，应用层不得直接操作 GPIO 寄存器；确需新增硬件能力时，先在 BSP 建立小而明确的接口。日志、产测等非产品主逻辑放在 diagnostics。

## 文件所有权

### GD32 资源管理器所有

- `Firmware/`
- `gcc_startup/`
- `ldscripts/`
- `.project.json`
- `src/` 和 `inc/` 根部的厂商入口、异常处理、SysTick、EVAL 文件（其中 `main.c` 和 `gd32c2x1_it.c` 带有少量 `PROJECT HOOK`）

这些文件以资源管理器版本为准。升级或重新生成后，不从旧工程覆盖回来。

### 项目团队所有

- `src/app/`、`inc/app/`
- `src/bsp/`、`inc/bsp/`
- `src/diagnostics/`、`inc/diagnostics/`
- `src/examples/`、`inc/examples/`
- `src/motor/`、`inc/motor/`
- `inc/config/`
- `docs/`

团队代码应通过评审后修改。硬件差异集中在 BSP，产品差异集中在 config 和 app。

## 模块接口

应用模块只向入口和 SysTick 暴露三个函数：

- `app_init()`：上电后调用一次，完成应用所需初始化。
- `app_process()`：主循环持续调用，不得永久阻塞。
- `app_tick_1ms_isr()`：仅由 SysTick 调用，只更新时间或置标志。

这个接口是应用模块的维护和验证入口。复杂状态机放在 `app.c` 内部，不把内部状态暴露给 `main.c`。

## 中断规则

1. 普通外设 ISR 只清标志、保存必要数据、更新时间或置事件标志。电机模式的 ADC ISR 执行有时间上限的 20 kHz 电流环和每 20 次一次的速度环，需实测最坏执行时间。
2. ISR 内禁止 `printf()`、阻塞等待、毫秒延时和 EEPROM 擦写。
3. 与主循环共享的变量必须声明为 `volatile`；多字节一致性需要临界区保护。
4. 新增 ISR 前先检查启动文件和现有 BSP，禁止重复定义同名 Handler。
5. `bsp_exti.c` 已定义 `EXTI2_IRQHandler`，`bsp_timer.c` 已定义 `TIMER15_IRQHandler`。

## 重新生成工程

1. 先保存或提交团队所有目录。
2. 用 GD32 资源管理器更新芯片、厂商库和基础配置。
3. 不用旧版本覆盖新生成的厂商文件。
4. 按现有注释恢复 `main.c` 和 `gd32c2x1_it.c` 中的 `PROJECT HOOK`。
5. 确认 CMake 仍递归扫描 `src/*.c`，且包含 `inc/`。
6. 恢复 Release 参数、自有代码严格告警和例程预设（若生成器覆盖）。
7. 确认链接脚本仍为应用保留最后两个 1 KB Flash EEPROM 页。
8. 完整构建 Debug、Release、FactoryTest、FlashEepromDemo，并执行板级回归测试。
