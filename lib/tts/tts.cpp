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

// DMA

dma_channel_config tts::dma_config = dma_channel_get_default_config(tts::dma_channel);

void tts::dma_setup()
{
    // PWM counter compare is 32 bit (16 bit for each pwm channel)
    // NOTE: Technically we are only using one channel, 
    // so this will make the other channel useless
    channel_config_set_transfer_data_size(&tts::dma_config, DMA_SIZE_32);
    // Increment read address
    channel_config_set_read_increment(&tts::dma_config, true);
    // NO increment write address
    channel_config_set_write_increment(&tts::dma_config, false);
    // DMA Data request when PWM is finished
    // This is because 16 DMA transfers are required to send a dshot frame
    channel_config_set_dreq(&tts::dma_config, DREQ_PWM_WRAP0 + tts::pwm_slice_num);
}