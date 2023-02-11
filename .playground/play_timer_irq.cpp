/*
    Standalone Program to flash an LED at a constant freq
    set using a hw_timer with interrupts
*/

#include <Arduino.h>
#include "hardware/timer.h"
#include "hardware/irq.h"

// #define PICO_TIME_DEFAULT_ALARM_POOL_DISABLED 1

// NOTE: How do we know using alarm 1
// will not affect other programs?
// Use alarm 1
#define ALARM_NUM 1
#define ALARM_IRQ TIMER_IRQ_1

// Alarm interrupt handler
volatile uint32_t alarm_irq_count;

void alarm_irq(void)
{
    // Change state of LED
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

    // Change state of LED
    // digitalWrite(LED_BUILTIN, HIGH);

    ++alarm_irq_count;
}

void alarm_in_us(uint32_t delay_us)
{
    // // Enable the interrupt for our alarm
    // // Timer outputs 4 alarm irqs!?
    // // I think we are setting it on 0 of 3
    // Serial.println("Setting hw set_bits");
    // hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);

    // // Set irq handler for alarm irq
    // Serial.println("Set handler");
    // irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);

    // // Enable the alarm irq
    // Serial.println("Enabling alarm");
    // irq_set_enabled(ALARM_IRQ, true);

    // Check if alarm irq is enabled
    Serial.print("Alarm Enabled? ");
    Serial.println(irq_is_enabled(ALARM_IRQ));

    // Alarm is only 32 bits so if trying to delay more
    // than that need to be careful and keep track of the upper
    // bits
    // Not sure what this comment means.
    Serial.print("Current Time: ");
    Serial.println(timer_hw->timerawl);
    uint64_t target = timer_hw->timerawl + delay_us;
    Serial.print("Target time: ");
    Serial.println(target);

    // Write the lower 32 bits of the target time to the alarm which
    // will arm it
    Serial.println("Arming alarm");
    timer_hw->alarm[ALARM_NUM] = (uint32_t)target;
    Serial.println("Completed alarm setup");
    Serial.println();
}

void reset_alarm(uint32_t delay_us)
{
    Serial.println("Resetting Alarm");
    Serial.print("Prev Target: ");
    Serial.println(timer_hw->alarm[ALARM_NUM]);

    uint64_t target = timer_hw->timerawl + delay_us;
    timer_hw->alarm[ALARM_NUM] = (uint32_t)target;

    Serial.print("Next Target: ");
    Serial.println(target);

    Serial.print("Alarm Enabled? ");
    Serial.println(irq_is_enabled(ALARM_IRQ));
}

void setup()
{
    Serial.begin(115200);
    delay(5000);
    Serial.println("Begin Setup");
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("End LED Flash");

    alarm_irq_count = 0;

    // Enable the interrupt for our alarm
    // Timer outputs 4 alarm irqs!?
    // I think we are setting it on 0 of 3
    Serial.println("Setting hw set_bits");
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);

    // Set irq handler for alarm irq
    Serial.println("Set handler");
    irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);

    // Enable the alarm irq
    Serial.println("Enabling alarm");
    irq_set_enabled(ALARM_IRQ, true);

    alarm_in_us(1000000 * 2);
    Serial.println("End Setup");
}

void loop()
{
    delay(1000);
    Serial.println(timer_hw->timerawl);

    // t = 2
    if (alarm_irq_count == 1)
    {
        ++alarm_irq_count;
        Serial.println("Raised interrupt");
        // Set interrupt
        alarm_in_us(1000000 * 2);
    }
    // t = 4
    else if (alarm_irq_count == 3)
    {
        ++alarm_irq_count;
        Serial.println("Raised interrup at val = 2");
        // Set interrupt
        alarm_in_us(1000000 * 2);
    }
    // t = 5
    else if (alarm_irq_count == 4)
    {
        ++alarm_irq_count;
        reset_alarm(1000000 * 2);
    }
    // t = 7
    else if (alarm_irq_count == 6)
    {
        ++alarm_irq_count;
        Serial.println("Raised interrupt at val = 6");
    }
}