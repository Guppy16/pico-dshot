/**
 * @file main.cpp
 *
 * Example that periodically receives onewire uart telemetry data from an ESC.
 *
 * This will send a constant stream of dshot packets with the command 0.
 * (NOTE: This will "arm" your motor, but will *not* send any throttle commands)
 *
 * Meanwhile, every 1s, the dshot packet will have the telemetry bit set to 1.
 * This will request telemetry and will be receieved by the uart pin separately
 * from the main loop. When telemetry data is receieved, an update flag will be
 * set, and the main loop will print this to the terminal. Importantly, the
 * main loop will also reset this update flag.
 *
 */

#include "pico/platform.h"
#include "stdio.h"
#include <string.h>

#include "dshot.h"
#include "onewire.h"

#define ESC_COUNT 1 // This example assumes there is only one ESC

constexpr uint esc_gpio = 14;
constexpr float dshot_speed = 1200.0f;           // khz
constexpr int64_t packet_interval_us = 1000 / 7; // 7 khz packet frequency
constexpr uint telem_gpio = 13; // Uart RX gpios are {1, 5, 9, 13, 17, 21}
constexpr long int telem_delay_us = 1e6; // Repeat telemetry every 1s
constexpr int motor_magnet_poles = 14;   // Used to convert erpm to rpm

int main() {
  stdio_init_all();

  // Sleep for some time to wait for serial uart to setup
  sleep_ms(1500); // ms

  // Get the default alarm pool. This is passed in to the dshot init,
  // so that we can setup a repeating timer to send dshot packets
  alarm_pool_t *pico_alarm_pool = alarm_pool_get_default();

  // initialise dshot config
  dshot_config dshot;
  dshot_config_init(&dshot, dshot_speed, esc_gpio, packet_interval_us,
                    pico_alarm_pool);
  print_dshot_config(&dshot);

  // Store this dshot config in an array
  dshot_config *dshots[ESC_COUNT] = {&dshot};

  // initialise telemetry
  telem_uart_init(&onewire, uart0, telem_gpio, pico_alarm_pool, telem_delay_us,
                  dshots, true, true);
  print_onewire_config(&onewire);

  // This may be required to an intial blip in the uart while the ESC is
  // powering up / first receiving dshot commands
  // onewire.buffer_idx = KISS_ESC_TELEM_BUFFER_SIZE;

  // Keep track of which esc to extract telemetry information from
  // This will always be 0 for ESC_COUNT = 1
  size_t esc_idx = 0;

  while (1) {
    // Check if telemetry has been updated
    if (is_telem_updated(&onewire)) {
      // Reset telemetry udpate variable
      reset_telem_updated(&onewire);

      // Print telemetry data
      kissesc_print_telem(&onewire.escs[esc_idx].telem_data);
      // Convert erpm to omega (rad/s)
      const float omega = onewire.escs[esc_idx].telem_data.erpm * 2 * 3.14 /
                          60 / motor_magnet_poles / 2;
      printf("Omega:\t\t%.2f\n", omega);
      kissesc_print_buffer(onewire.buffer, KISS_ESC_TELEM_BUFFER_SIZE);
    }

    // We require some operation here, otherwise the loop hangs:
    sleep_ms(telem_delay_us / 1000);
  }
}