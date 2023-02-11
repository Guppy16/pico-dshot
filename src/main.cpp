#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "dshot.h"

#define DSHOT_SPEED 1200  // kHz
#define MCU_FREQ 120      // MHz

#define DEBUG 0

// Note that these should be casted uint32_t when sent to the slice
constexpr uint16_t DMA_WRAP = DEBUG ? (1 << 16) - 1 : 1000 * MCU_FREQ / DSHOT_SPEED; // Total number of counts in PWM cycle
// For visualisation purposes:
// constexpr uint16_t DMA_WRAP = (1 << 16) - 1;
constexpr uint16_t DSHOT_LOW = 0.33 * DMA_WRAP;
constexpr uint16_t DSHOT_HIGH = 0.75 * DMA_WRAP;

constexpr size_t dma_buffer_length = 18; // 16 bits + 2 stop bits
uint32_t dma_buffer[dma_buffer_length];

void code_to_dma_buffer(const uint16_t &code, const bool &telemtry, const uint &channel){
  // Combine code with telemetry
  uint16_t cmd = code << 1 | telemtry;
  // Convert code to packet
  uint16_t packet = DShot::command_to_packet(cmd);

  // Convert packet to PWM values
  uint32_t pwm_val = 0;
  dma_buffer[0] = pwm_val;
  dma_buffer[dma_buffer_length - 1] = pwm_val;
  DShot::packet_to_pwm(packet, dma_buffer + 1, DSHOT_LOW, DSHOT_HIGH, channel);
  // for (size_t b = 0; b < 16; ++b){
  //   pwm_val = (uint32_t) (((packet >> b) & 1) ? DSHOT_HIGH : DSHOT_LOW);
  //   dma_buffer[b + 1] = pwm_val << (16 * channel);
  // }
}

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
  Serial.begin(9600);
  // Flash LED
  flash_led();

  // --- Setup PWM
  // constexpr uint MOTOR_GPIO = 14;
  constexpr uint MOTOR_GPIO = DEBUG ? 14: 14;
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

  // Other tests
  // pwm_config_set_clkdiv(&conf, 100.0f);
  // uint16_t max_uint16_t = (int)((int)(1 << 16) - 1);
  // Serial.print("TOP: "); Serial.println(max_uint16_t);
  // pwm_set_wrap(pwm_slice_num, max_uint16_t);
  // pwm_set_chan_level(pwm_slice_num, pwm_channel, 1 << 15);

  // --- Setup DMA packet
  uint16_t code = 1;
  bool telemetry = 1;
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


  // print_conf(conf);
}

void loop()
{

  // digitalWrite(LED_BUILTIN, HIGH);
  // delay(200);

  // digitalWrite(LED_BUILTIN, LOW);
  // delay(200);
}