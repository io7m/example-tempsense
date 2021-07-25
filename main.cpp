#include "uart.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsometimes-uninitialized"
#include <avr/io.h>
#include <avr/sfr_defs.h>
#include <util/delay.h>
#pragma clang diagnostic pop

constexpr uint8_t LED_PINS_OUTPUT = 0b11111111;
constexpr uint8_t LED_PIN_BLUE = 0b00000001;
constexpr uint8_t LED_PIN_RED = 0b00000010;
constexpr uint8_t LED_PIN_GREEN = 0b00000100;

constexpr uint8_t PIN_OUTPUT = 0b10000000;
constexpr uint8_t PIN_INPUT = 0b00000000;
constexpr uint8_t PIN_HIGH = 0b10000000;
constexpr uint8_t PIN_LOW = 0b00000000;

/**
 * Calculate the number of timer ticks required in order for `useconds`
 * to elapse. This assumes a /64 timer prescaler.
 *
 * @return The given useconds as a number of timer ticks
 */

static constexpr uint8_t usecToTicks64(uint8_t useconds) {
  return useconds / 4;
}

static constexpr uint8_t bit(uint8_t index) { return 1 << index; }

static void run(void) {
  uint32_t iteration = 0;
  uint8_t sensorData[5] = {0};

  uart_init();
  uart_puts("I: start\n");

  DDRB = LED_PINS_OUTPUT;
  PORTB = 0;

  /*
   * Configure the timer to use a /64 prescaler.
   *
   * With a 16mhz cpu speed, it would take (1 / 16000000) seconds = 0.0625
   * useconds to increment the timer once. With an 8-bit timer, the timer would
   * overflow in 0.0625 * 255 = 15.9375 useconds. The longest period we're
   * required to wait in the DHT11 datasheet (not counting the initial power-on,
   * or the initial 20ms request pause) is 80 useconds, so a timer at this
   * resolution won't be enough.
   *
   * With a /64 prescaler, it would take (1 / (16000000 / 64)) seconds = 4
   * useconds to increment the timer once. With an 8-bit timer, the timer would
   * overflow in 4 * 255 = 1020 useconds. Given our requirement of being able to
   * count 80 useconds, this is enough.
   */

  TCCR0B = 0b00000011;
  TCNT0 = 0;

  for (;;) {
    PORTB = 0;
    _delay_ms(3000);

    uart_puts("I: iteration ");
    uart_putu32(iteration);
    uart_puts("\n");

    /*
     * Pull the pin low and then wait for 20ms. This tells the sensor that we
     * want to take a reading.
     */

    PORTB = LED_PIN_BLUE;
    DDRA = PIN_OUTPUT;
    PORTA = PIN_LOW;
    _delay_ms(20);

    /*
     * Set the pin high, set the pin as an input, and then wait for the sensor
     * to pull the pin low.
     */

    PORTB = LED_PIN_GREEN;
    PORTA = PIN_HIGH;
    DDRA = PIN_INPUT;
    for (;;) {
      if (PINA == PIN_LOW) {
        break;
      }
    }

    /*
     * Now that the pin is pulled low, the sensor will wait approximately 80us
     * before pulling the pin high again.
     */

    PORTB = LED_PIN_RED;
    TCNT0 = 0;
    for (;;) {
      if (PINA == PIN_HIGH) {
        break;
      }
      if (TCNT0 > usecToTicks64(88)) {
        uart_puts("E: timed out waiting for 80us HIGH\n");
        goto END_ITERATION;
      }
    }

    /*
     * Now that the pin is pulled high, the sensor will wait approximately 80us
     * before it begins to send data.
     */

    PORTB = LED_PIN_BLUE;
    TCNT0 = 0;
    for (;;) {
      if (PINA == PIN_LOW) {
        break;
      }
      if (TCNT0 > usecToTicks64(88)) {
        uart_puts("E: timed out waiting for 80us LOW\n");
        goto END_ITERATION;
      }
    }

    PORTB = 0;

    for (unsigned int byteIndex = 0; byteIndex < 5; ++byteIndex) {
      uint8_t byte = 0;

      for (unsigned int bitIndex = 0; bitIndex < 8; ++bitIndex) {

        /*
         * The sensor sets the pin low for 50usec before every bit.
         */

        TCNT0 = 0;
        for (;;) {
          if (PINA == PIN_HIGH) {
            break;
          }
          if (TCNT0 > usecToTicks64(52)) {
            uart_puts("E: waiting for bit\n");
            goto END_ITERATION;
          }
        }

        /*
         * Then, the sensor sets the pin high before setting it low
         * again. If the pin was high for <= 28usec, then
         * this should be interpreted to mean that the sensor sent
         * a 0. Otherwise, it's interpreted to mean that the sensor
         * sent a 1.
         */

        TCNT0 = 0;
        for (;;) {
          if (PINA == PIN_LOW) {
            if (TCNT0 <= usecToTicks64(28)) {
              byte &= ~bit(7 - bitIndex);
            } else {
              byte |= bit(7 - bitIndex);
            }
            break;
          }
        }
      }

      sensorData[byteIndex] = byte;
    }

    uart_puts("D: ");
    for (unsigned int byteIndex = 0; byteIndex < 5; ++byteIndex) {
      uart_putu8b(sensorData[byteIndex]);
      uart_puts(" ");
    }

    {
      uint8_t checksum = 0;
      checksum += sensorData[0];
      checksum += sensorData[1];
      checksum += sensorData[2];
      checksum += sensorData[3];

      if (checksum == sensorData[4]) {
        uart_puts("ok");
      } else {
        uart_puts("bad");
      }
      uart_puts("\n");

      uart_puts("H: ");
      uart_putu32(sensorData[0]);
      uart_puts(".");
      uart_putu32(sensorData[1]);
      uart_puts("\n");
      uart_puts("T: ");
      uart_putu32(sensorData[2]);
      uart_puts(".");
      uart_putu32(sensorData[3]);
      uart_puts("\n");
    }

  END_ITERATION:
    ++iteration;
  }
}

extern "C" {
int main(void) {
  run();
  return 0;
}
}
