/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYJP2MARKERS_H
#define JAYJP2MARKERS_H

// Marker segments
// Delimiting markers
#define JAYPEG_JP2_MARKER_SOC	0x4f	// Start Of Codestream
#define JAYPEG_JP2_MARKER_EOC	0xd9	// End Of Codestream
#define JAYPEG_JP2_MARKER_SOT	0x90	// Start Of Tile-part
#define JAYPEG_JP2_MARKER_SOD	0x93	// Start Of Data

// Fixed information markers
#define JAYPEG_JP2_MARKER_SIZ	0x51	// Image and tile size

// Functional markers
#define JAYPEG_JP2_MARKER_COD	0x52	// Coding style default
#define JAYPEG_JP2_MARKER_COC	0x53	// Coding style component
#define JAYPEG_JP2_MARKER_RGN	0x5e	// Region of interest
#define JAYPEG_JP2_MARKER_QCD	0x5c	// Quantization default
#define JAYPEG_JP2_MARKER_QCC	0x5d	// Quantization component
#define JAYPEG_JP2_MARKER_POD	0x5f	// Progression order default

// Pointer markers
#define JAYPEG_JP2_MARKER_TLM	0x55	// Tile-part lengths, main header
#define JAYPEG_JP2_MARKER_PLM	0x57	// Packet length, main header
#define JAYPEG_JP2_MARKER_PLT	0x58	// Packet length, tile-part header
#define JAYPEG_JP2_MARKER_PPM	0x60	// Packed packet headers, main header
#define JAYPEG_JP2_MARKER_PPT	0x61	// Packed packet headers, tile-part header

// In bit-stream markers
#define JAYPEG_JP2_MARKER_SOP	0x91	// Start of packet
#define JAYPEG_JP2_MARKER_EPH	0x92	// End of packet header

// Informational markers
#define JAYPEG_JP2_MARKER_CME	0x64	// Comment and extension

#endif

