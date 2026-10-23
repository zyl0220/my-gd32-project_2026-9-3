# 整合验证记录

日期：2026-10-23。范围：`integrated_compressor_foc` 工程副本。

## 已核实结果

- 最终 `python -m pytest -q`：**5 passed in 4.73s**。
- GCC 15.2.1，Cortex-M23软浮点；5个常用预设及1个测试专用启用弱磁完整镜像均链接成功。各编译日志未出现编译器warning。
- 6套ELF/HEX/BIN/MAP/LST共30个文件均读取检查，ELF魔数和HEX格式起始标志有效；文件均非空，SHA-256记录见 `build/verification/artifact_manifest.json`。
- 普通Release符号表未保留FW_Update，测试专用启用镜像保留FW_Update和motor_fw_adapter_update，确认默认关闭分支与启用分支均已验证。
- 核对父目录81个原有源码、头文件、文档及关键构建文件SHA-256，**0个变化**；初始哈希见 `docs/INTEGRATION_SOURCE_HASHES.json`。

## 构建占用

下表Flash/RAM采用链接器的Memory region实际占用（包含链接脚本保留的栈/堆段），不是单独bss数值。应用Flash为62KiB，另保留2KiB EEPROM；RAM共12KiB。

| 构建 | Flash / B | RAM / B | 结果 |
| --- | ---: | ---: | --- |
| Debug | 12896 | 3272 | 通过，输出锁定 |
| Release | 6900 | 3272 | 通过，输出锁定 |
| FactoryTest | 15752 | 3584 | 通过，工厂诊断用途 |
| FlashEepromDemo | 14204 | 3632 | 通过，EEPROM例程用途 |
| MotorControl | 6900 | 3272 | 通过，输出锁定 |
| WeakeningCompileTest_DO_NOT_FLASH | 19212 | 3720 | 通过，虚构参数，禁止烧录 |

构建日志保存于 `build/verification/*-configure.log`、`*-build.log` 和 `*-final-build.log`。普通产物在对应 `build/<预设>/` 目录，文件前缀保持 `my-gd32-project_2026-9-3`。

## 测试覆盖

| 测试 | 核验内容 |
| --- | --- |
| test_field_weakening_archive.py | 原ZIP算法的方向、斜率、限幅、释放、reset和错误输入 |
| test_field_weakening_integration.py | 真实V与有效母线换算、非对称PWM余量、电流圆及1LSB边界、PI条件积分、电压矢量和真实FOC反馈 |
| test_field_weakening_lifecycle.py | 真实状态机的同周期失锁、每周期正负Iq限幅、停机/故障清零、重启恢复限幅、无效FW结果先关断再禁止PWM写入；仅硬件接口使用替身 |
| test_motor_math.py | 原有FOC数学、PI、保护与观测器回归 |
| test_timer0_pwm.py | 原有TIMER0驱动配置和安全输出顺序回归 |

新增适配和状态机测试均先确认新增功能缺失时失败，再实现并通过。审查发现的最大负Id量化越界已在ARM C仿真中确认失败，修复后全套5项测试通过。

## 未验证事项

未烧录、未接功率级、未验证实际板卡引脚/ADC采样窗口/压缩机参数/退磁边界/带载启动与弱磁整定。Cortex-M23无FPU，1kHz浮点弱磁仍在ADC ISR内，尚未实测最长执行时间和栈峰值，不能据此宣称满足50us电流环周期或可以驱动压缩机。

当前默认 `MOTOR_BOARD_READY=0U`、`MOTOR_ENABLE_FIELD_WEAKENING=0U`；完整测试镜像中的虚构参数仅存在于tests头文件和独立测试构建目录。

## 独立审查处理

独立审查确认Vdc/Q15比例、PWM余量、PI反饱和和复位接入；发现1项最大负Id可能越电流圆1LSB的问题，已复现并修复。未遗留需修改的审查问题。既有观测器标称母线归一化与实际母线变化的行为仍须台架核对；当前ADC/SysTick同优先级，若以后调整优先级需重新审查共享状态。
