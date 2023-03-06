#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "shoot.h"

/**
 * @brief Flash LED on and off `repeat` times with 1s delay
 * Ideally this should be implemented with timers
 * @param led_pin 
 * @param repeat 
 */
void flash_led(const int &led_pin, uint repeat = 1) {
  while (repeat--) {
    gpio_put(led_pin, 1);
    sleep_ms(1000);
    gpio_put(led_pin, 0);
    sleep_ms(1000);
  }
}

/**
 * @brief Read key input and execute command
 * 
 * @param key_input 
 * @return true 
 * @return false Not implemented
 * 
 * @attention 
 * Key presses are stored in a buffer in the pico, 
 * hence spamming a key will cause downstream keys 
 * to be executed later than expected.
 * This is especially noticeable when @ref DEBUG is set
 */
bool update_signal(const int &key_input) {
  printf("Processing key input %i\n", key_input);

  // l - led
  if (key_input == 108) {
    flash_led(LED_BUILTIN);
  }

  // b - beep
  if (key_input == 98) {
    shoot::throttle_code = 1;
    shoot::telemetry = 1;
  }

  // d - details
  if (key_input == 100) {
    printf("Writes to primary buffer: %li\tsecondary buffer: %li\n", shoot::writes_to_dma_buffer, shoot::writes_to_temp_dma_buffer);
  }

  // r - rise
  if (key_input == 114) {
    if (shoot::throttle_code >= ZERO_THROTTLE and
        shoot::throttle_code <= MAX_THROTTLE) {
      shoot::throttle_code =
          MIN(shoot::throttle_code + THROTTLE_INCREMENT, MAX_THROTTLE);

      printf("Throttle: %i\n", shoot::throttle_code - ZERO_THROTTLE);
      shoot::throttle_code == MAX_THROTTLE &&printf("Max Throttle reached\n");
    } else {
      printf("Motor is not in throttle mode\n");
    }
  }

  // f - fall
  if (key_input == 102) {
    if (shoot::throttle_code <= MAX_THROTTLE &&
        shoot::throttle_code >= ZERO_THROTTLE) {
      shoot::throttle_code =
          MAX(shoot::throttle_code - THROTTLE_INCREMENT, ZERO_THROTTLE);

      printf("Throttle: %i\n", shoot::throttle_code - ZERO_THROTTLE);
      shoot::throttle_code == ZERO_THROTTLE &&printf("Throttle is zero\n");
    } else {
      printf("Motor is not in throttle mode\n");
    }
  }

  // spacebar - send zero throttle
  if (key_input == 32) {
    shoot::throttle_code = ZERO_THROTTLE;
    shoot::telemetry = 0;
    printf("Throttle: %i\n", 0);
  }
  
  printf("Finished processing key input");

  return true;
}

int main() {

  // Set MCU clock frequency
  set_sys_clock_khz(MCU_FREQ * 1e3, false);

  stdio_init_all();

  // Setup builtin led to check if pico is running
  gpio_init(LED_BUILTIN);
  gpio_set_dir(LED_BUILTIN, GPIO_OUT);
  flash_led(LED_BUILTIN, 3);

  // --- setup dshot configurations ---
  // pwm is setup first
  tts::pwm_setup();
  // dma is setup next, because dma dreq requires tts::pwm_slice_num
  tts::dma_setup();
  // finally, repeating timer is setup to send the dshot frames repeatedly
  shoot::rt_setup();

  // sleep a bit, so that computer serial can connect to pico
  sleep_ms(1500);

  printf("MCU Freq (kHz): Expected %d Actual %d\n", MCU_FREQ * 1000,
         frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS));

  tts::print_gpio_setup();
  tts::print_dshot_setup();
  tts::print_dma_setup();
  shoot::print_rt_setup();

  printf("Initial throttle: %i\n", shoot::throttle_code);
  printf("Initial telemetry: %i\n", shoot::telemetry);

  int key_input = 0;
  while (1) {
    key_input = getchar_timeout_us(0);
    key_input != PICO_ERROR_TIMEOUT && update_signal(key_input);
    sleep_ms(100);
  }
}
