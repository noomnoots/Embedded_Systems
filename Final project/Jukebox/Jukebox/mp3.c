#include "mp3.h"
#include <util/delay.h>

#define MP3_SERIAL_UBRR(baud) ((F_CPU / (16UL * (baud))) - 1)

static void usartSendByte(uint8_t c)
{
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = c;
}

void mp3Init(uint32_t baud)
{
	uint16_t ubrr = MP3_SERIAL_UBRR(baud);
	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)ubrr;
	UCSR0B = (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
	_delay_ms(100);
}

void mp3PlayTrack(uint16_t track)
{
	if (track == 0 || track > 255) return;
	usartSendByte('O');
	usartSendByte((track / 100) + '0');
	usartSendByte(((track / 10) % 10) + '0');
	usartSendByte((track % 10) + '0');
}

void mp3Next(void)
{
	usartSendByte('F');
}

uint8_t mp3IsBusy(void)
{
	return (PIND & (1 << PD3)) == 0;
}
