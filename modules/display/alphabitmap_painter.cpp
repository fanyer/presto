/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DISPLAY_SUPPORTS_BROKEN_ALPHA_PAINTER

#include "modules/display/alphabitmap_painter.h"

AlphaBitmapPainter::AlphaBitmapPainter() : color(0), originalBitmap(NULL), originalPainter(NULL), tempBitmap(NULL), tempPainter(NULL), originalBitmapDirty(FALSE)
{}

AlphaBitmapPainter::~AlphaBitmapPainter()
{
		if (tempBitmap && tempPainter)
				tempBitmap->ReleasePainter();
		OP_DELETE(tempBitmap);
		if (originalBitmap && originalPainter)
				originalBitmap->ReleasePainter();
}

OP_STATUS AlphaBitmapPainter::Construct(OpBitmap* bitmap, BOOL bitmapContainsData)
{
		originalBitmap = bitmap;
		originalPainter = bitmap->GetPainter();
		if (!originalPainter)
				return OpStatus::ERR;
		originalBitmapDirty = bitmapContainsData;
		RETURN_IF_ERROR(OpBitmap::Create(&tempBitmap, bitmap->Width(), bitmap->Height(), FALSE, TRUE, 0, 0, TRUE));
		tempPainter = tempBitmap->GetPainter();
		if (!tempPainter)
				return OpStatus::ERR;
		// Clear the bitmap to transparent black
		tempPainter->SetColor(0);
		tempPainter->FillRect(OpRect(0,0,bitmap->Width(), bitmap->Height()));
		tempBitmap->ReleasePainter();
		return OpStatus::OK;
}

void AlphaBitmapPainter::PrepareAlphaRendering()
{
		if (!originalPainter)
				return;
		tempPainter = tempBitmap->GetPainter();
		if (!tempPainter)
				return;
		OpRect clip;
		originalPainter->GetClipRect(&clip);
		tempPainter->SetClipRect(clip);
		tempPainter->SetColor(0xffffffff);
		tempPainter->SetFont(originalPainter->GetFont());
}

void AlphaBitmapPainter::FinishAlphaRendering()
{
		if (!tempPainter)
				return;
		tempPainter->RemoveClipRect();
		tempBitmap->ReleasePainter();
		tempPainter = NULL;
		if (!originalPainter)
				return;
		// Copy the data to the original bitmap
		UINT32* linedata = OP_NEWA(UINT32, originalBitmap->Width()*(originalBitmapDirty?2:1));
		if (!linedata)
				return; // FIXME: OOM
		int alpha = color>>24;
		int red = (color>>16)&0xff;
		int green = (color>>8)&0xff;
		int blue = color&0xff;
		OpFont* fnt = originalPainter->GetFont();
		originalBitmap->ReleasePainter();
		originalPainter = NULL;
		for (UINT32 line = 0; line < originalBitmap->Height(); ++line)
		{
				if (tempBitmap->GetLineData(linedata, line))
				{
						if (originalBitmapDirty)
						{
							if (originalBitmap->GetLineData(linedata+originalBitmap->Width(), line))
							{
								for (UINT32 x = 0; x < originalBitmap->Width(); ++x)
								{
									int ra = linedata[x]&0xff;
									int sa = (linedata[x+originalBitmap->Width()]>>24)&0xff;
									int sr = (linedata[x+originalBitmap->Width()]>>16)&0xff;
									int sg = (linedata[x+originalBitmap->Width()]>>8)&0xff;
									int sb = (linedata[x+originalBitmap->Width()])&0xff;
#ifdef USE_PREMULTIPLIED_ALPHA
									if (ra)
									{
										int dr = red;
										int dg = green;
										int db = blue;
										if (ra < 255)
										{
											// Add ra to the premultiplication
											++ra;
											dr = (red*ra)>>8;
											dg = (green*ra)>>8;
											db = (blue*ra)>>8;
											ra = (ra*alpha)>>8;
										}
										if (ra == 255)
										{
											sr = dr;
											sg = dg;
											sb = db;
											sa = ra;
										}
										else if (ra > 0)
										{
											sr = ((sr*(256-ra))>>8)+dr;
											sg = ((sg*(256-ra))>>8)+dg;
											sb = ((sb*(256-ra))>>8)+db;
											sa = ((sa*(256-ra))>>8)+ra;
										}
									}
									linedata[x] = (sa<<24) | (sr<<16) | (sg<<8) | sb;
#else // !USE_PREMULTIPLIED_ALPHA
									linedata[x] = (((alpha*ra + sa*(255-ra))/255)<<24)|
												((red*ra + sr*(255-ra))/255)<<16|
												((green*ra + sg*(255-ra))/255)<<8|
												((blue*ra + sb*(255-ra))/255);
#endif // !USE_PREMULTIPLIED_ALPHA
								}
							}
						}
						else
						{
							for (UINT32 x = 0; x < originalBitmap->Width(); ++x)
							{
								int ra = linedata[x]&0xff;
								linedata[x] = (((alpha*ra)/255)<<24)|
										(red<<16)|
										(green<<8)|
										(blue);
							}
						}
				}
				originalBitmap->AddLine(linedata, line);
		}
		OP_DELETEA(linedata);
		originalPainter = originalBitmap->GetPainter();
		OP_ASSERT(originalPainter);
		originalPainter->SetColor(color);
		originalPainter->SetFont(fnt);
		
		tempPainter = tempBitmap->GetPainter();
		if (tempPainter && originalPainter)
		{
			// Clear to black again
			OpRect clip;
			originalPainter->GetClipRect(&clip);
			tempPainter->SetColor(0);
			tempPainter->FillRect(clip);
		}

}


