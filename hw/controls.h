#ifndef _CONTROLS_H_
#define _CONTROLS_H_

#include <stdbool.h>
#include <inttypes.h>

void Controls_Init(void);
uint8_t Encoder_Read(void);
uint8_t PTT_Read(void);
uint32_t keypad_read(void);
char get_key(void);
void Power_As_Input(void);
void Normal_Power(void);
int VOL_Read(void);
int VOL_Taper(int vol);

#define KEY_PTT		'T'
#define KEY_TOP		'~'
#define KEY_BOTTOM	'M'
#define KEY_GREEN	'\n'
#define KEY_UP		'\x18'
#define KEY_DOWN	'\x19'
#define KEY_RED		'\x27'

#endif
