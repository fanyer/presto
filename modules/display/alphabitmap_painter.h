/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef ALPHABITMAP_PAINTER_H
#define ALPHABITMAP_PAINTER_H

#ifdef DISPLAY_SUPPORTS_BROKEN_ALPHA_PAINTER

#include "modules/pi/OpPainter.h"

/** An instance of OpPainter which wraps around an OpPainter for an OpBitmap
 * and atempts to fix the alpha value. Should only be used if the OpPainter
 * has problem supporting alpha. */
class AlphaBitmapPainter : public OpPainter
{
public:
	AlphaBitmapPainter();
	virtual ~AlphaBitmapPainter();

	OP_STATUS Construct(OpBitmap* bitmap, BOOL bitmapContainsData);
	/** Prepare to render something with alpha correction. */
	void PrepareAlphaRendering();
	/** Finish rendering with alpha correction. */
	void FinishAlphaRendering();

	virtual void SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha){color=(alpha<<24)|(red<<16)|(green<<8)|blue; if (originalPainter) originalPainter->SetColor(red, green, blue, alpha);}
	virtual void SetFont(OpFont* font){if (originalPainter) originalPainter->SetFont(font);}
	virtual OP_STATUS SetClipRect(const OpRect& rect){if (originalPainter) return originalPainter->SetClipRect(rect); return OpStatus::ERR;}
	virtual void RemoveClipRect(){if (originalPainter) originalPainter->RemoveClipRect();}
	virtual void GetClipRect(OpRect* rect){if (originalPainter) originalPainter->GetClipRect(rect);}

	virtual void DrawRect(const OpRect& rect, UINT32 width = 1);
	virtual void FillRect(const OpRect& rect);
	virtual void ClearRect(const OpRect& rect);
	virtual void DrawEllipse(const OpRect& rect, UINT32 width = 1);
	virtual void FillEllipse(const OpRect& rect);
	virtual void DrawPolygon(const OpPoint* points, int count, UINT32 width = 1);
	virtual void DrawLine(const OpPoint& from, UINT32 length, BOOL horizontal, UINT32 width);
	virtual void DrawLine(const OpPoint& from, const OpPoint& to, UINT32 width);
	virtual void DrawString(const OpPoint& pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width = -1);

	virtual void DrawBitmapClipped(const OpBitmap* bitmap, const OpRect& source, OpPoint p);
	virtual void DrawBitmapClippedTransparent(const OpBitmap* bitmap, const OpRect& source, OpPoint p);
	virtual void DrawBitmapClippedAlpha(const OpBitmap* bitmap, const OpRect& source, OpPoint p);
	virtual BOOL DrawBitmapClippedOpacity(const OpBitmap* bitmap, const OpRect& source, OpPoint p, int opacity);
	virtual void DrawBitmapScaled(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest);
	virtual void DrawBitmapScaledTransparent(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest);
	virtual void DrawBitmapScaledAlpha(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest);
	virtual OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale);
	virtual OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale, UINT32 bitmapWidth, UINT32 bitmapHeight);
	
	virtual void InvertRect(const OpRect& rect){if (originalPainter) originalPainter->InvertRect(rect);}
	virtual void InvertBorderRect(const OpRect& rect, int border){if (originalPainter) originalPainter->InvertBorderRect(rect, border);}
	virtual void InvertBorderEllipse(const OpRect& rect, int border){if (originalPainter) originalPainter->InvertBorderEllipse(rect, border);}
	virtual void InvertBorderPolygon(const OpPoint* point_array, int points, int border){if (originalPainter) originalPainter->InvertBorderPolygon(point_array, points, border);}


	virtual OpBitmap* CreateBitmapFromBackground(const OpRect& rect){if (originalPainter) return originalPainter->CreateBitmapFromBackground(rect); return NULL;}
	virtual BOOL IsUsingOffscreenbitmap(){if (originalPainter) return originalPainter->IsUsingOffscreenbitmap(); return FALSE;}
	virtual OpBitmap* GetOffscreenBitmap(){if (originalPainter) return originalPainter->GetOffscreenBitmap(); return NULL;}
	virtual void GetOffscreenBitmapRect(OpRect* rect){if (originalPainter) originalPainter->GetOffscreenBitmapRect(rect);}
	virtual OP_STATUS BeginOpacity(const OpRect& rect, UINT8 opacity){if (originalPainter) return originalPainter->BeginOpacity(rect, opacity); return OpStatus::ERR;}
	virtual void EndOpacity(){if (originalPainter) originalPainter->EndOpacity();}
	virtual OpFont* GetFont(){if (originalPainter) return originalPainter->GetFont(); return NULL;}
	virtual UINT32 GetColor(){if (originalPainter) return originalPainter->GetColor(); return color;}

	virtual BOOL Supports(SUPPORTS supports);

private:
	UINT32 color;
	OpBitmap* originalBitmap;
	OpPainter* originalPainter;
	OpBitmap* tempBitmap;
	OpPainter* tempPainter;
	BOOL originalBitmapDirty;
};

#endif // DISPLAY_SUPPORTS_BROKEN_ALPHA_PAINTER

#endif // !ALPHABITMAP_PAINTER_H

