#ifndef MP3_H
#define MP3_H

#include <stdint.h>
#include "jukebox_config.h"

void mp3Init(uint32_t baud);
void mp3PlayTrack(uint8_t track);   // 1…TOTAL_SONGS            
void mp3Next(void);                 // skip forward             
void mp3Toggle(void);               // play/pause toggle        
void mp3Stop(void);                 // explicit stop            
uint8_t mp3IsBusy(void);            // BUSY line (PB2 == 0)     


#endif
