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
#ifdef VEGA_PRINTER_LISTENER_SUPPORT

#include "modules/libvega/src/postscript/postscriptvegaprinterlistener.h"
#include "modules/libvega/src/postscript/postscriptimage.h"
#include "modules/libvega/src/postscript/truetypeconverter.h"

#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "modules/libvega/src/oppainter/vegamdefont.h"
#include "modules/mdefont/mdefont.h"

#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegastencil.h"


PostScriptVEGAPrinterListener::PostScriptVEGAPrinterListener(OpFile& file)
	: destination_file(file),
	  current_color(0),
	  used_color(-1),
	  current_font(0),
	  used_font(0)
{
}


OP_STATUS PostScriptVEGAPrinterListener::Init(PostScriptOutputMetrics* screen_metrics, PostScriptOutputMetrics* paper_metrics)
{
	this->screen_metrics = screen_metrics;
	this->paper_metrics = paper_metrics;

	RETURN_IF_ERROR(prolog.Init(destination_file, screen_metrics, paper_metrics));

	RETURN_IF_ERROR(body_temp_file.Construct(UNI_L("psbody.ps"), OPFILE_TEMP_FOLDER));
	return body.Init(body_temp_file, screen_metrics, paper_metrics);
}


OP_STATUS PostScriptVEGAPrinterListener::concatenatePostScriptParts()
{
	RETURN_IF_ERROR(destination_file.Open(OPFILE_APPEND));
	RETURN_IF_ERROR(body_temp_file.Open(OPFILE_READ));

	OpFileLength read_len;
	char buf[1024]; // ARRAY OK 2010-12-16 wonko
	while(1)
	{
		RETURN_IF_ERROR(body_temp_file.Read(buf, 1024, &read_len));
		if (read_len == 0)
			break;
		RETURN_IF_ERROR(destination_file.Write(buf, read_len));
	}

	RETURN_IF_ERROR(destination_file.Close());
	RETURN_IF_ERROR(body_temp_file.Close());

	return body_temp_file.Delete();
}


void PostScriptVEGAPrinterListener::SetClipRect(const OpRect& clip)
{
	OP_NEW_DBG("PostScriptVEGAPrinterListener::SetClipRect()", "psprint");
	if (current_cliprect.Equals(clip))
		return;

	body.rectClip(clip.x, clip.y, clip.width, clip.height);
	current_cliprect = clip;
}


void PostScriptVEGAPrinterListener::SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha)
{
	OP_NEW_DBG("PostScriptVEGAPrinterListener::SetColor()", "psprint");
	OP_DBG(("New color: r=%d, g=%d, b=%d, a=%d", red, green, blue, alpha));
	UINT32 new_color = (alpha<<24)|(red<<16)|(green<<8)|blue;

	if (new_color == current_color)
		return;

	current_color = new_color;
}


void PostScriptVEGAPrinterListener::updateColor()
{
	OP_NEW_DBG("PostScriptVEGAPrinterListener::updateColor()", "psprint");
	if (used_color != current_color)
	{
		used_color = current_color;
		body.setColor(used_color);
	}
}


void PostScriptVEGAPrinterListener::SetFont(OpFont* font)
{
	OP_NEW_DBG("PostScriptVEGAPrinterListener::SetFont()", "psprint");
	current_font = font;

/*	VEGAOpFont* vega_op_font = reinterpret_cast<VEGAOpFont*>(font);
	VEGAFont* vega_font = vega_op_font->getVegaFont();
	const uni_char* font_name = vega_font->getFontName();

	OpString8 font_str;
	font_str.Set(font_name);

	printf("PostScriptVEGAPrinterListener::SetFont(): %s\n", font_str.CStr());*/
}


OP_STATUS PostScriptVEGAPrinterListener::updateFont()
{
	OP_NEW_DBG("PostScriptVEGAPrinterListener::updateFont()", "psprint");

	if (used_font != current_font)
	{
		//printf("Font metrics: height=%d, ascent=%d, descent=%d, internal=%d\n", current_font->Height(), current_font->Ascent(), current_font->Descent(), current_font->InternalLeading());
		OP_DBG(("Font metrics: height=%d, ascent=%d, descent=%d, internal=%d\n", current_font->Height(), current_font->Ascent(), current_font->Descent(), current_font->InternalLeading()));

		const char* ps_fontname;
		RETURN_IF_ERROR(prolog.addFont(current_font, ps_fontname));

		UINT8* font_data;
		UINT32 font_data_size;
		prolog.getCurrentFontData(font_data, font_data_size);
		RETURN_IF_ERROR(body.changeFont(current_font, font_data, font_data_size, ps_fontname, current_font->Height() - current_font->InternalLeading()));

		used_font = current_font;
	}

	return OpStatus::OK;
}

