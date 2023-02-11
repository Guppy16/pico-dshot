#include "utils.h"

void utils::print_pwm_config(const pwm_config &conf)
{
    Serial.print("Conf: ");
    Serial.print("csr: ");
    Serial.print(conf.csr);
    Serial.print(" div: ");
    Serial.print(conf.div);
    Serial.print(" top: ");
    Serial.print(conf.top);
    Serial.println();
}

void utils::flash_led(const pin_size_t &pin)
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
}