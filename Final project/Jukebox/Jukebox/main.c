#include <avr/io.h>
#include <avr/interrupt.h>
#include "mp3.h"

#define BTN_PIN     PD2
#define CLICK_WINDOW 400
#define NUM_TRACKS  10

volatile uint8_t clickCount = 0;
volatile uint32_t lastClickMs = 0;

static inline uint32_t millis(void)
{
	return (uint32_t)(TCNT1 / 16);
}

ISR(INT0_vect)
{
	if (PIND & (1 << BTN_PIN)) return;
	clickCount++;
	lastClickMs = millis();
}

int main(void)
{
	TCCR1B = (1 << CS11) | (1 << CS10);

	DDRD &= ~(1 << BTN_PIN);
	PORTD |= (1 << BTN_PIN);
	EICRA = (1 << ISC01);
	EIMSK = (1 << INT0);

	mp3Init(9600);

	sei();

	uint8_t currentTrack = 1;
	uint8_t playing = 0;

	while (1)
	{
		uint32_t now = millis();
		if (clickCount && (now - lastClickMs) > CLICK_WINDOW)
		{
			if (clickCount == 1)
			{
				if (!playing)
				{
					mp3PlayTrack(currentTrack);
					playing = 1;
				}
			}
			else if (clickCount >= 2)
			{
				currentTrack++;
				if (currentTrack > NUM_TRACKS) currentTrack = 1;
				mp3PlayTrack(currentTrack);
				playing = 1;
			}
			clickCount = 0;
		}
	}
}
