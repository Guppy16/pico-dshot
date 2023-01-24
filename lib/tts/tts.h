#pragma once
#include <stdio.h>

#include "config.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/time.h"

namespace tts {
// PWM config

/*! \brief The slice number is the upper or lower half of 32 bits.
 *
 * Timers are 16 bits, but are inefficint to store in 32 bit systems,
 * hence timers are stored in either the lower or upper "slice" of 32 bits
 *
 * \ingroup PWM
 * NOTE: const is extern by default
 */
const uint pwm_slice_num = pwm_gpio_to_slice_num(MOTOR_GPIO);
const uint pwm_channel = pwm_gpio_to_channel(MOTOR_GPIO);

/*! \brief setup PWM config and default duty cycle to 0.
 *  DEBUG sets PWM freq to ~ 8 Hz
 */
void pwm_setup();

// DMA config

const int dma_channel = dma_claim_unused_channel(true);
extern dma_channel_config dma_config;

/*! \brief setup DMA config
 */
void dma_setup();

// Debugging

void print_gpio_setup();
void print_dshot_setup();
void print_pwm_setup();
void print_dma_setup();

// Create alarm pool
extern alarm_pool_t *pico_alarm_pool;

}  // namespace tts
