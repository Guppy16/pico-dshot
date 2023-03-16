#pragma once
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "stdint.h"
#include "stdio.h"

#include "packet.h"

/** @file dshot.h
 *  @defgroup dshot dshot
 *
 * define constants used to setup dshot on a rpi pico
 */

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
 * @param packet_interval maximum time between start of packets (in micro secs)
 * @param send_packet_rt repeating timer config for sending dshot packets
 * regularly
 * @param send_packet_rt_state true if repeating timer was setup succesfully
 */
typedef struct dshot_config {
  uint dshot_speed_khz;
  uint esc_gpio_pin;
  pwm_config pwm_conf;
  int dma_channel;
  dma_channel_config dma_config;

  // An alternative declaration would be:
  // dshot_packet_t *const dshot_pckt;
  dshot_packet_t packet;

  /** @brief Maximum time between start of packets (in micro secs)
   *
   *  @attention
   *  dShot 150 sets a lower limit on this:
   *  20 bits / 150 kHz = 2/15 ms > 133 us
   *  Hence we can use a minimum of 7 kHz update rate
   */
  long int packet_interval;
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
 * @brief setup pwm for dshot
 *
 * @param dshot ptr to dshot config. Must have esc_gpio_pin set
 * @param pwm_wrap wrap sets the max count in each pwm period
 * @param pwm_div clock divider to slow down pwm. Set this to \a 1.0f
 *
 * @attention
 * We assume that pwm_wrap is within the limit of uint16_t
 * TODO: If this is not the case, then @ref pwm_div would have to compensate for it
 */
static inline void dshot_pwm_configure(dshot_config *const dshot,
                                       const uint16_t pwm_wrap,
                                       const float pwm_div) {

  gpio_set_function(dshot->esc_gpio_pin, GPIO_FUNC_PWM);

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
 * Ideally we want dma to be capable of smth like \a hw_write_masked()
 */
static inline void dshot_dma_configure(dshot_config *const dshot) {
  // slice corresponds to one of 8 timers
  const uint pwm_slice_num = pwm_gpio_to_slice_num(dshot->esc_gpio_pin);

  dshot->dma_channel = dma_claim_unused_channel(true);
  dshot->dma_config = dma_channel_get_default_config(dshot->dma_channel);
  channel_config_set_transfer_data_size(&dshot->dma_config, DMA_SIZE_32);
  channel_config_set_read_increment(&dshot->dma_config, true);
  channel_config_set_write_increment(&dshot->dma_config, false);
  channel_config_set_dreq(&dshot->dma_config, DREQ_PWM_WRAP0 + pwm_slice_num);
}

/**
 * @brief setup packet config for dshot (see @ref dshot_config::packet)
 *
 * @param dshot ptr to dshot config. must have @ref dshot_config::esc_gpio_pin set
 */
static inline void dshot_packet_configure(dshot_config *const dshot,
                                          const uint16_t pwm_wrap) {
  // Timers are 16 bits. channel is the upper or lower half of a 32 bit word
  const uint pwm_channel = pwm_gpio_to_channel(dshot->esc_gpio_pin);

  const dshot_packet_t pckt = {
      .packet_buffer = {0},
      .throttle_code = 0,
      .telemetry = 0,
      .packet_high = (uint32_t)(0.75 * pwm_wrap) << 16 * pwm_channel,
      .packet_low = (uint32_t)(0.37 * pwm_wrap) << 16 * pwm_channel};

  dshot->packet = pckt;
}

/**
 * @brief setup repeating timer for dshot.
 * Call this function after configuring pwm, dma, packet
 *
 * @param dshot ptr to dshot config
 */
static inline void dshot_rt_configure(dshot_config *const dshot,
                                  const uint32_t packet_interval,
                                  alarm_pool_t *const pool) {

  /// TODO: Assert packet_length / dshot_speed (MHz) < packet_interval
  dshot->packet_interval = packet_interval;

  /// Repeating timer
  dshot->send_packet_rt_state = alarm_pool_add_repeating_timer_us(
      pool, dshot->packet_interval, dshot_repeating_send_packet, dshot,
      &dshot->send_packet_rt);
}

/**
 * @brief initialise dshot config
 * 
 * @param dshot ptr to dshot config. All data will be overwritten
 * @param dshot_speed_khz
 * @param esc_gpio_pin
 * @param packet_interval maximum time between start of sending packets
 * @param pool alarm pool to add the repeating timer to send dshot packets regularly
 * @param pwm_div divider for pwm hw. Usuaully this is `1.0f`. 
 * TODO: later version will calculate this value
*/
static void dshot_config_init(dshot_config *const dshot,
                              const uint dshot_speed_khz,
                              const uint esc_gpio_pin,
                              const uint32_t packet_interval,
                              alarm_pool_t *const pool, const float pwm_div) {

  // General config
  dshot->dshot_speed_khz = dshot_speed_khz;
  dshot->esc_gpio_pin = esc_gpio_pin;

  const uint32_t mcu_freq_khz =
      frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
  const uint16_t pwm_wrap = mcu_freq_khz / dshot->dshot_speed_khz;

  // configure pwm, dma, packet, repeating timer
  dshot_pwm_configure(dshot, pwm_wrap, pwm_div);
  dshot_dma_configure(dshot);
  dshot_packet_configure(dshot, pwm_wrap);
  dshot_rt_configure(dshot, packet_interval, pool);
}

// Print dshot config
void print_dshot_config(dshot_config *dshot);

#ifdef __cplusplus
}
#endif