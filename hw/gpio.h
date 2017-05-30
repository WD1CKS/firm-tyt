#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdbool.h>
#include "stm32f4xx_gpio.h"

struct gpio_pin_def {
	GPIO_TypeDef	*bank;
	const uint16_t	pin;
};

#define pin_set(p)			GPIO_WriteBit((p)->bank, (p)->pin, 1)
#define pin_reset(p)		GPIO_WriteBit((p)->bank, (p)->pin, 0)
#define pin_read(p)			GPIO_ReadInputDataBit((p)->bank, (p)->pin)
#define pin_write(p, v)		GPIO_WriteBit((p)->bank, (p)->pin, v)

void gpio_input_setup(GPIO_TypeDef* bank, uint16_t pins);
void gpio_af_setup(GPIO_TypeDef* bank, uint16_t pins, uint8_t af,
    GPIOSpeed_TypeDef speed, GPIOOType_TypeDef type,
    GPIOPuPd_TypeDef pupd);
void gpio_output_setup(GPIO_TypeDef* bank, uint16_t pins,
    GPIOSpeed_TypeDef speed, GPIOOType_TypeDef type,
    GPIOPuPd_TypeDef pupd);

#define DECL_PIN(b, n) extern const struct gpio_pin_def _pin_ ## b ## n

#define DECL_BANK(b)	\
    DECL_PIN(b, 0);	\
    DECL_PIN(b, 1);	\
    DECL_PIN(b, 2);	\
    DECL_PIN(b, 3);	\
    DECL_PIN(b, 4);	\
    DECL_PIN(b, 5);	\
    DECL_PIN(b, 6);	\
    DECL_PIN(b, 7);	\
    DECL_PIN(b, 8);	\
    DECL_PIN(b, 9);	\
    DECL_PIN(b, 10);	\
    DECL_PIN(b, 11);	\
    DECL_PIN(b, 12);	\
    DECL_PIN(b, 13);	\
    DECL_PIN(b, 14);	\
    DECL_PIN(b, 15)

DECL_BANK(A);
DECL_BANK(B);
DECL_BANK(C);
DECL_BANK(D);
DECL_BANK(E);
DECL_BANK(F);
DECL_BANK(G);
DECL_BANK(H);
DECL_BANK(I);

#undef DECL_PIN
#undef DECL_BANK

/* Pin defines */
#define pin_a0	(&_pin_A0)
#define pin_a1	(&_pin_A1)
#define pin_a2	(&_pin_A2)
#define pin_a3	(&_pin_A3)
#define pin_a4	(&_pin_A4)
#define pin_a5	(&_pin_A5)
#define pin_a6	(&_pin_A6)
#define pin_a7	(&_pin_A7)
#define pin_a8	(&_pin_A8)
#define pin_a9	(&_pin_A9)
#define pin_a10	(&_pin_A10)
#define pin_a11	(&_pin_A11)
#define pin_a12	(&_pin_A12)
#define pin_a13	(&_pin_A13)
#define pin_a14	(&_pin_A14)
#define pin_a15	(&_pin_A15)

#define pin_b0  (&_pin_B0)
#define pin_b1  (&_pin_B1)
#define pin_b2  (&_pin_B2)
#define pin_b3  (&_pin_B3)
#define pin_b4  (&_pin_B4)
#define pin_b5  (&_pin_B5)
#define pin_b6  (&_pin_B6)
#define pin_b7  (&_pin_B7)
#define pin_b8  (&_pin_B8)
#define pin_b9  (&_pin_B9)
#define pin_b10 (&_pin_B10)
#define pin_b11 (&_pin_B11)
#define pin_b12 (&_pin_B12)
#define pin_b13 (&_pin_B13)
#define pin_b14 (&_pin_B14)
#define pin_b15 (&_pin_B15)

#define pin_c0  (&_pin_C0)
#define pin_c1  (&_pin_C1)
#define pin_c2  (&_pin_C2)
#define pin_c3  (&_pin_C3)
#define pin_c4  (&_pin_C4)
#define pin_c5  (&_pin_C5)
#define pin_c6  (&_pin_C6)
#define pin_c7  (&_pin_C7)
#define pin_c8  (&_pin_C8)
#define pin_c9  (&_pin_C9)
#define pin_c10 (&_pin_C10)
#define pin_c11 (&_pin_C11)
#define pin_c12 (&_pin_C12)
#define pin_c13 (&_pin_C13)
#define pin_c14 (&_pin_C14)
#define pin_c15 (&_pin_C15)

#define pin_d0  (&_pin_D0)
#define pin_d1  (&_pin_D1)
#define pin_d2  (&_pin_D2)
#define pin_d3  (&_pin_D3)
#define pin_d4  (&_pin_D4)
#define pin_d5  (&_pin_D5)
#define pin_d6  (&_pin_D6)
#define pin_d7  (&_pin_D7)
#define pin_d8  (&_pin_D8)
#define pin_d9  (&_pin_D9)
#define pin_d10 (&_pin_D10)
#define pin_d11 (&_pin_D11)
#define pin_d12 (&_pin_D12)
#define pin_d13 (&_pin_D13)
#define pin_d14 (&_pin_D14)
#define pin_d15 (&_pin_D15)

