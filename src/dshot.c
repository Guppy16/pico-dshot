#include "stdio.h"
#include "dshot.h"

/**
 * @brief send a dshot packet
 *
 * TODO: description of how this works
 */
void dshot_send_packet(dshot_config *dshot, bool debug) {
  if (debug) {
    printf("Throttle Code: %i\n", dshot->packet.throttle_code);
  }

  // TODO: IF dma is bust, write to a temp buffer
  // if (dma_channel_is_busy(dshot->dma_channel))
  dma_channel_wait_for_finish_blocking(dshot->dma_channel);
  dshot_packet_compose(&dshot->packet);
  // Re-configure dma and trigger transfer
  dma_channel_configure(
      dshot->dma_channel, &dshot->dma_config,
      // Write to pwm counter compare
      &pwm_hw->slice[pwm_gpio_to_slice_num(dshot->esc_gpio_pin)].cc,
      dshot->packet.packet_buffer, dshot_packet_length, true);
}

/// @section functions to log dshot setup

void print_dshot_config(dshot_config *dshot) {

  printf("--- Dshot config ---\n");

  // General config
  printf("dshot speed %u khz\n", dshot->dshot_speed_khz);
  printf("esc gpio: %u\n", dshot->esc_gpio_pin);

  // packet config
  printf("\ndshot packet config\n");
  printf("throttle code: %u\t", dshot->packet.throttle_code);
  printf("telemetry: %u\t", dshot->packet.telemetry);
  printf("packet high: %u low: %u\n", dshot->packet.packet_high,
         dshot->packet.packet_low);

  // pwm config
  printf("\npwm config\n");
  printf("slice: %u\t", pwm_gpio_to_slice_num(dshot->esc_gpio_pin));
  printf("channel: %u\t", pwm_gpio_to_channel(dshot->esc_gpio_pin));
  printf("wrap: TODO\t");

  // dma channel config
  printf("\ndma channel config\n");
  printf("dma channel: %i\t", dshot->dma_channel);
  printf("transfer count: %i \t", dshot_packet_length);

  // repeating timer setup
  printf("\ndma repeating timer setup: %d\n", dshot->send_packet_rt_state);
  printf("delay: %li\t", dshot->send_packet_rt.delay_us);
  printf("alarm id: %i\n", dshot->send_packet_rt.alarm_id);
}