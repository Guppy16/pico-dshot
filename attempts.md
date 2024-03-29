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

```c
#include "pico/stdlib.h"
#include "pico/platform.h"

int main() {
  // Set MCU clock frequency before configuring dshot
  set_sys_clock_khz(120e3, false);

  // ... initialise dshot

  // Check MCU freq
  printf("MCU Freq (kHz): %d\n", frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS));
}
```

## Initialise Pico USB / UART

To connect to the pico over serial, we had to initialise the `TinyUSB` submodule from within the pico-sdk.
Likely, this will get merged into main at some point.

Also, it's not clear what `stdio_init_all()` does because it is defined in multiple places.
The most accurate seems to be in `src/rp2_common/pico_stdio/include/pico/stdio.h`,
where this function calls `stdio_uart_init()`, `stdio_usb_init()` and `stdio_semihosting_init()`
depending on whether or not these interfaces exist.

We "worry" about this because telemetry uses a uart connection to the pico.
Currently, we call `stdio_init_all()` at the beginning, and then call `stdio_uart_init()` to setup the uart telemetry.

---

## Uart Telemetry Implementation

There are two supported protocols for telemetry using the BLHeli ESC:

- _onewire_: Telemetry is requested by setting the telemetry flag when sending a dshot cmd
- _auto_-telemetry: Telemetry is sent in regular intervals. This mode needs to be configured in BLHeli Suite.

Naturally, the better solution is to use auto-telemetry, however I only came upon it recently,
hence the bulk of the next section is about onewire.

## _Auto_-Telemetry - TODO

Some resources:

