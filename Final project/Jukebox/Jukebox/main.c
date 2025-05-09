#define F_CPU 16000000UL //Setting frequency for delays

#include <avr/io.h> // Register names for Ports, DDRC and stuff
#include <avr/interrupt.h> // ISR() vector
#include <util/delay.h> //uses delay_ms and delay_us
#include <util/twi.h> //I^2C register and macros
#include <avr/wdt.h> //watchdog control
#include <string.h> //memcmo, snprintf
#include <stdio.h> // sprintf (LCD credit text)
#include <stdlib.h> //rand+srand

#include "jukebox_config.h" //including other files
#include "mp3.h"


// ---------- LCD wiring & helper macros
#define LCD_DATA_PORT PORTC //setting PC0-PC3 (4-bit mode)
#define LCD_DATA_DDR  DDRC //setting data direction reg
#define LCD_CTRL_PORT PORTB // PB0 =RS, PB1 = E
#define LCD_CTRL_DDR  DDRB
#define LCD_RS        PB0
#define LCD_E         PB1
#define OVERFLOWS_PER_SECOND 977 //Timer0 overflows at prescaler-64 for 1Hz
// --------------------------------------------------------------

// ---------- UIDs & song metadata (single defs) 
const char admin_uid[MAX_UID_LEN] = {0x3A,0x00,0x6C,0x34,0xF9,0x9B}; //RFID codes for cards
const char user_uid [MAX_UID_LEN] = {0x3A,0x00,0x6C,0x6D,0xBA,0x81};

const char *titles[TOTAL_SONGS] = {
    "Go Robot","Migra","Expresso","Sticky",
    "Judas","Let It Be","Africa",
    "Sweet Child","Thunderstruck","Yesterday"
};
const char *artists[TOTAL_SONGS] = {
    "RHCP","Santana","Sabrina ","TylerTC",
    "Lady Gaga","The Beatles","Toto",
    "Guns N' R","AC/DC","The Beatles"
};

//Global---------------------------------------------------
volatile int      song_index       = 0; //current posi of the RPG
volatile int      selected_song    = -1; //Index of the track that is playing
volatile uint8_t  update_display   = 1; //Flag that tells main to redraw LCD next time you can
volatile uint32_t last_scroll_time = 0; //1 sec software clock incremented by Timer-0ISR for debouncing
//long press/display scrolling
volatile uint8_t  rpg_moved        = 0; // Set by RPG ISR when the knob turns to reset inactive timers
volatile uint8_t  no_credit_flag   = 0; //Indication that the user tried to play a song w/no credits
volatile uint32_t no_credit_time   = 0; //Records the timer val when no_credit_flag was raised
volatile uint8_t  credits          = 0; //Current credit balance
volatile uint8_t  prev_credits     = 0; //stores the user's credit count so it can be restored when admin is toggled
volatile uint8_t  admin_mode       = 0; //toggle for admin mode that unlocks PD5 and bypasses credit checks
volatile uint8_t  shuffle_mode     = 0; //For when shuffle is enabled via >2s pd5 press
volatile uint32_t pd5_press_time   = 0; //timestamp for when PD5 is press, classifies short vs. long presses
volatile uint32_t shuffle_grace_until = 0; //future time (s) after firmware can resume busy-polling
volatile uint8_t track_finished = 0; //set by USART RX ISR for the mp3 trigger to send an 'X' byte

  

static void shuffle_play_next(void) //starts a random track in shuffle
{
	selected_song = rand() % TOTAL_SONGS; //Picks a new random index
	song_index    = selected_song; // mirrors encoder pointer
	mp3PlayTrack(selected_song + 1); //mp3 trigger is 1
	update_display = 1; //Forces LCD refresh   
	
	shuffle_grace_until = last_scroll_time + .5;
	//Ignores busy polling for a half second so encoder can set busy flag
}


