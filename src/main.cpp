#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "dshot.h"
#include "utils.h"

#define DSHOT_SPEED 300  // kHz
#define MCU_FREQ 120      // MHz

#define DEBUG 0

// Note that these should be cast uint32_t when sent to the slice
// WRAP = Total number of counts in PWM cycle
constexpr uint16_t DMA_WRAP = DEBUG ? (1 << 16) - 1 : 1000 * MCU_FREQ / DSHOT_SPEED; 
constexpr uint16_t DSHOT_LOW = 0.33 * DMA_WRAP;
constexpr uint16_t DSHOT_HIGH = 0.75 * DMA_WRAP;

constexpr uint32_t cmd_repeats = 3000;
constexpr uint32_t cmd_size = 20;
constexpr size_t dma_buffer_length = 18 * cmd_repeats; // 16 bits + 2 stop bits
uint32_t dma_buffer[dma_buffer_length] = {0};

void code_to_dma_buffer(const uint16_t &code, const bool &telemtry, const uint &channel){
  for (size_t i = 0; i < cmd_repeats; ++i)
  {
    DShot::command_to_pwm_buffer(code, telemtry, dma_buffer + i * cmd_size, DSHOT_LOW, DSHOT_HIGH, channel);
  }
}

void setup()
{
  Serial.begin(9600);
  // Flash LED
  utils::flash_led(LED_BUILTIN);

  // --- Setup PWM
  constexpr uint MOTOR_GPIO = DEBUG ? 25: 14;
  gpio_set_function(MOTOR_GPIO, GPIO_FUNC_PWM);
  uint pwm_slice_num = pwm_gpio_to_slice_num(MOTOR_GPIO);
  uint pwm_channel = pwm_gpio_to_channel(MOTOR_GPIO);

  // Without config method
  // NOTE: pwm should start after DMA initialised!
  // OR can initialise PWM with 0 channel level
  pwm_set_wrap(pwm_slice_num, DMA_WRAP);
  pwm_set_chan_level(pwm_slice_num, pwm_channel, 0);
  pwm_set_clkdiv(pwm_slice_num, DEBUG ? 240.0f : 1.0f); // Should run at 500 kHz for cpu-clck = 120 Mhz
  pwm_set_enabled(pwm_slice_num, true);

  // Use config method
  // pwm_config conf = pwm_get_default_config();
  // pwm_config_set_wrap(&conf, 3);
  // pwm_set_chan_level(pwm_slice_num, pwm_channel, 2);
  // pwm_init(pwm_slice_num, &conf, true);
  // pwm_set_chan_level(pwm_slice_num, pwm_channel, 2);

  // --- Setup DMA packet
  uint16_t code = 0;
  bool telemetry = 0;
  code_to_dma_buffer(code, telemetry, pwm_channel);

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
      dma_buffer,
      dma_buffer_length,
      true
      );

  


  delay(11000);
  Serial.print("LED_BUILTIN GPIO: ");
  Serial.println(LED_BUILTIN);
  Serial.print("MOTOR GPIO: ");
  Serial.println(MOTOR_GPIO);
  Serial.print("PWM Slice num: ");
  Serial.println(pwm_slice_num);
  Serial.print("PWM Channel: ");
  Serial.println(pwm_channel);
  Serial.print("DMA channel: ");
  Serial.println(dma_chan);

  // Print DShot settings
  Serial.print("DShot: Wrap: ");
  Serial.print(DMA_WRAP);
  Serial.print(" Low: ");
  Serial.print(DSHOT_LOW);
  Serial.print(" High: ");
  Serial.println(DSHOT_HIGH);

  // Print packet setup
  Serial.print("Packet Setup:");
  Serial.print(" Code: ");
  Serial.print(code, HEX);
  Serial.print(" Telemetry: ");
  Serial.print(telemetry);
  Serial.print(" Packet: ");
  Serial.print(DShot::command_to_packet(code << 1 | telemetry), HEX);
  Serial.println();


  // utils::print_conf(conf);
}

void loop()
{

  // digitalWrite(LED_BUILTIN, HIGH);
  // delay(200);

  // digitalWrite(LED_BUILTIN, LOW);
  // delay(200);
}