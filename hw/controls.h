#ifndef _CONTROLS_H_
#define _CONTROLS_H_

#include <stdbool.h>
#include <inttypes.h>

void Controls_Init(void);
uint8_t Encoder_Read(void);
bool PTT_Read(void);

#endif