//Music note
uint8_t music_icon[8] = { 
    0b00000,0b00100,0b00110,0b00101, //bitmap for quarter-note symbol
    0b00101,0b11100,0b11100,0b00000
};

//LCD Driver stuff-----------------------------------------------------
static void lcd_nibble(uint8_t n) //sends 4-bitt nibble
{
    LCD_DATA_PORT = (LCD_DATA_PORT & 0xF0) | (n & 0x0F);//put high nibble as unchanged
    LCD_CTRL_PORT |=  (1 << LCD_E); //sets E = 1
    _delay_us(1); // holds for a microsecond
    LCD_CTRL_PORT &= ~(1 << LCD_E); //E = 0 (data receieved)
    _delay_us(1); //lets bus settle
}

static void lcd_command(uint8_t c)
{
    LCD_CTRL_PORT &= ~(1 << LCD_RS);  //RS = 0 -> instruct register
    lcd_nibble(c >> 4); //loads high 4 bits first
	 lcd_nibble(c & 0x0F); //loads low 4 bits
    _delay_us(40); //gives some delay for commands to go through
}

static void lcd_data(uint8_t d)
{
    LCD_CTRL_PORT |= (1 << LCD_RS); //RS = 1 -> data reg
    lcd_nibble(d >> 4); //high nib
	lcd_nibble(d & 0x0F); //low nib
    _delay_us(40); //delay for commands
}

static void lcd_init(void) //initialize LCD
{
    LCD_DATA_DDR |= 0x0F; //PC0-PC3 outputs (D4-D7)
    LCD_CTRL_DDR |= (1 << LCD_RS) | (1 << LCD_E); //(PB0, PB1) outputs
    _delay_ms(50); // waits for 50 ms after power up
	
    lcd_nibble(0x03); _delay_ms(5); //delays for 8-bit mode
    lcd_nibble(0x03); _delay_us(150);
    lcd_nibble(0x03); _delay_us(150);
	
    lcd_nibble(0x02); //switches to 4-bit mode
	
    lcd_command(0x28); lcd_command(0x0C); //func set to 4-bitm 2 lines, 5x8
    lcd_command(0x06); lcd_command(0x01); //display on, curser off, blinking off
    _delay_ms(2); //clears
}

static void lcd_create_char(uint8_t loc, uint8_t *map)
{
    lcd_command(0x40 | ((loc & 0x07) << 3)); //sets CGRAM address
    for(uint8_t i=0;i<8;i++) lcd_data(map[i]); //writes 8bitmap rows
}

static void lcd_clear(void)                       { lcd_command(0x01); _delay_ms(2); } // erases all characters and resets DDRAM address to 0
static void lcd_gotoxy(uint8_t x,uint8_t y)       { lcd_command(0x80 + (y?0x40:0) + x); } // sets DDRAM address/starts adresses low and 
	//sets row offset
static void lcd_putc(char c)                      { lcd_data(c); } //wrapper to forward one character  and handles RS line, 4-bit splits, and delay
static void lcd_puts(const char*s){ while(*s) lcd_putc(*s++); } //pritns C-string to display one char at a time
	//increments the pointer after each character
	// relies on LCD_put c to get 40microsec delay per byte
//timer 0(1Hz)-----------------------------------------------
static void timer_init(void)
{
    TCCR0A = 0; //clear control reg A
    TCCR0B = (1 << CS01) | (1 << CS00);  //prescaler of 64 (16MHz/64 = 250KHz, overflow = 256 counts = 1024ms
    TIMSK0 = (1 << TOIE0); //enables T0V0 interrupt so TIMER_OVF fires each overflow
    TCNT0  = 0; //reset counter to start timing from zero immediately 
}
ISR(TIMER0_OVF_vect)
{
    static uint16_t cnt = 0; //16-bit accumulator 
    if(++cnt >= OVERFLOWS_PER_SECOND)
	{
        last_scroll_time++; //increments global seconds counter
		cnt = 0; // restarts 1-second window
    }
}

