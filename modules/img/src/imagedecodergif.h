/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef IMAGEDECODERGIF_H
#define IMAGEDECODERGIF_H

#include "modules/img/image.h"

#define LZW_TABLE_SIZE 4096

#define LZW_NO_PARENT 4097
#define LZW_CLEAR 4098
#define LZW_END 4099

#define GIF_EXTENSION 0x21
#define GIF_IMAGE_DESCRIPTOR 0x2c
#define GIF_TRAILER 0x3b

#define GIF_COMMENT 0xfe
#define GIF_GRAPHICAL_CONTROL 0xf9
#define GIF_APPLICATION 0xff

enum GifState
{
	GIF_STATE_READ_HEADER,
	GIF_STATE_READ_LOGICAL_SCREEN,
	GIF_STATE_READ_GLOBAL_COLORS,
	GIF_STATE_READ_DATA,
	GIF_STATE_EXTENSION,
	GIF_STATE_COMMENT_EXTENSION,
	GIF_STATE_GRAPHICAL_CONTROL_EXTENSION,
	GIF_STATE_APPLICATION_EXTENSION,
	GIF_STATE_NETSCAPE_EXTENSION,
	GIF_STATE_SKIP_EXTENSION,
	GIF_STATE_SKIP_BLOCKS,
	GIF_STATE_IMAGE_DESCRIPTOR,
	GIF_STATE_LOCAL_COLORS,
	GIF_STATE_IMAGE_DATA_START,
	GIF_STATE_IMAGE_DATA,
	GIF_STATE_TRAILER,
	GIF_STATE_FINISHED
};

class LzwListener
{
public:
	virtual ~LzwListener() {}
	virtual void OnCodesDecoded(const UINT8* codes, int nr_of_codes) = 0;
};

enum LzwCodeType
{
	LZW_CODE,
	LZW_CLEAR_CODE,
	LZW_END_CODE,
	LZW_OUT_OF_BOUNDS_CODE
};

class LzwStringTable
{
public:
	LzwStringTable(LzwListener* listener);

	/**
	 * Sets the number of colors and occupied codes,
	 * and clears the table. To be able to reuse
	 * the table for another bit depth.
	 */
	void Reset(int num_cols, int occupied_codes, int transparent_index);

	/**
	 * Clears all codes in the table, and starts over with
	 * only codes for the base characters.
	 */
	void Clear();

	/**
	 * Adds a code to the string table, which will
	 * be represented by the code string belonging
	 * to the prefix code followed by the suffix character.
	 */
	OP_STATUS AddString(short prefix, UINT8 suffix);

	/**
	 * Returns the code type for the specific lzw code.
	 */
	LzwCodeType CodeType(short code) const;

	/**
	 * Outputs all of the character string for a specific lzw code, through the LzwListener interface.
	 */
	void OutputString(short code);

	/**
	 * Returns the first character in the string for a specific lzw code.
	 */
	OP_STATUS GetFirstCharacter(short code, UINT8& result);

	/**
	 * Returns the number of bits needed to represent all currently active codes.
	 */
	int GetCodeSize() const { return code_size; }

	/**
	 * Returns the number of lzw codes currently in string table.
	 */
	int GetNrOfCodes() const
	{
		OP_ASSERT(next_code <= LZW_TABLE_SIZE);
		return next_code;
	}

private:
	/**
	   Replace value of code with the value of its parent.
	 */
	void Parent(short& code) const { code = codes[code].parent; OP_ASSERT(CodeType(code) == LZW_CODE); }
	/**
	   @return TRUE if code has no parent.
	 */
	BOOL IsRoot(const short code) const { return codes[code].parent == LZW_NO_PARENT; }

	/**
	 * Returns the length of the string belonging to a specific lzw code.
	 */
	void SetNextCode(short code);
	void IncNextCode();

	struct code {
		short parent;
		UINT8 data;
	} codes[LZW_TABLE_SIZE];

	UINT8 code_buf[LZW_TABLE_SIZE];

	LzwListener* listener;
	short num_cols;
	short first_code;
	short next_code;
	short code_size;
	short zero_value;
};

class LzwCodeDecoder
{
public:
	static LzwCodeDecoder* Create(LzwListener* listener);
	~LzwCodeDecoder();

	/**
	 * Sets the number of colors and occupied codes,
	 * and clears the table. To be able reuse and
	 * restart for another bit depth.
	 */
	void Reset(int num_cols, int occupied_codes, int transparent_index);

	/**
	 * Decodes a lzw code into a number of characters.
	 */
	OP_STATUS DecodeCode(short code);

	/**
	 * Returns the number of lzw codes currently in string table.
	 */
	int GetCodeSize();

private:
	LzwCodeDecoder();

	LzwStringTable* table;
	short old_code;
	BOOL reached_end;
	BOOL decoding_started;
};

class LzwDecoder
{
public:
	static LzwDecoder* Create(LzwListener* listener);
	~LzwDecoder();

	void Reset(int num_cols, int occupied_codes, int transparent_index = 0); // Given in exponential form.

	OP_STATUS DecodeData(const UINT8* data, int len, BOOL more);

private:
	LzwDecoder();

	LzwCodeDecoder* code_decoder;
	int nr_of_rest_bits;
	UINT32 rest_bits;
};

class ImageDecoderGif : public ImageDecoder, public LzwListener
{
	// We need to add a converter from palette based data to RGB.
	// It should be generic, and not always on.

public:
	static ImageDecoderGif* Create(ImageDecoderListener* listener, BOOL decode_lzw = TRUE);
	~ImageDecoderGif();

	OP_STATUS DecodeData(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes, BOOL load_all = FALSE);

	void SetImageDecoderListener(ImageDecoderListener* imageDecoderListener);

	void OnCodesDecoded(const UINT8* codes, int nr_of_codes);

#ifdef SELFTEST
	void InitTestingCodesDecoded(int width, int height, BOOL interlaced);
#endif // SELFTEST

private:
	ImageDecoderGif();

	OP_STATUS DecodeDataInternal(const BYTE* data, INT32 numBytes, BOOL more, int& resendBytes);
	OP_STATUS SignalMainFrame(BOOL* continue_decoding);
	OP_STATUS SignalNewFrame();
	void InitializeFrame();

	UINT8* global_palette;
	int global_palette_cols;
	UINT8* local_palette;
	int local_palette_cols;
	int global_width;
	int global_height;
	BOOL global_colors;
	BOOL gif89a;
	ImageFrameData frame_data;
	ImageDecoderListener* listener;
	LzwDecoder* lzw_decoder;
	BOOL decode_lzw;
	GifState gif_state;
	BOOL frame_initialized;
	BOOL got_first_frame;
	BOOL got_graphic_extension;
	int interlace_pass;
	int current_x_pos;
	int current_line;
#ifdef SUPPORT_INDEXED_OPBITMAP
	UINT8* line_buf;
#else
	UINT32* line_buf;
#endif //
};

#endif // !IMAGEDECODERGIF_H
