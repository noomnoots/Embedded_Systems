// ===== main.c =====
// Single-button play/pause on SparkFun MP3 Trigger v2.4
// Remember: default serial = 38400 baud (not 9600)

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "mp3.h"

#define BTN_PIN      PD4
#define BTN_PCINT    PCINT20
#define DEBOUNCE_MS  50

volatile uint8_t btnFlag      = 0;
volatile uint8_t playing      = 0;
volatile uint8_t currentTrack = 1;

/* IRQ: record press (falling edge), blink PB5 */
ISR(PCINT2_vect) {
	if (!(PIND & (1 << BTN_PIN))) {
		btnFlag = 1;
		PORTB ^= (1 << PB5);
	}
}

/* Simple millis() from Timer1 @ F_CPU/64 = 250 kHz ? 1 ms = 250 ticks */
static inline uint32_t millis(void) {
	return TCNT1 / 250;
}

int main(void) {
	/* — DEBUG LED on PB5 — */
	DDRB  |=  (1 << PB5);
	PORTB |=  (1 << PB5);

	/* — Button input w/ pull-up — */
	DDRD  &= ~(1 << BTN_PIN);
	PORTD |=  (1 << BTN_PIN);

	/* — Timer1 for millis() — */
	TCCR1B = (1 << CS11) | (1 << CS10);  // prescale = 64

	/* — Enable pin-change interrupt on PD4 — */
	PCICR  |= (1 << PCIE2);    // group D
	PCMSK2 |= (1 << BTN_PCINT);

	/* — Initialize MP3 Trigger at 38400 baud — */
	mp3Init(38400);

	sei();

	/* — Main loop: handle one clean press at a time — */
	while (1) {
		if (btnFlag) {
			_delay_ms(DEBOUNCE_MS);
			if (!(PIND & (1 << BTN_PIN))) {
				if (!playing) {
					/* first press ? start track 1 */
					mp3PlayTrack(currentTrack);
					playing = 1;
					} else {
					/* subsequent presses ? pause/resume toggle */
					mp3Toggle();
				}
			}
			btnFlag = 0;
		}
	}
	return 0;
}


//End of stuff