//USART shuffle control
ISR(USART_RX_vect) //executes when a byte arrives on UART0      
{
    uint8_t c = UDR0; //Read byte and clear RX flag

    if (c == 'X') //ASCII for Mp3 triggers "finished" message
	{
        track_finished = 1; //sets flag for main loop so chuffle can start   
    }
    /* Debug:
       else if (c == 'x') {  }   // cancelled by new command
       else if (c == 'E') {  }   // track number error
    */
}



//RPG-------------------------------------------------------------------
static void encoder_init(void)
{
    DDRD  &= ~((1<<PD2)|(1<<PD3)); //set PD2,PD3 as input
    PORTD |=  (1<<PD2)|(1<<PD3);//enable pull-ups      
    EICRA |=  (1<<ISC01)|(1<<ISC00); // INT0 triggers on rising edge    
    EIMSK |=  (1<<INT0); //enable external interrupt 0
}


ISR(INT0_vect) //Fires on rising edge of RPG A pin
{
    uint8_t A = (PIND >> PD2) & 1; //read current logic level A
    uint8_t B = (PIND >> PD3) & 1; //read B phase
	
    song_index = (A != B) // if A != B, the knob is clockwise
	 ? (song_index+1)%TOTAL_SONGS //finds next title
     : (song_index?song_index-1:TOTAL_SONGS-1); //else for counter clockwise
	 
    last_scroll_time = 0; //reset inactivity timer (for auto snap back to song)
    rpg_moved = 1; //flags main loop that the knob has been turned
    update_display = 1; //requests LCD refresh
}


//Button Logic
#define BTN_SELECT PD4 //play/select button
#define BTN_ADMIN  PD5 //admin stop/shuffle button

static void button_init(void) //initilization for GPIO setup
{
    DDRD  &= ~((1<<BTN_SELECT)|(1<<BTN_ADMIN));  // inputs
    PORTD |=  (1<<BTN_SELECT)|(1<<BTN_ADMIN);    // pull?ups
}

static uint8_t btn_select_pressed(void) //edge detector for PD4
{
    static uint8_t last = 1; //remembers previous sampled state
    uint8_t cur = PIND & (1<<BTN_SELECT); //
    if(!cur && last) //Transitions from high to low
	{
		 _delay_ms(50); last = 0; //50ms debounce
		 return 1; //reports a new press
	}
    if( cur) last = 1; //button was released
    return 0;
}

//returns: 0no event, 1short (<2s), 2long
static uint8_t btn_admin_event(void)
{
    static uint8_t  last = 1; //tracks previous logic
    static uint32_t t0   = 0; //time stamp of btn press

    uint8_t cur = PIND & (1<<BTN_ADMIN);
    if(!cur && last){   // pressed
        t0   = last_scroll_time; //saves press time
        last = 0;
    }else if(cur && !last){ // released
        uint32_t dt = last_scroll_time - t0; //tracks # of seconds held
        last = 1;
        return (dt >= 2) ? 2 : 1; //classifies duration
    }
    return 0; //no press
}

//I^2C stuff--------------------------------------------------
static void i2c_init(void)
{
    TWBR = 0x48; //bit rate register; 16MHz -100kHz          
    TWCR = (1 << TWEN); //enables TWI hardware 
}

static uint8_t i2c_start(uint8_t addr) //generates START and SLA + R/W
{
    TWCR = (1<<TWSTA)|(1<<TWEN)|(1<<TWINT); //sends START
    while(!(TWCR & (1<<TWINT))); //waits until complete (status reg now holds START)
    TWDR = addr; //loads slave address byte
    TWCR = (1<<TWEN)|(1<<TWINT); // clears INT to start TX
    while(!(TWCR & (1<<TWINT))); //waits for ACK/NACK
    return (TWSR & 0xF8); //returns status upper bits
}

