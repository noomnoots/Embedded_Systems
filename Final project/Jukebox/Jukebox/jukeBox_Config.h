//jukebox_config.h  –  central definitions shared by all modules

#ifndef JBX_CONFIG_H
#define JBX_CONFIG_H

#define F_CPU 16000000UL      //* 16?MHz Arduino?Uno clock

//RIFD--------------------------------------
#define RFID_ADDR   0x13
#define MAX_UID_LEN 6

//Music total # of songs
#define TOTAL_SONGS 10      

// UIDs for cards 
extern const char admin_uid[MAX_UID_LEN];
extern const char user_uid [MAX_UID_LEN];

// Song metadata
extern const char *titles [TOTAL_SONGS];
extern const char *artists[TOTAL_SONGS];

#endif 
