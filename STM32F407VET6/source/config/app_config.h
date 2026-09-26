/**
 * @file app_config.h
 * @brief 基础频率、通信缓冲及自检开关；修改后重新编译生效。
 * @see 本目录 README.md（职责、调用顺序与使用限制）。
 */
#pragma once
/* Board assumptions are explicit here; validate against the assembled board. */
#define APP_CONTROL_HZ 500U
#define APP_MOTOR_COUNT 3U
#define APP_MOTOR_ADDRESS 1U
#define APP_MOTOR_REPLY_MS 80U
#define APP_MOTOR_POLL_MS 20U
#define APP_MOTOR_STALE_MS 300U
#define APP_MOTOR_BOOT_MS 600U
#define APP_UART_DMA_SIZE 128U
#define APP_UART_RING_SIZE 512U
#define APP_UART_TX_SIZE 32U
/* Matches CubeMX's initial PE3 LOW. Change both if the transistor is active-low. */
#define APP_BEEP_ACTIVE_HIGH 1
/* ---- Bench self test -------------------------------------------------------
 * 1 = on power-up, after the poll is alive: enable -> zero -> move target <-> 0,
 *     then keep repeating so the movement is obvious on the bench.
 * 0 = bench test disabled; automatic startup is controlled separately below.
 * This DOES command motion. Check the axis can travel from its startup position to the target
 * without hitting anything before you flash it. Set back to 0 when done. */
#ifndef APP_MOTOR_SELFTEST
#define APP_MOTOR_SELFTEST 0
#endif
#define APP_SELFTEST_AXIS MOTOR_X
#define APP_SELFTEST_TARGET 2.0f /* X/Z in mm, Yaw in degrees; 0.5mm is hard to see */
#define APP_SELFTEST_SPEED 1.0f  /* X/Z in mm/s, Yaw in deg/s; start slow */

/* 上电演示：只执行一次。当前位置设零，不执行机械寻零。 */
#ifndef APP_MOTOR_AUTOSTART
#define APP_MOTOR_AUTOSTART 0
#endif
#ifndef APP_AUTOSTART_MODE
#define APP_AUTOSTART_MODE ACTION_RUB
#endif
#define APP_AUTOSTART_SETUP_MS 5000U
#if APP_MOTOR_AUTOSTART && APP_MOTOR_SELFTEST
#error "Autostart and bench selftest cannot both be enabled"
#endif

/* 联调固件由远程 ARM/START 驱动，上电不自动运动。 */
#if APP_MOTOR_AUTOSTART || APP_MOTOR_SELFTEST
#error "Remote firmware requires automatic demos disabled"
#endif
