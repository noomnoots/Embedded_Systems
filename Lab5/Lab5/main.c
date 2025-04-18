/*
Nick/Andre - Lab 5
*/


#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>

#define BAUD 9600 // 9600 bits/sec
#define myUBRR ((F_CPU) / (16UL * (BAUD)) - 1) //eq to setup asynchronous mode

// Function protos
void usartSendChar(char c);
void usartSendString(const char* str);
void usartInit(unsigned int ubrr);
char usartReceiveChar(void);
void adcInit(void);
uint16_t adcRead(uint8_t channel);

void usartInit(unsigned int ubrr)
{
	UBRR0H = (unsigned char)(ubrr >> 8); //split reg into lower and upper bits
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1 << RXEN0) | (1 << TXEN0); //receive and transmit enable
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); //8-bit character size
}

void usartSendChar(char c)
{
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = c;
}

void usartSendString(const char* str)
{
	while (*str)
	{
		usartSendChar(*str++);
	}
}

char usartReceiveChar(void)
{
	while (!(UCSR0A & (1 << RXC0)));
	return UDR0;
}

void adcInit(void)
{
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);
}

uint16_t adcRead(uint8_t channel)
{
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return ADC;
}

int main(void)
{
	usartInit(myUBRR);
	adcInit();
	usartSendString("Type 'G' to get voltage reading...\r\n");

	while (1)
	{
		char cmd = usartReceiveChar();

		if (cmd == 'G' || cmd == 'g')
		{
			uint16_t val = adcRead(0);
			float voltage = (val / 1023.0) * 5.0;

			char floatStr[16];  // safe buffer for float-to-string
			char buffer[32];    // output string buffer

			dtostrf(voltage, 6, 2, floatStr);  // 6 width, 2 decimal places
			snprintf(buffer, sizeof(buffer), "Voltage = %sV\r\n", floatStr);
			usartSendString(buffer);

		}
	}
}




//cd "C:\users\n3400\documents\atmel studio\7.0\EmbeddedSystems\Lab5"
//
//git status
//
//git add .
//
//git commit -m "explain what you changed here"
//
//git push


//Remember not to continuously write null character