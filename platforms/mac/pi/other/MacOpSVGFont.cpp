/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "platforms/mac/pi/other/MacOpSVGFont.h"
#include "modules/svg/src/OpBpath.h"

struct OpGlyphCallbackStruct
{
	Boolean first;
	OpBpath* path;
};

pascal OSStatus MoveToProc(const Float32Point *pt, void *callBackDataPtr)
{
	OpGlyphCallbackStruct *data = (OpGlyphCallbackStruct*)callBackDataPtr;

	if(!data)
		return -1;

	OpBpath* p = data->path;
	if(p)
	{
		p->MoveTo(pt->x, pt->y, FALSE);
		return noErr;
	}

	return -1;
}

pascal OSStatus LineToProc(const Float32Point *pt, void *callBackDataPtr)
{
	OpGlyphCallbackStruct *data = (OpGlyphCallbackStruct*)callBackDataPtr;

	if(!data)
		return -1;

	OpBpath* p = data->path;
	if(p)
	{
		p->LineTo(pt->x, pt->y, FALSE);
		return noErr;
	}

	return -1;
}

pascal OSStatus CurveToProc(const Float32Point *pt1, const Float32Point *pt2, const Float32Point *pt3, void *callBackDataPtr)
{
	OpGlyphCallbackStruct *data = (OpGlyphCallbackStruct*)callBackDataPtr;

	if(!data)
		return -1;

	OpBpath* p = data->path;
	if(p)
	{
		p->CubicCurveTo(pt1->x, pt1->y, pt2->x, pt2->y, pt3->x, pt3->y, TRUE, FALSE);
		return noErr;
	}

	return -1;
}

pascal OSStatus ClosePathProc(void * callBackDataPtr)
{
	OpGlyphCallbackStruct *data = (OpGlyphCallbackStruct*)callBackDataPtr;

	if(!data)
		return -1;

	OpBpath* p = data->path;
	if(p)
	{
		p->Close();
		return noErr;
	}

	return -1;
}

pascal OSStatus QuadNewPathProc(void * callBackDataPtr)
{
	OpGlyphCallbackStruct *data = (OpGlyphCallbackStruct*)callBackDataPtr;

	if(!data)
		return -1;

	data->first = true;

	return noErr;
}

pascal OSStatus QuadLineProc(const Float32Point *pt1, const Float32Point *pt2, void *callBackDataPtr)
{
	OpGlyphCallbackStruct *data = (OpGlyphCallbackStruct*)callBackDataPtr;

	if(!data)
		return -1;

	OpBpath* p = data->path;
	if(p)
	{
		if(pt1->x == pt2->x && pt1->y == pt2->y)
		{
			// Useless line
			return noErr;
		}
		if(data->first)
		{
			p->MoveTo(pt1->x, pt1->y, FALSE);
			data->first = false;
		}


		p->LineTo(pt2->x, pt2->y, FALSE);
		return noErr;
	}

	return -1;
}

pascal OSStatus QuadCurveProc(const Float32Point *pt1, const Float32Point *controlPt, const Float32Point *pt2, void *callBackDataPtr)
{
	OpGlyphCallbackStruct *data = (OpGlyphCallbackStruct*)callBackDataPtr;

	if(!data)
		return -1;

	OpBpath* p = data->path;
	if(p)
	{
		if(data->first)
		{
			p->MoveTo(pt1->x, pt1->y, FALSE);
			data->first = false;
		}

		p->QuadraticCurveTo(controlPt->x, controlPt->y, pt2->x, pt2->y, FALSE, FALSE);
		return noErr;
	}

	return -1;
}

pascal OSStatus QuadClosePathProc(void * callBackDataPtr)
{
	OpGlyphCallbackStruct *data = (OpGlyphCallbackStruct*)callBackDataPtr;

	if(!data)
		return -1;

	OpBpath* p = data->path;
	if(p)
	{
		p->Close();
		return noErr;
	}

	return -1;
}

MacOpSVGFont::MacOpSVGFont(OpFont* font)
: SVGSystemFont(font)
, mMoveToUPP(NULL)
, mLineToUPP(NULL)
, mCurveToUPP(NULL)
, mClosePathUPP(NULL)
, mQuadNewPathUPP(NULL)
, mQuadLineUPP(NULL)
, mQuadCurveUPP(NULL)
, mQuadClosePathUPP(NULL)
, mCurveType(0)
#ifndef USE_CG_SHOW_GLYPHS
, mGlyphInfoArray(NULL)
#endif
{
	mTransform.LoadIdentity();

	if(noErr == ATSUGetNativeCurveType(((MacOpFont*)m_font)->GetATSUStyle(1.0), &mCurveType))
	{
		if(kATSQuadCurveType != mCurveType)
		{
			mMoveToUPP = NewATSCubicMoveToUPP(MoveToProc);
			mLineToUPP = NewATSCubicLineToUPP(LineToProc);
			mCurveToUPP = NewATSCubicCurveToUPP(CurveToProc);
			mClosePathUPP = NewATSCubicClosePathUPP(ClosePathProc);
		}
		else
		{
			mQuadNewPathUPP = NewATSQuadraticNewPathUPP(QuadNewPathProc);
			mQuadLineUPP = NewATSQuadraticLineUPP(QuadLineProc);
			mQuadCurveUPP = NewATSQuadraticCurveUPP(QuadCurveProc);
			mQuadClosePathUPP = NewATSQuadraticClosePathUPP(QuadClosePathProc);
		}
	}
	else
	{
		mMoveToUPP = NewATSCubicMoveToUPP(MoveToProc);
		mLineToUPP = NewATSCubicLineToUPP(LineToProc);
		mCurveToUPP = NewATSCubicCurveToUPP(CurveToProc);
		mClosePathUPP = NewATSCubicClosePathUPP(ClosePathProc);
	}

}

