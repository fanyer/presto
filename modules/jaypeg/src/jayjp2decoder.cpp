/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/jaypeg/jaypeg.h"

#if defined(_JPG_SUPPORT_) && defined(USE_JAYPEG) && defined(JAYPEG_JP2_SUPPORT)

#include "modules/jaypeg/src/jayjp2decoder.h"

#include "modules/jaypeg/src/jaystream.h"
#include "modules/jaypeg/src/jayjp2box.h"

#include "modules/jaypeg/src/jayjp2markers.h"

#include "modules/jaypeg/src/jayentropydecoder.h"
#include "modules/jaypeg/src/jayarithdecoder.h"


JayJP2Decoder::JayJP2Decoder() : currentBox(NULL), curBoxNum(0)
{
	// FIXME:
	entropyDecoder = OP_NEW(JayArithDecoder, ());
}

JayJP2Decoder::~JayJP2Decoder()
{
	OP_DELETE(entropyDecoder);
}

int JayJP2Decoder::pushBox(JayStream *stream)
{
	if (curBoxNum >= JAYPEG_MAX_BOXES)
		return JAYPEG_ERR;
	int result = boxes[curBoxNum].init(currentBox, stream);
	if (result != JAYPEG_OK)
		return result;
	currentBox = &boxes[curBoxNum];
	++curBoxNum;
	return JAYPEG_OK;
}

int JayJP2Decoder::pushMarkerBox(JayStream *stream)
{
	if (curBoxNum >= JAYPEG_MAX_BOXES)
		return JAYPEG_ERR;
	int result = boxes[curBoxNum].initMarker(currentBox, stream);
	if (result != JAYPEG_OK)
		return result;
	currentBox = &boxes[curBoxNum];
	++curBoxNum;
	return JAYPEG_OK;
}

void JayJP2Decoder::popBox()
{
	if (!currentBox)
		return;
	currentBox = currentBox->getParent();
	--curBoxNum;
}

int JayJP2Decoder::decode(JayStream *stream)
{
	int decoderesult = JAYPEG_OK;
	while (decoderesult == JAYPEG_OK)
	{
		while (!currentBox || currentBox->isEmpty())
		{
			// if we have an empty box, delete it
			if (currentBox)
			{
				popBox();
			}
			// if we have no box, create a new box
			if (!currentBox)
			{
				decoderesult = pushBox(stream);
				if (decoderesult != JAYPEG_OK)
				{
					return decoderesult;
				}
			}
		}
		switch (currentBox->getType())
		{
		case JP2_BOX_HEADER:
		case JP2_BOX_HEADER_RES:
		{
			// these only contains sub-boxes
			decoderesult = pushBox(stream);
			if (decoderesult != JAYPEG_OK)
			{
				return decoderesult;
			}
			break;
		}
		case JP2_BOX_CODESTREAM:
			decoderesult = decodeCodestream(stream);
			break;
		case JP2_BOX_HEADER_IMAGE:
			if (stream->getLength() < 14)
				decoderesult = JAYPEG_NOT_ENOUGH_DATA;
			else if (!currentBox->lengthLeft(14))
				currentBox->makeInvalid();
			else
			{
				//printf("Image header:\n");
				const unsigned char *buffer = stream->getBuffer(); 
				//printf("  w %d\n", (buffer[0]<<24) |(buffer[1]<<16)|(buffer[2]<<8) |buffer[3]);
				//printf("  h %d\n", (buffer[4]<<24)|(buffer[5]<<16)|(buffer[6]<<8) |buffer[7]);
				//printf("  nc %d\n", (buffer[8]<<8) |buffer[9]);
				//printf("  bpc %d\n", buffer[10]);
				//printf("  c %d\n", buffer[11]);
				//printf("  unk %d\n", buffer[12]);
				//printf("  ipr %d\n", buffer[13]);
				stream->advanceBuffer(currentBox->reduceLength(14));
			}
			break;
		case JP2_BOX_HEADER_COL_SPEC:
			if (stream->getLength() < 7)
				decoderesult = JAYPEG_NOT_ENOUGH_DATA;
			else if (!currentBox->lengthLeft(7))
				currentBox->makeInvalid();
			else
			{
				//printf("Colorspec:\n");
				unsigned int space;
				const unsigned char *buffer = stream->getBuffer();
				space = (buffer[3]<<24)|(buffer[4]<<16)|(buffer[5]<<8)|buffer[6];
				if (buffer[0] == 1){
					//printf("  Enumarated: %d\n", space);
				} else if (buffer[0] == 2){
					//printf("  Profile: %d\n", space);
				} else {
					//printf("  Unknown color space\n");
				}
				stream->advanceBuffer(currentBox->reduceLength(7));
			}
			break;

		case JP2_BOX_HEADER_BPC:
		case JP2_BOX_HEADER_PAL:
		case JP2_BOX_HEADER_COMP_DEF:
		case JP2_BOX_HEADER_CAP_RES:
		case JP2_BOX_HEADER_DISP_RES:
		case JP2_BOX_PROFILE:
		default:
			// Try to reduce the box by the length left. 
			// It will return the amount it actually reduced 
			// the box and the stream is advanced by that amount
			stream->advanceBuffer(currentBox->reduceLength(stream->getLength()));
			if (!currentBox->isEmpty())
				decoderesult = JAYPEG_NOT_ENOUGH_DATA;
			break;
		}
	}
	return decoderesult;
}

