// /* LCD_RFID_CRED.c  –  shared definitions for the jukebox project
//  * (included by main.c; do NOT compile separately)
//  */
// 
// #include <stdint.h>
// 
// #ifndef LCD_RFID_CRED_C_INCLUDED
// #define LCD_RFID_CRED_C_INCLUDED
// 
// #define F_CPU 16000000UL
// 
// /* ---------- LCD wiring ---------- */
// #define LCD_DATA_PORT PORTC
// #define LCD_DATA_DDR  DDRC
// #define LCD_CTRL_PORT PORTB
// #define LCD_CTRL_DDR  DDRB
// #define LCD_RS        PB0
// #define LCD_E         PB1
// #define OVERFLOWS_PER_SECOND 977
// 
// /* ---------- RFID ---------- */
// #define RFID_ADDR   0x13
// #define MAX_UID_LEN 6
// 
// /* ---------- Music library ---------- */
// #define TOTAL_SONGS 10
// 
// /* ----- constant data (flash?resident) ----- */
// const char admin_uid[MAX_UID_LEN] = {0x3A,0x00,0x6C,0x34,0xF9,0x9B};
// const char user_uid [MAX_UID_LEN] = {0x3A,0x00,0x6C,0x6D,0xBA,0x81};
// 
// const char *titles[TOTAL_SONGS] = {
//     "Go Robot","Migra","Expresso","Sticky",
//     "Judas","Let It Be","Africa",
//     "Sweet Child O' Mine","Thunderstruck","Yesterday"
// };
// const char *artists[TOTAL_SONGS] = {
//     "RHCP","Santana","Sabrina ","TylerTC",
//     "Led Zeppelin","The Beatles","Toto",
//     "Guns N' Roses","AC/DC","The Beatles"
// };
// 
// /* ---------- LCD custom glyph ---------- */
// uint8_t music_icon[8] = {
//     0b00000,
//     0b00100,
//     0b00110,
//     0b00101,
//     0b00101,
//     0b11100,
//     0b11100,
//     0b00000
// };
// 
// /* ---------- shared volatile state ---------- */
// volatile int      song_index       = 0;
// volatile int      selected_song    = -1;
// volatile uint8_t  update_display   = 1;
// volatile uint32_t last_scroll_time = 0;
// volatile uint8_t  rpg_moved        = 0;
// volatile uint8_t  no_credit_flag   = 0;
// volatile uint32_t no_credit_time   = 0;
// volatile uint8_t  credits          = 3;
// volatile uint8_t  prev_credits     = 3;
// volatile uint8_t  admin_mode       = 0;
// 
// #endif /* LCD_RFID_CRED_C_INCLUDED */