MacOpSVGFont::~MacOpSVGFont()
{
	if(mMoveToUPP)
		DisposeATSCubicMoveToUPP(mMoveToUPP);
	if(mLineToUPP)
		DisposeATSCubicLineToUPP(mLineToUPP);
	if(mCurveToUPP)
		DisposeATSCubicCurveToUPP(mCurveToUPP);
	if(mClosePathUPP)
		DisposeATSCubicClosePathUPP(mClosePathUPP);
	if(mQuadNewPathUPP)
		DisposeATSQuadraticNewPathUPP(mQuadNewPathUPP);
	if(mQuadLineUPP)
		DisposeATSQuadraticLineUPP(mQuadLineUPP);
	if(mQuadCurveUPP)
		DisposeATSQuadraticCurveUPP(mQuadCurveUPP);
	if(mQuadClosePathUPP)
		DisposeATSQuadraticClosePathUPP(mQuadClosePathUPP);
#ifndef USE_CG_SHOW_GLYPHS
	if(mGlyphInfoArray)
		free(mGlyphInfoArray);
#endif
}

#ifdef SVG_DEBUG_SHOW_FONTNAMES
const uni_char*	MacOpSVGFont::GetFontName()
{
	if(m_font)
	{
		const char* name = ((MacOpFont*)m_font)->GetFontName();
		UINT32 len = strlen(name);
		return TMP_DOUBLEBYTE(name, len);
	}

	return NULL;
}
#endif

OpBpath *MacOpSVGFont::GetGlyphOutline(uni_char uc)
{
	static OpGlyphCallbackStruct data;
	data.path = new OpBpath();

	if(m_font)
	{
		GlyphID glyph = 0;
#ifdef USE_CG_SHOW_GLYPHS
		glyph = ((MacOpFont*)m_font)->GetGlyphID((UniChar)uc);
#else
	#if 0
		if(!mGlyphInfoArray)
		{
			((MacOpFont*)m_font)->GetGlyphs(m_text, mGlyphInfoArray);
		}
		if(mGlyphInfoArray)
			glyph = mGlyphInfoArray->glyphs[gnum].glyphID;
	#endif
#endif
		OSStatus result = noErr;

		if(kATSQuadCurveType != mCurveType)
			ATSUGlyphGetCubicPaths(((MacOpFont*)m_font)->GetATSUStyle(1.0), glyph, mMoveToUPP, mLineToUPP, mCurveToUPP, mClosePathUPP, &data, &result);
		else
			ATSUGlyphGetQuadraticPaths(((MacOpFont*)m_font)->GetATSUStyle(1.0), glyph, mQuadNewPathUPP, mQuadLineUPP, mQuadCurveUPP, mQuadClosePathUPP, &data, &result);

	}

	return data.path;
}

SVGnumber MacOpSVGFont::GetGlyphAdvance(uni_char uc)
{
	ATSGlyphIdealMetrics metrics[1];
	GlyphID glyphs[1];

	if(m_font)
	{
		GlyphID glyph = 0;
#ifdef USE_CG_SHOW_GLYPHS
		glyph = ((MacOpFont*)m_font)->GetGlyphID((UniChar)uc);
#else
	#if 0
		if(!mGlyphInfoArray)
		{
			((MacOpFont*)m_font)->GetGlyphs(m_text, mGlyphInfoArray);
		}
		if(mGlyphInfoArray)
			glyph = mGlyphInfoArray->glyphs[gnum].glyphID;
	#endif
#endif
		OSStatus err = noErr;

		glyphs[0] = glyph;

		err = ATSUGlyphGetIdealMetrics(((MacOpFont*)m_font)->GetATSUStyle(1.0), 1, glyphs, 0, metrics);
		if(err == noErr)
		{
			return metrics[0].advance.x;
		}
	}

	return 0;
}

void MacOpSVGFont::SetSize(SVGnumber size)
{
	SVGnumber scaledSize = size / ((MacOpFont*)m_font)->mSize;
	SVGTransform scale;
	mTransform.LoadScale( scaledSize, scaledSize );
}

#endif // SVG_SUPPORT
