# 新压缩机弱磁模块整合说明

整合日期：2026-10-23。本文件夹是可独立构建的工程副本，父目录原工程保持原样。

## 已接入内容

来源：用户提供的 compressor_foc_starter (1).zip。SHA-256：`5b3b294101b2e45ae0711597b552b6c89c969dca9bd0c0b500552be6aa209ea9`。

| 文件 | 用途 |
| --- | --- |
| `src/motor/field_weakening.c`、`inc/motor/field_weakening.h` | 原包弱磁PI、Id斜率与电流圆；源文件仅调整include路径 |
| `src/motor/field_weakening_adapter.c`、对应头文件 | 真实V/Apeak与Q15、PWM余量、整数电流圆适配 |
| `src/motor/foc.c`、`pi.c`、`foc_math.c` | 未饱和PI请求、矢量电压限幅与条件积分 |
| `src/motor/motor_app.c` | 1kHz弱磁更新、20kHz参考限幅、停机/故障/失锁复位 |
| `inc/motor/motor_param.h` | 统一使能开关和待整定的弱磁参数 |
| `inc/motor/motor_profile.h` | 原包电机参数模板，全零，供收集资料；运行配置仍统一在motor_param.h |
| `third_party/compressor_foc_starter/` | 原ZIP全部7个文件原样保留，含指南、检查表和原测试 |

## 默认行为

`MOTOR_BOARD_READY=0U`，`MOTOR_ENABLE_FIELD_WEAKENING=0U`。Debug、Release、MotorControl镜像保持门极锁定。未填实际电机参数，不可驱动目标压缩机。FactoryTest和FlashEepromDemo保持各自用途。

启用弱磁后，仅RUN/OILING且观测器锁定时允许负Id；启动对齐和开环阶段不使用弱磁。每20次电流采样更新一次弱磁（1ms），新Id在下一电流周期生效。每个20kHz电流周期夹住正负Iq。失锁、停机和故障清除弱磁状态；无效弱磁输出先停机，故障周期不写PWM。重启恢复同一电压圆限幅。

## 单位与电压范围

- 相峰值电流：`I_A = I_q15 × MOTOR_CURRENT_FULL_SCALE_MA / (32767 × 1000)`。
- 相峰值电压：`V_dq = request_q15 × 实际Vdc / 32768`，弱磁使用PI限幅前的int32请求。
- PWM以500‰为中心，可用比例为 `2 × min(500-duty_min, duty_max-500) / 1000`。
- 默认250‰–750‰时比例0.5，向原算法传入“有效母线=真实Vdc×0.5”；请求电压仍用真实V。95%裕量下电压半径8986 Q15。
- d/q PI先形成未饱和请求，再做矢量限幅并按饱和方向决定是否积分；观测器使用限幅后的施加电压。
- Id量化后按整数总电流半径再次夹限；Iq向下取整并复核电流圆，覆盖float32和整数换算相差1LSB的边界。

## 待填写参数

先完成 `docs/MOTOR_CONTROL.md` 的主板和电机核对，再整定以下项目。总电流限值复用 `MOTOR_MAX_CURRENT_MA`。

| 参数 | 单位/要求 |
| --- | --- |
| `MOTOR_FW_KP_A_PER_V` | A/V，初始0，需整定 |
| `MOTOR_FW_KI_A_PER_V_S` | A/(V·s)，初始0，需整定 |
| `MOTOR_FW_ID_MIN_A_PEAK` | 负的相峰值A，绝对值不超过总电流限制，须核对退磁边界 |
| `MOTOR_FW_VOLTAGE_MARGIN` | (0,1]，默认0.95 |
| `MOTOR_FW_ID_SLEW_A_PER_S` | A/s，初始0，需整定 |

默认零Id下限/零斜率会被初始化拒绝。原包MotorProfile全零模板不写入运行配置。

## 构建与测试

在本文件夹运行：

```powershell
cmake --preset Debug
cmake --build --preset Debug --parallel
cmake --preset Release
cmake --build --preset Release --parallel
$env:PYTHONPATH = "$PWD/build/python-deps"
python -m pytest -q
```

本机工具链路径包含中文，GCC链接阶段会把路径转成乱码。验证在副本 `build/toolchain` 建立指向现有工具链的英文路径junction，并在CMake缓存指定其gcc、objcopy、objdump和size；未改系统PATH或父目录配置。已有缓存可直接继续构建。移动工程或更换电脑时，请使用英文路径安装的交叉工具链重新配置；junction不是独立工具链副本。

`build/python-deps` 从原工程复制已有Unicorn 2.1.4，未联网安装。依赖版本见已有 `tests/requirements.txt`。

额外的 `build/WeakeningCompileTest_DO_NOT_FLASH/` 使用 `tests/motor/field_weakening_parameters.h` 虚构参数，只证明启用分支可完整链接、容量未越界。**其中全部ELF/HEX/BIN禁止烧录。** 测试参数未写入默认配置，未加入常用预设。CMake入口是 `tests/motor/enable_field_weakening.cmake`，仅供验证。

## 验证范围与限制

完成原包数值、单位适配、电压/电流边界及真实控制模块配合硬件接口替身的状态机测试，并回归原有电机数学与TIMER0测试。结果与产物见 `docs/INTEGRATION_VERIFICATION.md`。

未烧录或连接功率级，未验证真实采样窗口、压缩机参数、退磁边界、弱磁整定或带载运行。Cortex-M23无FPU；1kHz浮点任务仍在ADC ISR，20kHz矢量限幅还有整数平方根/64位运算，必须实测最长ISR路径小于50us并留出ADC和其它任务余量。数值仿真不证明实时性。

当前ADC与SysTick优先级同为0；调整优先级须重新评估共享弱磁状态。原观测器R/L的标称母线归一化与实际母线变化仍需台架核对。
