/* mp3.c – low?level UART helper for SparkFun MP3 Trigger v2.4 */

#include "jukebox_config.h"
#include "mp3.h"
#include <avr/io.h>
#include <util/delay.h>

#define MP3_SERIAL_UBRR(b)  ((F_CPU / (16UL * (b))) - 1)

/* ---------- private helper ---------- */
static void usartSendByte(uint8_t c)
{
	while (!(UCSR0A & (1 << UDRE0)));   /* wait for TX buffer */
	UDR0 = c;
	while (!(UCSR0A & (1 << TXC0)));    /* wait for shift reg */
	UCSR0A |= (1 << TXC0);              /* clear flag         */
	_delay_ms(3);                       /* Trigger needs ~2?ms */
}

/* ---------- public API ---------- */
void mp3Init(uint32_t baud)
{
	uint16_t ubrr = MP3_SERIAL_UBRR(baud);
	UBRR0H  = ubrr >> 8;
	UBRR0L  = ubrr & 0xFF;
	UCSR0B  = (1 << TXEN0);                   /* TX only          */
	UCSR0C  = (1 << UCSZ01) | (1 << UCSZ00);  /* 8?N?1            */
	_delay_ms(10);
	/* No auto?play here – project spec says silent on power?up */
}

void mp3Stop(void)              { usartSendByte('0');        }  /* toggle => stop */
void mp3Next(void)              { usartSendByte('F');        }
void mp3Toggle(void)            { usartSendByte('0');        }  /* alias */

void mp3PlayTrack(uint8_t track)
{
	if (track < 1 || track > TOTAL_SONGS) return;

	mp3Stop();                               /* make sure we’re idle */
	_delay_ms(20);

	if (track <= 9)                          /* ASCII ‘T’ + digit 1?9 */
	{
		usartSendByte('T');
		usartSendByte(track + '0');
	}
	else                                     /* binary trigger for 10?255 */
	{
		usartSendByte('t');
		usartSendByte(track);                /* raw byte */
	}
}

/* BUSY on PB2 (active?low) */
#define BUSY_PIN PB2
uint8_t mp3IsBusy(void)
{
	return (PINB & (1 << BUSY_PIN)) == 0;
}
