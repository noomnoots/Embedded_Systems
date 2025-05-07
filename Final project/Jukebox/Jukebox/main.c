#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/twi.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "jukebox_config.h"
#include "mp3.h"

/* -------------------------------------------------------------- */
/* ---------- LCD wiring & helper macros------------ */
#define LCD_DATA_PORT PORTC
#define LCD_DATA_DDR  DDRC
#define LCD_CTRL_PORT PORTB
#define LCD_CTRL_DDR  DDRB
#define LCD_RS        PB0
#define LCD_E         PB1
#define OVERFLOWS_PER_SECOND 977
/* -------------------------------------------------------------- */

/* ---------- UIDs & song metadata (single defs) ---------- */
const char admin_uid[MAX_UID_LEN] = {0x3A,0x00,0x6C,0x34,0xF9,0x9B};
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
volatile int      song_index       = 0;
volatile int      selected_song    = -1;
volatile uint8_t  update_display   = 1;
volatile uint32_t last_scroll_time = 0;
volatile uint8_t  rpg_moved        = 0;
volatile uint8_t  no_credit_flag   = 0;
volatile uint32_t no_credit_time   = 0;
volatile uint8_t  credits          = 3;
volatile uint8_t  prev_credits     = 3;
volatile uint8_t  admin_mode       = 0;
volatile uint8_t  shuffle_mode     = 0;  
volatile uint32_t pd5_press_time   = 0;   

//Music note
uint8_t music_icon[8] = {
    0b00000,0b00100,0b00110,0b00101,
    0b00101,0b11100,0b11100,0b00000
};

//LCD Driver stuff-----------------------------------------------------
static void lcd_nibble(uint8_t n)
{
    LCD_DATA_PORT = (LCD_DATA_PORT & 0xF0) | (n & 0x0F);
    LCD_CTRL_PORT |=  (1 << LCD_E);
    _delay_us(1);
    LCD_CTRL_PORT &= ~(1 << LCD_E);
    _delay_us(1);
}
static void lcd_command(uint8_t c)
{
    LCD_CTRL_PORT &= ~(1 << LCD_RS);
    lcd_nibble(c >> 4); lcd_nibble(c & 0x0F);
    _delay_us(40);
}
static void lcd_data(uint8_t d)
{
    LCD_CTRL_PORT |= (1 << LCD_RS);
    lcd_nibble(d >> 4); lcd_nibble(d & 0x0F);
    _delay_us(40);
}
static void lcd_init(void)
{
    LCD_DATA_DDR |= 0x0F;
    LCD_CTRL_DDR |= (1 << LCD_RS) | (1 << LCD_E);
    _delay_ms(50);
    lcd_nibble(0x03); _delay_ms(5);
    lcd_nibble(0x03); _delay_us(150);
    lcd_nibble(0x03); _delay_us(150);
    lcd_nibble(0x02);
    lcd_command(0x28); lcd_command(0x0C);
    lcd_command(0x06); lcd_command(0x01);
    _delay_ms(2);
}
static void lcd_create_char(uint8_t loc, uint8_t *map)
{
    lcd_command(0x40 | ((loc & 0x07) << 3));
    for(uint8_t i=0;i<8;i++) lcd_data(map[i]);
}
static void lcd_clear(void)                       { lcd_command(0x01); _delay_ms(2); }
static void lcd_gotoxy(uint8_t x,uint8_t y)       { lcd_command(0x80 + (y?0x40:0) + x); }
static void lcd_putc(char c)                      { lcd_data(c); }
static void lcd_puts(const char*s){ while(*s) lcd_putc(*s++); }

//timer 0(1Hz)-----------------------------------------------
static void timer_init(void)
{
    TCCR0A = 0;
    TCCR0B = (1 << CS01) | (1 << CS00);  //prescaler of 64
    TIMSK0 = (1 << TOIE0);
    TCNT0  = 0;
}
ISR(TIMER0_OVF_vect)
{
    static uint16_t cnt = 0;
    if(++cnt >= OVERFLOWS_PER_SECOND){
        last_scroll_time++; cnt = 0;
    }
}

//RPG 
static void encoder_init(void)
{
    DDRD  &= ~((1<<PD2)|(1<<PD3));
    PORTD |=  (1<<PD2)|(1<<PD3);          // pull?ups      
    EICRA |=  (1<<ISC01)|(1<<ISC00);      // INT0 rising    
    EIMSK |=  (1<<INT0);
}
ISR(INT0_vect)
{
    uint8_t A = (PIND >> PD2) & 1;
    uint8_t B = (PIND >> PD3) & 1;
    song_index = (A != B) ? (song_index+1)%TOTAL_SONGS
                          : (song_index?song_index-1:TOTAL_SONGS-1);
    last_scroll_time = 0;
    rpg_moved = 1;
    update_display = 1;
}

//Button Logic
#define BTN_SELECT PD4
#define BTN_ADMIN  PD5

static void button_init(void)
{
    DDRD  &= ~((1<<BTN_SELECT)|(1<<BTN_ADMIN));  // inputs
    PORTD |=  (1<<BTN_SELECT)|(1<<BTN_ADMIN);    // pull?ups
}

static uint8_t btn_select_pressed(void)
{
    static uint8_t last = 1;
    uint8_t cur = PIND & (1<<BTN_SELECT);
    if(!cur && last){ _delay_ms(50); last = 0; return 1; }
    if( cur) last = 1;
    return 0;
}

//returns: 0?no event, 1?short (<2?s), 2?long (?2?s) 
static uint8_t btn_admin_event(void)
{
    static uint8_t  last = 1;
    static uint32_t t0   = 0;

    uint8_t cur = PIND & (1<<BTN_ADMIN);
    if(!cur && last){                       // pressed
        t0   = last_scroll_time;
        last = 0;
    }else if(cur && !last){                 // released
        uint32_t dt = last_scroll_time - t0;
        last = 1;
        return (dt >= 2) ? 2 : 1;
    }
    return 0;
}

