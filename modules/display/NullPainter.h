#ifndef NULL_PAINTER_H // -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
#define NULL_PAINTER_H

#include "modules/pi/OpPainter.h"

/** A "dummy"-object to set instead of NULL in some situations. It does nothing. */

class NullPainter : public OpPainter
{
public:
	NullPainter() {};
	~NullPainter() {};

	void SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha = 255) {};
	UINT32 GetColor() { return 0; }

	virtual void SetPreAlpha(UINT8 alpha) { }
	virtual UINT8 GetPreAlpha() { return 255; }

	void SetFont(OpFont *font) { current_font = font; };

	OP_STATUS SetClipRect(const OpRect &rect) { return OpStatus::OK; }
	void RemoveClipRect() {};
	void GetClipRect(OpRect* rect) { rect->Set(0, 0, 10000, 10000); };
	
	void DrawRect(const OpRect &rect, UINT32 width = 1) {};

	void FillRect(const OpRect &rect) {};
	void ClearRect(const OpRect &rect) {};

	void DrawEllipse(const OpRect &rect, UINT32 width = 1) {};

	void FillEllipse(const OpRect &rect) {};

	virtual void DrawPolygon(const OpPoint* points, int count, UINT32 width = 1) {}

	void DrawLine(const OpPoint &from, UINT32 length, BOOL horizontal, UINT32 width = 1) {};

	void DrawLine(const OpPoint &from, const OpPoint &to, UINT32 width = 1) {};

	void DrawString(const OpPoint &pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width = -1) {};

	void InvertRect(const OpRect &rect) {};

	void InvertBorderRect(const OpRect &rect, int border) {};

	void InvertBorderEllipse(const OpRect &rect, int border) {};

	void InvertBorderPolygon(const OpPoint* point_array, int points, int border) {};

	float GetScale() { return 1.0; };

	void DrawBitmapClipped(const OpBitmap* bitmap, const OpRect &source, OpPoint p) {};
	
	void DrawBitmapClippedTransparent(const OpBitmap* bitmap, const OpRect& source, OpPoint p) {}

	void DrawBitmapClippedAlpha(const OpBitmap* bitmap, const OpRect& source, OpPoint p) {}

	void DrawBitmapScaled(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest) {}

	void DrawBitmapScaledTransparent(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest) {}

	void DrawBitmapScaledAlpha(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest) {}

	OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale) { return OpStatus::OK; }

	OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale, UINT32 bitmapWidth, UINT32 bitmapHeight) { return OpStatus::OK; }

	OpBitmap* CreateBitmapFromBackground(const OpRect& rect) { return NULL; }

	OP_STATUS BeginOpacity(const OpRect& rect, UINT8 opacity) { return OpStatus::OK; }

	void EndOpacity() {}
};

#endif // NULL_PAINTER_H
