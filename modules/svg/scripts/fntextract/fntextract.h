/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FNTEXTRACT_H
#define FNTEXTRACT_H

#include <ft2build.h>
#include FT_FREETYPE_H

#include <list>

/**
 * This section describes the binary fileformat for the fonts included
 * where we have problem drawing fonts using the painter.
 * General:
 *  - All numbers are integers or fixpoint. The fixed-point factor is 65536.
 *  - All numbers are in big-endian format.
 *  - Strings are stored with first an four byte int telling the length n
 *    and after that n bytes representing the string. iso-8859 encoding.
 *
 *  BNF for our fontformat. (Version 0x0001)
 *
 *
 * Startsymbol: FILE
 *
 * FILE := MAGIC_FILE_START FILE_VERSION FONT_INFO GLYPHS
 * FONT_INFO := ASCENT DESCENT UNITS_PER_EM FAMILY_NAME STYLE_NAME GLYPH_COUNT
 * ASCENT := FIX
 * DESCENT := FIX
 * UNITS_PER_EM := INT
 * FAMILY_NAME := STRING
 * STYLE := STRING
 * GLYPH_COUNT := SHORT
 * GLYPHS := MAGIC_BEGIN_GLYPH GLYPH_SIZE GLYPH ADVANCE PATH_COMMANDS END_COMMANDS
 * GLYPH_SIZE := INT
 * GLYPH := SHORT
 * ADVANCE := FIX
 * PATH_COMMANDS := PATH_COMMAND PATH_COMMANDS | PATH_COMMAND
 * PATH_COMMAND := MOVE SHORT SHORT |
 *                 LINE SHORT SHORT |
 *                 QUADRATIC SHORT SHORT SHORT SHORT |
 *                 CUBIC SHORT SHORT SHORT SHORT SHORT SHORT
 * MAGIC_FILE_START := '0x4712'
 * MAGIC_BEGIN_GLYPH := '0x0992'
 * FILE_VERSION := '0x0001'
 * STRING := <string>
 * FIX := <fixedpoint>
 * SHORT := <2 byte integer>
 * INT := <4 byte integer>
 * MOVE := '0x0000'
 * LINE := '0x0001'
 * QUADRATIC := '0x0002'
 * CUBIC := '0x0003'
 * END_COMMANDS := '0x0991'
 *
 * Comments:
 *
 *  - END_COMMANDS needs to be unique among the other path commands.
 *  - ASCENT is the height from the baseline to the top of the font.
 *  - DESCENT is the (negative) height from the baseline to the bottom of
 *    the font.
 *  - GLYPH_SIZE is for fast skipping in the file without having to read
 *    through all the path commands.
 *
 */

/**
 * Draft v2 format:
 *
 * (Only changes are highlighted)
 *
 * ...
 * PATH_COMMANDS := PATH_COMMAND PATH_COMMANDS | PATH_COMMAND
 * PATH_COMMAND := MOVE_COMMAND | LINE_COMMAND | QUAD_COMMAND | CUBIC_COMMAND
 * MOVE_COMMAND := MOVE OPT_COORD OPT_COORD
 * LINE_COMMAND := LINE OPT_COORD OPT_COORD
 * QUAD_COMMAND := QUADRATIC COORD COORD COORD COORD
 * CUBIC_COMMAND := CUBIC COORD COORD COORD COORD COORD COORD
 * OPT_COORD := COORD | <epsilon>
 * COORD := SHORT | BYTE
 * ...
 * MAGIC_BEGIN_GLYPH := '0xFD'
 * FILE_VERSION := '0x0002'
 * ...
 * BYTE := <1 byte integer>
 * MOVE := '0x00' + MOVE/LINE ENCODING FLAGS
 * LINE := '0x01' + MOVE/LINE ENCODING FLAGS
 * QUADRATIC := '0x02' + QUAD ENCODING FLAGS
 * CUBIC := '0x03' + CUBIC ENCODING FLAGS
 * END_COMMANDS := '0xFC'
 *
 * Encoding flags:
 * + General
 *   - Bits[0:1] are used to encode the command
 *   - The bit sequences 0xFC and 0xFD, end and start a path command list
 *   - Coordinates are coded either as:
 *     <> signed 16-bit absolute [ABS]
 *     <> signed 8-bit relative to last computed point [REL]
 *   - MOVE/LINE may omit zero-coordinates entirely [ZERO]
 *   - Each coordinate pair yields a new computed point
 *   - Initial computed point is (0, 0)
 * + MOVE/LINE [1 coordinate pair, (x, y)]
 *   - If bit[2] == 1 then x == 0 and omitted from the stream, else ABS/REL
 *   - If bit[3] == 1 then y == 0 and omitted from the stream, else ABS/REL
 *   - If bit[4] == 1 then x == REL, else x == ABS
 *   - If bit[5] == 1 then y == REL, else y == ABS
 *   - Bits[6:7] are reserved and MUST be zero
 * + QUAD [2 coordinate pairs, (cx, cy) (x, y)]
 *   - If bit[2] == 1 then cx == REL, else cx == ABS
 *   - If bit[3] == 1 then cy == REL, else cy == ABS
 *   - If bit[4] == 1 then x == REL, else x == ABS
 *   - If bit[5] == 1 then y == REL, else y == ABS
 *   - Bits[6:7] are reserved and MUST be zero
 * + CUBIC [3 coordinate pairs, (c1x, c1y) (c2x, c2y) (x, y)]
 *   - If bit[2] == 1 then c1x == REL, else c1x == ABS
 *   - If bit[3] == 1 then c1y == REL, else c1y == ABS
 *   - If bit[4] == 1 then c2x == REL, else c2x == ABS
 *   - If bit[5] == 1 then c2y == REL, else c2y == ABS
 *   - If bit[6] == 1 then x == REL, else x == ABS
 *   - If bit[7] == 1 then y == REL, else y == ABS
 *
 */

