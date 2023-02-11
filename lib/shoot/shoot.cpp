#include "shoot.h"
#include "dshot.h"

uint32_t shoot::dma_buffer[DSHOT_FRAME_LENGTH] = {0};
uint32_t shoot::temp_dma_buffer[DSHOT_FRAME_LENGTH] = {0};

uint16_t shoot::throttle_code = 0;
uint16_t shoot::telemetry = 0;

uint16_t shoot::writes_to_dma_buffer = 0;
uint16_t shoot::writes_to_temp_dma_buffer = 0;

bool shoot::dma_alarm_rt_state = false;
struct repeating_timer shoot::send_frame_rt;

void shoot::rt_setup()
{
    shoot::dma_alarm_rt_state = alarm_pool_add_repeating_timer_us(tts::pico_alarm_pool, DMA_ALARM_PERIOD, shoot::repeating_send_dshot_frame, NULL, &shoot::send_frame_rt);

    Serial.print("\nDMA Repeating Timer Setup: ");
    Serial.print(shoot::dma_alarm_rt_state);
}

void shoot::send_dshot_frame(bool debug)
{
    // Stop timer interrupt JIC
    // irq_set_enabled(DMA_ALARM_IRQ, false);

    // TODO: add more verbose debugging
    // NOTE: caution as this function is executed as an interrupt service routine
    if (debug)
    {
        Serial.print("Throttle Code: ");
        Serial.println(shoot::throttle_code);
    }

    // IF DMA is busy, then write to temp_dma_buffer
    // AND wait for DMA buffer to finish transfer
    // (NOTE: waiting is risky because this is used in an interrupt)
    // Then copy the temp buffer to dma buffer
    if (dma_channel_is_busy(tts::dma_channel))
    {
        DShot::command_to_pwm_buffer(shoot::throttle_code, shoot::telemetry, shoot::temp_dma_buffer, DSHOT_LOW, DSHOT_HIGH, tts::pwm_channel);
        dma_channel_wait_for_finish_blocking(tts::dma_channel);
        memcpy(shoot::dma_buffer, shoot::temp_dma_buffer, DSHOT_FRAME_LENGTH * sizeof(uint32_t));
        ++shoot::writes_to_temp_dma_buffer;
    }
    // ELSE write to dma_buffer directly
    else
    {
        DShot::command_to_pwm_buffer(shoot::throttle_code, shoot::telemetry, shoot::dma_buffer, DSHOT_LOW, DSHOT_HIGH, tts::pwm_channel);
        ++shoot::writes_to_dma_buffer;
    }
    // Re-configure DMA and trigger transfer
    dma_channel_configure(
        tts::dma_channel,
        &tts::dma_config,
        &pwm_hw->slice[tts::pwm_slice_num].cc, // Write to PWM counter compare
        shoot::dma_buffer,
        DSHOT_FRAME_LENGTH,
        true);

    // Restart interrupt
    // irq_set_enabled(DMA_ALARM_IRQ, true);
    // NOTE: Alarm is only 32 bits
    // so, be careful if delay is more than that
    // uint64_t target = timer_hw->timerawl + DMA_ALARM_PERIOD;
    // timer_hw->alarm[DMA_ALARM_NUM] = (uint32_t)target;
}

bool shoot::repeating_send_dshot_frame(struct repeating_timer *rt)
{
    // Send DShot frame
    shoot::send_dshot_frame(false);
    // CAN DO: Use rt-> for debug
    // Return true so that timer repeats
    return true;
}