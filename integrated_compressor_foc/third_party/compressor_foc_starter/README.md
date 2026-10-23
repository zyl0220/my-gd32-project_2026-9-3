# 压缩机 PMSM FOC 匹配：GD32 移植起点（算法示例）

> 此工程 **不是** GD32 原厂 FOC SDK，也不是可直接烧录的压缩机驱动固件。
> 只提供与 MCU 无关的弱磁控制模块、未填写的电机参数模板、测试代码和移植检查表。
> 它不包含 ADC/PWM 驱动、无感观测器、完整 FOC、过流/欠压/过温、压缩机保护。
> 禁止未完成验证就连接高压逆变器或密闭制冷压缩机。

## 项目文件

- `include/field_weakening.h`, `src/field_weakening.c`：基于未饱和 dq 电压指令的弱磁 PI、负 Id 限幅、反积分饱和、Id 斜率限制、Iq 电流圆约束
- `include/motor_profile.h`：压缩机参数配置模板，初始全为 0 以强制识别“未配置”
- `tests/test_fw.c`：PC 仿真单元测试（数值为虚构测试值）
- `INTEGRATION_GUIDE.md`：从 GD32 FOC 工程集成的接口和逐步联调说明
- `matching_checklist.csv`：现场需要收集的电机与驱动板资料

## 在 PC 上验证算法

```bash
gcc -std=c99 -Wall -Wextra -pedantic -Iinclude src/field_weakening.c tests/test_fw.c -lm -o fw_test
./fw_test
```

## 推荐的基础工程

1. GD32303R-FOC 原厂参考方案（需索取源码）：https://www.gd32mcu.com/cn/detail/486
2. Microchip 带 Field Weakening + MTPA 的示例：https://github.com/Microchip-MPLAB-Harmony/mc_apps_sam_c2x
3. TI Motor Control SDK（参数识别、压缩机保护思路）：https://www.ti.com/tool/C2000WARE-MOTORCONTROL-SDK

商业项目移植第三方代码之前必须逐文件检查许可条款；公开可读并不等于可任意跨芯片商用。
