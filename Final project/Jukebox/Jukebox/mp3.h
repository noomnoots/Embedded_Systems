#ifndef MP3_H
#define MP3_H
#include <avr/io.h>
#include <stdint.h>

void mp3Init(uint32_t baud);
void mp3PlayTrack(uint16_t track);
void mp3Next(void);
uint8_t mp3IsBusy(void);

#endif
