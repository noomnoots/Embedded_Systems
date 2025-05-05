#ifndef MP3_H
#define MP3_H
#define F_CPU 16000000UL

#include <stdint.h>

void     mp3Init(uint32_t baud);
void     mp3PlayTrack(uint16_t track);
void     mp3Next(void);
void     mp3Toggle(void);
uint8_t  mp3IsBusy(void);

#endif /* MP3_H */