static void i2c_stop(void) //stop condition
{
    TWCR = (1<<TWSTO)|(1<<TWEN)|(1<<TWINT); //requests stop
    while(TWCR & (1<<TWSTO)); //waits until the bus is released
}

static uint8_t i2c_read(uint8_t ack) //read one byte with an optional ACK
{
    TWCR = (1<<TWEN)|(1<<TWINT)|(ack?1<<TWEA:0); //sets/ckears ack bit
    while(!(TWCR & (1<<TWINT))); //waits to receive
    return TWDR; //returns the byte
}

static uint8_t read_rfid_uid(char *uid)
{
    if(i2c_start(RFID_ADDR<<1) != 0x18) //sends SLA +W, expects ACK
	{
		 i2c_stop(); return 0; //if it failed, gives up
	}
	
    i2c_stop(); //WRITE PHASE is finished
	
    if(i2c_start((RFID_ADDR<<1)|1) != 0x40) //repeated-START, SLA+R. expects an ACK
	{
		 i2c_stop(); return 0; 
	}
	
    for(uint8_t i=0;i<MAX_UID_LEN;i++)
        uid[i] = i2c_read(i < MAX_UID_LEN-1); //Acknowledges everything but last byte
    i2c_stop(); return 1; //it worked
}

//Display stuff
static void show_admin_message(uint8_t on) //shows ADMIN ENABLED/DISABLED
{
    lcd_clear(); lcd_gotoxy(4,0); lcd_puts("ADMIN");
    lcd_gotoxy(1,1); lcd_puts(on?" MODE ENABLED":"MODE DISABLED");
    _delay_ms(2500);
}
static void display_song(int idx)
{
    lcd_clear(); // "now showing" func
    const char *t = titles [idx]; //looks up title/artist by the index
    const char *a = artists[idx];

    lcd_gotoxy(0,0); lcd_puts(t); //first line (title)
    lcd_gotoxy(0,1); lcd_puts(a); //seconds line (artist
    lcd_gotoxy(11,1); //right side shows credit info
    if(credits == 255) lcd_puts("C:I"); //I = infinite
    else{
        char b[6]; snprintf(b,sizeof(b),"C:%u",credits); lcd_puts(b);  //converts number to text
    }
    if(idx == selected_song){ lcd_gotoxy(15,1); lcd_putc(0); } //marks currently played track
		//adds custom music note thing
}

