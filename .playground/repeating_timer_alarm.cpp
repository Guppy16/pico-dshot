#include <Arduino.h>

#include "pico/time.h"

#define DMA_ALARM_NUM 1

struct repeating_timer repeating_send_frame_timer;
bool repeating_send_dshot_frame(struct repeating_timer *t)
{
  // Flick LED
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  return true;
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  delay(5000);
  Serial.println("Begin Setup");

  Serial.println("Setting up pico Alarm");
  // Setup repeating timer
  alarm_pool_t *pico_alarm_pool = alarm_pool_create(DMA_ALARM_NUM, PICO_TIME_DEFAULT_ALARM_POOL_MAX_TIMERS);

  Serial.println("Adding repeating timer to alarm pool");
  bool dma_alarm_state = alarm_pool_add_repeating_timer_us(pico_alarm_pool, 1000000, repeating_send_dshot_frame, NULL, &repeating_send_frame_timer);
  Serial.print("Alarm setup state: ");
  Serial.println(dma_alarm_state);

  Serial.println("Finish setup");
}

void loop()
{
}