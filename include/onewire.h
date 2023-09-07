/**
 * @file onewire.h
 * @defgroup onewire onewire
 * @brief
 *
 * Container to store results from telemetry receivied over one wire uart
 * telemetry
 */

#pragma once
#include "dshot.h"
#include "hardware/uart.h"
#include "kissesctelem.h"
#include "stdint.h"

/**
 * @brief set the number of ESCs at compile time
 * This is used by the telemetry to tell how many ESCs to read the telemetry
 * from
 */
#ifndef ESC_COUNT
#define ESC_COUNT 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ONEWIRE_BAUDRATE 115200

/**
 * @brief minimum time interval between telemetry request
 *
 * NOTE: minium time interval between telem should be:
 * min_telem_interval = dshot_interval + telem_delay + telem_data_transfer
 *                    = 133 us (DSHOT150) + 0 (?)    + 115200 / 10*8 (=695 us)
 *                    = 827 us
 * Hence min_telem_interval ~ 1 ms is a safe value
 */
static const long int ONEWIRE_MIN_INTERVAL_US = 1000;

/**
 * @brief Struct to represent ESC data
 *
 */
typedef struct esc_motor {
  dshot_config *dshot;
  kissesc_telem_t telem_data;
} esc_motor_t;

/**
 * @brief struct to configue one wire uart telemetry
 *
 * @ingroup onewire
 * General config:
 * @param gpio NOTE: only one gpio because esc telem is one wire
 * @param send_req_rt Repeating timer config for preodically requesting
 * telemetry
 * @param buffer_size = 10 KISS telemetry protocol outputs 10 bytes
 * Storing uart telemetry output
 * @param buffer_idx Keep track of idx in uart telemetry buffer
 * @param buffer Array to hold telemetry buffer data
 * @param telem_updated_esc -1 <= telem_updated_esc < ESC_COUNT
 * default -1. If telemetry is received for an ESC, then the ESC idx
 * is stored in this variable. This idx should be reset
 * by a main process (e.g. upon reading the onewire data).
 */
typedef struct telem_uart {
  uart_inst_t *uart;
  uint gpio;
  uint baudrate;                 /// Defaults to @ref ONEWIRE_BAUDRATE
  repeating_timer_t send_req_rt; // Repeating timer config for periodically
                                 // requesting telemetry
  bool send_req_rt_state;        // Keep track of repeating timer state
  volatile size_t esc_motor_idx; // Keep track of which esc to request telem
  volatile esc_motor_t escs[ESC_COUNT];

  // variable to store the result from a telemetry read in an irq
  volatile size_t buffer_idx;
  volatile uint8_t buffer[KISS_ESC_TELEM_BUFFER_SIZE];
  // flag is updated when esc telemetry has been received
  int telem_updated_esc;
} onewire_t;

// Global variable for onewire
onewire_t onewire;

/**
 * @brief return if gpio supports uart rx.
 * This doesn't check if the gpio is already being used
 *
 * @param gpio
 * @return true
 * @return false
 */
static inline bool validate_uart_rx_gpio(const uint gpio) {
  const size_t valid_gpios = 6;
  uint uart_rx_gpios[] = {1, 5, 9, 13, 17, 21};

  for (size_t i = 0; i < valid_gpios; ++i) {
    if (gpio == uart_rx_gpios[i])
      return true;
  }
  return false;
}

/**
 * @brief IRQ for reading telemetry data over uart
 *
 * When onewire is receiving data over uart, an interrupt will be raised.
 * This routine is called as an interrupt service routine.
 * This routine stores the telemtry data in onewire->buffer.
 * Once the buffer is full, the buffer is parsed to telemtry data
 * and stored in the relevant ESC's telem_data data store.
 * Also, onewire->telem_updated_esc is set after translation.
 *
 * NOTE: we assume that the uart is automatically cleared in hw
 */
