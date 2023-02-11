# Pico DShot

---
## DShot Protocol

A flight controller (Pico) sends commands to the ESC to control the motor.
DShot is a digital protocol utilising PWM to send these commands.
DShot is necessarily slower and discretised compared to analogue PWM, 
however the advantages in accuracy, precision and communication outweigh this.

To send a command to the ESC, we send a _packet_, constructed as follows:
- 11 bit number representing the code (0 to 2047)
- 1 bit telemetry flag
- 4 bit CRC
- final 0 bit to signify end of packet. Not sure, but it looks like some ppl just set the duty cycle to 0 before and after transmission

Each bit represents a PWM duty cycle
- 0 = \< 33%
- 1 = > 75%

The PWM freq is determined by by the DShot speed:
- DShot150 has f = 150 kHz, etc.

---
## Functions

- :tick: calculating duty cycle of bits from dshot speed
- enum to represent codes
- function in the processor to handle telemtry. Can this be handled in HW?
- code to packet (i.e. calculate checksum)

- Write some tests for these function!

---
## Test Data

Command: 1, Tel: 1
0x0033
Transmitted from left to right (I think)
LLLL LLLL LLHH LLHH