// ===== mp3.c =====
#include <avr/io.h>
#define F_CPU 16000000UL
#include <util/delay.h>
#include "mp3.h"

#define MP3_SERIAL_UBRR(baud)  ((F_CPU / (16UL * (baud))) - 1)

/** Send one byte and wait until fully shifted out. */
static void usartSendByte(uint8_t c)
{
    /* wait for UDR ready */
    while (!(UCSR0A & (1 << UDRE0)));
    /* send the byte */
    UDR0 = c;
    /* wait for transmission complete */
    while (!(UCSR0A & (1 << TXC0)));
    /* clear TXC0 by writing a 1 */
    UCSR0A |= (1 << TXC0);
    /* small gap so the RX LED can register the pulse */
    _delay_ms(3);
}

void mp3Toggle(void)
{
    usartSendByte('O');
}

void mp3Init(uint32_t baud)
{
    uint16_t ubrr = MP3_SERIAL_UBRR(baud);
    UBRR0H = (ubrr >> 8);
    UBRR0L = (ubrr & 0xFF);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    _delay_ms(100);
}

void mp3PlayTrack(uint16_t track)
{
    if (track == 0 || track > 255) return;
    usartSendByte('O');
    usartSendByte((track / 100)       + '0');
    usartSendByte(((track / 10) % 10) + '0');
    usartSendByte((track % 10)        + '0');
}

void mp3Next(void)
{
    usartSendByte('F');
}

uint8_t mp3IsBusy(void)
{
    return (PIND & (1 << PD3)) == 0;
}


//End of stuff