//I^2C stuff--------------------------------------------------
static void i2c_init(void)
{
    TWBR = 0x48;            /* ?100?kHz at 16?MHz */
    TWCR = (1 << TWEN);
}
static uint8_t i2c_start(uint8_t addr)
{
    TWCR = (1<<TWSTA)|(1<<TWEN)|(1<<TWINT);
    while(!(TWCR & (1<<TWINT)));
    TWDR = addr;
    TWCR = (1<<TWEN)|(1<<TWINT);
    while(!(TWCR & (1<<TWINT)));
    return (TWSR & 0xF8);
}
static void i2c_stop(void)
{
    TWCR = (1<<TWSTO)|(1<<TWEN)|(1<<TWINT);
    while(TWCR & (1<<TWSTO));
}
static uint8_t i2c_read(uint8_t ack)
{
    TWCR = (1<<TWEN)|(1<<TWINT)|(ack?1<<TWEA:0);
    while(!(TWCR & (1<<TWINT)));
    return TWDR;
}
static uint8_t read_rfid_uid(char *uid)
{
    if(i2c_start(RFID_ADDR<<1) != 0x18){ i2c_stop(); return 0; }
    i2c_stop();
    if(i2c_start((RFID_ADDR<<1)|1) != 0x40){ i2c_stop(); return 0; }
    for(uint8_t i=0;i<MAX_UID_LEN;i++)
        uid[i] = i2c_read(i < MAX_UID_LEN-1);
    i2c_stop(); return 1;
}

//Display stuff
static void show_admin_message(uint8_t on)
{
    lcd_clear(); lcd_gotoxy(4,0); lcd_puts("ADMIN");
    lcd_gotoxy(1,1); lcd_puts(on?" MODE ENABLED":"MODE DISABLED");
    _delay_ms(2500);
}
static void display_song(int idx)
{
    lcd_clear();
    const char *t = titles [idx];
    const char *a = artists[idx];

    lcd_gotoxy(0,0); lcd_puts(t);
    lcd_gotoxy(0,1); lcd_puts(a);
    lcd_gotoxy(11,1);
    if(credits == 255) lcd_puts("C:I");
    else{
        char b[6]; snprintf(b,sizeof(b),"C:%u",credits); lcd_puts(b);
    }
    if(idx == selected_song){ lcd_gotoxy(15,1); lcd_putc(0); }
}

//MAIN oooohhwweeeee-----------------------------------------------------------
int main(void)
{
    //disable watchdog after reset
    MCUSR = 0; wdt_disable();

    // --- init peripherals 
    lcd_init(); lcd_create_char(0,music_icon);
    i2c_init(); encoder_init(); button_init(); timer_init();
    mp3Init(38400);

    // configure BUSY pin 
    DDRB  &= ~(1<<PB2);
    PORTB |=  (1<<PB2);

    sei();                           // enable global interrupts
    srand(13);                       // simple seed

    display_song(song_index); update_display = 0;
    uint32_t last_rpg_time = 0;

    while(1)
    {
        //RFID scan handler
        char uid[MAX_UID_LEN];
        if(read_rfid_uid(uid))
        {
            if(!memcmp(uid,admin_uid,MAX_UID_LEN))
            {
                if(!admin_mode){
                    prev_credits = credits; credits = 255; admin_mode = 1;
                    show_admin_message(1);
                }else{
                    credits = prev_credits; admin_mode = 0;
                    show_admin_message(0);
                }
            }
            else if(!memcmp(uid,user_uid,MAX_UID_LEN) && !admin_mode){
                if(credits < 254) credits++;
            }
            update_display = 1; _delay_ms(1000);
        }

        //Admin button (PD5) lgoic
        uint8_t ad_evt = btn_admin_event();
        if(ad_evt && admin_mode)
        {
            if(ad_evt == 1){                   // short press => stop
                mp3Stop();
            }else{                             // long press => shuffle toggle
                shuffle_mode ^= 1;
                lcd_clear(); lcd_gotoxy(3,0);
                lcd_puts(shuffle_mode?"Shuffle ON":"Shuffle OFF");
                _delay_ms(1500);
            }
            update_display = 1;
        }

        //User select button (PD4)
        if(btn_select_pressed() && !mp3IsBusy())
        {
            if(credits > 0 || admin_mode)
            {
                if(!admin_mode && credits != 255) credits--;
                selected_song   = song_index;
                mp3PlayTrack(selected_song + 1);
            }
            else{
                no_credit_flag = 1; no_credit_time = last_scroll_time;
            }
            update_display = 1;
        }

        //Encoder turn logic
        if(rpg_moved){ last_rpg_time = last_scroll_time; rpg_moved = 0; }

        if(selected_song != -1 && song_index != selected_song &&
           (last_scroll_time - last_rpg_time) >= 10)
        {
            song_index = selected_song;
            last_scroll_time = last_rpg_time = 0;
            update_display = 1;
        }

        //shuffle and nice stuff
        if(shuffle_mode && !mp3IsBusy())
        {
            static uint32_t last_auto = 0;
            if(last_scroll_time - last_auto >= 1){
                last_auto = last_scroll_time;
                selected_song = rand() % TOTAL_SONGS;
                song_index    = selected_song;
                mp3PlayTrack(selected_song + 1);
                update_display = 1;
            }
        }

        //displaying refresh
        if(update_display){ update_display = 0; display_song(song_index); }
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
  //| 15      | LED+ | +5V			     | Backlight                   |
  //| 16      | LED- | GND               | Backlight ground            |