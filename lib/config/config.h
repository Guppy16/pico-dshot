/*
 * File to store main constants
 */
#pragma once
#include "inttypes.h"

#define DSHOT_SPEED 1200 // kHz
#define MCU_FREQ 120     // MHz. Keep track of MCU_FREQ. This doesn't set it
#define DMA_ALARM_NUM 1  // HW alarm num for alarm pool

#define DEBUG 0

#if DEBUG
#define MOTOR_GPIO 25 // BUILTIN_LED
#else
#define MOTOR_GPIO 14
#endif

// --- DMA Variables
// Note that these should be cast uint32_t when sent to the slice

/*! \brief WRAP = Total number of counts in PWM cycle
 *  \ingroup config
 *
 *  WRAP = mcu freq / dshot freq. The factor of 1000 is for MHz -> kHz
 *  DEBUG = True sets WRAP to maximum value (2^16 - 1) to slow down the signal
 */
constexpr uint16_t DSHOT_PWM_WRAP = DEBUG ? (1 << 16) - 1 : 1000 * MCU_FREQ / DSHOT_SPEED;

// Number of counts to represent a digital low
constexpr uint16_t DSHOT_LOW = 0.37 * DSHOT_PWM_WRAP;

// Number of counts to represent a digital high
constexpr uint16_t DSHOT_HIGH = 0.75 * DSHOT_PWM_WRAP;

// DShot cmd size is defined to be 16
constexpr uint32_t DSHOT_CMD_SIZE = 16;

// Frame = DShot command + 0 padding
constexpr unsigned int DSHOT_FRAME_LENGTH = DSHOT_CMD_SIZE + 4;

/*! \brief Maximum time between DShot commands in micro secs
 *  \ingroup config
 *
 * DShot 150 sets a lower limit on this:
 * 20 bits / 150 kHz = 2/15 ms > 133 us
 * Hence we can use a minimum of 7 kHz update rate
 *
 * TODO: Assert DShot_SPEED / DSHOT_CMD_SIZE > DMA_ALARM_PERIOD
 */
constexpr uint32_t DMA_ALARM_PERIOD = 1000 / 7;

// --- Throttle ---

constexpr uint16_t ZERO_THROTTLE = 48;  // 0 Throttle code
constexpr uint16_t MAX_THROTTLE = 2047; // 2^12 - 1
constexpr uint16_t ARM_THROTTLE = 300;  // < 50% MAX_THROTTLE