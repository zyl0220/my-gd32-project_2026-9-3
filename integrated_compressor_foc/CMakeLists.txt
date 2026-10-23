cmake_minimum_required(VERSION 3.20)

project(my-gd32-project_2026-9-3 C ASM)

# 构建入口见 CMakePresets.json：配置阶段选择预设，下面将功能选项映射为 C 宏。
# 源文件参与编译不等于外设已启动；调用入口由 app.c 的条件编译决定。
option(PROJECT_FACTORY_SELF_TEST "Enable factory EEPROM/UART self-test" OFF)
option(PROJECT_FLASH_EEPROM_DEMO "Enable Flash EEPROM boot counter demo" OFF)
option(PROJECT_MOTOR_CONTROL "Enable GD32C231C8T7 motor controller" OFF)

if(PROJECT_MOTOR_CONTROL AND PROJECT_FACTORY_SELF_TEST)
    message(FATAL_ERROR "Motor control owns ADC and cannot run with factory ADC self-test")
endif()

set(LINKER_SCRIPT gd32c2x1_flash.ld)

# 编译参数
set(MCU_FLAGS "-mcpu=cortex-m23 -mthumb")

set(CMAKE_C_FLAGS
    "${MCU_FLAGS} -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -std=gnu11"
    CACHE STRING "" FORCE)

set(CMAKE_ASM_FLAGS "${MCU_FLAGS} -x assembler-with-cpp" CACHE STRING "" FORCE)

# 各配置编译标志（按 CMAKE_BUILD_TYPE 自动叠加）
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3" CACHE STRING "" FORCE)
# Release 用于交付固件：优化体积、保留最小调试信息并关闭 assert 类调试逻辑。
set(CMAKE_C_FLAGS_RELEASE "-Os -g1 -DNDEBUG" CACHE STRING "" FORCE)

