// mp3.c low?level UART helper for SparkFun MP3 Trigger v2.4

#include "jukebox_config.h"  // Includes code from jukebox_config.h
#include <avr/io.h>	     // AVR I/O register definitions
#include <util/delay.h>	     // Avr Delay functions
#include "mp3.h"	     // Header file

// Macro to do math to find the UBRR value for a given baud rate
#define MP3_SERIAL_UBRR(b)  ((F_CPU / (16UL * (b))) - 1)

// Sends a single byte over USART0
static void usartSendByte(uint8_t c)
{
	while (!(UCSR0A & (1<<UDRE0)));	// Wait for transmit buffer to be empty
	UDR0 = c;			// Load byte into register
	while (!(UCSR0A & (1<<TXC0)));	// Wait for transmission to be finished
	UCSR0A |= (1<<TXC0);		// Clear TX complete flag
	_delay_ms(3);			// Short delay
}

// Initializes USART0 for communication with the MP3 Trigger
void mp3Init(uint32_t baud)
{
	uint16_t ubrr = MP3_SERIAL_UBRR(baud);	// Compute Baud rate
	UBRR0H = ubrr >> 8;			// set high byte
	UBRR0L = ubrr & 0xFF;			// set low byte
	UCSR0B = (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0);;     // TX + RX + interrupt
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);	// Set 8-bit data format
	_delay_ms(100);                      // let Trigger finish boot

	// Guaranteed STOP: two toggles = pause, then resume?toggle leaves idle
	usartSendByte('O');  _delay_ms(10);   // pause if playing
	usartSendByte('O');  _delay_ms(10);    // resume?toggle -> stopped
}

// Sends O to stop/start playback
void mp3Stop(void) {
	usartSendByte('O');
}

// Plays a specific track on the MP3
void mp3PlayTrack(uint8_t track)
{
	if (track < 1 || track > TOTAL_SONGS) return;	// Checks if it is an invalid track number

	mp3Stop();                              // Ensure the current playback is stopped
	_delay_ms(20);				// Short delay for harware to work

	// Both the if and else are used to play the selected numbered track
	if (track <= 9)                          // ASCII ‘T’ + digit 1-9
	{
		usartSendByte('T');		// Send T to play track 1-9
		usartSendByte(track + '0');	// Convert digit to ASCII
	}
	else                                    // binary trigger for 10-255
	{
		usartSendByte('t');		// Send t for extended range
		usartSendByte(track);           // Send binary track number
	}
}

// Checks to see if the MP3 is playing audio
uint8_t mp3IsBusy(void)
{
	usartSendByte('Q');          // Sends status query
	_delay_ms(3);                // delay for response
	
	if(!(UCSR0A & (1 << RXC0)))  // If no response recieved
	return 0;		 // Assumes that is is not playing
	
	uint8_t resp = UDR0;         // Read response byte 0 = idle, 1 = playing       */
	return resp == 1;
	}