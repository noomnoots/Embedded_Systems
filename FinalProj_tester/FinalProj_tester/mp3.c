/* mp3.c – implementation for SparkFun MP3 Trigger v2.4 */

#define F_CPU 16000000UL     // must come before <util/delay.h>
#include <avr/io.h>
#include <util/delay.h>
#include "mp3.h"
#include <stdlib.h>
#define MP3_SERIAL_UBRR(b)  ((F_CPU/(16UL*(b)))-1)


static void usartSendByte(uint8_t c) {
	while (!(UCSR0A & (1<<UDRE0)));
	UDR0 = c;
	while (!(UCSR0A & (1<<TXC0)));
	UCSR0A |= (1<<TXC0);
	_delay_ms(3);
}

void mp3Init(uint32_t baud) {
	uint16_t ubrr = MP3_SERIAL_UBRR(baud);
	UBRR0H = ubrr>>8;
	UBRR0L = ubrr;
	UCSR0B = (1<<TXEN0);
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
	_delay_ms(10);
	
	usartSendByte('T');
	usartSendByte('1');
}

void mp3PlayTrack(uint16_t track) {
	if (track < 1 || track > 9) return;
	usartSendByte('O');
	_delay_ms(10);
	
	usartSendByte('O');
	_delay_ms(10);
	
	usartSendByte('T');
	usartSendByte(track + '0');
}

void mp3Next(void)   { 
	usartSendByte('F'); 
	}
void mp3Toggle(void) { 
	usartSendByte('O'); 
	}

/* Busy on PB2 so PD3 is free for the encoder */
#define BUSY_PIN PB2
uint8_t mp3IsBusy(void) {
	return (PINB & (1<<BUSY_PIN)) == 0;
}
