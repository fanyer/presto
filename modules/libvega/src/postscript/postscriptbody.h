/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef POSTSCRIPT_BODY_H
#define POSTSCRIPT_BODY_H

#ifdef VEGA_POSTSCRIPT_PRINTING

#include "modules/libvega/src/postscript/postscriptcommandbuffer.h"

class OpFileDescriptor;
class TrueTypeConverter;


/** @brief Helper class to create a PostScript body and abstract graphics commands
  */
class PostScriptBody : public PostScriptCommandBuffer
{
public:
	PostScriptBody();

	/**
	  * @param file A closed, constructed file object - will be opened for writing
	  */
	OP_STATUS Init(OpFileDescriptor& file, PostScriptOutputMetrics* screen_metrics, PostScriptOutputMetrics* paper_metrics);
	OP_STATUS finish();

	class GraphicsStatePusher
	{
	public:
		GraphicsStatePusher(PostScriptBody* body) : m_body(body), m_pushed(FALSE) {}
		~GraphicsStatePusher() { pop(); }
		OP_STATUS push() { RETURN_IF_ERROR(m_body->pushGraphicsState()); m_pushed = TRUE; return OpStatus::OK; }
		OP_STATUS pop() { if (!m_pushed) return OpStatus::OK; m_pushed = FALSE; return m_body->popGraphicsState(); }
	private:
		PostScriptBody* m_body;
		BOOL m_pushed;
	};
	// various PostScript commands
	OP_STATUS pushGraphicsState();
	OP_STATUS popGraphicsState();

	inline OP_STATUS newPath() { return addCommand("newpath"); }
	inline OP_STATUS closePath() { return addCommand("closepath"); }
	inline OP_STATUS fill() { return addCommand("fill"); }

	inline OP_STATUS setLineWidth(int w) { return addFormattedCommand("%d setlinewidth", w); }
	inline OP_STATUS moveTo(int x, int y) { return addFormattedCommand("%d %d mt", getX(x), getY(y)); }
	inline OP_STATUS lineTo(int x, int y) { return addFormattedCommand("%d %d lt", getX(x), getY(y)); }
	inline OP_STATUS stroke() { return addCommand("stroke"); }

	inline OP_STATUS rectDraw(int x, int y, int w, int h) { return addFormattedCommand("%d %d %d %d rs", getX(x), getY(y)-h, w, h); }
	inline OP_STATUS rectFill(int x, int y, int w, int h) { return addFormattedCommand("%d %d %d %d rf", getX(x), getY(y)-h, w, h); }

	inline OP_STATUS ellipseDraw(int x, int y, int w, int h) { return addFormattedCommand("%f %f %f %f false ellipse", 
											getX(x) + double(w)/2.0, getY(y) - double(h)/2.0, double(w)/2.0, double(h)/2.0); }

	inline OP_STATUS ellipseFill(int x, int y, int w, int h) { return addFormattedCommand("%f %f %f %f true ellipse", 
											getX(x) + double(w)/2.0, getY(y) - double(h)/2.0, double(w)/2.0, double(h)/2.0); }

	inline OP_STATUS rectClip(int x, int y, int w, int h) { return addFormattedCommand("initclip %d %d %d %d rectclip", getX(x), getY(y)-h, w, h); }

	inline OP_STATUS setColor(UINT32 color) { return addFormattedCommand("%0.3f %0.3f %0.3f sc", getRed(color), getGreen(color), getBlue(color)); }

	inline OP_STATUS translate(int x, int y) { return addFormattedCommand("%d %d translate", getX(x), getY(y)); }
	inline OP_STATUS scale(int w, int h) { return addFormattedCommand("%d %d scale", w, h); }

	inline OP_STATUS defineFontName(const char* name) { return addFormattedCommand("/FontName /%s def", name); }

	OP_STATUS changeFont(OpFont* font, UINT8* font_data, UINT32 font_data_size, const char* ps_fontname, int size);

	OP_STATUS drawString(TrueTypeConverter* font_converter, UINT32 x, UINT32 y, UINT32 size, uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width);
	OP_STATUS drawString(TrueTypeConverter* font_converter, UINT32 x, UINT32 y, UINT32 size, const struct ProcessedString* processed_string);
	
	OP_STATUS startPage();
	OP_STATUS endPage();

private:
	inline double getColor(UINT8 color) const { return double(color) / 255.0; }
	inline double getAlpha(UINT32 color) const { return double((color & 0xff000000) >> 24) / 255.0; }
	inline double getRed(UINT32 color) const { return double((color & 0x00ff0000) >> 16) / 255.0; }
	inline double getGreen(UINT32 color) const { return double((color & 0x0000ff00) >> 8) / 255.0; }
	inline double getBlue(UINT32 color) const { return double(color & 0x000000ff) / 255.0; }

	inline int getY(int y) { return screen_metrics->height - (y+1); }
	inline int getX(int x) { return x; }

	bool in_page;
	UINT32 page_count;

	PostScriptOutputMetrics* screen_metrics;
	PostScriptOutputMetrics* paper_metrics;

	OpFont* current_font;
	UINT8* current_font_data;
	UINT32 current_font_data_size;
};



#endif // VEGA_POSTSCRIPT_PRINTING

#endif // POSTSCRIPT_COMMAND_BUFFER_H
