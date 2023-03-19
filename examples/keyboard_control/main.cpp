/**
 * @file main.cpp
 * @example main.cpp
 *
 * Example to send a dshot command based on a key press.
 *
 */

#include "pico/platform.h"
#include "stdio.h"
#include <string.h>

#include "dshot.h"

#define LED_BUILTIN 25
constexpr uint esc_gpio = 14;
constexpr float dshot_speed = 1200.0f;
constexpr int64_t packet_interval_us = 1000 / 7; // 7 kHz packet freq
constexpr uint16_t throttle_increment = 50;

/**
 * @brief Flash LED on and off `repeat` times with 1s delay.
 * This is useful to check the pico is responsive.
 * @param led_pin
 * @param repeat
 */
void flash_led(const int &led_pin, uint repeat = 1) {
  while (repeat--) {
    gpio_put(led_pin, 1);
    sleep_ms(500);
    gpio_put(led_pin, 0);
    sleep_ms(500);
  }
}

/**
 * @brief Map key input to a dshot command
 *
 * @param key_input keyboard input
 * @param dshot dshot_config to set the throttle code and telemetry
 * depending on the key input
 * @return true
 * @return false Not implemented
 *
 * @attention
 * Key presses are stored in a buffer in the pico,
 * hence spamming a key will cause downstream keys
 * to be executed later than expected.
 */
bool update_signal(const int &key_input, dshot_config &dshot) {
  printf("Processing key input %i\n", key_input);

  switch (key_input) {
  // b - beep
  case 98:
    dshot.packet.throttle_code = 1;
    dshot.packet.telemetry = 1;
    break;

  // r - rise
  case 114:
    // Check if dshot is in throttle mode
    if (dshot.packet.throttle_code >= DSHOT_ZERO_THROTTLE) {
      dshot.packet.throttle_code = MIN(
          dshot.packet.throttle_code + throttle_increment, DSHOT_MAX_THROTTLE);
      printf("Throttle: %i\n",
             dshot.packet.throttle_code - DSHOT_ZERO_THROTTLE);
      dshot.packet.throttle_code ==
          DSHOT_MAX_THROTTLE &&printf("Max Throttle reached\n");
    } else {
      printf("Motor is not in throttle mode\n");
    }
    break;

  // f - fall
  case 102:
    // Check if dshot is in throttle mode
    if (dshot.packet.throttle_code >= DSHOT_ZERO_THROTTLE) {
      dshot.packet.throttle_code = MAX(
          dshot.packet.throttle_code - throttle_increment, DSHOT_ZERO_THROTTLE);

      printf("Throttle: %i\n",
             dshot.packet.throttle_code - DSHOT_ZERO_THROTTLE);
      dshot.packet.throttle_code ==
          DSHOT_ZERO_THROTTLE &&printf("Throttle is zero\n");
    } else {
      printf("Motor is not in throttle mode\n");
    }
    break;

  // spacebar - send zero throttle
  case 32:
    dshot.packet.throttle_code = DSHOT_ZERO_THROTTLE;
    dshot.packet.telemetry = 0;
    printf("Throttle: 0\n");
    break;

  // l - led: flash led on pico to check it is responsive
  // ironically, this is a blocking process
  case 108:
    flash_led(LED_BUILTIN, 1);
    break;

  default:
    printf("Key is not registered with a command\n");
  }

  printf("Finished processing key input\n");

  return true;
}

int main() {
  stdio_init_all();

  // setup builtin led
  gpio_init(LED_BUILTIN);
  gpio_set_dir(LED_BUILTIN, GPIO_OUT);

  // flash led to wait a few seconds for serial uart to setup
  // also to check if the pico is responsive
  flash_led(LED_BUILTIN, 2);

  // Get the default alarm pool. This is passed in to the dshot init,
  // so that we can setup a repeating timer to send dshot packets
  alarm_pool_t *pico_alarm_pool = alarm_pool_get_default();

  // initialise dshot config
  dshot_config dshot;
  dshot_config_init(&dshot, dshot_speed, esc_gpio, packet_interval_us,
                    pico_alarm_pool);
  print_dshot_config(&dshot);

  int key_input = 0;
  while (1) {
    // Read key input from keyboard
    key_input = getchar_timeout_us(0);
    key_input != PICO_ERROR_TIMEOUT &&update_signal(key_input, dshot);
    // sleep_ms(100);
  }
}
