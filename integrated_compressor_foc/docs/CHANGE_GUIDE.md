# 代码修改指南

## 修改产品业务

1. 在 `src/app/app.c` 修改或增加状态机。
2. 周期、超时、功能开关放入 `inc/config/project_config.h`，不要散落魔法数字。
3. 主循环逻辑保持非阻塞；需要等待时使用 1 ms 时间状态机。
4. 不在 `main.c` 增加业务实现。

## 修改硬件引脚

1. 对照 PCB 原理图和 `docs/HARDWARE_RESOURCES.md`。
2. 修改对应 `inc/bsp/bsp_*.h` 的 `PORT/PIN/RCU/AF` 定义。
3. 检查复用功能、有效电平、上下拉和外设时钟。
4. 搜索同一 GPIO 是否被其他模块占用。
5. Debug、Release 全量构建，并在实板逐项验证。

## 新增外设模块

1. 新建 `inc/bsp/bsp_xxx.h`，只暴露调用者真正需要的接口。
2. 新建 `src/bsp/bsp_xxx.c`，隐藏寄存器配置、超时和状态。
3. 使用 `#include "bsp/bsp_xxx.h"`，不要使用相对路径 `../`。
4. 在 `docs/HARDWARE_RESOURCES.md` 登记引脚、DMA、中断和时钟资源。
5. 在应用模块调用，不从其他 BSP 反向调用应用代码。

## 启用工厂自检

推荐直接使用独立产测预设：

```powershell
cmake --preset FactoryTest
cmake --build --preset FactoryTest --parallel 1
```

该预设通过 CMake 打开 `PROJECT_ENABLE_FACTORY_SELF_TEST`，不会修改默认 Release 配置。当前自检会初始化 EEPROM 并通过 USART0 输出结果；只有硬件存在 AT24C02 且串口连接正确时才能通过。正式交付必须重新构建 `Release`。

## 修改版本号

只修改 `inc/config/project_config.h` 中的 `PRODUCT_VERSION_*` 和 `PRODUCT_VERSION_STRING`，同时在 `CHANGELOG.md` 记录变更。版本字符串与三个数字必须保持一致。

## 使用片内 Flash 模拟 EEPROM

驱动位于 `src/bsp/bsp_flash_eeprom.c`，接口位于 `inc/bsp/bsp_flash_eeprom.h`。先运行 `FlashEepromDemo` 预设验证实板，再参考 `docs/FLASH_EEPROM.md` 接入产品参数。不要取消链接脚本末尾 2 KB 的保留区。

## 提交前最低验证

```powershell
cmake --preset Debug
cmake --build --preset Debug --parallel 1
cmake --preset Release
cmake --build --preset Release --parallel 1
```

编译通过只证明软件可生成，不能替代烧录、上电和外设实测。
