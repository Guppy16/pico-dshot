# Attempts


### Resending a DSHOT Frame using DMA

Aim: minimise the number of intrsuctions used to resend a dshot frame. This will decrease the overhead on the CPU (note this uses CPU instructoins as opposed to DMA instructions)

To send a dshot frame again, it should take less instructions to just change the read memory address and trigger a DMA send, but this doesn't work:

```cpp
#include "hardware/dma.h"

// Reset DMA read address
// AND set trigger <- true to start sending the frame
dma_channel_set_read_addr(dma_chan, dma_buffer, true);
```

Instead, we need to re-configure the DMA channel as if we were setting it up from the beginning:

```cpp
// Re-configure DMA channel and trigger transfer
dma_channel_configure(
    dma_chan,
    &dma_conf,
    &pwm_hw->slice[pwm_slice_num].cc, // Write to PWM counter compare
    dma_buffer,
    DSHOT_FRAME_LENGTH,
    true);
```

### Setting a low level repeating timer

Aim: Set a low level repeating timer (to trigger a DMA transfer) using minimal instructions

NOTE: it is difficult to figure out which HW timers / alarms have been used already by Arduino. 

```cpp
#include <Arduino.h>
#include "hardware/dma.h"

// NOTE: These must have the same number
#define DMA_ALARM_NUM 1 // HW alarm num
#define DMA_ALARM_IRQ TIMER_IRQ 1

constexpr uint32_t DMA_ALARM_PERIOD = 8000 / 1; // N kHz -> micro secs

void alarm_irq_send_dshot_frame(void)
{
    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << DMA_ALARM_NUM);

    // Resend DShot frame again
    send_dshot_frame();

    // Enable the alarm irq
    // NOTE: This may not actualy need to be reset
    // because it is done in the setup()
    irq_set_enabled(DMA_ALARM_IRQ, true);

    // NOTE: Alarm is only 32 bits
    // so, be careful if delay is more than that
    uint64_t target = timer_hw->timerawl + DMA_ALARM_PERIOD;
    timer_hw->alarm[DMA_ALARM_NUM] = (uint32_t)target;
}

// Routine used in main::setup() to setup alarm
void alarm_setup()
{
    // Enable interrupt for alarm
    hw_set_bits(&timer_hw->inte, 1u << DMA_ALARM_NUM);
    // Set irq handler for alarm irq
    irq_set_exclusive_handler(DMA_ALARM_IRQ, alarm_irq_send_dshot_frame);
    // Enable the alarm irq
    irq_set_enabled(DMA_ALARM_IRQ, true);
    // Start the dma transfer interrupt
    alarm_irq_send_dshot_frame();
}
```

This needs to be tested. I tried this with wrong `send_dshot_frame()`, hence didn't work. I may try this method in the future. It should be more efficient, but isn't so scalable compared to the high level pico alarm pool functionality. I would've like to use the pico default timer pools, but this seems to have been overwritten by `Arduino.h`, specifically, `PICO_TIME_DEFAULT_ALARM_POOL_DISABLED` seems to be overwritten to being `1`, which disables the default timers. 
Below is a snippet using default pico pool timers:

```cpp
struct repeating_timer send_frame_rt;
bool dma_alarm_rt_state = false;

bool repeating_send_dshot_frame(struct repeating_timer *t)
{
    // Send DShot frame
    send_dshot_frame(false);
    // CAN DO: Some debugging possible using repeating_timer struct
    // Return true so that timer repeats
    return true;
}

dma_alarm_rt_state = add_repeating_timer_us(DMA_ALARM_PERIOD, repeating_send_dshot_frame, NULL, &send_frame_rt);
```

This didn't work, for the reasons stated above. Instead, we need to create an alarm pool:

```cpp
#define DMA_ALARM_NUM 1 // HW alarm num

// Create alarm pool
alarm_pool_t *pico_alarm_pool = alarm_pool_create(DMA_ALARM_NUM, PICO_TIME_DEFAULT_ALARM_POOL_MAX_TIMERS);

...

// Set repeating timer
dma_alarm_rt_state = alarm_pool_add_repeating_timer_us(pico_alarm_pool, DMA_ALARM_PERIOD, repeating_send_dshot_frame, NULL, &send_frame_rt);
```