static void onewire_uart_irq(void) {
  // Read uart greedily
  while (uart_is_readable(onewire.uart)) {
    // Read char from uart
    const char c = uart_getc(onewire.uart);
    // Check if reached end of buffer
    if (onewire.buffer_idx >= KISS_ESC_TELEM_BUFFER_SIZE) {
      /// NOTE: This printf may not be printed because its long
      // hence code should handle this elsewhere
      // for example, take a look at validate_onewire_repeating_req
      printf("Onewire overflow\n");
    } else {
      onewire.buffer[onewire.buffer_idx] = (uint8_t)c;
      // printf("%x\t", c);
    }
    onewire.buffer_idx++;
    // printf("UART: %x\n", onewire.buffer[onewire.buffer_idx - 1]);
  }
  if (onewire.buffer_idx == KISS_ESC_TELEM_BUFFER_SIZE) {
    // Convert buffer to telemetry data and populate the relevant ESC
    kissesc_buffer_to_telem(onewire.buffer,
                            &onewire.escs[onewire.esc_motor_idx].telem_data);
    // Update parameter to let main process know that telemetry data has been
    // receieved
    onewire.telem_updated_esc = onewire.esc_motor_idx;
    // Reset buffer idx
    onewire.buffer_idx = 0;
    // Debug: Print onewire buffer:
    // kissesc_print_buffer(onewire.buffer, KISS_ESC_TELEM_BUFFER_SIZE);
  }
}

/**
 * @brief Validation checks, else return false
 * This function should be called before sending a uart telemetry request (e.g.
 * in @ref onewire_repeating_req)
 *
 * 1. All telemtry bits in ESCs should be unset
 * 2. telem->buffer_idx == KISS_ESC_TELEM_BUFFER_SIZE
 *
 * @param telem onewire_t
 * @return true  if validation checks pass
 * @return false if validation checks fail
 */
static inline bool validate_onewire_repeating_req(onewire_t *const telem) {
  // Check if telemetry bit set in any ESC
  uint telemetry_bit_set = 0;
  for (size_t i = 0; i < ESC_COUNT; ++i) {
    telemetry_bit_set += telem->escs[i].dshot->packet.telemetry;
  }
  if (telemetry_bit_set) {
    printf("WARN: Telemetry bit set:\t%i\n", telemetry_bit_set);
    return false;
  }

  // Check if buffer_idx = buffer size
  // to verify we have recieved all telemetry data
  if (telem->buffer_idx != KISS_ESC_TELEM_BUFFER_SIZE) {
    printf("WARN: Telemetry buffer idx:\t%i\t", telem->buffer_idx);
    printf("Buffer:\t");
    // Dump contents of buffer up till buffer_idx
    for (size_t i = 0; i < MIN(telem->buffer_idx, KISS_ESC_TELEM_BUFFER_SIZE);
         ++i) {
      printf("%.2x", telem->buffer[i]);
    }
    printf("\n");
    return false;
  }

  return true;
}

/**
 * @brief Function for repeatedly request telemetry
 *
 * Set the telemetry bit in the ESCs in a round-robin fashion.
 * @attention This assumes that dshot_send_packet resets the telemetry bit after
 * sending the packet. We also assume that this routine will not interrupt
 * dshot_send_packet (which I believe is true if we use the same alarm pool,
 * because alarms on the same pool have the same priority, hence don't interrupt
 * each other)
 *
 * @param rt
 * @return `telem->send_req_rt_state` (set this to false to stop requesting
 * telemetry)
 */
static inline bool onewire_repeating_req(repeating_timer_t *rt) {
  onewire_t *telem = (onewire_t *)(rt->user_data);

  // Perform some validation checks (didn't get this working properly)
  // if (!validate_onewire_repeating_req(telem)) {
  //   printf("Warn: onewire failed...\n");
  //   for (size_t i = 0; i < ESC_COUNT; ++i) {
  //     telem->escs[i].dshot->packet.telemetry = 0;
  //   }
  //   telem->send_req_rt_state = false;
  //   return false;
  // }
  // printf("TlmReq\n");

  // --- Reset data:
  // Reset telemetry bit if not done so
  telem->escs[telem->esc_motor_idx].dshot->packet.telemetry = 0;
  // Reset buffer_idx
  telem->buffer_idx = 0;
  // No need to reset onewire->buffer, because it will be overwritten anyways

  // Configure the next ESC to request telemetry over uart
  telem->esc_motor_idx = (telem->esc_motor_idx + 1) % ESC_COUNT;
  telem->escs[telem->esc_motor_idx].dshot->packet.telemetry = 1;

  return telem->send_req_rt_state;
}

/**
 * @brief Configure uart and gpio
 *
 * @param telem onewire variable
 * @param uart uart0 or uart1
 * @param gpio
 * @param pull_up
 *
 * Panics if gpio cannot be used for uart rx
 */
