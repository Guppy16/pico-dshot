#include "tts.h"

// PWM

void tts::pwm_setup()
{
    gpio_set_function(MOTOR_GPIO, GPIO_FUNC_PWM);

    // Wrap is the number of counts in each PWM period
    pwm_set_wrap(tts::pwm_slice_num, DSHOT_PWM_WRAP);

    // 0 duty cycle by default
    pwm_set_chan_level(tts::pwm_slice_num, tts::pwm_channel, 0); 

    // Set clock divier
    pwm_set_clkdiv(tts::pwm_slice_num, DSHOT_PWM_DIV);  

    // Turn on PWM
    pwm_set_enabled(tts::pwm_slice_num, true);
}
