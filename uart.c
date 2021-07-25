#include "uart.h"
#include <avr/io.h>
#include <stdio.h>
#include <util/setbaud.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_init() {
  UBRR0H = UBRRH_VALUE;
  UBRR0L = UBRRL_VALUE;

#if USE_2X
  UCSR0A |= _BV(U2X0);
#else
  UCSR0A &= ~(_BV(U2X0));
#endif

  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
}

int uart_putchar(char c) {
  if (c == '\n') {
    uart_putchar('\r');
  }
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

int uart_getchar() {
  loop_until_bit_is_set(UCSR0A, RXC0);
  return UDR0;
}

void uart_puts(const char *text) {
  const char *ptr = text;
  for (;;) {
    char c = *ptr;
    if (c == 0) {
      break;
    }
    uart_putchar(c);
    ++ptr;
  }
}

void uart_putu32(uint32_t x) {
  char buffer[12] = {0};
  snprintf(buffer, sizeof(buffer), "%u", x);
  uart_puts(buffer);
}

void uart_putu8b(uint8_t x) {
  char buffer[9] = {0};
  for (int index = 0; index < 8; ++index) {
    uint8_t bit = (x >> (8 - index)) & 1;
    if (bit) {
      buffer[index] = '1';
    } else {
      buffer[index] = '0';
    }
  }
  uart_puts(buffer);
}

void uart_putu32x(uint32_t x) {
  char buffer[12] = {0};
  snprintf(buffer, sizeof(buffer), "%x", x);
  uart_puts(buffer);
}

#ifdef __cplusplus
}
#endif
