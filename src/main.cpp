#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "shoot.h"
#include "utils.h"

void loop() {
    char key_input[1024];

    scanf("%1024s", key_input);
    printf("Byte: %1024s", key_input);

    // l (led)
    if (strcmp(key_input, "l") == 0) {
        utils::flash_led(LED_BUILTIN);
    }

    // a (arm)
    // if (strcmp(key_input, ) == 97)
    // {
    //     // Disable timer
    //     cancel_repeating_timer(&send_frame_rt);
    //     // Arm motor
    //     arm_motor();
    //     // Re-enable timer
    //     dma_alarm_rt_state =
    //     alarm_pool_add_repeating_timer_us(pico_alarm_pool,
    //     DMA_ALARM_PERIOD, repeating_send_dshot_frame, NULL,
    //     &send_frame_rt); printf("Re-enabled repeating DMA alarm:
    //     "); printfln(dma_alarm_rt_state);

    //     return;
    // }

    // b - beep
    if (strcmp(key_input, "b") == 0) {
        shoot::throttle_code = 1;
        shoot::telemetry = 1;
    }

    // s - spin
    // if (strcmp(key_input, ) == 115)
    // {
    //     // Disable timer
    //     cancel_repeating_timer(&send_frame_rt);
    //     // Ramp up to speed ARM_THROTTLE + 100
    //     ramp_motor();
    //     // Re-enable timer
    //     dma_alarm_rt_state =
    //     alarm_pool_add_repeating_timer_us(pico_alarm_pool,
    //     DMA_ALARM_PERIOD, repeating_send_dshot_frame, NULL,
    //     &send_frame_rt); printf("Re-enabled repeating DMA alarm:
    //     "); printfln(dma_alarm_rt_state);
    // }

    // r - rise
    if (strcmp(key_input, "r") == 0) {
        if (shoot::throttle_code >= ZERO_THROTTLE and
            shoot::throttle_code <= MAX_THROTTLE) {
            // Check for max throttle
            if (shoot::throttle_code == MAX_THROTTLE) {
                printf("Max Throttle reached\n");
            } else {
                shoot::throttle_code += 1;
                printf("Throttle: %i", shoot::throttle_code);
            }
        } else {
            printf("Motor is not in throttle mode\n");
        }
    }

    // f - fall
    if (strcmp(key_input, "f") == 0) {
        if (shoot::throttle_code <= MAX_THROTTLE &&
            shoot::throttle_code >= ZERO_THROTTLE) {
            if (shoot::throttle_code == ZERO_THROTTLE) {
                printf("Throttle is zero\n");
            } else {
                shoot::throttle_code -= 1;
                printf("Throttle: %i", shoot::throttle_code);
            }
        } else {
            printf("Motor is not in throttle mode\n");
        }
    }

    // q - disarm
    // Not sure how to disarm yet. Maybe set throttle to 0 and don't send a
    // cmd for some secs?
    if (strcmp(key_input, "q") == 0) {
        shoot::throttle_code = ZERO_THROTTLE;
        shoot::telemetry = 0;
    }

    // NOTE: In DEBUG mode, sending a DSHOT Frame takes a lot of time!
    // So it may seem as if the PICO is unable to detect key presses
    // while sending commands!
    // But is this even needed?
    shoot::send_dshot_frame();

    printf("Finished processing byte.\n");
}

int main() {
    stdio_init_all();

    gpio_init(LED_BUILTIN);
    gpio_set_dir(LED_BUILTIN, GPIO_OUT);

    // Flash LED
    utils::flash_led(LED_BUILTIN);

    // pwm config
    // Note that PWM needs to be setup first,
    // because the dma dreq requires tts::pwm_slice_num
    tts::pwm_setup();

    // dma config
    tts::dma_setup();

    // Set repeating timer
    // NOTE: this can be put in main loop to start
    // repeating timer on key press (e.g. a for arm)
    shoot::rt_setup();

    sleep_ms(1500);

    tts::print_gpio_setup();
    tts::print_dshot_setup();
    tts::print_dma_setup();

    printf("Initial throttle: %i", shoot::throttle_code);
    printf("\tInitial telemetry: %i", shoot::telemetry);

    while (1) loop();
}

