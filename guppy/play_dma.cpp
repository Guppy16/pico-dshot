#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/dma.h"

#define DSHOT_SPEED 1200  // kHz
#define MCU_FREQ 120      // MHz

void print_conf(const pwm_config &conf)
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

constexpr size_t buffer_length = 256;
uint32_t fade[buffer_length];

void flash_led()
{
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
}

void setup()
{
  // Is this done in runtime or compile time?
  for (size_t i = 0; i < buffer_length; ++i)
  {
    // Shift by 16 because in pwm_channel B
    fade[i] = (i * i) << 16;
  }
  fade[buffer_length - 1] = 0; // Just for visual clarity

  Serial.begin(9600);
  // Flash LED
  flash_led();

  // --- Setup PWM
  gpio_set_function(LED_BUILTIN, GPIO_FUNC_PWM);
  uint pwm_slice_num = pwm_gpio_to_slice_num(LED_BUILTIN);
  uint pwm_channel = pwm_gpio_to_channel(LED_BUILTIN);

  // Without config method
  // NOTE: pwm should start after DMA initialised!
  // OR can initialise PWM with 0 channel level
  constexpr uint16_t MAX_UINT16 = (1 << 16) - 1;
  pwm_set_wrap(pwm_slice_num, MAX_UINT16);
  pwm_set_chan_level(pwm_slice_num, pwm_channel, 0);
  pwm_set_clkdiv(pwm_slice_num, 240.f); // Should run at 500 kHz for cpu-clck = 120 Mhz
  pwm_set_enabled(pwm_slice_num, true);

  // Use config method
  // pwm_config conf = pwm_get_default_config();
  // pwm_config_set_wrap(&conf, 3);
  // pwm_set_chan_level(pwm_slice_num, pwm_channel, 2);
  // pwm_init(pwm_slice_num, &conf, true);
  // pwm_set_chan_level(pwm_slice_num, pwm_channel, 2);

  // Other tests
  // pwm_config_set_clkdiv(&conf, 100.0f);
  // uint16_t max_uint16_t = (int)((int)(1 << 16) - 1);
  // Serial.print("TOP: "); Serial.println(max_uint16_t);
  // pwm_set_wrap(pwm_slice_num, max_uint16_t);
  // pwm_set_chan_level(pwm_slice_num, pwm_channel, 1 << 15);

  // --- Setup DMA
  int dma_chan = dma_claim_unused_channel(true);
  dma_channel_config dma_conf = dma_channel_get_default_config(dma_chan);
  // PWM counter compare is 32 bit (16 bit per channel)
  channel_config_set_transfer_data_size(&dma_conf, DMA_SIZE_32);
  // Increment read address
  channel_config_set_read_increment(&dma_conf, true);
  // NO increment write address
  channel_config_set_write_increment(&dma_conf, false);
  // DMA Data request when PWM is finished
  channel_config_set_dreq(&dma_conf, DREQ_PWM_WRAP0 + pwm_slice_num);

  dma_channel_configure(
      dma_chan,
      &dma_conf,
      &pwm_hw->slice[pwm_slice_num].cc, // Write to PWM counter compare
      fade,
      buffer_length,
      true);

  delay(11000);
  Serial.print("LED_BUILTIN GPIO: ");
  Serial.println(LED_BUILTIN);
  Serial.print("PWM Slice num: ");
  Serial.println(pwm_slice_num);
  Serial.print("PWM Channel: ");
  Serial.println(pwm_channel);
  Serial.print("DMA channel: ");
  Serial.println(dma_chan);
  // print_conf(conf);
}

void loop()
{

  // digitalWrite(LED_BUILTIN, HIGH);
  // delay(200);

  // digitalWrite(LED_BUILTIN, LOW);
  // delay(200);
}