#include "utils.h"

void utils::print_pwm_config(const pwm_config &conf) {
  printf("Conf:");
  printf(" csr: %i", conf.csr);
  printf(" div: %i", conf.div);
  printf(" top: %i\n", conf.top);
}

void utils::flash_led(const int &pin, uint repeat) {
  while (repeat--) {
    gpio_put(pin, 1);
    sleep_ms(1000);
    gpio_put(pin, 0);
    sleep_ms(1000);
  }
}
