/*!
    \file    auto_probe.h
    \brief   自主调试闭环 - 双回传通道探针 (UART printf + J-Link 读内存/RTT)

    \note    本模块与产品业务隔离，仅由 FactoryTest 构建启用。
             正式交付请使用 Release 构建，不要在 main.c 直接调用本模块。
*/

#ifndef AUTO_PROBE_H
#define AUTO_PROBE_H

#include <stdint.h>

/* 通道A(读内存)用的自测结果块。
   J-Link 从 .map 取 g_test_block 地址后直接读这 16 字节即可判定。
   magic 用于校验"读到的确实是这块内存"。*/
#define AUTO_TEST_MAGIC   0xC231600DU

typedef struct {
    uint32_t magic;      /* 固定 AUTO_TEST_MAGIC，校验读对了地址 */
    uint32_t heartbeat;  /* 每跑完一轮流水灯 +1，证明程序在运行(没跑飞) */
    uint32_t led_mask;   /* 当前点亮的 LED 位图，便于核对外设行为 */
    uint32_t test_pass;  /* 1=上电自测全过, 0=有失败项 */
} auto_test_block_t;

extern volatile auto_test_block_t g_test_block;

/* 初始化回传通道(串口) + 打印启动横幅 + 跑上电自测。由 app_init() 调用一次。*/
void auto_probe_init(void);

/* 按配置周期调用：更新心跳与 LED 位图，并经串口打印一行带标签的状态。*/
void auto_probe_tick(uint32_t led_mask);

#endif /* AUTO_PROBE_H */
