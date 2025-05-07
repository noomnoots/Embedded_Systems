#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/twi.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdio.h>
#include "mp3.h"

// ==== LCD CONFIG ====
#define LCD_DATA_PORT PORTC
#define LCD_DATA_DDR  DDRC
#define LCD_CTRL_PORT PORTB
#define LCD_CTRL_DDR  DDRB
#define LCD_RS        PB0
#define LCD_E         PB1
#define OVERFLOWS_PER_SECOND 977

// ==== RFID ====
#define RFID_ADDR 0x13
#define MAX_UID_LEN 6
const char admin_uid[MAX_UID_LEN] = {0x3A, 0x00, 0x6C, 0x34, 0xF9, 0x9B};
const char user_uid[MAX_UID_LEN]  = {0x3A, 0x00, 0x6C, 0x6D, 0xBA, 0x81};

// ==== SONGS ====
#define TOTAL_SONGS 9
const char *titles[] = {
	"Go Robot", "Migra", "Expresso", "Sticky",
	"Judas", "Let It Be", "Africa",
	"Sweet Child", "Thunderstruck"
};
const char *artists[] = {
	"RHCP", "Santana", "Sabrina ", "TylerTC",
	"Lady Gaga", "The Beatles", "Toto",
	"Guns N' R", "AC/DC"
};

volatile int song_index = 0;
volatile int selected_song = -1;
volatile uint8_t update_display = 1;
volatile uint32_t last_scroll_time = 0;
volatile uint8_t rpg_moved = 0;
volatile uint8_t no_credit_flag = 0;
volatile uint32_t no_credit_time = 0;
volatile uint8_t credits = 3;
volatile uint8_t prev_credits = 3;
volatile uint8_t admin_mode = 0;

// ==== LCD Custom Characters ====
uint8_t music_icon[8] = {
	0b00000,
	0b00100,
	0b00110,
	0b00101,
	0b00101,
	0b11100,
	0b11100,
	0b00000
};

// ==== LCD Functions ====
void lcd_nibble(uint8_t nibble) {
	LCD_DATA_PORT = (LCD_DATA_PORT & 0xF0) | (nibble & 0x0F);
	LCD_CTRL_PORT |= (1 << LCD_E);
	_delay_us(1);
	LCD_CTRL_PORT &= ~(1 << LCD_E);
	_delay_us(1);
}

void lcd_command(uint8_t cmd) {
	LCD_CTRL_PORT &= ~(1 << LCD_RS);
	lcd_nibble(cmd >> 4);
	lcd_nibble(cmd & 0x0F);
	_delay_us(40);
}

void lcd_data(uint8_t data) {
	LCD_CTRL_PORT |= (1 << LCD_RS);
	lcd_nibble(data >> 4);
	lcd_nibble(data & 0x0F);
	_delay_us(40);
}

void lcd_init(void) {
	LCD_DATA_DDR |= 0x0F;
	LCD_CTRL_DDR |= (1 << LCD_RS) | (1 << LCD_E);
	_delay_ms(50);
	lcd_nibble(0x03); _delay_ms(5);
	lcd_nibble(0x03); _delay_us(150);
	lcd_nibble(0x03); _delay_us(150);
	lcd_nibble(0x02);
	lcd_command(0x28);
	lcd_command(0x0C);
	lcd_command(0x06);
	lcd_command(0x01);
	_delay_ms(2);
}

void lcd_create_char(uint8_t location, uint8_t *charmap) {
	location &= 0x07;
	lcd_command(0x40 | (location << 3));
	for (uint8_t i = 0; i < 8; i++) {
		lcd_data(charmap[i]);
	}
}

void lcd_clear(void) {
	lcd_command(0x01);
	_delay_ms(2);
}

void lcd_gotoxy(uint8_t x, uint8_t y) {
	lcd_command(0x80 + (y ? 0x40 : 0x00) + x);
}

void lcd_putc(char c) {
	lcd_data(c);
}

void lcd_puts(const char *s) {
	while (*s) lcd_putc(*s++);
}

// ==== Timer/Interrupt ====
void timer_init(void) {
	TCCR0A = 0;
	TCCR0B = (1 << CS01) | (1 << CS00);
	TIMSK0 = (1 << TOIE0);
	TCNT0 = 0;
}

ISR(TIMER0_OVF_vect) {
	static uint16_t count = 0;
	count++;
	if (count >= OVERFLOWS_PER_SECOND) {
		last_scroll_time++;
		count = 0;
	}
}

void encoder_init(void) {
	// PD2 = A, PD3 = B
	DDRD  &= ~((1<<PD2)|(1<<PD3));
	PORTD |=  (1<<PD2)|(1<<PD3);   // pull-ups

	// Configure INT0 on PD2 for rising-edge only
	// ISC01 ISC00 = 1 1 ? rising edge
	EICRA &= ~((1<<ISC01)|(1<<ISC00));      // clear bits first
	EICRA |=  (1<<ISC01)|(1<<ISC00);

	EIMSK |= (1<<INT0);                     // enable INT0
}



