/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "fntextract.h"
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>

#define FONT_EXTRACT_USAGE \
	"Usage: ./fntextract <infile> <outfile>\n"

#define WRITE_SHORT(X, Y, Z)											\
	{																	\
		const unsigned char buffer[] = { ((Y) >> 8) & 0xFF , (Y) & 0xFF  }; \
		X += 2 * fwrite(buffer, 2, 1, (Z)); }							\


#define WRITE_INT(X, Y, Z)												\
	{																	\
		const unsigned char buffer[] = { ((Y) >> 24) & 0xFF,			\
										 ((Y) >> 16) & 0xFF,			\
										 ((Y) >> 8) & 0xFF,				\
										 (Y) & 0xFF };					\
		X += 4 * fwrite(buffer, 4, 1, (Z)); }							\

#define WRITE_FLOAT(X, Y, Z)						\
	{												\
		unsigned int fix = (unsigned int)Y * 65536;		\
		WRITE_INT(X, fix, Z); }							\

#define WRITE_STRING(X, Y, Z)						\
	{												\
		int len = strlen(Y);						\
		WRITE_INT(X, len, Z);						\
		X += len * fwrite(Y, len, 1, (Z));			\
	}												\

#define READ_SHORT_VERIFY(X, Y, Z)							\
    {	unsigned char buffer[2]; /* ARRAY OK 2007-05-14 davve */					\
		X += 2 * fread(&buffer, 2, 1, (Z));					\
		unsigned short res = (buffer[0] << 8) | buffer[1];	\
		if (res != Y)										\
		{													\
			fprintf(stderr, "SHORT: %d != %d\n", res, Y);	\
			return -1;										\
		} }													\

#define READ_INT(X, Y, Z)												\
	{	unsigned char _int_buffer[4];									\
		X += 4 * fread(&_int_buffer, 4, 1, (Z));						\
		Y = ((_int_buffer[0] << 24) | (_int_buffer[1] << 16) |			\
			 (_int_buffer[2] << 8) | _int_buffer[3]); }					\

#define READ_FLOAT_VERIFY(X, Y, Z)								\
	{	int buffer;												\
		READ_INT(X, buffer, Z);									\
		float res = buffer / 65536.0;							\
		if (res != Y)											\
		{														\
			fprintf(stderr, "FLOAT: %f != %f\n", buffer, Y);	\
			return -1;											\
		} }														\

#define READ_INT_VERIFY(X, Y, Z)										\
	{	unsigned char _int_buffer[4];									\
		X += 4 * fread(&_int_buffer, 4, 1, (Z));						\
		unsigned int res = ((_int_buffer[0] << 24) | (_int_buffer[1] << 16) | \
							(_int_buffer[2] << 8) | _int_buffer[3]);	\
		if (res != Y)													\
		{																\
			fprintf(stderr, "INT: %d != %d\n", res, Y);					\
			return -1;													\
		} }																\

#define READ_STRING_VERIFY(X, Y, Z)								\
	{															\
		unsigned int len = 0;									\
		READ_INT(X, len, Z);									\
		char* buffer = new char[len + 1];						\
        X += len * fread(buffer, len, 1, (Z));					\
		buffer[len] = '\0';										\
        int res = strcmp(buffer, Y);							\
		delete[] buffer;										\
		if (res != 0)											\
		{														\
			fprintf(stderr, "STRING comparision failed.\n");	\
        	return -1;											\
		}														\
	}															\

#define FT_POS_TO_CMD_NUMBER_TYPE(X) ((CMD_NUMBER_TYPE)(X))
#define WRITE_CMD_NUMBER(X, Y, Z) WRITE_SHORT(X, Y, Z)
#define READ_CMD_NUMBER_VERIFY(X, Y, Z) READ_SHORT_VERIFY(X, Y, Z)

int MoveToProc(const FT_Vector* to, void* data)
{
	if(!data)
		return -1;

	ExtractedChar* p = (ExtractedChar*)data;
	p->MoveTo(FT_POS_TO_CMD_NUMBER_TYPE(to->x),
			  FT_POS_TO_CMD_NUMBER_TYPE(to->y));
	return 0;
}

int LineToProc(const FT_Vector* to, void* data)
{
	if(!data)
		return -1;

	ExtractedChar* p = (ExtractedChar*)data;
	p->LineTo(FT_POS_TO_CMD_NUMBER_TYPE(to->x),
			  FT_POS_TO_CMD_NUMBER_TYPE(to->y));
	return 0;
}