static inline void onewire_uart_gpio_configure(onewire_t *const telem,
                                               uart_inst_t *const uart,
                                               const uint gpio, bool pull_up) {
  telem->uart = uart;
  telem->gpio = gpio;
  // Initialise uart pico and set baudrate
  telem->baudrate = uart_init(telem->uart, ONEWIRE_BAUDRATE);
  // Turn off flow control (default)
  uart_set_hw_flow(telem->uart, false, false);
  /// Set data format (default)
  uart_set_format(telem->uart, 8, 1, UART_PARITY_NONE);
  /// enable fifo (default)
  uart_set_fifo_enabled(telem->uart, true);

  // Set GPIO pin mux for UART RX
  if (!validate_uart_rx_gpio(telem->gpio))
    panic("onewire gpio cannot be configured for uart rx");
  gpio_set_function(telem->gpio, GPIO_FUNC_UART);
  // Optionally set pull up resistor
  gpio_pull_up(telem->gpio);
}

/**
 * @brief Setup IRQ handler for onewire uart on RX
 *
 * @param telem
 */
static void onewire_setup_irq(onewire_t *const telem, irq_handler_t handler) {
  // Reset buffer idx
  telem->buffer_idx = 0;
  // No esc telemetry data has been received, so set update variable to -1
  telem->telem_updated_esc = -1;
  // Add exclusive interrupt handler on RX (for parsing onewire telemetry)
  const int UART_IRQ = telem->uart == uart0 ? UART0_IRQ : UART1_IRQ;
  irq_set_exclusive_handler(UART_IRQ, handler);
  irq_set_enabled(UART_IRQ, true);
  // Enable uart interrupt on RX
  uart_set_irq_enables(telem->uart, true, false);
}

/**
 * @brief Configure repeating timer for onewire to request telemetry
 *
 * @param telem onewire_t variable
 * @param telem_interval delay between repeating timer
 * @param pool this should be the same alarm pool used to configure dshot
 */
static void onewire_rt_configure(onewire_t *const telem,
                                 long int telem_interval, alarm_pool_t *pool) {
  telem_interval = MAX(ONEWIRE_MIN_INTERVAL_US * ESC_COUNT, telem_interval);
  telem->send_req_rt_state = alarm_pool_add_repeating_timer_us(
      pool, telem_interval, onewire_repeating_req, telem, &telem->send_req_rt);
}

/**
 * @brief initialise uart telemetry config
 *
 * @param gpio GPIO port connected to onewire telemetry uart
 * @param req_flag Request telemetry on initialisation
 * @param pool alarm pool to add the repeating timer to request telemetry
 * regularly
 * @param dshot array of dshot configs. The pointers are stored so that
 * telemetry bit can be set when required
 *
 * panics if @ref ESC_COUNT < 1
 */
static void telem_uart_init(onewire_t *const telem, uart_inst_t *const uart,
                            const uint gpio, alarm_pool_t *const pool,
                            long int telem_interval,
                            dshot_config *escs[ESC_COUNT], bool req_flag,
                            bool pull_up) {
  // Initialise uart and gpio
  onewire_uart_gpio_configure(telem, uart, gpio, pull_up);

  // copy pointers to ESC configs
  if (ESC_COUNT < 1)
    panic("Expected more than one ESC");
  for (size_t esc_num = 0; esc_num < ESC_COUNT; ++esc_num) {
    telem->escs[esc_num].dshot = escs[esc_num];
  }
  telem->esc_motor_idx = 0;

  // Setup onewire uart IRQ to handle telemetry data
  onewire_setup_irq(telem, onewire_uart_irq);

  // Setup telemetry repeating timer
  telem->send_req_rt_state = req_flag;
  if (telem->send_req_rt_state) {
    onewire_rt_configure(telem, telem_interval, pool);
  }
}

/**
 * @brief return true if onewire telemtry has been updated
 *
 * The way onewire is setup is that a repeating timer is used
 * to request telemetry. The uart then receieves the telemetry
 * transmission and decodes this. However, the main loop doesn't
 * know when the data has been received. Hence, this function can
 * be used to check if the data has been receieved;
 * i.e. this checks if onewire telemetry data has been updated
 *
 * @param onewire
 */
static inline bool is_telem_updated(onewire_t *const telem) {
  return (telem->telem_updated_esc >= 0);
}

/**
 * @brief helper function to reset telem update flag
 *
 * This is useful in the main loop code.
 * (see examples/onewire_telemetry)
 * After reading telemetry data, the telem_updated_esc flag
 * should be reset, which is the purpose of this function.
 *
 * @param telem
 */
static inline void reset_telem_updated(onewire_t *const telem) {
  telem->telem_updated_esc = -1;
}

/// @brief print uart_telem config
void print_onewire_config(onewire_t *onewire);

#ifdef __cplusplus
}
#endif