TrueTypeConverter* PostScriptVEGAPrinterListener::getFontConverter()
{
	if (OpStatus::IsError(updateFont()))
		return NULL;
	return prolog.getCurrentFontConverter();
}


void PostScriptVEGAPrinterListener::DrawRect(const OpRect& rect, UINT32 width)
{
	updateColor();
	body.pushGraphicsState();
	body.setLineWidth(width);
	body.rectDraw(rect.x, rect.y, rect.width, rect.height);
	body.popGraphicsState();
}


void PostScriptVEGAPrinterListener::FillRect(const OpRect& rect)
{
	updateColor();
	body.rectFill(rect.x, rect.y, rect.width, rect.height);
}


void PostScriptVEGAPrinterListener::DrawEllipse(const OpRect& rect, UINT32 width)
{
	updateColor();
	body.pushGraphicsState();
	body.setLineWidth(width);
	body.ellipseDraw(rect.x, rect.y, rect.width, rect.height);
	body.popGraphicsState();
}


void PostScriptVEGAPrinterListener::FillEllipse(const OpRect& rect)
{
	updateColor();
	body.ellipseFill(rect.x, rect.y, rect.width, rect.height);
}


void PostScriptVEGAPrinterListener::DrawLine(const OpPoint& from, const OpPoint& to, UINT32 width)
{
	updateColor();
	body.pushGraphicsState();
	body.setLineWidth(width);
	body.moveTo(from.x, from.y);
	body.lineTo(to.x, to.y);
	body.stroke();
	body.popGraphicsState();
}


void PostScriptVEGAPrinterListener::DrawString(const OpPoint& pos, const struct ProcessedString* processed_string)
{
	updateColor();
	if (TrueTypeConverter* current_font_converter = getFontConverter())
		body.drawString(current_font_converter, pos.x, pos.y, current_font->Height(), processed_string);
}

void PostScriptVEGAPrinterListener::DrawString(const OpPoint& pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width)
{
	ProcessedString processed_string;
	RETURN_VOID_IF_ERROR(((VEGAOpFont*)current_font)->getVegaFont()->ProcessString(&processed_string, text, len, extra_char_spacing, word_width, true));
	DrawString(pos, &processed_string);
}



void PostScriptVEGAPrinterListener::DrawBitmapClipped(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
	updateColor();
	// Do not bother about images to be painted completely outside page
	if ( p.x + source.width < 0 ||
		 (UINT32)p.x > screen_metrics->width ||
		 p.y + source.height < 0 ||
		 (UINT32)p.y > screen_metrics->height )
		return;

	PostScriptImage ps_image(bitmap, source, p);
	ps_image.generate(body);
}


OP_STATUS PostScriptVEGAPrinterListener::startPage()
{
	used_color = -1; // Force color to be set on new page
	return body.startPage();
}


OP_STATUS PostScriptVEGAPrinterListener::endPage()
{
	return body.endPage();
}


void PostScriptVEGAPrinterListener::printFinished()
{
	OP_NEW_DBG("PostScriptVEGAPrinterListener::printFinished()", "psprint");
	prolog.finish();
	body.finish();
	concatenatePostScriptParts();
}

void PostScriptVEGAPrinterListener::printAborted()
{
	prolog.finish();
	body.finish();
}


void PostScriptVEGAPrinterListener::FillPath(VEGAPath *path, VEGAStencil *stencil)
{
	if (!path->getNumLines())
		return;

	updateColor();
	body.pushGraphicsState();
	DrawPath(path);
	body.fill();
	body.popGraphicsState();
}


void PostScriptVEGAPrinterListener::DrawPath(VEGAPath *path)
{
	body.newPath();
	int x = 0, y = 0;
	for (unsigned i = 0; i < path->getNumLines(); i++)
		AddLineToPath(path->getNonWarpLine(i), x, y);
	body.closePath();
}


void PostScriptVEGAPrinterListener::AddLineToPath(VEGA_FIX* line, int& curpos_x, int& curpos_y)
{
	if (!line)
		return;
	
	int start_x = VEGA_FIXTOINT(line[VEGALINE_STARTX]);
	int start_y = VEGA_FIXTOINT(line[VEGALINE_STARTY]);
	int end_x   = VEGA_FIXTOINT(line[VEGALINE_ENDX]);
	int end_y   = VEGA_FIXTOINT(line[VEGALINE_ENDY]);

	if (start_x != curpos_x || start_y != curpos_y)
		body.moveTo(start_x, start_y);

	body.lineTo(end_x, end_y);

	curpos_x = end_x;
	curpos_y = end_y;
}


#endif // VEGA_PRINTER_LISTENER_SUPPORT
#endif // VEGA_SUPPORT