int JayJP2Decoder::decodeCodestream(JayStream *stream)
{
	int decoderesult;
	const unsigned char *buffer;
	if (stream->getLength() == 0)
		return JAYPEG_NOT_ENOUGH_DATA;
	switch (currentBox->getMarker())
	{
	// No marker at all or SOT means we should have sub-markers
	case 0:
	case JAYPEG_JP2_MARKER_SOT:
		// these only contains sub-boxes
		decoderesult = pushMarkerBox(stream);
		if (decoderesult != JAYPEG_OK)
		{
			return decoderesult;
		}
		switch(currentBox->getMarker())
		{
		case JAYPEG_JP2_MARKER_SOC:
			//printf("Start of codestream\n");
			break;
		case JAYPEG_JP2_MARKER_EOC:
			//printf("End of codestream\n");
			break;
		case JAYPEG_JP2_MARKER_SOD:
		{
			//printf("Start of tiledata\n");
			int result = entropyDecoder->init(stream);
			if (result != JAYPEG_OK)
			{
				popBox();
				stream->rewindBuffer(4);
				return result;
			}
			break;
		}
		case JAYPEG_JP2_MARKER_SOT:
			if (!currentBox->lengthLeft(8))
			{
				return JAYPEG_ERR;
			}
			if (stream->getLength() < 8)
			{
				popBox();
				stream->rewindBuffer(4);
				return JAYPEG_NOT_ENOUGH_DATA;
			}
			//printf("Start of tile\n");
			// FIXME: setup the tile header
			//printf("num %u\n", (stream->getBuffer()[0]<<8) | stream->getBuffer()[1]);
			currentBox->setLength(((stream->getBuffer()[2]<<24) |
				(stream->getBuffer()[3]<<16) | (stream->getBuffer()[4]<<8)|
				stream->getBuffer()[5])-2);
			//printf("Instance %u / %u\n", stream->getBuffer()[6], stream->getBuffer()[7]);
			stream->advanceBuffer(currentBox->reduceLength(8));
			break;
		case JAYPEG_JP2_MARKER_SIZ:
			if (stream->getLength() < 36)
			{
				popBox();
				stream->rewindBuffer(4);
				return JAYPEG_NOT_ENOUGH_DATA;
			}
			if (!currentBox->lengthLeft(36))
			{
				return JAYPEG_ERR;
			}
			//printf("Image size\n");
			// Read the parmeters we need
			buffer = stream->getBuffer();
			imageSize.capabilities = (buffer[0]<<8)|buffer[1];
			imageSize.width = (buffer[2]<<24)|(buffer[3]<<16)|(buffer[4]<<8)|buffer[5];
			imageSize.height = (buffer[6]<<24)|(buffer[7]<<16)|(buffer[8]<<8)|buffer[9];
			imageSize.x = (buffer[10]<<24)|(buffer[11]<<16)|(buffer[12]<<8)|buffer[13];
			imageSize.y = (buffer[14]<<24)|(buffer[15]<<16)|(buffer[16]<<8)|buffer[17];
			imageSize.tile_width = (buffer[18]<<24)|(buffer[19]<<16)|(buffer[20]<<8)|buffer[21];
			imageSize.tile_height = (buffer[22]<<24)|(buffer[23]<<16)|(buffer[24]<<8)|buffer[25];
			imageSize.tile_x = (buffer[26]<<24)|(buffer[27]<<16)|(buffer[28]<<8)|buffer[29];
			imageSize.tile_y = (buffer[30]<<24)|(buffer[31]<<16)|(buffer[32]<<8)|buffer[33];
			imageSize.components = (buffer[34]<<8)|buffer[35];
			imageSize.cur_component = 0;
			imageSize.comp_size = OP_NEWA(unsigned char, imageSize.components);
			imageSize.comp_x = OP_NEWA(unsigned char, imageSize.components);
			imageSize.comp_y = OP_NEWA(unsigned char, imageSize.components);

			stream->advanceBuffer(currentBox->reduceLength(36));
			if (!imageSize.comp_size || !imageSize.comp_x || !imageSize.comp_y)
				return JAYPEG_ERR_NO_MEMORY;
			//printf("  w: %u\n", imageSize.width);
			//printf("  h: %u\n", imageSize.height);
			//printf("  x: %u\n", imageSize.x);
			//printf("  y: %u\n", imageSize.y);
			//printf("  tile w: %u\n", imageSize.tile_width);
			//printf("  tile h: %u\n", imageSize.tile_height);
			//printf("  tile x: %u\n", imageSize.tile_x);
			//printf("  tile y: %u\n", imageSize.tile_y);
			//printf("  c: %u\n", imageSize.components);
			break;
		case JAYPEG_JP2_MARKER_QCD:
			//printf("QCD\n");
			if (!currentBox->lengthLeft(2))
			{
				return JAYPEG_ERR;
			}
			if (stream->getLength() < 2)
			{
				popBox();
				stream->rewindBuffer(4);
				return JAYPEG_NOT_ENOUGH_DATA;
			}
			//printf("  Quant style: %x\n", stream->getBuffer()[0]&0x1f);
			//printf("  Quant guard bits: %u\n", stream->getBuffer()[0]>>5);
			if ((stream->getBuffer()[0]&0x1f)!=0)
			{
				if (!currentBox->lengthLeft(3))
				{
					return JAYPEG_ERR;
				}
				if (stream->getLength() < 3)
				{
					popBox();
					stream->rewindBuffer(4);
					return JAYPEG_NOT_ENOUGH_DATA;
				}
				//printf("  quant values: %u\n", (stream->getBuffer()[1]<<8)|stream->getBuffer()[2]);
				stream->advanceBuffer(currentBox->reduceLength(3));
			}
			else
			{
				//printf("  rev step size: %u\n", stream->getBuffer()[1]&0x1f);
				stream->advanceBuffer(currentBox->reduceLength(2));
			}
			break;
		case JAYPEG_JP2_MARKER_QCC:
			//printf("FIXME: QCC\n");
			break;
		case JAYPEG_JP2_MARKER_COD:
			if (!currentBox->lengthLeft(10))
			{
				return JAYPEG_ERR;
			}
			if (stream->getLength() < 10)
			{
				popBox();
				stream->rewindBuffer(4);
				return JAYPEG_NOT_ENOUGH_DATA;
			}
			//printf("COD\n");
			buffer = stream->getBuffer();
			// print the results
			//printf("  Flags: %x\n", buffer[0]);
			//printf("  Decomposition levels: %u\n", buffer[1]);
			//printf("  Progression order: %u\n", buffer[2]);
			//printf("  Layers: %u\n", (buffer[3]<<8)|buffer[4]);
			//printf("  cb-width: %u\n", buffer[5]);
			//printf("  cb-height: %u\n", buffer[6]);
			//printf("  cb-style: %u\n", buffer[7]);
			//printf("  transform: %u\n", buffer[8]);
			//printf("  multiple comp transform: %u\n", buffer[9]);
			stream->advanceBuffer(currentBox->reduceLength(10));
			break;
		case JAYPEG_JP2_MARKER_COC:
			//printf("FIXME: COC\n");
			break;
		default:
			break;
		}
		break;
	case JAYPEG_JP2_MARKER_SIZ:
		if (stream->getLength() < 3)
		{
			return JAYPEG_NOT_ENOUGH_DATA;
		}
		imageSize.comp_size[imageSize.cur_component] = stream->getBuffer()[0];
		imageSize.comp_x[imageSize.cur_component] = stream->getBuffer()[1];
		imageSize.comp_y[imageSize.cur_component] = stream->getBuffer()[2];
		stream->advanceBuffer(currentBox->reduceLength(3));
		break;
	case JAYPEG_JP2_MARKER_SOD:
	{
		// data
		unsigned short sample;
		unsigned int startlen = stream->getLength();
		int result = entropyDecoder->readStream(stream, sample);
		currentBox->reduceLength(startlen-stream->getLength());
		if (result != JAYPEG_OK)
			return result;
		//printf("Sample %d (%u bytes)\n", sample, startlen-stream->getLength());
		break;
	}
	case JAYPEG_JP2_MARKER_CME:

	case JAYPEG_JP2_MARKER_SOP:
	case JAYPEG_JP2_MARKER_EPH:
		
	case JAYPEG_JP2_MARKER_TLM:
	case JAYPEG_JP2_MARKER_PLM:
	case JAYPEG_JP2_MARKER_PLT:
	case JAYPEG_JP2_MARKER_PPM:
	case JAYPEG_JP2_MARKER_PPT:
		
	case JAYPEG_JP2_MARKER_RGN:
	case JAYPEG_JP2_MARKER_POD:

	case JAYPEG_JP2_MARKER_COD:
		// If we get here, it is the partition sizes
	case JAYPEG_JP2_MARKER_COC:
	default:
		// skip it since it is an unknown marker
		stream->advanceBuffer(currentBox->reduceLength(stream->getLength()));
		break;
	}
		/*else if (stream->getBuffer()[1] == JAYPEG_JP2_MARKER_SIZ)
		else if (stream->getBuffer()[1] == JAYPEG_JP2_MARKER_SOT)
		{
			printf("Start of tile\n");
			if (stream->getLength() < 12)
			{
				--curMarker;
				return JAYPEG_NOT_ENOUGH_DATA;
			}
			marker[curMarker].length = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];
			// The length of this _must_ be 10
			if (marker[curMarker].length != 10)
			{
				--curMarker;
				return JAYPEG_ERR;
			}
			// FIXME: setup the tile header
			printf("num %u\n", (stream->getBuffer()[4]<<8) | stream->getBuffer()[5]);
			marker[curMarker].length = (stream->getBuffer()[6]<<24) |
				(stream->getBuffer()[7]<<16) | (stream->getBuffer()[8]<<8)|
				stream->getBuffer()[9];
			printf("Length %u\n", marker[curMarker].length);
			printf("Instance %u / %u\n", stream->getBuffer()[10], stream->getBuffer()[11]);
			stream->advanceBuffer(currentBox->reduceLength(10));
		}
		else if ((stream->getBuffer()[1] < 0x30 || 
			stream->getBuffer()[1] > 0x3f))
		{
			// this marker has a length
			if (stream->getLength() < 4)
			{
				--curMarker;
				return JAYPEG_NOT_ENOUGH_DATA;
			}
			marker[curMarker].length = (stream->getBuffer()[2]<<8) | stream->getBuffer()[3];
			marker[curMarker].length -= 2;
			stream->advanceBuffer(currentBox->reduceLength(2));
			printf("Marker %x\n", marker[curMarker].type);
			printf("Length %u\n", marker[curMarker].length);
		}
		stream->advanceBuffer(currentBox->reduceLength(2));
		break;
	case JAYPEG_JP2_MARKER_SIZ:
		if (stream->getLength() < 3)
		{
			return JAYPEG_NOT_ENOUGH_DATA;
		}
		imageSize.comp_size[imageSize.cur_component] = stream->getBuffer()[0];
		imageSize.comp_x[imageSize.cur_component] = stream->getBuffer()[1];
		imageSize.comp_y[imageSize.cur_component] = stream->getBuffer()[2];
		stream->advanceBuffer(currentBox->reduceLength(3));
		marker[curMarker].length -= 3;
		break;
	case JAYPEG_JP2_MARKER_SOD:
		// Special case, this contains data
	default:
		// just skip the tile for now..
		unsigned int redlen = marker[curMarker].length;
		if (redlen > stream->getLength())
			redlen = stream->getLength();
		redlen = currentBox->reduceLength(redlen);
		stream->advanceBuffer(redlen);
		// Reduce the length of all parent markers aswell
		for (int i = 0; i <= curMarker; ++i)
			marker[i].length -= redlen;
		break;
	}*/
	return JAYPEG_OK;
}
#endif

