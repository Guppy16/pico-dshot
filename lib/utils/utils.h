#pragma once
#include <Arduino.h>
#include "hardware/pwm.h"

namespace utils
{
    void print_pwm_config(const pwm_config &conf);

    void flash_led(const pin_size_t &pin);

}