int QuadraticCurveToProc(const FT_Vector* control, const FT_Vector* to, void* data)
{
	if(!data)
		return -1;

	ExtractedChar* p = (ExtractedChar*)data;
	p->QuadraticCurveTo(FT_POS_TO_CMD_NUMBER_TYPE(control->x),
						FT_POS_TO_CMD_NUMBER_TYPE(control->y),
						FT_POS_TO_CMD_NUMBER_TYPE(to->x),
						FT_POS_TO_CMD_NUMBER_TYPE(to->y));
	return 0;
}

int CubicCurveToProc(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* data)
{
	if(!data)
		return -1;

	ExtractedChar* p = (ExtractedChar*)data;
	p->CubicCurveTo(FT_POS_TO_CMD_NUMBER_TYPE(control1->x),
					FT_POS_TO_CMD_NUMBER_TYPE(control1->y),
					FT_POS_TO_CMD_NUMBER_TYPE(control2->x),
					FT_POS_TO_CMD_NUMBER_TYPE(control2->y),
					FT_POS_TO_CMD_NUMBER_TYPE(to->x),
					FT_POS_TO_CMD_NUMBER_TYPE(to->y));
	return 0;
}

int FontExtract::Init(FT_Library library, const char* font_name, ExtractedFont* extracted_font)
{
	m_ft_library = library;
	m_font_name = font_name;
	m_extracted_font = extracted_font;

	int error = FT_New_Face( m_ft_library,
							 m_font_name,
							 0,
							 &m_ft_face);

	m_extracted_font->m_ascent = m_ft_face->ascender;
	m_extracted_font->m_descent = m_ft_face->descender;
	m_extracted_font->m_glyphs= m_ft_face->num_glyphs;
	m_extracted_font->m_unit_per_em= m_ft_face->units_per_EM;

	m_extracted_font->m_family_name = strdup(m_ft_face->family_name);
	m_extracted_font->m_style_name = strdup(m_ft_face->style_name);

	if (error)
		return error;

	m_init = 1;
	return 0;
}

ExtractedFont::ExtractedFont() :
		m_family_name(NULL),
		m_style_name(NULL)
{
}

ExtractedFont::~ExtractedFont()
{
	free(m_family_name);
	free(m_style_name);

	for (std::list<ExtractedChar*>::iterator it = m_chars.begin();
		 it != m_chars.end();
		 ++it)
	{
		delete *it;
	}

	m_chars.clear();
}

int ExtractedFont::AddExtractedChar(ExtractedChar* extracted_char)
{
	m_chars.push_back(extracted_char);
	return 0;
}

int ExtractedFont::StreamData(FILE* outfile)
{
	int bytes_written = 0;

	WRITE_SHORT(bytes_written, MAGIC_FILE_START, outfile); // magic token
	WRITE_SHORT(bytes_written, FILE_VERSION, outfile); // version

	// font-info
	WRITE_FLOAT(bytes_written, m_ascent, outfile);
	WRITE_FLOAT(bytes_written, m_descent, outfile);
	WRITE_INT(bytes_written, m_unit_per_em, outfile);
	WRITE_STRING(bytes_written, m_family_name, outfile);
	WRITE_STRING(bytes_written, m_style_name, outfile);

	if (m_chars.size() > 0xFFFF)
	{
		fprintf(stderr, "Warning: Character overflow\n");
	}

	WRITE_SHORT(bytes_written, m_chars.size(), outfile);

	for (std::list<ExtractedChar*>::iterator it = m_chars.begin();
		 it != m_chars.end();
		 ++it)
	{
		WRITE_SHORT(bytes_written, MAGIC_BEGIN_GLYPH, outfile);
		ExtractedChar* extracted_char = *it;
		WRITE_INT(bytes_written, extracted_char->CalculateDataSize(), outfile);

		int result = extracted_char->StreamData(outfile);
		if (result >= 0)
			bytes_written += result;
		else
			return result;
	}
	return bytes_written;
}

int ExtractedFont::Verify(FILE* outfile)
{
	int bytes_read = 0;

	READ_SHORT_VERIFY(bytes_read, 0x4712, outfile);
	READ_SHORT_VERIFY(bytes_read, FILE_VERSION, outfile);
	READ_FLOAT_VERIFY(bytes_read, m_ascent, outfile);
	READ_FLOAT_VERIFY(bytes_read, m_descent, outfile);
	READ_INT_VERIFY(bytes_read, m_unit_per_em, outfile);
	READ_STRING_VERIFY(bytes_read, m_family_name, outfile);
	READ_STRING_VERIFY(bytes_read, m_style_name, outfile);

	READ_SHORT_VERIFY(bytes_read, m_chars.size(), outfile);

	for (std::list<ExtractedChar*>::iterator it = m_chars.begin();
		 it != m_chars.end();
		 ++it)
	{
		READ_SHORT_VERIFY(bytes_read, 0x0992, outfile);
		ExtractedChar* extracted_char = *it;

		READ_INT_VERIFY(bytes_read, extracted_char->CalculateDataSize(), outfile);

		int result = extracted_char->Verify(outfile);
		if (result >= 0)
			bytes_read += result;
		else
			return result;
	}

	return bytes_read;
}

