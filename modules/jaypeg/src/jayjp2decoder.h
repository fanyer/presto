/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYJP2DECODER_H
#define JAYJP2DECODER_H

#include "modules/jaypeg/jaypeg.h"
#ifdef JAYPEG_JP2_SUPPORT

#include "modules/jaypeg/src/jayformatdecoder.h"
#include "modules/jaypeg/src/jayjp2box.h"

#define JAYPEG_MAX_BOXES 10

struct JayJP2Marker
{
	unsigned int type;
	unsigned int length;
};

class JP2ImageSize
{
public:
	JP2ImageSize() : capabilities(0), width(0), height(0), x(0), y(0),
			 tile_width(0), tile_height(0), tile_x(0), tile_y(0),
			 components(0), comp_size(NULL), comp_x(NULL), comp_y(NULL)
	{}
	~JP2ImageSize()
	{
		OP_DELETEA(comp_size);
		OP_DELETEA(comp_x);
		OP_DELETEA(comp_y);
	}
	unsigned short capabilities;
	unsigned int width;
	unsigned int height;
	unsigned int x;
	unsigned int y;
	unsigned int tile_width;
	unsigned int tile_height;
	unsigned int tile_x;
	unsigned int tile_y;

	unsigned short components;
	unsigned short cur_component;
	unsigned char *comp_size;
	unsigned char *comp_x;
	unsigned char *comp_y;
};

class JP2CodingStyleComponent
{
public:
	int component;
	bool entropyPartitions;

	unsigned char decompositionLevels;
	unsigned char cbWidth;
	unsigned char cbHeight;
	unsigned char cbStyle;
	unsigned char transform;

	unsigned char *partitionSize;

	JP2CodingStyleComponent *next;
};

class JP2CodingStyle
{
public:
	bool entropyPartitions;
	bool sopMarkers;
	bool ephMarkers;

	unsigned char decompositionLevels;
	unsigned char progressionOrder;
	unsigned short layers;
	unsigned char cbWidth;
	unsigned char cbHeight;
	unsigned char cbStyle;
	unsigned char transform;
	unsigned char multipleComponentTransform;

	unsigned char *partitionSize;

	JP2CodingStyleComponent *componentCodingStyle;
};

class JP2Quantization
{
public:
	JP2Quantization() : component(-1), quantType(0), next(NULL)
	{}
	int component; // or -1 for the default

	unsigned char quantType;

	union{
		unsigned char reversibleStepSize;
		unsigned short quantStepSize;
	};
	
	JP2Quantization *next; // or first component for default
};

class JayJP2Decoder : public JayFormatDecoder
{
public:
	JayJP2Decoder();
	~JayJP2Decoder();
	int decode(class JayStream *stream);
private:
	int pushBox(class JayStream *stream);
	int pushMarkerBox(class JayStream *stream);
	void popBox();
	JayJP2Box *currentBox;
	JayJP2Box boxes[JAYPEG_MAX_BOXES];
	int curBoxNum;

	int decodeCodestream(class JayStream *stream);

	JP2ImageSize imageSize;

	JP2CodingStyle codingStyle;
	JP2Quantization quantization;

	class JayArithDecoder *entropyDecoder;
};
#endif // JAYPEG_JP2_SUPPORT
#endif