ISR(INT0_vect) {
	uint8_t A = (PIND & (1 << PD2)) >> PD2;
	uint8_t B = (PIND & (1 << PD3)) >> PD3;
	if (A != B)
	song_index = (song_index + 1) % TOTAL_SONGS;
	else
	song_index = (song_index == 0) ? TOTAL_SONGS - 1 : song_index - 1;
	last_scroll_time = 0;
	rpg_moved = 1;
	update_display = 1;
}

// ==== Button ====
#define BUTTON_PIN PD4

void button_init(void) {
	DDRD &= ~(1 << BUTTON_PIN) | (1 << PD5);
	PORTD |= (1 << BUTTON_PIN) | (1 << PD5); 
}

uint8_t button_pressed(void) {
	static uint8_t last_state = 1;
	uint8_t current = PIND & (1 << BUTTON_PIN);
	if (!current && last_state) {
		_delay_ms(50);
		last_state = 0;
		return 1;
		} else if (current) {
		last_state = 1;
	}
	return 0;
}

uint8_t button_pressed2(void) {
	static uint8_t last_state2 = 1;
	uint8_t current = PIND & (1 << PD5);
	if (!current && last_state2) {
		_delay_ms(50);
		last_state2 = 0;
		return 1;
		} else if (current) {
		last_state2 = 1;
	}
	return 0;
}
// ==== I2C ====
void i2c_init(void) {
	TWBR = 0x48;
	TWCR = (1 << TWEN);
}

uint8_t i2c_start(uint8_t address) {
	TWCR = (1 << TWSTA) | (1 << TWEN) | (1 << TWINT);
	while (!(TWCR & (1 << TWINT)));
	TWDR = address;
	TWCR = (1 << TWEN) | (1 << TWINT);
	while (!(TWCR & (1 << TWINT)));
	return (TWSR & 0xF8);
}

void i2c_stop(void) {
	TWCR = (1 << TWSTO) | (1 << TWEN) | (1 << TWINT);
	while (TWCR & (1 << TWSTO));
}

uint8_t i2c_read_ack(void) {
	TWCR = (1 << TWEN) | (1 << TWINT) | (1 << TWEA);
	while (!(TWCR & (1 << TWINT)));
	return TWDR;
}

uint8_t i2c_read_nack(void) {
	TWCR = (1 << TWEN) | (1 << TWINT);
	while (!(TWCR & (1 << TWINT)));
	return TWDR;
}

uint8_t read_rfid_uid(char *uid_buf) {
	if (i2c_start(RFID_ADDR << 1) != TW_MT_SLA_ACK) return 0;
	i2c_stop();
	if (i2c_start((RFID_ADDR << 1) | 1) != TW_MR_SLA_ACK) return 0;
	for (uint8_t i = 0; i < MAX_UID_LEN; i++) {
		uid_buf[i] = (i < MAX_UID_LEN - 1) ? i2c_read_ack() : i2c_read_nack();
	}
	i2c_stop();
	return 1;
}

// ==== Display ====
void show_admin_message(uint8_t enabled) {
	lcd_clear();
	lcd_gotoxy(4, 0);
	lcd_puts("ADMIN");
	lcd_gotoxy(1, 1);
	lcd_puts(enabled ? " MODE ENABLED" : "MODE DISABLED");
	_delay_ms(4000);
}

void display_song(int index) {
	lcd_clear();
	const char *title = titles[index];
	const char *artist = artists[index];
	size_t len = strlen(title);
	if (len <= 16) {
		lcd_gotoxy(0, 0);
		lcd_puts(title);
		lcd_gotoxy(0, 1);
		lcd_puts(artist);
		lcd_gotoxy(11, 1);
		if (credits == 255)
		lcd_puts("C:I");
		else {
			char buf[6];
			snprintf(buf, sizeof(buf), "C:%u", credits);
			lcd_puts(buf);
		}
		if (index == selected_song) {
			lcd_gotoxy(15, 1);
			lcd_putc(0); // music icon
		}
		return;
	}

	char buf[128];
	snprintf(buf, sizeof(buf), "%s   %s   ", title, title);
	size_t buf_len = strlen(buf);
	size_t scroll = 0;

	while (song_index == index && !update_display) {
		lcd_gotoxy(0, 0);
		for (int j = 0; j < 16; j++) {
			lcd_putc(buf[(scroll + j) % buf_len]);
		}
		lcd_gotoxy(0, 1);
		lcd_puts(artist);
		lcd_gotoxy(11, 1);
		if (credits == 255)
		lcd_puts("C:I");
		else {
			char buf2[6];
			snprintf(buf2, sizeof(buf2), "C:%u", credits);
			lcd_puts(buf2);
		}
		if (index == selected_song) {
			lcd_gotoxy(15, 1);
			lcd_putc(0); // music icon
		}
		if (button_pressed()) {
			if (credits > 0) {
				if (credits != 255) credits--;
				selected_song = index;
				} else {
				no_credit_flag = 1;
				no_credit_time = last_scroll_time;
			}
			update_display = 1;
			break;
		}
		if (button_pressed2()) {
			if (admin_mode) {
				mp3Toggle();
			}
			update_display = 1;
			break;
		}
		_delay_ms(400);
		scroll = (scroll + 1) % buf_len;
	}
}

