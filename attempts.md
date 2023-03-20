# Attempts

## Setting default values for a typedef C struct

Forums suggest that this shouldn't work:

```cpp
extern "C" {
  typedef struct dshot_packet{
    uint32_t volatile packet_buffer[dshot_packet_length] = {0};
    uint16_t throttle_code = 0;
    uint16_t telemetry = 0;
    const uint32_t pulse_high;
    const uint32_t pulse_low;
  } dshot_packet_t;
}
```

Here, a `dshot_packet` _C_ struct is defined, and the associated type is defined as `dshot_packet_t`. There are default values defined here. Forums suggest that because this is a _type_, default values are not allowed. However, this compiles fine, which could be because the compiler could interpret this as cpp code.

## Resending a DSHOT Frame using DMA

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

## Setting a low level repeating timer

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

## BLHeli _Arm_ Sequence

Aim: _Arm_ a BLHeli motor as per its spec.

For a motor, a commands are only recognised once the motor is _Armed_. This is not to be confused by arming a drone, which is similar in that a drone's motors doesn't spin until it's armed; however, the motor can still accept commands such as beeping while the drone is disarmed.

To arm a BLHeli motor, the [BLHeli_32 manual](https://github.com/bitdump/BLHeli/blob/master/BLHeli_32%20ARM/BLHeli_32%20manual%20ARM%20Rev32.x.pdf) tells us to ramp up the motor (to a throttle less than 50%), then ramp it down. During the arm, the max throttle reached becomes the minimum thorttle required to start spinning the motor.

Implementing this arm procedure, showed that the ramp takes a certain amount of time (however long the beeps last, which I think is around 100 ms).

```cpp
constexpr uint16_t ARM_THROTTLE = 300;  // < 50% MAX_THROTTLE
constexpr uint16_t MAX_THROTTLE = 2047; // 2^12 - 1
constexpr uint16_t THROTTLE_ZERO = 48;  // 0 Throttle code

void arm_motor()
{
    // Debugging
    writes_to_temp_dma_buffer = 0;
    writes_to_dma_buffer = 0;

    uint64_t duration;    // milli seconds
    uint64_t target_time; // micro seconds
    uint n = 101 - 1;     // Num of commands to send on rise and fall

    // Send 0 command for 200 ms
    throttle_code = THROTTLE_ZERO;
    telemtry = 0;
    duration = 200; // ms
    target_time = timer_hw->timerawl + duration * 1000;
    // TODO: re implement as a timer interrupt
    while (timer_hw->timerawl < target_time)
        send_dshot_frame();

    // Increase throttle from 0 to ARM_THROTTLE in n steps
    // < 20 ms time for Dshot 150, 20 bit frame length, n = 100
    telemtry = 0;
    for (uint16_t i = 0; i <= n; ++i)
    {
        throttle_code = THROTTLE_ZERO + i * (ARM_THROTTLE - THROTTLE_ZERO) / n;
        send_dshot_frame();
    }

    // Send ARM_THROTTLE command for 300 ms
    throttle_code = ARM_THROTTLE;
    telemtry = 0;
    duration = 300; // ms
    target_time = timer_hw->timerawl + duration * 1000;
    // TODO: re implement as a timer interrupt
    while (timer_hw->timerawl < target_time)
        send_dshot_frame();

    // Decrease throttle to 48
    // < 20 ms time for Dshot 150, 20 bit frame length, n = 100
    telemtry = 0;
    for (uint16_t i = n; i < n; --i)
    {
        throttle_code = THROTTLE_ZERO + i * (ARM_THROTTLE - THROTTLE_ZERO) / n;
        send_dshot_frame();
    }

    // Send THROTTLE_ZERO for 500 ms
    throttle_code = THROTTLE_ZERO;
    telemtry = 0;
    duration = 500; // ms
    target_time = timer_hw->timerawl + duration * 1000;
    // TODO: re implement as a timer interrupt
    while (timer_hw->timerawl < target_time)
        send_dshot_frame();

    Serial.println();
    Serial.print("Writes to temp dma buffer: ");
    Serial.println(writes_to_temp_dma_buffer);
    Serial.print("Writes to dma buffer: ");
    Serial.println(writes_to_dma_buffer);
}
```

However, upon implementation of the repeating timer alarm to send a dshot frame, the motor armed itself without having to ramp up the throttle. I believe what this is actually doing is setting the arm throttle to 0. (This needs to be confirmed by actually performing the arm procedure).

I think arm_sequence should be coded such that it returns a throttle code. This will make the code testable.
To make this non-blocking / concurrent, we can set a flag. I'm not sure if this will work, because the serial may not be read in time.
Note that the state of the arm sequence may need to be tracked using a `static` parameter.

## Premature optimisation of `dshot_send_packet`

Motivation: It takes some time to calculate a packet (in `dshot_cmd_to_packet`) before sending it to the DMA hw. We can pipeline this by using two buffers:

- a primary buffer stores the current frame being sent by DMA
- a second buffer to store the next frame

```c
void dshot_send_packet(dshot_config *dshot){
  // write to secondary buffer if dma is busy
  if (dma_channel_is_busy(dshot->dma_channel)){
    secondary_buffer = dshot_packet_compose(&dshot->packet);
    dma_channel_wait_for_finish_blocking(dshot->dma_channel);
    memcpy(primary_buffer, secondary_buffer, dshot_packet_length * sizeof(uint32_t))
  }else{
    primary_buffer = dshot_packet_compose(&dshot->packet);
  }
  // Re-configure dma and trigger transfer
  dma_channel_configure(
      dshot->dma_channel, &dshot->dma_config,
      // Write to pwm counter compare
      &pwm_hw->slice[pwm_gpio_to_slice_num(dshot->esc_gpio_pin)].cc,
      dshot->packet.packet_buffer, dshot_packet_length, true);
}
```

This function is called as an isr to a repeating timer. The interval of this timer, however, is configured to be greater than the time taken to send a packet. This means that by the time `dshot_send_packet` is called, it will have finished sending the packet, so the secondary buffer will never be used.

I don't think it makes sense to have this pre-calculation (for example, even with a faster loop frequency), because we'll be calculating values faster than we can send them, which is pointless.

Also, note that `dma_channel_wait_for_finish_blocking()` could be "risky" because this is executed as an interrupt service routine, which expects the routine to finish within a certain amount of time.

Further optimisation can be done by checking if the current dshot code and telem are the same as the next one, in which case there is no need to recalculate the frame.

## Converting pwm period to wrap and div

A dshot bit is transmit using the pwm hw (remember that each bit is represented as a high or low duty cycle in a _pulse_).
Note that a pulse duration is equivalent to the period of a pwm cycle; for clarity, we refer to this as the _pulse period_.
Also note that the pulse period is the same for each bit.
In `dshot.h::dshow_pwm_configure()`, we specify `pwm_period`, which is the number of cycles the mcu clock will do in a pulse period.

The pico micro-architecture is implemented such that each pwm hw has it's own clock as well as a counter.
The clock is setup using a divider (`div`) relative to the mcu clock, which feeds into the counter.
Once the counter reaches a value set by `wrap` (aka `top`), the pwm counter resets for the next pulse.
This means that `pwm_period` needs to be factorised into a value for the wrap and divider:

$\text{period = div} \times \text{wrap}$

This factorisation is limited by the precision and max values that the variables can hold:

| var  | bits | bit representation    | max      |
| ---- | ---- | --------------------- | -------- |
| wrap | 16   | uint16_t              | 65535    |
| div  | 12   | 8 bit int, 4 bit frac | 255.9375 |

I'm not sure of an algorithm that can efficiently calculate the optimum split.
I suspect it is smth similar to the calculation used to calculate `vco` in `pico-sdk/src/rp2_common/hardware_clocks/scripts/vcocalc.py`.
However, an extravagent method is not needed. For all reasonable dshot values, we can get an accurate period, while maximising the value of `wrap` (this is explained later):

```c
static inline void pwm_period_to_div_wrap(const float period, float *const div,
                                          uint16_t *const wrap) {
  const float max_div = 255.9375;
  const uint max_wrap = (1u << 16) - 1;

  if (period > max_div * max_wrap) {
    panic("pwm_period of %d is higher than max %d\n", period,
          max_div * max_wrap);
  }

  if (period <= max_wrap){
    *wrap = period;
    *div = 1.0f;
  }else{
    *wrap = max_wrap;
    *div = period / *wrap;
  }
}
```

The reason for maximising wrap, is becauase downstream code uses the value of `wrap` to set high and low values of a bit in a dshot pulse:

```c
static inline void dshot_packet_configure(...){
  ...
  const uint32_t pulse_period = dshot->pwm_conf.top; // wrap, aka top
  ...
  pulse_high = (uint32_t)(0.75 * pulse_period) << packet_shift,
  pulse_low = (uint32_t)(0.37 * pulse_period) << packet_shift
}
```

The values of `pulse_high` and `pulse_low` set the duty cycle on the pwm hw, which is used to transmit the pulses of a dshot packet. It is useful for these to be high numbers, so that it is easier to distinguish between the two pulses.

### Aside: DSHOT freq limit based on PWM hw

From the above snippet, we can see that if `wrap = pulse_period = 10`, then we can just about represent the high and low bits as: `pulse_high = 7` and `pulse_low = 3`.
This is probably the bare minimum `wrap` we can use for the dshot protocol to distinguish between a high and low pulse. This motivates our calculation for an upper and lower bound of _dshot speed_...

Recall that the dshot speed sets the pulse period as:

$\text{pulse period (ms)} = 1 \text{ / dshot speed (kHz)}$

this can be converted to a `pwm_period` (measured in mcu clk cycles):

$\text{pwm period (cycles)} = \text{pulse period} \times \text{mcu freq} = \frac{\text{mcu freq (kHz)}}{\text{dshot speed (kHz)}}$

From earlier, we also know that:

$\text{pwm period (cycles) = div} \times \text{wrap}$

Rearranging for _dshot speed_:

$\text{dshot speed} = \frac{f_{mcu}}{\text{div} \times \text{wrap}}$

From this we can calculate the min and max dshot speeds on the RP2040:

| dshot bound     | upper      | lower      |
| --------------- | ---------- | ---------- |
| $f_{mcu}$ / MHz | 125        | 125        |
| wrap            | 10         | $2^{16}-1$ |
| div             | 1.0        | 255.9375   |
|                 |
| **dshot speed** | 12.5 _M_Hz | 7.5 Hz     |

### Aside 2: Why is our method accurate enough

Earier, we claimed that our method for calculating the pwm `wrap` and `div` values was accurate enough for reasonable dshot frequencies. This is shown here.

Looking at the relationship between dshot speed and period:

$\text{pwm period (cycles)} = \frac{\text{mcu freq (kHz)}}{\text{dshot speed (kHz)}}$

The relative error can be calculated as follows:

$$
\begin{align*}
  s &= \frac{f}{p} \\
  s + \delta s &\approx \frac{f}{p + \delta p} \\
    &\approx \frac{f}{p} (1 - \frac{\delta p}{p}) \\
  \frac{\delta s}{s} &= - \frac{\delta p}{p} = - \frac{\delta p}{f/s}
\end{align*}
$$

Reasonable dshot speeds used in betaflight are dshot 150 -> 1200,
corresponding to `period = 833.3 -> 104.2` (for $f_{mcu} = 125$ MHz).
Hence $\frac{f}{s} \isin (100,1000)$.
If we set `wrap = period`, then we truncate to the nearest integer,
giving $\delta p < 1$.
So the maximum relative error in dshot speed is $\frac{1}{100} = 1\%$ (!)

Although 1% may seem high, this is an upper bound. The actual error for dshot 1200 is 0.2%. The ESC is robust to this error. Funnily, since all dshot speeds are a multiple of 150, setting the mcu clock to a high multiple of this such as 120 MHz will completely remove this error!

## Initialise Pico USB / UART

To connect to the pico over serial, we had to initialise the `TinyUSB` submodule from within the pico-sdk.
Likely, this will get merged into main at some point.

Also, it's not clear what `stdio_init_all()` does because it is defined in multiple places.
The most accurate seems to be in `src/rp2_common/pico_stdio/include/pico/stdio.h`,
where this function calls `stdio_uart_init()`, `stdio_usb_init()` and `stdio_semihosting_init()`
depending on whether or not these interfaces exist.

We "worry" about this because telemetry uses a uart connection to the pico.
Currently, we call `stdio_init_all()` at the beginning, and then call `stdio_uart_init()` to setup the uart telemetry.