#define pin_e0  (&_pin_E0)
#define pin_e1  (&_pin_E1)
#define pin_e2  (&_pin_E2)
#define pin_e3  (&_pin_E3)
#define pin_e4  (&_pin_E4)
#define pin_e5  (&_pin_E5)
#define pin_e6  (&_pin_E6)
#define pin_e7  (&_pin_E7)
#define pin_e8  (&_pin_E8)
#define pin_e9  (&_pin_E9)
#define pin_e10 (&_pin_E10)
#define pin_e11 (&_pin_E11)
#define pin_e12 (&_pin_E12)
#define pin_e13 (&_pin_E13)
#define pin_e14 (&_pin_E14)
#define pin_e15 (&_pin_E15)

#define pin_f0  (&_pin_F0)
#define pin_f1  (&_pin_F1)
#define pin_f2  (&_pin_F2)
#define pin_f3  (&_pin_F3)
#define pin_f4  (&_pin_F4)
#define pin_f5  (&_pin_F5)
#define pin_f6  (&_pin_F6)
#define pin_f7  (&_pin_F7)
#define pin_f8  (&_pin_F8)
#define pin_f9  (&_pin_F9)
#define pin_f10 (&_pin_F10)
#define pin_f11 (&_pin_F11)
#define pin_f12 (&_pin_F12)
#define pin_f13 (&_pin_F13)
#define pin_f14 (&_pin_F14)
#define pin_f15 (&_pin_F15)

#define pin_g0  (&_pin_G0)
#define pin_g1  (&_pin_G1)
#define pin_g2  (&_pin_G2)
#define pin_g3  (&_pin_G3)
#define pin_g4  (&_pin_G4)
#define pin_g5  (&_pin_G5)
#define pin_g6  (&_pin_G6)
#define pin_g7  (&_pin_G7)
#define pin_g8  (&_pin_G8)
#define pin_g9  (&_pin_G9)
#define pin_g10 (&_pin_G10)
#define pin_g11 (&_pin_G11)
#define pin_g12 (&_pin_G12)
#define pin_g13 (&_pin_G13)
#define pin_g14 (&_pin_G14)
#define pin_g15 (&_pin_G15)

#define pin_h0  (&_pin_H0)
#define pin_h1  (&_pin_H1)
#define pin_h2  (&_pin_H2)
#define pin_h3  (&_pin_H3)
#define pin_h4  (&_pin_H4)
#define pin_h5  (&_pin_H5)
#define pin_h6  (&_pin_H6)
#define pin_h7  (&_pin_H7)
#define pin_h8  (&_pin_H8)
#define pin_h9  (&_pin_H9)
#define pin_h10 (&_pin_H10)
#define pin_h11 (&_pin_H11)
#define pin_h12 (&_pin_H12)
#define pin_h13 (&_pin_H13)
#define pin_h14 (&_pin_H14)
#define pin_h15 (&_pin_H15)

#define pin_i0  (&_pin_I0)
#define pin_i1  (&_pin_I1)
#define pin_i2  (&_pin_I2)
#define pin_i3  (&_pin_I3)
#define pin_i4  (&_pin_I4)
#define pin_i5  (&_pin_I5)
#define pin_i6  (&_pin_I6)
#define pin_i7  (&_pin_I7)
#define pin_i8  (&_pin_I8)
#define pin_i9  (&_pin_I9)
#define pin_i10 (&_pin_I10)
#define pin_i11 (&_pin_I11)
#define pin_i12 (&_pin_I12)
#define pin_i13 (&_pin_I13)
#define pin_i14 (&_pin_I14)
#define pin_i15 (&_pin_I15)


/* Pin aliases */
#define pin_ecn3 pin_b10
#define pin_ecn0 pin_b11
#define pin_lcd_bl pin_c6
#define pin_lcd_d2 pin_d0
#define pin_lcd_d3 pin_d1
#define pin_lcd_rd pin_d4
#define pin_lcd_wr pin_d5
#define pin_lcd_cs pin_d6
#define pin_lcd_rs pin_d12
#define pin_lcd_rst pin_d13
#define pin_lcd_d0 pin_d14
#define pin_lcd_d1 pin_d15
#define pin_green_led pin_e0
#define pin_red_led pin_e1
#define pin_lcd_d4 pin_e7
#define pin_lcd_d5 pin_e8
#define pin_top pin_e9
#define pin_lcd_d6 pin_e9
#define pin_bottom pin_e10
#define pin_lcd_d7 pin_e10
#define pin_ptt pin_e11
#define pin_ecn2 pin_e14
#define pin_ecn1 pin_e15

#endif
