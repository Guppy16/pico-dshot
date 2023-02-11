#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "shoot.h"
#include "utils.h"

bool update_signal(const int &key_input) {
  // l (led)
  if (key_input == 108) {
    utils::flash_led(LED_BUILTIN);
  }

  // b - beep
  if (key_input == 98) {
    shoot::throttle_code = 1;
    shoot::telemetry = 1;
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

  // NOTE: In DEBUG mode, sending a DSHOT Frame takes a lot of time!
  // So it may seem as if the PICO is unable to detect key presses
  // while sending commands!
  // But is this even needed?
  shoot::send_dshot_frame();
  printf("Finished processing key input %i\n", key_input);

  return true;
}

int main() {
  // Set MCU clock frequency. Should we assert this?
  set_sys_clock_khz(MCU_FREQ * 1e3, false);

  int key_input;

  stdio_init_all();
  gpio_init(LED_BUILTIN);
  gpio_set_dir(LED_BUILTIN, GPIO_OUT);

  // Flash LED on and off 3 times
  utils::flash_led(LED_BUILTIN, 3);

  // Setup DShot

  // pwm config
  // Note that PWM needs to be setup first,
  // because the dma dreq requires tts::pwm_slice_num
  tts::pwm_setup();
  tts::dma_setup();
  // Set repeating timer
  // NOTE: this can be put in main loop to start
  // repeating timer on key press (e.g. a for arm)
  shoot::rt_setup();

  sleep_ms(1500);

  printf("MCU Freq (kHz): Expected %d Actual %d\n", MCU_FREQ * 1000,
         frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS));

  tts::print_gpio_setup();
  tts::print_dshot_setup();
  tts::print_dma_setup();
  shoot::print_rt_setup();

  printf("Initial throttle: %i\n", shoot::throttle_code);
  printf("Initial telemetry: %i\n", shoot::telemetry);

  while (1) {
    key_input = getchar_timeout_us(0);
    key_input != PICO_ERROR_TIMEOUT && update_signal(key_input);
    sleep_ms(100);
  }
}
