#ifndef __COLORS_H__
#define __COLORS_H__

/*
Calculation of the corresponding uint16_t values are according to the following function:
R,G,B -> ((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)
*/

#define COLOR_BG ((uint16_t)0x20)
#define COLOR_TEXT ((uint16_t)0xff8b)

#endif//__COLORS_H__