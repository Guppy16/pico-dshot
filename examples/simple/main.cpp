/**
 * @file main.cpp
 *
 * Minimum setup required to send dshot commands to an esc.
 * This will send a constant stream of dshot packets with the command 0.
 * This may arm your motor, but will not send any throttle commands.
 */

#include <string.h>
#include "pico/platform.h"
#include "stdio.h"

#include "dshot.h"

#define LED_BUILTIN 25
#define MOTOR_GPIO 14
constexpr float dshot_speed = 1200;           // khz
constexpr int64_t packet_interval = 1000 / 7; // 7 khz packet frequency

int main()
{
  stdio_init_all();

  sleep_ms(1500); // ms

  alarm_pool_t *pico_alarm_pool = alarm_pool_get_default();

  // --- setup dshot configuration ---
  dshot_config dshot;
  dshot_config_init(&dshot, dshot_speed, MOTOR_GPIO, packet_interval, pico_alarm_pool, 1.0f);
  print_dshot_config(&dshot);

  while (1)
    tight_loop_contents();
}