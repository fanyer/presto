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
#include "modules/libvega/src/postscript/postscriptbody.h"
#include "modules/libvega/src/postscript/truetypeconverter.h"
#include "modules/mdefont/processedstring.h"


PostScriptBody::PostScriptBody()
	: in_page(false)
	, page_count(0)
	, current_font(0)
	, current_font_data(NULL)
	, current_font_data_size(0)
{
}


OP_STATUS PostScriptBody::Init(OpFileDescriptor& file, PostScriptOutputMetrics* screen_metrics, PostScriptOutputMetrics* paper_metrics)
{
	OP_NEW_DBG("PostScriptBody::Init()", "psprint");
	OP_DBG(("Document size: width=%d, height=%d\n", screen_metrics->width, screen_metrics->height));

	this->file = &file;
	this->screen_metrics = screen_metrics;
	this->paper_metrics = paper_metrics;

	return this->file->Open(OPFILE_WRITE);
}


OP_STATUS PostScriptBody::finish()
{
	OP_NEW_DBG("PostScriptBody::finish()", "psprint");
	RETURN_IF_ERROR(endPage());
	return file->Close();
}


OP_STATUS PostScriptBody::startPage()
{
	OP_NEW_DBG("PostScriptBody::startPage()", "psprint");
	in_page = true;
	++page_count;

	RETURN_IF_ERROR(addFormattedCommand("%%%%Page: %d %d", page_count, page_count));
	RETURN_IF_ERROR(addCommand("%%PageOrientation: Portrait"));
	RETURN_IF_ERROR(addFormattedCommand("%%%%PageBoundingBox: 0 0 %d %d", paper_metrics->width, paper_metrics->height));
 	RETURN_IF_ERROR(addCommand("translatescreen"));
 	RETURN_IF_ERROR(addCommand("scalescreen"));

	return OpStatus::OK;
}

OP_STATUS PostScriptBody::endPage()
{
	OP_NEW_DBG("PostScriptBody::endPage()", "psprint");
	if (in_page)
	{
		in_page = false;
		return addCommand("showpage");
	}
	return OpStatus::OK;
}


OP_STATUS PostScriptBody::pushGraphicsState()
{
	RETURN_IF_ERROR(addCommand("gsave"));
	depth++;
	return OpStatus::OK;
}


OP_STATUS PostScriptBody::popGraphicsState()
{
	depth--;
	return addCommand("grestore");
}


OP_STATUS PostScriptBody::changeFont(OpFont* font, UINT8* font_data, UINT32 font_data_size, const char* ps_fontname, int size)
{
	current_font = font;
	current_font_data = font_data;
	current_font_data_size = font_data_size;
	return addFormattedCommand("/%s %d changefont", ps_fontname, size);
}


OP_STATUS PostScriptBody::drawString(TrueTypeConverter* font_converter, UINT32 x, UINT32 y, UINT32 size, const struct ProcessedString* processed_string)
{
	OP_NEW_DBG("PostScriptBody::drawString() - processed string", "psprint");
	if (!current_font || !current_font_data)
		return OpStatus::ERR;
	
	OpString8 result;

	OP_STATUS status = OpStatus::OK;

	for (unsigned int g = 0; g < processed_string->m_length; ++g)
	{
		UINT8 ps_charcode;
		OpString8 glyph_name;
		bool has_charcode;
		OP_STATUS s = font_converter->getGlyphIdentifiers(processed_string->m_processed_glyphs[g].m_id, ps_charcode, glyph_name, has_charcode, current_font_data, current_font_data_size);
		if (OpStatus::IsError(s))
		{
			if (status != OpStatus::ERR_NO_MEMORY) // OOM takes precedence
				status = s;
			continue;
		}

		if (has_charcode)
		{
			RETURN_IF_ERROR(result.AppendFormat("[%d %d <%02x>]", 
							getX(x + processed_string->m_processed_glyphs[g].m_pos.x), 
							getY(y + processed_string->m_processed_glyphs[g].m_pos.y),
							ps_charcode));
		}
		else
		{
			RETURN_IF_ERROR(result.AppendFormat("[%d %d /%s]", 
							getX(x + processed_string->m_processed_glyphs[g].m_pos.x), 
							getY(y + processed_string->m_processed_glyphs[g].m_pos.y),
							glyph_name.CStr()));
		}

	}				

	if (!result.IsEmpty())
		return addFormattedCommand("[%s] opshow", result.CStr()); 

	return status;
}


#endif // VEGA_POSTSCRIPT_PRINTING
