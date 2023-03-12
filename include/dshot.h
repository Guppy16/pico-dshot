#pragma once
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "stdint.h"

#include "packet.h"
// #include "pico/time.h"
// #include <stdio.h>

/** @file config.h
 *  @defgroup dshot_config dshot_config
 *
 * define constants used to setup dshot on a rpi pico
 */

/**
 * @brief dshot frequency in kHz
 *
 * TODO:
 * The equivalent dshot speed is 8 Hz in DEBUG mode
 * This could be set here directlt instead of changing:
 * \a DSHOT_PWM_WRAP, \a DSHOT_PWM_DIV, \a DMA_ALARM_PERIOD(?)
 */
// #define DSHOT_SPEED 1200 // kHz

/**
 * @brief Keep track of the pico clock frequency (MHz)
 *
 * @attention
 * This doesn't set the mcu clock. Do this using
 * `hardware/clocks.h::set_sys_clock_khz(MCU_FREQ * 1e3, false);`
 */
// #define MCU_FREQ 120

// HW alarm num for alarm pool
#define DMA_ALARM_NUM 1

// #define DEBUG 0

// #if DEBUG
// #define MOTOR_GPIO 25
// #else
// #define MOTOR_GPIO 14
// #endif

#ifdef __cplusplus
extern "C" {
#endif

// const uint dshot_frame_length = 16;
// const uint dshot_packet_length = 20;

enum dshot_code {
  DSHOT_DISARM = 0,
  DSHOT_ZERO_THROTTLE = 48,
  DSHOT_ARM_THROTTLE = 300,
  DSHOT_MAX_THROTTLE = 2047 // 2^12 - 1
};

typedef struct dshot_config {
  uint dshot_speed_khz;

  uint esc_gpio_pin;

  int dma_channel;
  dma_channel_config dma_config;

  // An alternative declaration would be:
  // dshot_packet_t *const dshot_pckt;
  dshot_packet_t packet;

  // packet = dshot frame | frame reset(= 0 padding)
  // const uint16_t packet_length = dshot_packet_length;

  /** @brief Maximum time between start of packets in micro secs
   *
   *  @attention
   *  A lower limit is set by the dshot speed:
   *
   * hi
   * DShot 150 sets a lower limit on this:
   *  20 bits / 150 kHz = 2/15 ms > 133 us
   *  Hence we can use a minimum of 7 kHz update rate
   *
   *  TODO: Assert packet_length / dshot_speed (MHz) < packet_interval
   *  NOTE: DEBUG sets this to 3 s, because the time for each command is:
   *  20 / 8 = 2.5 s (NOTE the effective dshot speed is 8 Hz)
   */
  long int packet_interval;
  // store repeating timer configuration
  repeating_timer_t send_packet_rt;
  // temp variable to keep track of rt status
  bool send_packet_rt_state;
} dshot_config;

/**
 * @brief send a dshot packet
 */
void dshot_send_packet(dshot_config *dshot, bool debug);

/**
 * @brief isr to send dshot packet over dma
 *
 * TODO: include `void * userdata` parameter to get dshot_config.
 * interpret this using:
 *    dshot_config *dshot = (dshot_config **)userdata
 *
 * @return true, so that timer repeats
 */
static inline bool dshot_repeating_send_packet(repeating_timer_t *rt) {
  // NOTE: not sure if the use of ** is correct here
  dshot_config *dshot = (dshot_config *)(rt->user_data);
  dshot_send_packet(dshot, false);
  return true;
}

/** @brief
 *
 */
static void dshot_config_init(dshot_config *dshot, const uint dshot_speed_khz,
                              const uint esc_gpio_pin,
                              const uint32_t packet_interval,
                              alarm_pool_t *pool, const float pwm_div) {

  printf("setting general dshot config\n");
  dshot->dshot_speed_khz = dshot_speed_khz;
  dshot->esc_gpio_pin = esc_gpio_pin;

  // General config

  /// @brief PWM config

  const uint32_t mcu_freq_khz =
      frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
  printf("mcu freq: %u khz\n", mcu_freq_khz);

  const uint16_t pwm_wrap = mcu_freq_khz / dshot->dshot_speed_khz;
  // We assume that pwm_wrap is within the limit of uint16_t
  // If this was not the case, then pwm_div would have to compensate for it
  // dshot.pwm_div = pwm_div;
  printf("pwm wrap: %u\n", pwm_wrap);

  // slice corresponds to 1 of 8 timers
  const uint pwm_slice_num = pwm_gpio_to_slice_num(dshot->esc_gpio_pin);
  /* Timers are 16 bits. Hence a channel is the upper or lower half of a
  32 bit integer*/
  const uint pwm_channel = pwm_gpio_to_channel(dshot->esc_gpio_pin);
  gpio_set_function(dshot->esc_gpio_pin, GPIO_FUNC_PWM);
  // Wrap sets the max count in each pwm period
  pwm_set_wrap(pwm_slice_num, pwm_wrap);
  // Set 0 duty cycle by default
  pwm_set_chan_level(pwm_slice_num, pwm_channel, 0);
  // Set clock divider
  pwm_set_clkdiv(pwm_slice_num, pwm_div);
  // Enable pwm
  pwm_set_enabled(pwm_slice_num, true);

  /// @brief DMA channel config
  /** @attention counter compare is 32 bits (16 bits for each pwm channel)
   *  dma is setup to overwrite the all 32 bits of cc,
   *  instead of just the cc for the corresponding channel.
   *  Unfortunately, this makes one channel redundant.
   *  Ideally we want dma to be capable of smth like \a hw_write_masked()
   */
  dshot->dma_channel = dma_claim_unused_channel(true);
  dshot->dma_config = dma_channel_get_default_config(dshot->dma_channel);
  channel_config_set_transfer_data_size(&dshot->dma_config, DMA_SIZE_32);
  channel_config_set_read_increment(&dshot->dma_config, true);
  channel_config_set_write_increment(&dshot->dma_config, false);
  channel_config_set_dreq(&dshot->dma_config, DREQ_PWM_WRAP0 + pwm_slice_num);

  /// Packet config
  const dshot_packet_t pckt = {
      .packet_buffer = {0},
      .throttle_code = 0,
      .telemetry = 0,
      .packet_high = (uint32_t)(0.75 * pwm_wrap) << 16 * pwm_channel,
      .packet_low = (uint32_t)(0.37 * pwm_wrap) << 16 * pwm_channel};
  dshot->packet = pckt;

  dshot->packet_interval = packet_interval;

  /// Repeating timer
  dshot->send_packet_rt_state = alarm_pool_add_repeating_timer_us(
      pool, dshot->packet_interval, dshot_repeating_send_packet, dshot,
      &dshot->send_packet_rt);
}

/// @brief functions to log setup for debugging

void print_dshot_config(dshot_config *dshot);

#ifdef __cplusplus
}
#endif