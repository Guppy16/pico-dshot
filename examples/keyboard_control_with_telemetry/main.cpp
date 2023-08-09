/**
 * @file main.cpp
 * @example main.cpp
 *
 * Example to send a dshot command based on a key press.
 * You can also request telemetry upon a key press.
 * TODO: play with uart FIFO to see if we can reduce latenct of keyboard
 * interpretation (in main)
 */

#include "pico/platform.h"
#include "stdio.h"
#include <string.h>

#include "dshot.h"
#include "onewire.h"

#define LED_BUILTIN 25
constexpr uint esc_gpio = 14;
constexpr float dshot_speed = 1200.0f;
constexpr int64_t packet_interval_us = 1000 / 7; // 7 kHz packet freq
constexpr uint16_t throttle_increment = 50;
constexpr uint onewire_gpio = 13;
constexpr long int onewire_delay_us = 1e6;

// Get the default alarm pool. This is passed in to the dshot init,
// so that we can setup a repeating timer to send dshot packets
alarm_pool_t *pico_alarm_pool = alarm_pool_get_default();

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

bool send_beep_rt_state = false;
repeating_timer_t send_beep_rt; // Repeating timer config for beeper

/**
 * @brief repeating isr to send a beep command
 *
 * @param rt
 * @return bool(send_beep_rt_state). True if isr should repeat
 */
bool repeating_dshot_beep(repeating_timer_t *rt) {
  dshot_config *dshot = (dshot_config *)(rt->user_data);

  // Handle Request telemetry
  dshot->packet.throttle_code = 0;
  // dshot->packet.telemetry = 1;
  // dshot_send_packet(dshot, false);

  // Beep command: 1;1
  dshot->packet.throttle_code = 1;
  dshot->packet.telemetry = 1;
  dshot_send_packet(dshot, false);
  dshot->packet.throttle_code = 0;


  return send_beep_rt_state;
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
    // Set throttle code to 0
    dshot.packet.throttle_code = 0;
    dshot.packet.telemetry = 0;
    dshot_send_packet(&dshot, false);

    // Create a repeating timer to send a beep dshot frame every 260 ms
    send_beep_rt_state = !send_beep_rt_state;
    if (send_beep_rt_state) {
      send_beep_rt_state = alarm_pool_add_repeating_timer_ms(
          pico_alarm_pool, 270, repeating_dshot_beep, &dshot, &send_beep_rt);
    }
    printf("Beeper rt state:\t%d\n", send_beep_rt_state);

    // Hopefully, this will allow telemetry frames to be sent effectively
    // dshot.packet.throttle_code = 1;
    // dshot.packet.telemetry = 1;
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

  // t = telemetry
  case 116:
    onewire.send_req_rt_state = !onewire.send_req_rt_state;
    if (onewire.send_req_rt_state) {
      onewire_rt_configure(&onewire, onewire_delay_us, pico_alarm_pool);
      onewire.buffer_idx = 0;
    }
    break;

  default:
    printf("Key is not registered with a command\n");
  }

  printf("Finished processing key input\n");

  return true;
}

int main() {
  stdio_init_all();
  // uart_set_fifo_enabled(uart1, false);

  // setup builtin led
  gpio_init(LED_BUILTIN);
  gpio_set_dir(LED_BUILTIN, GPIO_OUT);

  // flash led to wait a few seconds for serial uart to setup
  // also to check if the pico is responsive
  flash_led(LED_BUILTIN, 2);

  // initialise dshot config
  dshot_config dshot;
  dshot_config_init(&dshot, dshot_speed, esc_gpio, packet_interval_us,
                    pico_alarm_pool);
  print_dshot_config(&dshot);

  // Initialise telemetry
  dshot_config *dshots[ESC_COUNT] = {&dshot};
  telem_uart_init(&onewire, uart0, onewire_gpio, pico_alarm_pool,
                  onewire_delay_us, dshots, false, true);
  print_onewire_config(&onewire);
  // This may be required due to an intial blip in the uart while the ESC is
  // powering up / first receiving dshot commands
  onewire.buffer_idx = KISS_ESC_TELEM_BUFFER_SIZE;
  /// TODO: print whenever we receieve new telemetry data

  int key_input = 0;
  while (1) {
    // Read key input from keyboard
    key_input = getchar_timeout_us(0);
    key_input != PICO_ERROR_TIMEOUT &&update_signal(key_input, dshot);
    // sleep_ms(100);
  }
}