//MAIN
int main(void)
{
	//disable watchdog timer after reset
	MCUSR = 0;
	wdt_disable();

	// Initializes: LCD, I2C, Rotary Encoder, Buttons, Timer, MP3 player
	lcd_init();
	lcd_create_char(0,music_icon);
	i2c_init();
	encoder_init();
	button_init();
	timer_init();
	mp3Init(38400);

	// configure MP3 BUSY pin (PB2) as input
	DDRB  &= ~(1<<PB2);
	PORTB |=  (1<<PB2);

	sei();                           // enable global interrupts
	srand(12);                       // Set seed for shuffle mode

	// Display the first song on startup
	display_song(song_index);
	update_display = 0;

	// Tracks when RPG was last moved
	uint32_t last_rpg_time = 0;

	// Infinite loop
	while(1)
	{
		//RFID scan handler
		char uid[MAX_UID_LEN];

		// Check for new RFID scan
		if(read_rfid_uid(uid))
		{
			// Admin card detected
			if(!memcmp(uid,admin_uid,MAX_UID_LEN))
			{
				// if not in admin mode it will enter it
				if(!admin_mode){
					prev_credits = credits;	// Store current credits
					credits = 255; 		// Grant 255 which is nearly infinite credits for pratical use
					admin_mode = 1;		// Set admin mode flag to 1 to show it is currently on
					show_admin_message(1);	// Display message that admin is on
				}
				// If it is in admin mode it will exit it
				else{
					credits = prev_credits; // Restore previous credits
					admin_mode = 0;	    // Set admin mode flag to 0
					show_admin_message(0);  // Show admin off message
				}
			}
			// If the user card is used while not in admin mode it will at a credit
			else if(!memcmp(uid,user_uid,MAX_UID_LEN) && !admin_mode){
				if(credits < 254) credits++;	// Increases user credit if under the limit of 255
			}
			update_display = 1; // Flag to show that the LCD needs to update
			_delay_ms(1000);	// Delay to be able to show feedback
		}

		//Admin button (PD5) lgoic
		
		//Checks if admin button was pressed and if it was short or long press
		uint8_t ad_evt = btn_admin_event();

		// if the event is triggered and admin mode is active
		if(ad_evt && admin_mode)
		{
			if(ad_evt == 1){                    // short press => stop
				mp3Stop();
				}else{                              // long press => shuffle toggle
				shuffle_mode ^= 1;		// Toggle the shuffle mode
				lcd_clear(); 			// Clear the LCD disply
				lcd_gotoxy(3,0);		// Move the cursor to the correct position
				lcd_puts(shuffle_mode ? "Shuffle ON" : "Shuffle OFF");  // Display shuffle on or shuffle off
				_delay_ms(1000);		// Wait for 1 second to show the status

				// If we just turned shuffle ON and no track is playing, start one
				if(shuffle_mode && !mp3IsBusy()){
					shuffle_play_next();	// Start playing a randomly selected song
				}
			}
			update_display = 1;			// Flag to show that the LCD needs to update
		}

		//User select button (PD4)
		// Check if the user select button (PD4) is pressed
		if(btn_select_pressed())
		{
			// If there are credits available or we are in admin mode
			if(credits > 0 || admin_mode)
			{
				// Deduct one credit if not in admin mode
				if(!admin_mode && credits != 255) {
					credits--;
				}
				selected_song = song_index;	// Store the current song index as the selected song
				mp3PlayTrack(selected_song + 1); // Play the selected song
			}
			else  // If the user has no credits
			{
				no_credit_flag = 1;			// Set the flag to indicate that the user has no credits left
				no_credit_time = last_scroll_time;	// Record the time when the user ran out of credits
			}
			update_display = 1;	// Set flag to update the display with the latest info
			
		}


		//Encoder turn logic
		
		// Check if the rpg has mpved
		if(rpg_moved){
			last_rpg_time = last_scroll_time; 	// Update the time of the last RPG movement
			rpg_moved = 0; 				// Reset the RPG moved flag
		}

		// If a song is selected and the song index is different from the selected song,
		// and enough time (10ms or more) has passed since the last RPG movement
		if(selected_song != -1 && song_index != selected_song &&
		(last_scroll_time - last_rpg_time) >= 10)
		{
			song_index = selected_song;			// Update the song index with the selected song
			last_scroll_time = last_rpg_time = 0;	// Reset the times to 0
			update_display = 1;				// Set the flag to update the display with the new song
		}

		// shuffling pick a new song only after the last one ends
		static uint8_t busy_prev = 1;            // remember last BUSY level
		uint8_t busy_now = mp3IsBusy();		// Get current BUSY status (1 = playing, 0 = idle)

		// If shuffle mode is on, and we detect a falling edge on BUSY (song just finished)
		if(shuffle_mode && !busy_now && busy_prev)
		{
			selected_song = rand() % TOTAL_SONGS;	// Randomly pick a new song index
			song_index    = selected_song;		// Update the current song index
			mp3PlayTrack(selected_song + 1);		// Play the selected song
			update_display = 1;                      // Flag display for update
		}
		busy_prev = busy_now;				// Store current BUSY state for next loop


		// SHUFFLE based on X message
		if(shuffle_mode && track_finished)	// If a song ended and shuffle is enabled
		{
			track_finished = 0;		// Clear the track finished flag
			shuffle_play_next();		// Play the next random song
		}

		// Refresh display if update_display is flagged
		if(update_display){
			update_display = 0;		// Clear update flag
			display_song(song_index);	// Show the currently selected song on the LCD
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