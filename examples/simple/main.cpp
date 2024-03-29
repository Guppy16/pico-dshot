/**
 * @file main.cpp
 *
 * Minimum setup required to send dshot commands to an esc.
 * This will send a constant stream of dshot packets with the command 0.
 * This may arm your motor, but will not send any throttle commands.
 */

#include "pico/platform.h"
#include "stdio.h"
#include <string.h>

#include "dshot.h"

constexpr uint esc_gpio = 14;
constexpr float dshot_speed = 1200.0f;           // khz
constexpr int64_t packet_interval_us = 1000 / 7; // 7 khz packet frequency

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

  while (1)
    tight_loop_contents();
}