set(CMAKE_EXE_LINKER_FLAGS
    "${MCU_FLAGS} \
     -T${LINKER_SCRIPT} \
     -Wl,--print-memory-usage \
     -Xlinker --gc-sections \
     -Wl,-Map=${PROJECT_NAME}.map \
     --specs=nano.specs \
     --specs=nosys.specs \
     -L${CMAKE_SOURCE_DIR}/ldscripts \
     -Wl,--no-warn-rwx-segments"
    CACHE STRING "" FORCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# 注意：不要加 -ffile-prefix-map=${CMAKE_BINARY_DIR}=${CMAKE_SOURCE_DIR}。
# CMake+Ninja 从 build/<Type>/ 调用 gcc，C 源码 DWARF 形态是
# comp_dir=<ws>/build/<Type> + DW_AT_name=../../src/xxx.c（相对 build 目录）。
# 该 prefix-map 只把 comp_dir 的 build/<Type> 前缀改成 <ws>，却改不到以 ../
# 开头的相对源名，导致 GDB 拼出 <ws>/../../src/xxx.c 再上跳两级，源文件全部找不到。
# 保持默认即可：真实 comp_dir=build/<Type> + ../../src/xxx.c 能正确解析到
# <ws>/src/xxx.c；启动汇编 .S 由调试配置注入的 dir "<ws>/gcc_startup" 兜底。

# 预处理器宏定义
add_compile_definitions(
    GD32C231
    GD32C231C8T7
    GD_ECLIPSE_GCC
    USE_STDPERIPH_DRIVER
)

# 头文件路径
set(INC_DIRS
    ${CMAKE_SOURCE_DIR}/Firmware/CMSIS
    ${CMAKE_SOURCE_DIR}/Firmware/CMSIS/GD/GD32C2x1/Include
    ${CMAKE_SOURCE_DIR}/Firmware/GD32C2x1_standard_peripheral/Include
    ${CMAKE_SOURCE_DIR}/inc
)

# 源文件列表
# 资源管理器文件位于 src 根目录；自有代码位于 src/app、src/bsp、
# src/diagnostics。这里使用递归扫描，因此新增自有 .c 文件无需修改本文件。
set(SOURCES)
file(GLOB_RECURSE SOURCE_GLOB_0 CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.c"
)
list(REVERSE SOURCE_GLOB_0)
list(APPEND SOURCES ${SOURCE_GLOB_0})

file(GLOB_RECURSE SOURCE_GLOB_1 CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/gcc_startup/*.c"
    "${CMAKE_SOURCE_DIR}/gcc_startup/*.s"
    "${CMAKE_SOURCE_DIR}/gcc_startup/*.S"
)
list(REVERSE SOURCE_GLOB_1)
list(APPEND SOURCES ${SOURCE_GLOB_1})

file(GLOB_RECURSE SOURCE_GLOB_2 CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Firmware/GD32C2x1_standard_peripheral/Source/*.c"
)
list(REVERSE SOURCE_GLOB_2)
list(APPEND SOURCES ${SOURCE_GLOB_2})

file(GLOB_RECURSE SOURCE_GLOB_3 CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Firmware/CMSIS/GD/GD32C2x1/Source/*.c"
)
list(REVERSE SOURCE_GLOB_3)
list(APPEND SOURCES ${SOURCE_GLOB_3})

if(NOT PROJECT_MOTOR_CONTROL)
    list(FILTER SOURCES EXCLUDE REGEX "/src/motor/[^/]+[.]c$")
endif()

# ELF 包含调试符号；HEX/BIN 用于烧录；MAP 查看代码/数据段占用；LST 查看反汇编。
# --gc-sections 会剔除未被引用的节，不能根据 src 文件数量判断固件实际功能。
set(TARGET_ELF ${PROJECT_NAME}.elf)

add_executable(${TARGET_ELF}
    ${SOURCES}
)

target_include_directories(${TARGET_ELF} PRIVATE ${INC_DIRS})

if(PROJECT_FACTORY_SELF_TEST)
    target_compile_definitions(${TARGET_ELF} PRIVATE
        PROJECT_ENABLE_FACTORY_SELF_TEST=1U
    )
endif()

if(PROJECT_FLASH_EEPROM_DEMO)
    target_compile_definitions(${TARGET_ELF} PRIVATE
        PROJECT_ENABLE_FLASH_EEPROM_DEMO=1U
    )
endif()

if(PROJECT_MOTOR_CONTROL)
    # 弱磁浮点算法使用工具链自带数学库，未启用时由--gc-sections裁剪。
    target_link_libraries(${TARGET_ELF} PRIVATE m)
    target_compile_definitions(${TARGET_ELF} PRIVATE
        PROJECT_ENABLE_MOTOR_CONTROL=1U
        # 未知主板没有已核对的日志串口引脚，电机镜像不占用该 GPIO。
        PROJECT_ENABLE_STARTUP_LOG=0U
    )
endif()

# 自有代码启用严格告警；厂商库保持资源管理器默认参数，便于后续原样升级。
file(GLOB_RECURSE FIRST_PARTY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/app/*.c"
    "${CMAKE_SOURCE_DIR}/src/bsp/*.c"
    "${CMAKE_SOURCE_DIR}/src/diagnostics/*.c"
    "${CMAKE_SOURCE_DIR}/src/examples/*.c"
    "${CMAKE_SOURCE_DIR}/src/motor/*.c"
)
set_source_files_properties(${FIRST_PARTY_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Wformat=2;-Wundef;-Werror=return-type;-Werror=implicit-function-declaration"
)

# 构建后处理：默认生成 hex / bin / lst / size；
set(TARGET_HEX ${PROJECT_NAME}.hex)
set(TARGET_BIN ${PROJECT_NAME}.bin)
set(TARGET_LST ${PROJECT_NAME}.lst)

add_custom_command(TARGET ${TARGET_ELF} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${TARGET_ELF}> ${TARGET_HEX}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_ELF}> ${TARGET_BIN}
    COMMAND ${CMAKE_OBJDUMP} --source --all-headers --demangle --line-numbers --wide
            $<TARGET_FILE:${TARGET_ELF}> > ${TARGET_LST}
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${TARGET_ELF}>
    COMMENT "Generating ${TARGET_HEX}, ${TARGET_BIN}, ${TARGET_LST}"
)