ExtractedChar::~ExtractedChar()
{
	for (std::list<ExtractedCommand*>::iterator it = m_commands.begin();
		 it != m_commands.end();
		 ++it)
	{
		delete *it;
	}
}

void ExtractedChar::MoveTo(CMD_NUMBER_TYPE x, CMD_NUMBER_TYPE y)
{
	ExtractedCommand* cmd = new ExtractedCommand;
	cmd->type = CMD_TYPE_MOVE_TO;
	cmd->x = x;
	cmd->y = y;
	m_commands.push_back(cmd);
}

void ExtractedChar::LineTo(CMD_NUMBER_TYPE x, CMD_NUMBER_TYPE y)
{
	ExtractedCommand* cmd = new ExtractedCommand;
	cmd->type = CMD_TYPE_LINE_TO;
	cmd->x = x;
	cmd->y = y;
	m_commands.push_back(cmd);
}

void ExtractedChar::QuadraticCurveTo(CMD_NUMBER_TYPE x1, CMD_NUMBER_TYPE y1, CMD_NUMBER_TYPE x, CMD_NUMBER_TYPE y)
{
	ExtractedCommand* cmd = new ExtractedCommand;
	cmd->type = CMD_TYPE_QUADRATIC_CURVE_TO;
	cmd->x1 = x1;
	cmd->y1 = y1;
	cmd->x = x;
	cmd->y = y;
	m_commands.push_back(cmd);
}

void ExtractedChar::CubicCurveTo(CMD_NUMBER_TYPE x1, CMD_NUMBER_TYPE y1, CMD_NUMBER_TYPE x2, CMD_NUMBER_TYPE y2, CMD_NUMBER_TYPE x, CMD_NUMBER_TYPE y)
{
	ExtractedCommand* cmd = new ExtractedCommand;
	cmd->type = CMD_TYPE_CUBIC_CURVE_TO;
	cmd->x1 = x1;
	cmd->y1 = y1;
	cmd->x2 = x2;
	cmd->y2 = y2;
	cmd->x = x;
	cmd->y = y;
	m_commands.push_back(cmd);
}

int ExtractedChar::CalculateDataSize()
{
	if (!final)
		return -1;

	int bytes = 0;

	bytes += 2; // uc
	bytes += 4; // advance

	for (std::list<ExtractedCommand*>::iterator it = m_commands.begin();
		 it != m_commands.end();
		 ++it)
	{
		ExtractedCommand* cmd = *it;
		bytes += 2; // cmd
		if (cmd->type == CMD_TYPE_CUBIC_CURVE_TO ||
			cmd->type == CMD_TYPE_QUADRATIC_CURVE_TO)
		{
			bytes += 2 * sizeof(CMD_NUMBER_TYPE); // x1, y1
		}
		if (cmd->type == CMD_TYPE_CUBIC_CURVE_TO)
		{
			bytes += 2 * sizeof(CMD_NUMBER_TYPE); // x2, y2
		}
		bytes += 2 * sizeof(CMD_NUMBER_TYPE); // x, y
	}

	bytes += 2; // stop command
	return bytes;
}

int ExtractedChar::StreamData(FILE* outfile)
{
	if (!final)
		return -1;

	int bytes_written = 0;
	WRITE_SHORT(bytes_written, uc, outfile);
	WRITE_FLOAT(bytes_written, advance, outfile);

	for (std::list<ExtractedCommand*>::iterator it = m_commands.begin();
		 it != m_commands.end();
		 ++it)
	{
		ExtractedCommand* cmd = *it;
		WRITE_SHORT(bytes_written, cmd->type, outfile);
		if (cmd->type == CMD_TYPE_CUBIC_CURVE_TO ||
			cmd->type == CMD_TYPE_QUADRATIC_CURVE_TO)
		{
			WRITE_CMD_NUMBER(bytes_written, cmd->x1, outfile);
			WRITE_CMD_NUMBER(bytes_written, cmd->y1, outfile);
		}

		if (cmd->type == CMD_TYPE_CUBIC_CURVE_TO)
		{
			WRITE_CMD_NUMBER(bytes_written, cmd->x2, outfile);
			WRITE_CMD_NUMBER(bytes_written, cmd->y2, outfile);
		}

		WRITE_CMD_NUMBER(bytes_written, cmd->x, outfile);
		WRITE_CMD_NUMBER(bytes_written, cmd->y, outfile);
	}
	WRITE_SHORT(bytes_written, END_COMMANDS, outfile);

	return bytes_written;
}

