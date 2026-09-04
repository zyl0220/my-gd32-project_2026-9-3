/**
 * @file app.h
 * @brief 产品应用模块的唯一对外接口。
 *
 * 调用约束：
 * 1. 系统时钟和 SysTick 配置完成后调用一次 app_init()。
 * 2. 主循环持续调用 app_process()，函数内部不得永久阻塞。
 * 3. SysTick 中断每 1 ms 调用 app_tick_1ms_isr()；该函数只能做常数时间操作。
 */

#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

void app_init(void);
void app_process(void);
void app_tick_1ms_isr(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
