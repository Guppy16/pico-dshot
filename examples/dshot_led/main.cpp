/**
 * @file
 * 
 * This file will run dshot at 8 Hz on the builtin led on the RP2040 Pico.
 * At this frequency, the pulses are slow enough to see with your eyes:
 *  A low pulse is a short pulse
 *  A high pulse is a long pulse
 * This is useful to get a sense of the packets being sent,
 * and was primarily used at the beginning to debug bit endianness.
 * 
 * Now, better tools such as unit testing and a logic analyser can be used,
 * but this file has been kept for fun.
*/

#include <string.h>
#include "pico/platform.h"
#include "stdio.h"

#include "dshot.h"

#define LED_BUILTIN 25
#define MOTOR_GPIO LED_BUILTIN
constexpr float dshot_speed = 0.008;           // khz
// At 8 Hz, the packet interval is: 20 bits / 8 Hz = 2.5 s
constexpr int64_t packet_interval_us = 3000000; // 3s interval

int main()
{
  stdio_init_all();

  // Sleep for some time to wait for serial uart to setup
  sleep_ms(1500); // ms

  alarm_pool_t *pico_alarm_pool = alarm_pool_get_default();

  // --- setup dshot configuration ---
  dshot_config dshot;
  dshot_config_init(&dshot, dshot_speed, MOTOR_GPIO, packet_interval_us, pico_alarm_pool);
  print_dshot_config(&dshot);

  dshot.packet.throttle_code = 0b00001111000;
  dshot.packet.telemetry = 0;
  /// NOTE: without this print statement, the compiler optimises out the above code!
  printf("throttle code: %u\n", dshot.packet.throttle_code);

  while (1)
    tight_loop_contents();
}