int ExtractedChar::Verify(FILE* outfile)
{
	if (!final)
		return -1;

	int bytes_read = 0;

	READ_SHORT_VERIFY(bytes_read, uc, outfile);
	READ_FLOAT_VERIFY(bytes_read, advance, outfile);

	for (std::list<ExtractedCommand*>::iterator it = m_commands.begin();
		 it != m_commands.end();
		 ++it)
	{
		ExtractedCommand* cmd = *it;
		ExtractedCommand filecmd;

		READ_SHORT_VERIFY(bytes_read, cmd->type, outfile);
		if (cmd->type == CMD_TYPE_CUBIC_CURVE_TO ||
			cmd->type == CMD_TYPE_QUADRATIC_CURVE_TO)
		{
				READ_CMD_NUMBER_VERIFY(bytes_read, cmd->x1, outfile);
				READ_CMD_NUMBER_VERIFY(bytes_read, cmd->y1, outfile);
		}

		if (cmd->type == CMD_TYPE_CUBIC_CURVE_TO)
		{
				READ_CMD_NUMBER_VERIFY(bytes_read, cmd->x2, outfile);
				READ_CMD_NUMBER_VERIFY(bytes_read, cmd->y2, outfile);
		}

		READ_CMD_NUMBER_VERIFY(bytes_read, cmd->x, outfile);
		READ_CMD_NUMBER_VERIFY(bytes_read, cmd->y, outfile);
	}

	READ_SHORT_VERIFY(bytes_read, 0x0991, outfile);

	return bytes_read;
}

int FontExtract::ExtractAll()
{
	int error = 0;
	FT_UInt gindex;
	FT_ULong  charcode = FT_Get_First_Char( m_ft_face, &gindex );
	while ( gindex != 0 )
	{
		error = Extract(charcode);
		if (error)
		{
			return error;
		}
		charcode = FT_Get_Next_Char( m_ft_face, charcode, &gindex );
	}
	return error;
}

int FontExtract::Extract(unsigned short the_char)
{
	if (m_init == 0)
		return -1;

	ExtractedChar* extracted_char = new ExtractedChar();

	float advance = 0;
	FT_Error err = FT_Load_Char(m_ft_face, the_char, FT_LOAD_NO_BITMAP | FT_LOAD_NO_SCALE);
	if(err == 0)
	{
		FT_GlyphSlot slot = m_ft_face->glyph;
		FT_Outline outline = slot->outline;
		FT_Outline_Funcs funcs = {
			&MoveToProc,
			&LineToProc,
			&QuadraticCurveToProc,
			&CubicCurveToProc,
			0,
			0
		};
		FT_Outline_Decompose(&outline, &funcs, (void *)extracted_char);
		advance = slot->advance.x;
	}

	extracted_char->Done();

	extracted_char->uc = the_char;
	extracted_char->advance = advance;

	return m_extracted_font->AddExtractedChar(extracted_char);
}

int main(int argc, char** argv)
{
	FT_Library  library;   /* handle to library     */

	if (argc != 3)
	{
		fprintf(stderr, "Wrong number of arguments\n");
		fprintf(stderr, FONT_EXTRACT_USAGE);
		return -1;
	}

	int error = FT_Init_FreeType( &library );
	if (error)
	{
		fprintf(stderr, "Error initializing freetype library\n");
		return -1;
	}

	const char* font_name = argv[1];
	const char* target_name = argv[2];

	FontExtract fnt_extract;
	ExtractedFont extracted_font;
	error = fnt_extract.Init(library, font_name, &extracted_font);
	if (error)
	{
		fprintf(stderr, "Error initializing font extractor\n");
		return -1;
	}

	fnt_extract.ExtractAll();

	FILE* out_file = fopen(target_name, "w+");
	if (out_file == NULL)
	{
		fprintf(stderr, "Error opening output file\n");
		return -1;
	}

	int written_bytes = extracted_font.StreamData(out_file);
	if (written_bytes < 0)
	{
		fprintf(stderr, "Error writing data to disc\n");
		fclose(out_file);
		return -1;
	}

	printf("%d bytes written to %s extracted from %s\n", written_bytes, target_name, font_name);

	rewind(out_file);

	int read_bytes = extracted_font.Verify(out_file);
	if (read_bytes < 0)
	{
		fprintf(stderr, "Error with verifying written data\n");
		return -1;
	}

	fclose(out_file);
	if (written_bytes == read_bytes)
		printf("%d bytes verified.\n", written_bytes);
	else
	{
		fprintf(stderr, "Internal error detected. Aborting. (%d => %d)\n", written_bytes, read_bytes);
		return -1;
	}

	return 0;
}
