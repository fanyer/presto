// This file contains lookup tables used for YCrCb to RGB convertion

#ifndef JAYCOLORLT_H
#define JAYCOLORLT_H

// Note: the array jay_clamp_data is defined in src/jayjfifdecoder.cpp:
extern const unsigned char jay_clamp_data[];
#define g_jay_clamp (jay_clamp_data+384)
#define JAY_CLAMP(x) jay_clamp_data[((x)+384)&0x3ff]

// Note: the array jaypeg_zigzag is defined in src/jayidict.cpp:
extern const unsigned int jaypeg_zigzag[];


#endif // JAYCOLORLT_H

