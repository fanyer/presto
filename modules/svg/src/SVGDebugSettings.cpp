
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#ifdef _DEBUG
# include "modules/libvega/vegarendertarget.h"
#endif // _DEBUG

#include "modules/svg/src/SVGManagerImpl.h"

#include "modules/img/imagedump.h"

#ifdef SVG_DEBUG_TRACE

#include "modules/svg/src/SVGDebugSettings.h"

/**
 * Keep this updated with new categories, in the same order as Category.
 */
const SVGDebugTrace::Entry SVGDebugTrace::entries[] =  {
	{ "PARSER", SVGDebugTrace::PARSER },
	{ "LEXER",  SVGDebugTrace::LEXER },
	{ "ANIMATION", SVGDebugTrace::ANIMATION },
	{ "EVENTS", SVGDebugTrace::EVENTS },
	{ "RENDER", SVGDebugTrace::RENDER },
	{ "COMPARE", SVGDebugTrace::COMPARE },
	{ "UNKNOWN", SVGDebugTrace::UNKNOWN },
	{ "GENERAL", SVGDebugTrace::GENERAL }
};

SVGDebugTrace tracer;

SVGDebugTrace::SVGDebugTrace()
{
	switches = OP_NEWA(BOOL, NUM_CAT);
	op_memset(switches, 0, NUM_CAT * sizeof(BOOL));

	switches[GENERAL] = TRUE;

	FILE* in = fopen("svg_debug_settings.txt", "r");
	if (in)
	{
		char s[500]; /* ARRAY OK 2007-11-14 ed */
		while (fgets(s, 499, in) != NULL)
		{
			char* tok1 = strtok(s, "=");
			char* tok2 = strtok(NULL, ";");

			if (tok1 == NULL || tok2 == NULL)
				continue;

			BOOL val = FALSE;
			if (op_strcmp(tok2, "TRUE") == 0 ||
				op_strcmp(tok2, "YES") == 0 ||
				op_strcmp(tok2, "1") == 0)
			{
				val = TRUE;
			}

			Category cat = UNKNOWN;
			for (int i = 0; i < NUM_CAT; i++)
			{
				if (op_strcmp(entries[i].str, tok1) == 0)
				{
					cat = entries[i].cat;
				}
			}
			switches[cat] = val;
		}
		fclose(in);
	}
}

SVGDebugTrace::~SVGDebugTrace()
{
	OP_DELETEA(switches);
}

#ifdef __GNUC__
void SVGDebugTrace::Output(SVGDebugTrace::Category cat,
						   SVGDebugTrace::Level prio,
						   const char* func,
						   const char* msg, ...) const
{
	va_list ap;
	va_start (ap, msg);
	if (switches[cat])
	{
		fprintf(stderr, "[SVG][%s:%s]: ", entries[cat].str, func);
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
	}
	va_end (ap);
}
#else // __GNUC__
void SVGDebugTrace::Output(SVGDebugTrace::Category cat,
						   SVGDebugTrace::Level prio,
						   const char* msg, ...) const
{
	va_list ap;
	va_start (ap, msg);
	if (switches[cat])
	{
		fprintf(stderr, "[SVG][%s]: ", entries[cat].str);
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
	}
	va_end (ap);
}
#endif // __GNUC__

#endif // SVG_DEBUG_TRACE

#ifdef _DEBUG

#include "modules/pi/OpBitmap.h"

// Crude bitmap (BMP) writer
void DumpRenderTarget(VEGARenderTarget* rt, const uni_char* filestr)
{
	OpString filename;
	RETURN_VOID_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, filename));
	RETURN_VOID_IF_ERROR(filename.AppendFormat(UNI_L("%s%05d.bmp"), filestr, g_svg_manager_impl->seq_num++));

	OpBitmap* tmp_bm = NULL;
	RETURN_VOID_IF_ERROR(OpBitmap::Create(&tmp_bm, rt->getWidth(), rt->getHeight(),
										  FALSE, TRUE, 0, 0, TRUE));

	OP_STATUS status = rt->copyToBitmap(tmp_bm);
	if (OpStatus::IsSuccess(status))
		// Change 3rd parameter to FALSE to render onto white
		// background instead of grid
		OpStatus::Ignore(DumpOpBitmap(tmp_bm, filename, TRUE));

	OP_DELETE(tmp_bm);
}

#include "modules/encodings/encoders/utf8-encoder.h"

void TmpSingleByteString::Set(const uni_char* src, unsigned int srclen)
{
	UTF16toUTF8Converter converter;

	int written = converter.BytesNeeded(src, UNICODE_SIZE(srclen));
	converter.Reset();

	m_str = OP_NEWA(char, written+1);
	if (!m_str)
		return;

	int read;
	written = converter.Convert(src, UNICODE_SIZE(srclen), m_str, written+1, &read);

	m_str[written] = 0;
}
#endif // _DEBUG

#endif // SVG_SUPPORT
