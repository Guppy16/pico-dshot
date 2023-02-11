#include <Arduino.h>
#include "pico/time.h"

// #include "hardware/dma.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

// #include "tts.h"
// #include "dshot.h"
#include "shoot.h"
#include "utils.h"

// TODO: Change default behviour to: bool debug = false
// void send_dshot_frame(bool = true);

// DMA Alarm config
// ISR to send DShot frame over DMA
bool repeating_send_dshot_frame(struct repeating_timer *rt)
{
    // Send DShot frame
    shoot::send_dshot_frame(false);
    // CAN DO: Use rt-> for debug
    // Return true so that timer repeats
    return true;
}

// Keep track of repeating timer status
bool dma_alarm_rt_state = false;
// Setup a repeating timer configuration
struct repeating_timer send_frame_rt;
// Create alarm pool
alarm_pool_t *pico_alarm_pool = alarm_pool_create(DMA_ALARM_NUM, PICO_TIME_DEFAULT_ALARM_POOL_MAX_TIMERS);
// uint16_t throttle_code = 0;
// uint16_t telemtry = 0;

void setup()
{
    Serial.begin(115200);

    // Flash LED
    utils::flash_led(LED_BUILTIN);

    // pwm config
    // Should PWM start before or after DMA config?
    // I think before, so that DMA doesn't write to invalid memory?
    tts::pwm_setup();

    // dma config
    tts::dma_setup();

    // Set repeating timer
    dma_alarm_rt_state = alarm_pool_add_repeating_timer_us(pico_alarm_pool, DMA_ALARM_PERIOD, repeating_send_dshot_frame, NULL, &send_frame_rt);

    delay(1500);

    tts::print_gpio_setup();
    tts::print_dshot_setup();
    tts::print_pwm_setup();
    tts::print_dma_setup();

    Serial.print("\nDMA Repeating Timer Setup: ");
    Serial.print(dma_alarm_rt_state);

    Serial.print("Initial throttle: ");
    Serial.print(shoot::throttle_code);
    Serial.print("\tInitial telemetry: ");
    Serial.println(shoot::telemetry);
}

int incomingByte;

/// NOTE
// I think arm_sequence should be coded such that it returns
// a throttle code. This will make the main loop more efficient
// Note that the state of the arm sequence will still need
// to be tracked somehow.
// This could be done efficiently using the first 16 bits!?s

void loop()
{
    if (Serial.available() > 0)
    {
        incomingByte = Serial.read();
        Serial.print("Byte: ");
        Serial.println(incomingByte, DEC);

        // l (led)
        if (incomingByte == 108)
        {
            utils::flash_led(LED_BUILTIN);
        }

        // a (arm)
        // if (incomingByte == 97)
        // {
        //     // Disable timer
        //     cancel_repeating_timer(&send_frame_rt);
        //     // Arm motor
        //     arm_motor();
        //     // Re-enable timer
        //     dma_alarm_rt_state = alarm_pool_add_repeating_timer_us(pico_alarm_pool, DMA_ALARM_PERIOD, repeating_send_dshot_frame, NULL, &send_frame_rt);
        //     Serial.print("Re-enabled repeating DMA alarm: ");
        //     Serial.println(dma_alarm_rt_state);

        //     return;
        // }

        // b - beep
        if (incomingByte == 98)
        {
            shoot::throttle_code = 1;
            shoot::telemetry = 1;
        }

        // s - spin
        // if (incomingByte == 115)
        // {
        //     // Disable timer
        //     cancel_repeating_timer(&send_frame_rt);
        //     // Ramp up to speed ARM_THROTTLE + 100
        //     ramp_motor();
        //     // Re-enable timer
        //     dma_alarm_rt_state = alarm_pool_add_repeating_timer_us(pico_alarm_pool, DMA_ALARM_PERIOD, repeating_send_dshot_frame, NULL, &send_frame_rt);
        //     Serial.print("Re-enabled repeating DMA alarm: ");
        //     Serial.println(dma_alarm_rt_state);
        // }

        // r - rise
        if (incomingByte == 114)
        {
            if (shoot::throttle_code >= ZERO_THROTTLE and shoot::throttle_code <= MAX_THROTTLE)
            {
                // Check for max throttle
                if (shoot::throttle_code == MAX_THROTTLE)
                {
                    Serial.println("Max Throttle reached");
                }
                else
                {
                    shoot::throttle_code += 1;
                    Serial.print("Throttle: ");
                    Serial.println(shoot::throttle_code);
                }
            }
            else
            {
                Serial.println("Motor is not in throttle mode");
            }
        }

        // f - fall
        if (incomingByte == 102)
        {
            if (shoot::throttle_code <= MAX_THROTTLE && shoot::throttle_code >= ZERO_THROTTLE)
            {
                if (shoot::throttle_code == ZERO_THROTTLE)
                {
                    Serial.println("Throttle is zero");
                }
                else
                {
                    shoot::throttle_code -= 1;
                    Serial.print("Throttle: ");
                    Serial.println(shoot::throttle_code);
                }
            }
            else
            {
                Serial.println("Motor is not in throttle mode");
            }
        }

        // spacebar = 0 throttle --> This could be disarm.
        // Not sure how to disarm yet. Maybe set throttle to 0 and don't send a cmd for some secs?
        if (incomingByte == 32)
        {
            shoot::throttle_code = ZERO_THROTTLE;
            shoot::telemetry = 0;
        }

        // NOTE: In Debug mode, sending a DSHOT Frame takes a lot of time!
        // So it may seem as if the PICO is unable to detect key presses
        // while sending commands!
        // But is this even needed?
        shoot::send_dshot_frame();

        Serial.println("Finished processing byte.");
    }
}