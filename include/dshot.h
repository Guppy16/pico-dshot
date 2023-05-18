/** @file dshot.h
 *  @defgroup dshot dshot
 *
 * define constants used to setup dshot on a rpi pico
 */
#pragma once
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "stdint.h"
#include "stdio.h"

#include "packet.h"

#ifdef __cplusplus
extern "C" {
#endif

enum dshot_code {
  DSHOT_DISARM = 0,
  DSHOT_ZERO_THROTTLE = 48,
  DSHOT_ARM_THROTTLE = 300,
  DSHOT_MAX_THROTTLE = 2047 // 2^11 - 1
};

/**
 * @brief config used to setup hardware to send dshot packets
 * @ingroup dshot
 *
 * @param dshot_speed_khz
 * @param esc_gpio_pin GPIO pin connected to ESC
 * @param pwm_conf
 * @param dma_channel
 * @param dma_config pico dma config
 * @param packet dshot packet config
 * @param send_packet_rt repeating timer config to send dshot packets regularly
 * @param send_packet_rt_state true if repeating timer was setup succesfully
 *
 * TODO: should the configs be pointers?
 * e.g. dshot_packet_t *const dshot_pckt?
 */
typedef struct dshot_config {
  float dshot_speed_khz;
  uint esc_gpio_pin;
  pwm_config pwm_conf;
  int dma_channel;
  dma_channel_config dma_config;
  dshot_packet_t packet;
  repeating_timer_t send_packet_rt;
  bool send_packet_rt_state;
} dshot_config;

void dshot_send_packet(dshot_config *dshot, bool debug);

/**
 * @brief isr to send dshot packet over dma
 *
 * @param rt ptr to repeating timer (defined in @ref
 * dshot_config::send_packet_rt)
 * @return \a true, so that timer repeats
 */
static inline bool dshot_repeating_send_packet(repeating_timer_t *rt) {
  dshot_config *dshot = (dshot_config *)(rt->user_data);
  dshot_send_packet(dshot, false);
  return true;
}

/**
 * @brief helper function to convert a pwm period to wrap and div values
 * (assuming the divide-mode is free running)
 *
 * @param period = div x wrap.
 * This is asserted: 1.0f < period < 16.7e6
 * @param div divider for the pwm clock.
 * The PWM counter will run at clk_sys / div
 * @param max highest value PWM counter will reach before restarting
 * @param required TODO: assert if period is not attainable.
 * Currently we just get close to it, which is good enough for dshot
 * values between 150 and 1200 (incl).
 *
 * @attention
 * The maximum value of period is:
 * @verbatim (1u << 16 - 1) x 255.9375 = 16,772,864.0625 ~ 16.7 M @endverbatim
 */
static inline void pwm_period_to_div_wrap(const float period, float *const div,
                                          uint16_t *const wrap) {
  const float max_div = 255.9375;
  const uint max_wrap = (1u << 16) - 1;

  if (period > max_div * max_wrap) {
    panic("pwm_period of %d is higher than max %d\n", period,
          max_div * max_wrap);
  }

  if (period <= max_wrap) {
    *wrap = period;
    *div = 1.0f;
  } else {
    *wrap = max_wrap;
    *div = period / *wrap;
  }
}

/**
 * @brief setup pwm for dshot
 *
 * @param dshot ptr to dshot config. Must have esc_gpio_pin set
 * @param pwm_period PWM period (in mcu clk counts)
 * = mcu_freq_khz / dshot_speed_khz
 */
static inline void dshot_pwm_configure(dshot_config *const dshot,
                                       const float pwm_period) {

  gpio_set_function(dshot->esc_gpio_pin, GPIO_FUNC_PWM);

  float pwm_div;
  uint16_t pwm_wrap;
  pwm_period_to_div_wrap(pwm_period, &pwm_div, &pwm_wrap);

  dshot->pwm_conf = pwm_get_default_config();
  pwm_config_set_wrap(&dshot->pwm_conf, pwm_wrap);
  pwm_config_set_clkdiv(&dshot->pwm_conf, pwm_div);
  pwm_init(pwm_gpio_to_slice_num(dshot->esc_gpio_pin), &dshot->pwm_conf, true);

  pwm_set_gpio_level(dshot->esc_gpio_pin, 0); // default 0 duty cycle
}

/**
 * @brief setup dma config for dshot
 *
 * @param dshot ptr to dshot config. Must have esc_gpio_pin set
 *
 * @attention
 * counter compare is 32 bits (16 bits for each pwm channel)
 * dma is setup to overwrite all 32 bits of cc,
 * instead of just the cc for the corresponding channel.
 * Unfortunately, this makes one channel redundant.
 * Ideally we want dma to be capable of smth like \a hw_write_masked().
 * The dma read and write addresses are set in @ref dshot_rt_configure
 */
static inline void dshot_dma_configure(dshot_config *const dshot) {
  dshot->dma_channel = dma_claim_unused_channel(true);
  dshot->dma_config = dma_channel_get_default_config(dshot->dma_channel);
  // increment read address to read from next element in packet_buffer array
  channel_config_set_read_increment(&dshot->dma_config, true);
  channel_config_set_transfer_data_size(&dshot->dma_config, DMA_SIZE_32);
  // Writing to same address (pwm slice counter compare register)
  // Note that the address is set later
  channel_config_set_write_increment(&dshot->dma_config, false);
  // dma data request is triggered when pwm "wraps" (i.e. every period)
  const uint pwm_slice_num = pwm_gpio_to_slice_num(dshot->esc_gpio_pin);
  channel_config_set_dreq(&dshot->dma_config, DREQ_PWM_WRAP0 + pwm_slice_num);
}

/**
 * @brief setup packet config for dshot (see @ref dshot_config::packet)
 *
 * @param dshot ptr to dshot config. must have @ref dshot_config::esc_gpio_pin
 * and @ref dshot_config::pwm_conf configured
 *
 * @attention
 * There are two 16 bit timers stored in a 32 bit word,
 * corresponding to two separate pwm channels.
 * This fn shifts the packet values based on the pwm channel
 */
static inline void dshot_packet_configure(dshot_config *const dshot) {
  const uint packet_shift = pwm_gpio_to_channel(dshot->esc_gpio_pin)
                                ? PWM_CH0_CC_B_LSB
                                : PWM_CH0_CC_A_LSB;
  const uint32_t pulse_period = dshot->pwm_conf.top;

  const dshot_packet_t pckt = {
      .packet_buffer = {0},
      .throttle_code = 0,
      .telemetry = 0,
      .pulse_high = (uint32_t)(0.75 * pulse_period) << packet_shift,
      .pulse_low = (uint32_t)(0.37 * pulse_period) << packet_shift};

  dshot->packet = pckt;
}

/**
 * @brief setup repeating timer for dshot.
 * Call this function after configuring pwm, dma, packet
 *
 * @param dshot ptr to dshot config
 */
static inline void dshot_rt_configure(dshot_config *const dshot,
                                      const long int packet_interval,
                                      alarm_pool_t *const pool) {

  // Ensure packet_length (bits) < packet_interval (us) x dshot_speed (MHz)
  if (dshot_packet_length * 1000 > packet_interval * dshot->dshot_speed_khz) {
    const int64_t min_pckt_interval =
        dshot_packet_length * 1000 / dshot->dshot_speed_khz;
    panic("packet_interval of %d is lower than min: %d\n", packet_interval,
          min_pckt_interval);
  }

  dshot->send_packet_rt_state = alarm_pool_add_repeating_timer_us(
      pool, packet_interval, dshot_repeating_send_packet, dshot,
      &dshot->send_packet_rt);
}

/**
 * @brief initialise dshot config
 *
 * @param dshot ptr to dshot config. All data will be overwritten
 * @param dshot_speed_khz
 * @param esc_gpio_pin
 * @param packet_interval maximum time between start of sending packets (in
 * micro secs)
 * @param pool alarm pool to add the repeating timer to send dshot packets
 * regularly
 *
 *  @attention
 *  dShot 150 sets a lower limit on @a packet_interval
 *  20 bits / 150 kHz = 2/15 ms > 133 us
 *  Hence we can use a minimum of 7 kHz update rate
 *
 * TODO: check if long int is the correct definition for int64_t
 * for some reason int64_t doesn't work in C
 */
static void dshot_config_init(dshot_config *const dshot,
                              const float dshot_speed_khz,
                              const uint esc_gpio_pin,
                              const long int packet_interval,
                              alarm_pool_t *const pool) {
  // General config
  dshot->dshot_speed_khz = dshot_speed_khz;
  dshot->esc_gpio_pin = esc_gpio_pin;

  const uint32_t mcu_freq_khz =
      frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);

  const float pwm_period = mcu_freq_khz / dshot->dshot_speed_khz;

  // configure pwm, dma, packet, repeating timer
  dshot_pwm_configure(dshot, pwm_period);
  dshot_dma_configure(dshot);
  dshot_packet_configure(dshot);
  dshot_rt_configure(dshot, packet_interval, pool);
}

/// @brief print dshot config
void print_dshot_config(dshot_config *dshot);

#ifdef __cplusplus
}
#endif