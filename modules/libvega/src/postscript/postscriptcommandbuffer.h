/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef POSTSCRIPT_COMMAND_BUFFER_H
#define POSTSCRIPT_COMMAND_BUFFER_H

#ifdef VEGA_POSTSCRIPT_PRINTING


class OpFileDescriptor;


class PostScriptOutputMetrics
{
public:
	PostScriptOutputMetrics() : width(0), height(0), offset_top(0), offset_left(0), offset_right(0), offset_bottom(0), dpi_x(0), dpi_y(0) {}

	UINT32 width;
	UINT32 height;

	UINT32 offset_top;
	UINT32 offset_left;
	UINT32 offset_right;
	UINT32 offset_bottom;

	UINT16 dpi_x;
	UINT16 dpi_y;
};


/** @brief Helper class to abstract postscript command output
  */
class PostScriptCommandBuffer
{
public:
	PostScriptCommandBuffer();

	/**
	  * @param file A closed, constructed file object - will be opened for writing
	  */
	OP_STATUS Init(OpFileDescriptor& file);

	OP_STATUS addCommand(const char* command);
	OP_STATUS addFormattedCommand(const char* commandfmt, ...);
	OP_STATUS write(const char* data);

	OP_STATUS beginHexCodeBlock();
	OP_STATUS endHexCodeBlock();
	OP_STATUS newHexCodeLine();
	OP_STATUS fitHexCodeTable(UINT32 size);
	UINT32 getMaxPSStringSize();

	OP_STATUS writeHexCode(UINT8* data, UINT32 size, bool check_size=true);
	OP_STATUS writeHexCodeUINT8(UINT8 data);
	OP_STATUS writeHexCodeUINT16(UINT16 data);
	OP_STATUS writeHexCodeUINT32(UINT32 data);

protected:
	OP_STATUS checkHexCodeBounds();

	OpFileDescriptor * file;
	int depth;

	bool hex_code_block_open;
	int hexcode_byte_count;
	int hexcode_line_width;
};


#endif // VEGA_POSTSCRIPT_PRINTINGT

#endif // POSTSCRIPT_COMMAND_BUFFER_H