void AlphaBitmapPainter::DrawRect(const OpRect& rect, UINT32 width /* = 1 */)
{
		if (originalPainter)
		{
			originalPainter->DrawRect(rect, width);
			originalBitmapDirty = TRUE;
		}
}

void AlphaBitmapPainter::FillRect(const OpRect& rect)
{
		if (originalPainter)
		{
			originalPainter->FillRect(rect);
			originalBitmapDirty = TRUE;
		}
}
void AlphaBitmapPainter::ClearRect(const OpRect& rect)
{
		if (originalPainter)
		{
			originalPainter->ClearRect(rect);
			originalBitmapDirty = TRUE;
		}
}

void AlphaBitmapPainter::DrawEllipse(const OpRect& rect, UINT32 width /* = 1 */)
{
		if (originalPainter)
		{
			originalPainter->DrawEllipse(rect, width);
			originalBitmapDirty = TRUE;
		}
}

void AlphaBitmapPainter::FillEllipse(const OpRect& rect)
{
		if (originalPainter)
		{
			originalPainter->FillEllipse(rect);
			originalBitmapDirty = TRUE;
		}
}

void AlphaBitmapPainter::DrawPolygon(const OpPoint* points, int count, UINT32 width /* = 1 */)
{
		if (originalPainter)
		{
			originalPainter->DrawPolygon(points, count, width);
			originalBitmapDirty = TRUE;
		}
}

void AlphaBitmapPainter::DrawLine(const OpPoint& from, UINT32 length, BOOL horizontal, UINT32 width)
{
		if (originalPainter)
		{
			originalPainter->DrawLine(from, length, horizontal, width);
			originalBitmapDirty = TRUE;
		}
}

void AlphaBitmapPainter::DrawLine(const OpPoint& from, const OpPoint& to, UINT32 width)
{
		if (originalPainter)
		{
			originalPainter->DrawLine(from, to, width);
			originalBitmapDirty = TRUE;
		}
}

void AlphaBitmapPainter::DrawString(const OpPoint& pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width)
{
		PrepareAlphaRendering();
		if (tempPainter)
			tempPainter->DrawString(pos, text, len, extra_char_spacing, word_width);
		FinishAlphaRendering();
		originalBitmapDirty = TRUE;
}

void AlphaBitmapPainter::DrawBitmapClipped(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
		if (originalPainter)
		{
			originalPainter->DrawBitmapClipped(bitmap, source, p);
			originalBitmapDirty = TRUE;
		}
}
void AlphaBitmapPainter::DrawBitmapClippedTransparent(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
		if (originalPainter)
		{
			originalPainter->DrawBitmapClippedTransparent(bitmap, source, p);
			originalBitmapDirty = TRUE;
		}
}
void AlphaBitmapPainter::DrawBitmapClippedAlpha(const OpBitmap* bitmap, const OpRect& source, OpPoint p)
{
		if (originalPainter)
		{
			originalPainter->DrawBitmapClippedAlpha(bitmap, source, p);
			originalBitmapDirty = TRUE;
		}
}
BOOL AlphaBitmapPainter::DrawBitmapClippedOpacity(const OpBitmap* bitmap, const OpRect& source, OpPoint p, int opacity)
{
		if (originalPainter)
		{
			originalBitmapDirty = TRUE;
			return originalPainter->DrawBitmapClippedOpacity(bitmap, source, p, opacity);
		}
		return FALSE;
}
void AlphaBitmapPainter::DrawBitmapScaled(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest)
{
		if (originalPainter)
		{
			originalPainter->DrawBitmapScaled(bitmap, source, dest);
			originalBitmapDirty = TRUE;
		}
}
void AlphaBitmapPainter::DrawBitmapScaledTransparent(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest)
{
		if (originalPainter)
		{
			originalPainter->DrawBitmapScaledTransparent(bitmap, source, dest);
			originalBitmapDirty = TRUE;
		}
}
void AlphaBitmapPainter::DrawBitmapScaledAlpha(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest)
{
		if (originalPainter)
		{
			originalPainter->DrawBitmapScaledAlpha(bitmap, source, dest);
			originalBitmapDirty = TRUE;
		}
}
OP_STATUS AlphaBitmapPainter::DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale)
{
		if (originalPainter)
		{
			originalBitmapDirty = TRUE;
			return originalPainter->DrawBitmapTiled(bitmap, offset, dest, scale);
		}
		return OpStatus::ERR;
}
OP_STATUS AlphaBitmapPainter::DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale, UINT32 bitmapWidth, UINT32 bitmapHeight)
{
		if (originalPainter)
		{
			originalBitmapDirty = TRUE;
			return originalPainter->DrawBitmapTiled(bitmap, offset, dest, scale, bitmapWidth, bitmapHeight);
		}
		return OpStatus::ERR;
}

BOOL AlphaBitmapPainter::Supports(SUPPORTS supports)
{
	// We cannot blindly claim transitive support for everything
	switch (supports) {
	case SUPPORTS_GETSCREEN:
	case SUPPORTS_TRANSPARENT:
	case SUPPORTS_ALPHA:
	case SUPPORTS_TILING:
		if (originalPainter)
			return originalPainter->Supports(supports);
		else
			return FALSE;
	// SUPPORTS_PLATFORM is intentionally left out
	default:
		return FALSE;
	}
}

#endif // DISPLAY_SUPPORTS_BROKEN_ALPHA_PAINTER
