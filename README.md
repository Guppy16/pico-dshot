# Pico DShot

This repo is being developed to use a RPi Pico to send dshot commands to ESCs.

Normally, a flight controller sends commands to an ESC to control a motor. 
In this repo, we are using a Rpi Pico as a stand in for the flight controller. 
This is a work in progress.
This repo is being developed to be used as submodule for [pico-tts](https://github.com/Guppy16/pico-tts), which is a project to measure drone motor KPIs using a pico. 

## Setting up the repo

```
git clone git@github.com:Guppy16/pico-dshot.git
git submodule update --init
cd lib/extern/pico-sdk
git submodule update --init lib/tinyusb
```

Note that `TinyUSB` is required to use the uart on the pico for serial read and write.

## Running the Unit Tests

We use [Unity Test](https://github.com/ThrowTheSwitch/Unity) as the framework for our tests. 
Assuming all the submodules have been setup:

```terminal
mkdir test/build && cd $_
cmake ..
cmake --build .
ctest --verbose
```

---
## Example

An example is included in `example/`:

```terminal
mkdir example/build && cd $_
cmake ..
cmake --build .
```

Connecte the ESC signal pin to GPIO 14, and ESC gnd to pico gnd. 
Upload `dshot_example.uf2` to the pico, and open a serial connection to it. 
The motor will automatically arm, and you will be able to enter commands.
View the list of commands in `example/main.cpp`. 

---
## Code Overview

```
|-- include/
    |-- config.h
    |-- packet.h
|-- lib/
    |-- tts/
    |-- shoot/
|-- example/
    |-- main.cpp
|-- test/
    |-- test_packet.cpp
```

- `include` header files to setup dshot variables and functions
    - `config.h` configure dshot globals
    - `packet.h` convert a dshot command to a packet
- `lib/` contains the libraries used to implement dshot on the pico   
    - `tts/` pico hw implementations for dma, pwm, alarm pool
    - `shoot/` send a dshot packet repeatedly
- `example/`
  - `main.cpp` allows you to use serial input to send dshot commands
- `test/` contains a unit test for `dshot/` as well as a cmake config file

Dependency Graph:

```
|-- shoot
|   |-- tts
|   |   |-- config
|   |-- packet
```

## To Do
- [ ] This whole repo can be setup as header only: `./dshot/include/globals.h, hw.h`
- [ ] The print statemtents could be placed in an implementation file: `./dshot/src/logging.cpp`

## Backlog
- [ ] attempt proper arm sequence
- [ ] Try: `DSHOT_SPEED = DEBUG ? 0.008 : 1200 // kHz` (this may get rid of some other ternaries on DEBUG)
- [ ] Add validation to ensure PWM, DMA, repeating timer have been setup correctly. MCU_CLK could take in a measured reading. Use `lib/extern/pico-sdk/src/rp2_common/hardware_clocks/scripts/vcocalc.py` to find valid sys clock frequencies.
- [ ] Currently dma writes to a PWM counter compare. This actually writes to two dma channels (because upper / lower 16 bits are separate counters). Hence we render one dma channel useless. Is it possible to implement this in a better way?
- [ ] Scheme to represent DShot codes. enum is possible for special codes, but shouldn't be used for throttle values? Explore the idea of using / generating a look up table to convert dshot to pwm counter values (and vice versa for bidir dshot). 
Can this be hooked up with Programmable IO?
Memory usage: 2^16 command x 32 bit word = 32k x 64 bit word (that might be too much). 
- [ ] I believe that the uart and serial are initialised in `stdio_init_all()` (called in `main.cpp`). Perhaps we should use `stdio_usb_init()` and `stdio_uart_init()` separately. For telemetry (not yet committed), Peter used `stdio_uart_init()` after `stdio_init_all()` and didn't have errors.

---
## DShot Protocol

DShot is a digital protocol used to send commands from a flight controller to an ESC, in order to control a motor. 
DShot has discretised resolution, where as PWM is analogue (on the FC side albeit discretised on the ESC due to hw limitations). 
The digital protocal has advantages in accuracy, precision, resolution and communication.
Typically, dshot commands are sent over PWM hardware (note that we are *not* controlling the motor using "pulse width modulation", but are rather piggy backing off of the PWM hardware to send dshot frames!). 

A Dshot *frame* is constructed as follows:

| *Value* | *Telemetry* | *Checksum* |
| ------- | ----------- | ---------- |
| 11 bits | 1 bit       | 4 bit      |
| 0 - 2047| Boolean     | CRC        |

| Dshot *Value* | Meaning                                |
| ------------- | -------------------------------------- |
| 0             | Disarm, but hasn't been implemented    |
| 1 - 47        | Reserved for special use               |
| 48 - 2047     | Throttle position (resolution of 2000) |

A bit is converted to a *duty cycle*, which is transmit as a "pulse" using PWM hw. 

| bit | duty cycle |
| --- | ---------- |
| 0   | \< 33%     |
| 1   | >75%       |

The *period* of each pulse (or bit) is set by the Dshot freqeuncy (e.g. Dshot150 corresponds to 150 kHz). 

To indicate a frame reset, a pause of at least $2 \mu$s is required (source [Betaflight wiki](https://betaflight.com/docs/development/Dshot)). This pause should have a duty cycle of 0. 
This pause can be converted to bits as follows:

$$
\text{Pause bits} = \lceil \frac{2 \mu \text{s}}{\text{Pulse Period}} \rceil
$$


| Dshot freq / kHz | pulse period / $\mu$s | Pause bits |
| ---------------- | --------------------- | ---------- |
| 150              | 6.67                  | 1          |
| 600              | 1.67                  | 2          |
| 1200             | 0.833                 | 3          |

Our implementation uses a constant value of 4 bits as a pause between frames (i.e. we assume a max Dshot freq of 2000). 
Hence, the PWM hw is sent a *packet* of 20 duty cycles (16 for a DShot frame and 4 for a frame reset).

### Example

To make the motors beep a DShot packet is constructed as follows:

The Dshot *command* is: `Value = 1`, `Telemetry = 1`

→ First 12 bits of the command `0x003`

→ `CRC = 0 XOR 0 XOR 3 = 3`

→ `Frame = 0x0033`

→ `Packet = LLLL LLLL LLHH LLHH 0000`

The packet is transmit from left to right (i.e. big endian). 

(Please note that most of this nomenclature is taken from the [betaflight dshot wiki](https://betaflight.com/docs/development/Dshot), but some of it may not be standard)

---
## Sources

### Docs and Sample Implementation
- [Pico SDK API Docs](https://raspberrypi.github.io/pico-sdk-doxygen/modules.html). Some quick links: [dma](https://raspberrypi.github.io/pico-sdk-doxygen/group__hardware__dma)
- [Documentation on the Pico](https://www.raspberrypi.com/documentation/microcontrollers/?version=E0C9125B0D9B) incl spec, datasheets, [pinout](https://datasheets.raspberrypi.com/pico/Pico-R3-A4-Pinout.pdf), etc.
- [Pico examples](https://github.com/raspberrypi/pico-examples) from the rpi github incl `dma/`. There's an interesting example on pairing an adc with dma [here](https://github.com/raspberrypi/pico-examples/blob/master/adc/dma_capture/dma_capture.c). Note that when viewing pico examples, they use `#include "pico/stdlib.h"`. This is *not* to be used in the *Arduino* framework! as explained in [this post](https://community.platformio.org/t/include-pico-stdlib-h-causes-errors/22997). 
- [Unity Assertion Reference](https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md) is a useful guide handbook for unit testing with Unity framework. 
- [Unit testing on Embedded Target using PlatformIO](https://piolabs.com/blog/insights/unit-testing-part-2.html)
- CMake file for Unity Testing was inspired from [Rainer Poisel](https://www.poisel.info/posts/2019-07-15-cmake-unity-integration/), [Throw The Switch](http://www.throwtheswitch.org/build/cmake) and [Testing with CMake and CTest](https://cmake.org/cmake/help/book/mastering-cmake/chapter/Testing%20With%20CMake%20and%20CTest.html). 

### Explanations of DShot

- [Betaflight DShot Wiki](https://betaflight.com/docs/development/Dshot)
- [Spencer's HW Blog](https://www.swallenhardware.io/battlebots/2019/4/20/a-developers-guide-to-dshot-escs) has a quick overview on the DShot protocol, list of the dshot command codes (which shd be sourced somewhere in the [betaflight repo](https://github.com/betaflight/betaflight)), and implmenetation overviews using scp and dma. 
- [This post](https://blck.mn/2016/11/dshot-the-new-kid-on-the-block/) has a simple explanation of dshot with a few examples
- [DShot - the missing handbook](https://brushlesswhoop.com/dshot-and-bidirectional-dshot/) has supported hw, dshot frame example, arming, telemetry, bi-directional dshot
- [BLHeli dshot special command spec](https://github.com/bitdump/BLHeli/blob/master/BLHeli_32%20ARM/BLHeli_32%20Firmware%20specs/Digital_Cmd_Spec.txt)
- [Missing Handbook](https://brushlesswhoop.com/dshot-and-bidirectional-dshot/#special-commands) also has a good explanation of commands
[Original RC Groups post on dshot](https://www.rcgroups.com/forums/showthread.php?2756129-Dshot-testing-a-new-digital-parallel-ESC-throttle-signal)
- [SiieeFPV](https://www.youtube.com/watch?v=fNLxHWd0Bvg) has a YT vid explaining DMA implementation on a *Kinetis K66*. This vid was a useful in understanding what was needed for our implementation. 
- KISS ESC onewire telemetry protocol [pdf](https://www.google.co.uk/url?sa=t&rct=j&q=&esrc=s&source=web&cd=&ved=2ahUKEwic6OitkJ3-AhXJUMAKHaL6DiEQFnoECAoQAQ&url=https%3A%2F%2Fwww.rcgroups.com%2Fforums%2Fshowatt.php%3Fattachmentid%3D8524039%26d%3D1450424877&usg=AOvVaw1FWow1ljvZue1ImISgzlca)

### Other

- Use [Zadig](https://zadig.akeo.ie/) to install drivers for the RPi boot interface on Windows. This makes the flashing experience a LOT better! This has been mentioned in platform io and github forums (in depth [here](https://community.platformio.org/t/official-platformio-arduino-ide-support-for-the-raspberry-pi-pico-is-now-available/20792/9), briefly [here](https://community.platformio.org/t/raspberry-pi-pico-upload-problem/22809/7), which links to [this](https://github.com/platformio/platform-raspberrypi/issues/2#issuecomment-828586398)).
- [Wiz IO Pico](https://github.com/Wiz-IO/wizio-pico): seems like an alternative to the Arduino framework used in PlatformIO? More details can be found on their [Baremetal wiki](https://github.com/Wiz-IO/wizio-pico/wiki/BAREMETAL)
- [Rpi Pico Forum post](https://forums.raspberrypi.com/viewtopic.php?t=332483). This person has balls to try and implemenet dshot using assembly!!
- List of [Arduino libraries for the RP2040](https://www.arduinolibraries.info/architectures/rp2040). I've not used any yet, but it might be best to use them as reference instead of importing them?
- Interesting post on [processing serial without blocking](http://www.gammon.com.au/serial)
- [Upload port required issue](https://github.com/platformio/platform-raspberrypi/issues/2). I don't think this issue will be faced if using Zadig
- [BLHeli 32 Manual](https://github.com/bitdump/BLHeli/blob/master/BLHeli_32%20ARM/BLHeli_32%20manual%20ARM%20Rev32.x.pdf)
- In the future, this repo may want to use [GitHub hosted runners](https://docs.github.com/en/actions/using-github-hosted-runners/about-github-hosted-runners) to create [GitHub Packages](https://docs.github.com/en/packages/learn-github-packages/introduction-to-github-packages). This may require some knowledge on setting up [GitHub Actions](https://docs.github.com/en/actions/quickstart#creating-your-first-workflow), as well as the [Workflow syntax](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions)
- A [thread on the rpi forum](https://forums.raspberrypi.com/viewtopic.php?p=2091821#p2091821) about inspecting the contents of the alarm pool for logging 

NOTE: (Although we don't use this functionality), a common implmentation measuring timer uses 32 bit time (note that 64 bit is possible using `timer_hw->timerawl` but more effort..)
- `timer_hw->timerawl` returns a 32 bit number representing the number of microseconds since power on
- This will overflow at around 72 hrs, so must assume that this longer than the operation timer of the pico
- This has been the cause of many accidental failures in the past :)

### Bidirectional DShot

- [DShot - the missing handbook](https://brushlesswhoop.com/dshot-and-bidirectional-dshot/) also talks about bi-directional dshot
- [RPi Pico Forum thread on Evaluating PWM signal](https://forums.raspberrypi.com/viewtopic.php?t=308269). This could be useful for bidirectional dshot
- [Pico Example to measure duty cycle](https://github.com/raspberrypi/pico-examples/blob/master/pwm/measure_duty_cycle/measure_duty_cycle.c)
- [Interesting PR on the first implementation of bidir dshot](https://github.com/betaflight/betaflight/pull/7264). This discussion alludes to the *politics* of the protocol
- Note that we need to check the FW version on the ESC to see if it supports bidir (and EDT). [BLHeli Passthrough](https://github.com/BrushlessPower/BlHeli-Passthrough) is implemented as an Arduino Lib for ESP32 and some Arduinos. A good exercise would be to add support for the Pico. [This](https://github.com/betaflight/betaflight/blob/master/src/main/drivers/serial_escserial.c#L943) may be a betaflight implementation of passthrough, but I couldn't understand it. [BLHeli Suite](https://github.com/bitdump/BLHeli) is also needed.
- Researching this topic, I came across DMA "burst" mode, which apparently helps in transitioning from send to receive. Not sure, but maybe a starting point can be achieved from [this post](http://forum.chibios.org/viewtopic.php?t=5677)
- [Chained DMA](https://vanhunteradams.com/Pico/DAC/DMA_DAC.html) may be useful to switch from write to read configuration
