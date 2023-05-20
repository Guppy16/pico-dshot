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

// #include "dshot.h"
#include "onewire.h"

constexpr uint esc_gpio = 14;
constexpr float dshot_speed = 1200.0f;           // khz
constexpr int64_t packet_interval_us = 1000 / 7; // 7 khz packet frequency
constexpr uint telem_gpio = 13;
constexpr long int telem_delay_us = 1e6; // Repeat telemetry every 1s
constexpr motor_magnet_poles = 14;       // Used to convert erpm to rpm

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

  dshot_config *dshots[ESC_COUNT] = {&dshot};

  // initialise telemetry
  telem_uart_init(&onewire, uart0, telem_gpio, pico_alarm_pool, telem_delay_us,
                  dshots, true, true);
  print_onewire_config(&onewire);
  // This may be required to an intial blip in the uart while the ESC is
  // powering up / first receiving dshot commands
  onewire.buffer_idx = KISS_ESC_TELEM_BUFFER_SIZE;

  while (1) {
    kissesc_print_telem(&onewire.escs[0].telem_data);
    // Convert erpm to omega (rad/s)
    const float omega = onewire.escs[0].telem_data.erpm * 2 * 3.14 / 60 /
                        motor_magnet_poles / 2;
    printf("Omega:\t\t%.2f\n", omega);
    kissesc_print_buffer(onewire.buffer, KISS_ESC_TELEM_BUFFER_SIZE);

    sleep_ms(telem_delay_us / 1000);
  }
}