/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYPEGJFIFMARKERS_H
#define JAYPEGJFIFMARKERS_H

// Start of frame, non-differential, huffman coding
#define JAYPEG_JFIF_MARKER_SOF0	0xc0
#define JAYPEG_JFIF_MARKER_SOF1	0xc1
#define JAYPEG_JFIF_MARKER_SOF2	0xc2
#define JAYPEG_JFIF_MARKER_SOF3	0xc3
// Start of frame, differential, huffman coding
#define JAYPEG_JFIF_MARKER_SOF5	0xc5
#define JAYPEG_JFIF_MARKER_SOF6	0xc6
#define JAYPEG_JFIF_MARKER_SOF7	0xc7
// Start of frame, non-differential, arithmetic coding
#define JAYPEG_JFIF_MARKER_JPG	0xc8
#define JAYPEG_JFIF_MARKER_SOF9	0xc9
#define JAYPEG_JFIF_MARKER_SOFa	0xca
#define JAYPEG_JFIF_MARKER_SOFb	0xcb
// Start of frame, differential, arithmetic coding
#define JAYPEG_JFIF_MARKER_SOFd	0xcd
#define JAYPEG_JFIF_MARKER_SOFe	0xce
#define JAYPEG_JFIF_MARKER_SOFf	0xcf

// Huffman table spec
#define JAYPEG_JFIF_MARKER_DHT	0xc4

// Arithmetic coding spec
#define JAYPEG_JFIF_MARKER_DAC	0xcc

// Restart markers
#define JAYPEG_JFIF_MARKER_RST0	0xd0
#define JAYPEG_JFIF_MARKER_RST1	0xd1
#define JAYPEG_JFIF_MARKER_RST2	0xd2
#define JAYPEG_JFIF_MARKER_RST3	0xd3
#define JAYPEG_JFIF_MARKER_RST4	0xd4
#define JAYPEG_JFIF_MARKER_RST5	0xd5
#define JAYPEG_JFIF_MARKER_RST6	0xd6
#define JAYPEG_JFIF_MARKER_RST7	0xd7

// Other markers
#define JAYPEG_JFIF_MARKER_SOI	0xd8
#define JAYPEG_JFIF_MARKER_EOI	0xd9
#define JAYPEG_JFIF_MARKER_SOS	0xda
#define JAYPEG_JFIF_MARKER_DQT	0xdb
#define JAYPEG_JFIF_MARKER_DNL	0xdc
#define JAYPEG_JFIF_MARKER_DRI	0xdd
#define JAYPEG_JFIF_MARKER_DHP	0xde
#define JAYPEG_JFIF_MARKER_EXP	0xdf

// Application specific
#define JAYPEG_JFIF_MARKER_APP0	0xe0
#define JAYPEG_JFIF_MARKER_APP1	0xe1
#define JAYPEG_JFIF_MARKER_APP2	0xe2
#define JAYPEG_JFIF_MARKER_APP3	0xe3
#define JAYPEG_JFIF_MARKER_APP4	0xe4
#define JAYPEG_JFIF_MARKER_APP5	0xe5
#define JAYPEG_JFIF_MARKER_APP6	0xe6
#define JAYPEG_JFIF_MARKER_APP7	0xe7
#define JAYPEG_JFIF_MARKER_APP8	0xe8
#define JAYPEG_JFIF_MARKER_APP9	0xe9
#define JAYPEG_JFIF_MARKER_APPa	0xea
#define JAYPEG_JFIF_MARKER_APPb	0xeb
#define JAYPEG_JFIF_MARKER_APPc	0xec
#define JAYPEG_JFIF_MARKER_APPd	0xed
#define JAYPEG_JFIF_MARKER_APPe	0xee
#define JAYPEG_JFIF_MARKER_APPf	0xef

// Jpeg extensions
#define JAYPEG_JFIF_MARKER_JPG0	0xf0
#define JAYPEG_JFIF_MARKER_JPG1	0xf1
#define JAYPEG_JFIF_MARKER_JPG2	0xf2
#define JAYPEG_JFIF_MARKER_JPG3	0xf3
#define JAYPEG_JFIF_MARKER_JPG4	0xf4
#define JAYPEG_JFIF_MARKER_JPG5	0xf5
#define JAYPEG_JFIF_MARKER_JPG6	0xf6
#define JAYPEG_JFIF_MARKER_JPG7	0xf7
#define JAYPEG_JFIF_MARKER_JPG8	0xf8
#define JAYPEG_JFIF_MARKER_JPG9	0xf9
#define JAYPEG_JFIF_MARKER_JPGa	0xfa
#define JAYPEG_JFIF_MARKER_JPGb	0xfb
#define JAYPEG_JFIF_MARKER_JPGc	0xfc
#define JAYPEG_JFIF_MARKER_JPGd	0xfd

#define JAYPEG_JFIF_MARKER_COM	0xfe

// Reserved
#define JAYPEG_JFIF_MARKER_TEM	0x01
// Also 0x02 - 0xbf

// EXIF markers
#define JAYPEG_EXIF_EXIF_IFD	0x8769
#define JAYPEG_EXIF_INTEROP_IFD	0xA005
#define JAYPEG_EXIF_GPS_IFD		0x8825

#endif
