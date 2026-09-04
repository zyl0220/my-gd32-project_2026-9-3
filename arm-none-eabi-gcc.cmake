set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m23)

# 交叉编译器
if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER arm-none-eabi-gcc)
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
endif()
if(NOT DEFINED CMAKE_ASM_COMPILER)
    set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
endif()

# 辅助工具
if(NOT DEFINED CMAKE_OBJCOPY)
    set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
endif()
if(NOT DEFINED CMAKE_OBJDUMP)
    set(CMAKE_OBJDUMP arm-none-eabi-objdump)
endif()
if(NOT DEFINED CMAKE_SIZE)
    set(CMAKE_SIZE arm-none-eabi-size)
endif()

# 跳过编译器测试（交叉编译的程序无法在主机上运行）
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 搜索路径
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