int main(void) {
	// —————————————————————————————————————
	// 1) Clear any prior reset flags
	MCUSR = 0;

	// 2) Turn off the watchdog
	wdt_disable();
	// —————————————————————————————————————

	// 3) Now do your normal init…
	lcd_init();
	lcd_create_char(0, music_icon);
	i2c_init();
	encoder_init();
	button_init();
	timer_init();
	mp3Init(38400);
	sei(); 


	display_song(song_index);
	update_display = 0;
	uint32_t last_rpg_time = 0;

	while (1) {
		char uid[MAX_UID_LEN];
		if (read_rfid_uid(uid)) {
			if (memcmp(uid, admin_uid, MAX_UID_LEN) == 0) {
				if (!admin_mode) {
					prev_credits = credits;
					credits = 255;
					admin_mode = 1;
					show_admin_message(1);
					} else {
					credits = prev_credits;
					admin_mode = 0;
					show_admin_message(0);
				}
				} else if (memcmp(uid, user_uid, MAX_UID_LEN) == 0) {
				if (admin_mode) {
					lcd_clear();
					lcd_gotoxy(0, 0);
					lcd_puts(" Incorrect Card");
					lcd_gotoxy(0, 1);
					lcd_puts("   Admin Only");
					_delay_ms(4000);
					} else if (credits < 254) {
					credits++;
				}
			}
			update_display = 1;
			_delay_ms(1000);
		}

		if (button_pressed()) {
			if (credits > 0 || admin_mode) {         /* allow if credit or admin */
				if (selected_song == song_index) {
					no_credit_flag = 1;
					no_credit_time = last_scroll_time;
					} else {
					if (!admin_mode && credits != 255) credits--;
					selected_song   = song_index;
					last_scroll_time = 0;
					last_rpg_time   = 0;

					mp3PlayTrack(selected_song + 1);  /* <??? ADDED */
				}
				} else {
				no_credit_flag = 1;
				no_credit_time = last_scroll_time;
			}
			update_display = 1;
		}
		
		if (button_pressed2()) {
			if (admin_mode) {
				mp3Toggle();
			}
			update_display = 1;
			break;
		}

		if (rpg_moved) {
			last_rpg_time = last_scroll_time;
			rpg_moved = 0;
		}

		if (selected_song != -1 && song_index != selected_song &&
		(last_scroll_time - last_rpg_time >= 10)) {
			song_index = selected_song;
			last_scroll_time = 0;
			last_rpg_time = 0;
			update_display = 1;
		}

		if (no_credit_flag) {
			if ((last_scroll_time - no_credit_time) == 0) {
				lcd_clear();
				if (selected_song == song_index) {
					lcd_gotoxy(0, 0);
					lcd_puts("Already Selected");
					lcd_gotoxy(0, 1);
					lcd_puts("  Choose Again");
					} else {
					lcd_gotoxy(0, 0);
					lcd_puts("  No credits!");
					lcd_gotoxy(0, 1);
					lcd_puts("Scan RFID to add");
				}
			}
			if ((last_scroll_time - no_credit_time) >= 3) {
				no_credit_flag = 0;
				update_display = 1;
			}
			continue;
		}

		if (update_display) {
			update_display = 0;
			display_song(song_index);
		}
	}
}









//Push button
//PD4
//RPG Pin
//A	PD2
//B	PD3
//C GND
//| LCD Pin | Name | Connect To        | Notes                       |
//|---------|------|-------------------|-----------------------------|
//| 1       | VSS  | GND               | Ground                      |
//| 2       | VCC  | +5V               | Power                       |
//| 3       | V0   | resistor to ground| For contrast                |
//| 4       | RS   | PB0 (Pin 14)      | Register Select             |
//| 5       | RW   | GND               | Write only                  |
//| 6       | E    | PB1 (Pin 15)      | Enable                      |
//| 11      | D4   | PC0 (Pin 23)      | Data bit 4                  |
//| 12      | D5   | PC1 (Pin 24)      | Data bit 5                  |
//| 13      | D6   | PC2 (Pin 25)      | Data bit 6                  |
//| 14      | D7   | PC3 (Pin 26)      | Data bit 7                  |
//| 15      | LED+ | +5V			   | Backlight                   |
//| 16      | LED- | GND               | Backlight ground            |