/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef POSTSCRIPT_VEGA_PRINTER_LISTENER_H
#define POSTSCRIPT_VEGA_PRINTER_LISTENER_H

#ifdef VEGA_POSTSCRIPT_PRINTING
#ifdef VEGA_PRINTER_LISTENER_SUPPORT

#include "modules/libvega/src/oppainter/vegaprinterlistener.h"

#include "modules/util/opfile/opfile.h"
#include "modules/libvega/src/postscript/postscriptprolog.h"
#include "modules/libvega/src/postscript/postscriptbody.h"


class OpFileDescriptor;
class PostScriptBody;
class VEGAPath;
class VEGAStencil;


/** @brief Helper class that generates PostScript image data
  */
class PostScriptVEGAPrinterListener : public VEGAPrinterListener
{
public:
	PostScriptVEGAPrinterListener(OpFile& file);

	OP_STATUS Init(PostScriptOutputMetrics* screen_metrics, PostScriptOutputMetrics* paper_metrics);

	virtual void SetClipRect(const OpRect& clip);
	virtual void SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha);
	/**
	   sets current_font to font - use updateFont to set current_font as used
	 */
	virtual void SetFont(OpFont* font);

	virtual void DrawRect(const OpRect& rect, UINT32 width);
	virtual void FillRect(const OpRect& rect);
	virtual void DrawEllipse(const OpRect& rect, UINT32 width);
	virtual void FillEllipse(const OpRect& rect);
	virtual void FillPath(VEGAPath *path, VEGAStencil *stencil);

	virtual void DrawLine(const OpPoint& from, const OpPoint& to, UINT32 width);
	virtual void DrawString(const OpPoint& pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width);

	virtual void DrawBitmapClipped(const OpBitmap* bitmap, const OpRect& source, OpPoint p);

	OP_STATUS startPage();
	OP_STATUS endPage();

	void printFinished();
	void printAborted();

private:
	void DrawString(const OpPoint& pos, const struct ProcessedString* processed_string);
	void DrawPath(VEGAPath *path);
	void AddLineToPath(VEGA_FIX* line, int& curpos_x, int& curpos_y);

	/**
	   calls updateFont, and on success returns prolog.getCurrentFontConverter()
	 */
	TrueTypeConverter* getFontConverter();
	/**
	   changes the currently used font (used_font) to current_font
	   @return OpStatus::OK on success, OpStatus::ERR on failure, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS updateFont();
	void updateColor();

	OP_STATUS concatenatePostScriptParts();

	PostScriptOutputMetrics* screen_metrics;
	PostScriptOutputMetrics* paper_metrics;

	OpFile& destination_file;
	OpFile body_temp_file;

	PostScriptProlog prolog;
	PostScriptBody body;

	OpRect current_cliprect;

	UINT32 current_color;
	UINT32 used_color;

	OpFont* current_font; ///< font currently set with setFont
	OpFont* used_font; ///< font currently in use
};

#endif // VEGA_PRINTER_LISTENER_SUPPORT
#endif // VEGA_POSTSCRIPT_PRINTING

#endif // POSTSCRIPT_VEGA_PRINTER_LISTENER_H
