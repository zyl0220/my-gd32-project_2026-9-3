# 压缩机弱磁整合实施计划

> **For agentic workers:** 使用 superpowers:executing-plans 在本会话连续实施；原 ZIP 归档/算法测试与主实现分开处理，最后独立审查。

**Goal:** 把新 ZIP 的弱磁算法接入已有 GD32C231C8T7 FOC，在独立工程副本验证构建，默认关闭弱磁及功率输出。

**Architecture:** 保留原浮点 FW API，在适配层统一 Apeak/V 与 Q15、PWM 电压余量；1 kHz 更新弱磁，20 kHz 电流环执行定点电压矢量限幅和电流参考限幅。仅闭环锁定可使用负 Id，停机/故障/失锁清零。

**Tech Stack:** 现有 C11、CMake、arm-none-eabi GCC、pytest/Unicorn；数学运算使用工具链已有 libm，不安装依赖。

**Spec:** 用户当前对话确认按新压缩包整合；原始包的 INTEGRATION_GUIDE.md。

## Global Constraints

- 原工程文件不改；交付 integrated_compressor_foc 副本。
- MOTOR_BOARD_READY=0U，MOTOR_ENABLE_FIELD_WEAKENING=0U；不填写猜测电机参数、不烧录。
- 不修改厂商启动、库、链接脚本，不提交或推送。
- 原包完整保留在 third_party/compressor_foc_starter；生产模块放在现有 src/motor、inc/motor。

## Review Focus

- 25%–75% PWM 可用电压及非对称占空窗，以 500‰ 为中心保守取余量。
- 原 PI 输出已限幅：新增 int32 未饱和请求并对矢量饱和实施条件积分。
- Id/Iq 换算采用相峰值 A，舍入后仍满足电流圆；正负 Iq 都受限。
- 失锁、停机、故障及无效输入清除弱磁，默认关闭路径保持现有启动行为。
- Cortex-M23 无 FPU；数值仿真/编译不能证明 50us ISR 预算或电机可运行。

## Task 1: 新算法与定点适配

**Files:** 新增原包归档；inc/motor/field_weakening.h、field_weakening_adapter.h；src/motor/field_weakening.c、field_weakening_adapter.c；tests/test_field_weakening*.py、tests/motor/test_field_weakening*.c。

**Interfaces:** FW_Init/Reset/Update 保留原 API。motor_fw_adapter_init(ctx,cfg,current_full_scale_ma,duty_min,duty_max)；motor_fw_adapter_update(ctx,vd_q15,vq_q15,bus_mv,dt_s,locked)；motor_fw_adapter_reset(ctx)；motor_fw_adapter_limit_iq(ctx,iq)。适配器提供 id_ref_q15、iq_limit_q15、voltage_limit_q15。

- [x] 先写并运行测试，确认缺少新增符号失败。
- [x] 原包归档并导入算法，生产 include 路径适配现有 motor 目录。
- [x] 实现单位/PWM余量适配；测试 300V 母线、150V有效母线、8986 Q15电压半径，负Id斜率/释放、电流圆、零母线/错误配置/失锁。
- [x] 运行对应 Cortex-M23 仿真测试并记录结果。

## Task 2: 控制链路接入

**Files:** 修改副本 motor/pi、foc_math、foc、motor_app 及 motor_param.h；副本 CMakeLists.txt 链接已有 libm；新增整合说明。

**Interfaces:** motor_pi_request(pi,error) 返回候选 int32 请求；motor_pi_track(pi,error,applied) 根据饱和方向提交积分；motor_voltage_limit(d,q,limit) 限制定点矢量。保留原 motor_pi_step 及 motor_foc_step 调用约定。

- [x] 先补充并运行电压矢量饱和/反向恢复/FOC未饱和请求测试，观察缺功能失败。
- [x] 在启用弱磁时接入矢量限幅、限幅后观测器电压反馈；关闭路径保留旧控制方式。
- [x] motor_app 在1kHz闭环速度任务计算弱磁；每电流周期夹住Iq，失锁立即重置，故障/停机/重启重置。参数默认关闭，启用时无效参数拒绝初始化。
- [x] 全量pytest及 Debug/Release/FactoryTest/FlashEepromDemo/MotorControl 构建；另外构建隔离的启用弱磁合成参数测试镜像，明确不得烧录。
- [x] 独立审查、处理实质问题、复跑相关验证，核对原工程哈希未变，记录产物和限制。
