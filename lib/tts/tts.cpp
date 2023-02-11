#include "tts.h"

// PWM

void tts::pwm_setup()
{
    gpio_set_function(MOTOR_GPIO, GPIO_FUNC_PWM);

    // Wrap is the number of counts in each PWM period
    pwm_set_wrap(tts::pwm_slice_num, DSHOT_PWM_WRAP);

    // 0 duty cycle by default
    pwm_set_chan_level(tts::pwm_slice_num, tts::pwm_channel, 0); 

    // Clock divider slows down the PWM period
    // This is default set to 1 (i.e. no divider) for fastest speed
    // DEBUG is used to slow down the pwm period to ~ 1/8 s
    // 120 MHz / (240 * WRAP), where WRAP = 2^16  - 1 in debug mode
    pwm_set_clkdiv(tts::pwm_slice_num, DEBUG ? 240.0f : 1.0f);  

    // Turn on PWM
    pwm_set_enabled(tts::pwm_slice_num, true);
}
