/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#include "core/pch.h"

#ifdef VEGA_POSTSCRIPT_PRINTING

#include "modules/util/opfile/opfile.h"
#include "modules/libvega/src/postscript/postscriptcommandbuffer.h"

#define MAX_PS_STRING_SIZE 64*1024
#define MAX_PS_LINE_WIDTH 64


PostScriptCommandBuffer::PostScriptCommandBuffer()
  : depth(0)
  , hex_code_block_open(false)
  , hexcode_byte_count(0)
  , hexcode_line_width(0)
{
}

OP_STATUS PostScriptCommandBuffer::Init(OpFileDescriptor& file)
{
	OP_NEW_DBG("PostScriptCommandBuffer::Init()", "psprint");

	this->file = &file;
	return this->file->Open(OPFILE_WRITE);
}

OP_STATUS PostScriptCommandBuffer::addFormattedCommand(const char* commandfmt, ...)
{
	va_list args;

	va_start(args, commandfmt);
	OpString8 command;
	OP_STATUS ret = command.AppendVFormat(commandfmt, args);
	va_end(args);

	RETURN_IF_ERROR(ret);

	return addCommand(command.CStr());
}


OP_STATUS PostScriptCommandBuffer::addCommand(const char* command)
{
	for (int i = 0; i < depth; i++)
		RETURN_IF_ERROR(file->Write("  ", 2));

	RETURN_IF_ERROR(file->Write(command, op_strlen(command)));
	return file->Write("\n", 1);
}


OP_STATUS PostScriptCommandBuffer::write(const char* data)
{
	return file->Write(data, op_strlen(data));
}


UINT32 PostScriptCommandBuffer::getMaxPSStringSize()
{
	return MAX_PS_STRING_SIZE;
}


OP_STATUS PostScriptCommandBuffer::beginHexCodeBlock()
{	
	if (hex_code_block_open)
	{
		if (hexcode_byte_count == 0)
			return OpStatus::OK;
		else
			RETURN_IF_ERROR(endHexCodeBlock());
	}

	RETURN_IF_ERROR(write("<\n"));

	hex_code_block_open = true;
	hexcode_byte_count = 0;
	hexcode_line_width = 0;
	return OpStatus::OK;
}


OP_STATUS PostScriptCommandBuffer::endHexCodeBlock()
{
	if (! hex_code_block_open)
		return OpStatus::OK;
	hex_code_block_open = false;

	return write("00\n>\n");  // PostScript SFNT hexcode blocks should have an extra null padding
}


OP_STATUS PostScriptCommandBuffer::newHexCodeLine()
{
	hexcode_line_width = 0;
	return write("\n");
}


OP_STATUS PostScriptCommandBuffer::checkHexCodeBounds()
{
	// previously written + end block
	if (hexcode_byte_count + 2 >= MAX_PS_STRING_SIZE)  // too large PostScript strings might cause limit error
	{
		RETURN_IF_ERROR(endHexCodeBlock());
		return beginHexCodeBlock();
	}
	
	if (hexcode_line_width == MAX_PS_LINE_WIDTH)
	{
		RETURN_IF_ERROR(newHexCodeLine());
	}
	return OpStatus::OK;
}


OP_STATUS PostScriptCommandBuffer::fitHexCodeTable(UINT32 size)
{
	// previously written  + to write + end block
	if (hexcode_byte_count + size*2 + 2 <= MAX_PS_STRING_SIZE)
		return OpStatus::OK;

	RETURN_IF_ERROR(endHexCodeBlock());
	return beginHexCodeBlock();
}


OP_STATUS PostScriptCommandBuffer::writeHexCode(UINT8* data, UINT32 size, bool check_size)
{
	if (! hex_code_block_open)
		beginHexCodeBlock();

	UINT8* data_ptr = data;
	for(UINT32 count = 0; count < size; count++)
	{
		char hexcode[3]; // ARRAY OK 2010-12-16 wonko
		if (op_snprintf(hexcode, 3, "%.2X", *data_ptr) < 0)
			return OpStatus::ERR;
		RETURN_IF_ERROR(write(hexcode));

		if(check_size)
			hexcode_byte_count += 2;
		hexcode_line_width += 2;
		RETURN_IF_ERROR(checkHexCodeBounds());
		data_ptr++;
	}

	return OpStatus::OK;
}


OP_STATUS PostScriptCommandBuffer::writeHexCodeUINT8(UINT8 data)
{
	if (! hex_code_block_open)
		beginHexCodeBlock();

	char hexcode[3]; // ARRAY OK 2010-12-16 wonko
	if (op_snprintf(hexcode, 3, "%.2X", data) < 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(write(hexcode));
	hexcode_byte_count += 2;
	hexcode_line_width += 2;
	return checkHexCodeBounds();
}


OP_STATUS PostScriptCommandBuffer::writeHexCodeUINT16(UINT16 data)
{
	RETURN_IF_ERROR(writeHexCodeUINT8((data & 0xff00) >> 8));
	return writeHexCodeUINT8(data & 0x00ff);
}


OP_STATUS PostScriptCommandBuffer::writeHexCodeUINT32(UINT32 data)
{
	RETURN_IF_ERROR(writeHexCodeUINT8((data & 0xff000000) >> 24));
	RETURN_IF_ERROR(writeHexCodeUINT8((data & 0x00ff0000) >> 16));
	RETURN_IF_ERROR(writeHexCodeUINT8((data & 0x0000ff00) >> 8));
	return writeHexCodeUINT8(data & 0x000000ff);
}

#endif // VEGA_POSTSCRIPT_PRINTING
