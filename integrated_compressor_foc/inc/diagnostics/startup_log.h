/**
 * @file startup_log.h
 * @brief 启动信息输出模块。
 */

#ifndef STARTUP_LOG_H
#define STARTUP_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化板载调试串口，并输出产品、库版本和系统时钟。 */
void startup_log_print(void);

#ifdef __cplusplus
}
#endif

#endif /* STARTUP_LOG_H */
