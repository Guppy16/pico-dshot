/**
 * @file
 * 
 * This file will run dshot at 8 Hz on the builtin led on the RP2040 Pico.
 * This is useful to get a sense of what commands are being sent.
 * This can be used for extremely basic debugging.
 * More advanced debugging should use a [INSERT]
 * NOTE: you may need to setup dshot so that it can accept float / hz
 * 
 * This is because the equivalent dshot speed is 8 Hz
 * Some usefule checks would be for:
 * \a DSHOT_PWM_WRAP = 1 << 16 - 1
 * \a DSHOT_PWM_DIV = 240
 * \a DMA_ALARM_PERIOD = \a packet_interval = 3 000 000
 * The latter is set as follows:
 * 20 bits / 8 Hz = 2.5 s (time between start of frames)
*/

#include <string.h>
#include "pico/platform.h"
#include "stdio.h"

#include "dshot.h"

#define LED_BUILTIN 25
#define MOTOR_GPIO LED_BUILTIN
constexpr float dshot_speed = 0.008;           // khz
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
  printf("throttle code: %u\n", dshot.packet.throttle_code);

  while (1)
    tight_loop_contents();
}