- [Protocol discussion on Github issue](https://github.com/bitdump/BLHeli/issues/431)
- [GitHub issue with info on BlHel Suite](https://github.com/bitdump/BLHeli/issues/431)

## _Onewire_-Telemetry

Aim: Setup onewire uart telemetry following the [KISS ESC 32-bit series onewire telemetry protocol datasheet](./resources/KISS_telemetry_protocol.pdf). The process looks something like this:

```mermaid
sequenceDiagram
  box Pico
  participant pico_dshot as dshot
  participant Pico onewire as onewire
  end
  box ESC
  participant ESC dshot as dshot
  participant ESC onewire as onewire
  end

  loop Every 1ms
    Note over pico_dshot: Set Telemetry = 1
    pico_dshot->>ESC dshot: Send dshot packet
    Note over pico_dshot: Reset Telemetry = 0

  ESC onewire->>Pico onewire: 10 bytes Uart Telemetry _Transmission_
  Note over pico_dshot, Pico onewire: Process transmission <br/> into telemetry data
  end
```

1. Pico: send dshot packet with `Telemetry = 1`. Reset telemetry immediately.
2. ESC: receive dshot packet; send a _transmission_ over _onewire_ uart telemetry to Pico.
3. Pico: receive the _transmission_, check its CRC8 and then convert the _transmission_ to _telemetry data_.

### Setting and Restting the Telemetry bit

Aim: Request telemetry data using onewire

Conundrum Context:
The ESC is configured to respond to _every_ telemetry request; i.e. if we send 10 dshot packets with `telemetry=1`, then we will receieve 10 _onewire transmissions_.
Although this may sound desirable, a problem arises due to the significant delay in these _onewire transmissions_ will be significantly delayed: _onewire transmissions_ take 10 times longer to transmit than a dshot packet.
This means that when we request telemetry, we must immediately reset it `telemetry=0` before the next dshot packet is sent.

Solution: To resolve this, we need some sort of timer to know when to set and reset the telemetry bit. This could either be lumped with sending a dshot packet, or having a separate interrupt.

Note: An extra problem arises due to the nature of dshot special commands requiring (re)setting the telemetry bit. This is briefly discussed in an _Aside_ later.

Consider lumping this with `dshot_send_packet()`: we are effectively manually implementing a timer by using a counter, and then set/reset telemetry based on a _counter compate_ value.
On the other hand, a hardware interrupt gives the closest guarantee to a consistent rate of requeseting telemetr (consistency is desirable for measurements!). One may think that introducing another interrupt based repeating timer for onewire uart on top of the repeating timer used for `dshot_send_packet()` could lead to impinging behaviour. However, the extra interrupt (repeating timer) for onewire uart will not impinge on the dshot so long as it is implemented on the [same alarm pool](https://forums.raspberrypi.com/viewtopic.php?t=328648), because alarms on the same alarm pool have equal priority, which means that they will not interrupt each other!

```cpp
void onewire_repeating_request(dshot_t &dshot)
{
  auto t = dshot.telemetry; // Store current value of telemetry
  dshot.telemetry = 1;      // Set telmetry bit
  dshot_send_packet();      // Send shot packet to request telemetry
  dshot.telemetry = t;      // reset telemetry to previous value
}
```

An intersting question is: if this repaating timer sends a dshot frame to request telemetry, and is out of sync to the repeating timer sending dshot commands, will it lead to any inconsistency? 
One obvious impact is that `dshot_send_packet()` may have to wait for the current packet to finish sending,
but this will likely have negligible effect, because the function is very fast.
However, I don't know about any other affects. Nevertheless for this implementation, I prioritised keeping the `dshot_send_packet()` consistent by implementing a mix of the mentioned options:

- Set the telemetry bit in a repeating timer to request telemetry
- In `dshot_send_packet()`, always reset telemetry bit

```cpp
// (Simplified) function to set telemetry request flag
bool onewire_repeating_req(repeating_timer_t *rt){
  onewire_t *telem = (onewire_t *)(rt->user_data);
  ...
  telem->escs[telem->esc_motor_idx].dshot->packet.telemetry = 1;
  return telem->send_req_rt_state;
}

// Modified function to reset telemetry request flag
void dshot_send_packet(dshot_config *dshot, bool debug){
  ...
  // Send dshot packet
  ...
  // Reset telemetry bit (so that the onewire uart isn't overloaded)
  dshot->packet.telemetry = 0;
}
```

#### Aside: Dshot special commands require (re)setting the telemetry flag

As mentioned in the previous section, dshot special commands require setting the telemetry flag before transmission, _and_ some of the commands have some requirements on the timings of the command.
Also, note that onewire will not send a telemetry _transmission_ if the dshot frame being sent is a special command frame, because these command frames require `telemetry=1`, which could be ambiguous for the ESC (I believe some dshot command frames send data over UART).
The only exception I have tested is `value=0`, where setting the telemetry bit does indeed send back a _transmission_. Currently this has been tested for `value=0, 1, >47`.

This can lead to conflicts with the preceding implementation of requesting telemetry. The easy way to get around this is by not requesting telemetry while sending special commands. This works for most, but it would be desirable to request telemetry while e.g. playing the beep command. _If_ we want this to be a feature (i.e. be able to receive telemetry while sending special commands),
there are a few complications:

- When setting / resetting the telemetry bit, onewire / send_dshot will need to check the dshot frame to see if it's a command frame, before validating/setting the onewire telemetry bit!
- OR is there a method to send an inbetween frame without disruption?!

These are two complicated to resolve, and isn't a high priority. Hopefully auto-telemetry will resolve this.
Nevertheless, below are some attempts:

Note that a naive solution is to create a separate flag for `onewire_telemetry` and `special_cmd_telmetry`, and always reset `onewire_telemetry`. `telemetry_bit = onewire_telemetry | special_cmd_telmetry`. This will not work, because, when requesting for onewire telemetry, the ESC will not be able to distinguish between onetie and special cmd.

Let's consider the the problem with adding the capability to turn on the beeper while requesting telelemtry from the ESC. This is because the beeper requires `packet.telemetry=1`, but doesn't actually request for telemetry. This is inconvenient, because for the beeper to run continuously this command is sent in regular intervals, which makes it difficult to request telemetry separately. Something like this can be done instead:

```cpp
bool send_beep_rt_state = false; // Keep track of state of beeper
repeating_timer_t send_beep_rt;  // Repeating timer config for beeper

/**
 * @brief repeating isr to send a beep command
 *
 * @param rt
 * @return bool(send_beep_rt_state). True if isr should repeat
 */
bool repeating_dshot_beep(repeating_timer_t *rt) {
  dshot_config *dshot = (dshot_config *)(rt->user_data);

  // **Handle Request telemetry separately**
  dshot->packet.throttle_code = 0;
  dshot->packet.telemetry = 1;
  dshot_send_packet(dshot, false);

  // Beep command: 1;1
  dshot->packet.throttle_code = 1;
  dshot->packet.telemetry = 1;
  dshot_send_packet(dshot, false);
  dshot->packet.throttle_code = 0;

  return send_beep_rt_state;
}

/// Main loop code
...
// t = telemetry
  case 116:
    onewire.send_req_rt_state = !onewire.send_req_rt_state;
    if (onewire.send_req_rt_state) {
      onewire_rt_configure(&onewire, onewire_delay_us, pico_alarm_pool);
      onewire.buffer_idx = 0;
      printf("Onewire Telem ON\n");
    } else {
      printf("Onewire Telem OFF\n");
    }
    break;
...
```

I don't like this because the telemetry request is forced at the update rate of the beep.
Hopefully _auto-telemetry_ will resolve this.

### Converting _transmission_ to telemetry data

TODO (upon request):

- Go into details on how CRC8 works
- Explain how each transmission corresponds to a specific ESC

### How to send telemetry data to main computer

The aim of this project is to capture KPIs of drone motors.
For this, we need to capture telemetry data and stream it to a main computer.
This data will likely be transmit over wifi or uart, which can be processor heavy protocols.
A naive implementation may be to allow the user to add a callback, such that the Pico can call this
function to execture the transmission when new telemetry data is received.
However, this is unsafe because the callback could be processor heavy,
which could lead to the interrupt service routine exiting early.
Instead, in this section, we consider different methods of storing and sending this data.

First, we need to consider _how_ we are _storing_ the data.

1. 1 variable: Store the most recent data in a single variable alongside a timestamp.
The timestamp can be used as a flag to check if this is the most recent data.
Every so often, the main processor can compare this timestamp to see if it needs to send the data.
This has a disadvantage that some data point may be replaced before the pico gets a chance to send it.
1. Circular Queue Buffer: This is more complicated, as we keep a buffer of telemetry data.
The buffer gives more leeway in terms of transmission.
There is no need to keep a record of the timestamp (though it will probably be useful anyways).
1. Update flag: Have a variable in `onewire_t` called `onewire.update_flag`, 
which is set to `true` when a transmission has been decoded. A main process can check this flag 
and consume this (i.e. reset the flag) and copy the transmission. 
[This is an interesting thread](https://forums.raspberrypi.com/viewtopic.php?t=320793) on the atomicity of this process.
Assuming this read/write is done on one core on a 32 bit word, there will not be any data corruption.

Next we need to consider _when_ to _send_ the data.

1. Round-robin: In a main loop, the processor will check for new telemetry data, and then send it.
2. Priortiy schedules queue: Push tasks / functions to a priority queue. The main loop will check this queue and execute tasks as necessary.

We impelemente: _Update flag_ and _Ronud-robin_ for simplicity.
