#pragma once
#include <stdio.h>

#include "hardware/pwm.h"
#include "pico/stdlib.h"

namespace utils {
void print_pwm_config(const pwm_config &conf);

/*! \brief Flash LED on and off `repeat` times with 1000ms delay
 *  \ingroup utils
 *
 *  \param pin LED pin
 * 	\param repeat Number of flashes (on and off). Default = 1
 */
void flash_led(const int &pin, uint repeat = 1);
} // namespace utils