class ExtractedChar;

class ExtractedFont
{
public:
	ExtractedFont();
	~ExtractedFont();

	int AddExtractedChar(ExtractedChar* extracted_char);
	int StreamData(FILE* out_file);
	int Verify(FILE* out_file);

	float m_ascent;
	float m_descent;
	int m_glyphs;
	int m_unit_per_em;
	char* m_family_name;
	char* m_style_name;

	unsigned short monospaced;
	unsigned short italic;
	unsigned short bold;

private:
	std::list<ExtractedChar*> m_chars;
};

#define MAGIC_FILE_START            0x4712
#define MAGIC_BEGIN_GLYPH           0x0992
#define FILE_VERSION                0x0001
#define CMD_TYPE_MOVE_TO            0x0
#define CMD_TYPE_LINE_TO            0x1
#define CMD_TYPE_QUADRATIC_CURVE_TO 0x2
#define CMD_TYPE_CUBIC_CURVE_TO     0x3
#define END_COMMANDS                0x0991

#define CMD_NUMBER_TYPE unsigned short

struct ExtractedCommand
{
	ExtractedCommand() : type(0), x1(0), y1(0), x2(0), y2(0), x(0), y(0) {}
	int type;
	CMD_NUMBER_TYPE x1;
	CMD_NUMBER_TYPE y1;
	CMD_NUMBER_TYPE x2;
	CMD_NUMBER_TYPE y2;
	CMD_NUMBER_TYPE x;
	CMD_NUMBER_TYPE y;
};

struct ExtractedChar
{
	std::list<ExtractedCommand*> m_commands;
	unsigned short uc;
	float advance;
	int final;

	ExtractedChar() : final(0) {}
	~ExtractedChar();

	/**
	 * Generating interface
	 */
	void MoveTo(CMD_NUMBER_TYPE x, CMD_NUMBER_TYPE y);
	void LineTo(CMD_NUMBER_TYPE x, CMD_NUMBER_TYPE y);
	void QuadraticCurveTo(CMD_NUMBER_TYPE x1, CMD_NUMBER_TYPE y1, CMD_NUMBER_TYPE x, CMD_NUMBER_TYPE y);
	void CubicCurveTo(CMD_NUMBER_TYPE x1, CMD_NUMBER_TYPE y1, CMD_NUMBER_TYPE x2, CMD_NUMBER_TYPE y2, CMD_NUMBER_TYPE x, CMD_NUMBER_TYPE y);
	void Done() { final = 1; }

	/**
	 * When complete (final = 1)
	 */
	int StreamData(FILE* out_file);
	int CalculateDataSize();
	int Verify(FILE* out_file);
};

class FontExtract
{
	public:
	FontExtract() : m_init(0) {}
	~FontExtract() {}

	/**
	 * @param The font to extract
	 * @return Zero if successfull. -1 otherwise
	 */
	int Init(FT_Library library, const char* font_name, ExtractedFont* extracted_font);
	int Extract(unsigned short the_char);
	int ExtractAll();

	private:
	const char* m_font_name;
	FT_Library m_ft_library;
	FT_Face m_ft_face;
	ExtractedFont* m_extracted_font;
	int m_init;
};

#endif // FNTEXTRACT_H
