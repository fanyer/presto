/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/subclasses/MacThreadedPainter.h"

#ifdef THREADED_PAINTER

MacThreadedPainter::MacThreadedPainter(MacOpPainter* native_painter)
 : ThreadedPainter(native_painter)
{
	if (native_painter)
		native_painter->BeginPaint();
	m_graf = ((MacOpPainter*)native_painter)->GetGraphicsContext();
	mIsPrinter = false;
}

void MacThreadedPainter::SetGraphicsContext(CGrafPtr inPort)
{
	m_graf = inPort;
	AddPainterTask(new SetGraphicsContextPainterTask(m_native_painter, inPort));
}

CGrafPtr MacThreadedPainter::GetGraphicsContext()
{
	return m_graf;
}

void MacThreadedPainter::SetIsPrinter(Boolean isPrinter)
{
	mIsPrinter = isPrinter;
	AddPainterTask(new SetIsPrinterPainterTask(m_native_painter, isPrinter));
}

Boolean	MacThreadedPainter::IsPrinter()
{
	return mIsPrinter;
}

float MacThreadedPainter::GetWinHeight()
{
// reentrant, but should be fine.
	return ((MacOpPainter*)m_native_painter)->GetWinHeight();
}

void MacThreadedPainter::GetPortBoundingRect(Rect &bounds)
{
// reentrant, but should be fine.
	return ((MacOpPainter*)m_native_painter)->GetPortBoundingRect(bounds);
}

OSStatus MacThreadedPainter::PainterHIThemeDrawTabPane(const HIRect * inRect, const HIThemeTabPaneDrawInfo * inDrawInfo, HIThemeOrientation inOrientation)
{
	AddPainterTask(new PainterHIThemeDrawTabPanePainterTask(m_native_painter, inRect, inDrawInfo, inOrientation));
	return noErr;
}

OSStatus MacThreadedPainter::PainterHIThemeDrawBackground(const HIRect * inBounds, const HIThemeBackgroundDrawInfo * inDrawInfo, HIThemeOrientation inOrientation)
{
	AddPainterTask(new PainterHIThemeDrawBackgroundPainterTask(m_native_painter, inBounds, inDrawInfo, inOrientation));
	return noErr;
}

OSStatus MacThreadedPainter::PainterHIThemeDrawTab(const HIRect * inRect, const HIThemeTabDrawInfo * inDrawInfo, HIThemeOrientation inOrientation)
{
	AddPainterTask(new PainterHIThemeDrawTabPainterTask(m_native_painter, inRect, inDrawInfo, inOrientation));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemePopupArrow(const Rect * bounds, ThemeArrowOrientation orientation, ThemePopupArrowSize size, ThemeDrawState state)
{
	AddPainterTask(new PainterDrawThemePopupArrowPainterTask(m_native_painter, bounds, orientation, size, state));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemeEditTextFrame(const Rect * inRect, ThemeDrawState inState)
{
	AddPainterTask(new PainterDrawThemeEditTextFramePainterTask(m_native_painter, inRect, inState));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemeListBoxFrame(const Rect * inRect, ThemeDrawState inState)
{
	AddPainterTask(new PainterDrawThemeListBoxFramePainterTask(m_native_painter, inRect, inState));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemeTabPane(const Rect * inRect, ThemeDrawState inState)
{
	AddPainterTask(new PainterDrawThemeTabPanePainterTask(m_native_painter, inRect, inState));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemeTab(const Rect * inRect, ThemeTabStyle inStyle, ThemeTabDirection inDirection)
{
	AddPainterTask(new PainterDrawThemeTabPainterTask(m_native_painter, inRect, inStyle, inDirection));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemeWindowHeader(const Rect * inRect, ThemeDrawState inState)
{
	AddPainterTask(new PainterDrawThemeWindowHeaderPainterTask(m_native_painter, inRect, inState));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemePlacard(const Rect * inRect, ThemeDrawState inState)
{
	AddPainterTask(new PainterDrawThemePlacardPainterTask(m_native_painter, inRect, inState));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemePrimaryGroup(const Rect * inRect, ThemeDrawState inState)
{
	AddPainterTask(new PainterDrawThemePrimaryGroupPainterTask(m_native_painter, inRect, inState));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemeButton(const Rect * inBounds, ThemeButtonKind inKind, const ThemeButtonDrawInfo * inNewDrawInfo, const ThemeButtonDrawInfo * inPrevInfo)
{
	AddPainterTask(new PainterDrawThemeButtonPainterTask(m_native_painter, inBounds, inKind, inNewDrawInfo, inPrevInfo));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemeTrack(const ThemeTrackDrawInfo * drawInfo)
{
	AddPainterTask(new PainterDrawThemeTrackPainterTask(m_native_painter, drawInfo));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemeFocusRect(const Rect * inRect, Boolean inHasFocus)
{
	AddPainterTask(new PainterDrawThemeFocusRectPainterTask(m_native_painter, inRect, inHasFocus));
	return noErr;
}

OSStatus MacThreadedPainter::PainterDrawThemeFocusRegion(RgnHandle inRegion, Boolean inHasFocus)
{
	AddPainterTask(new PainterDrawThemeFocusRegionPainterTask(m_native_painter, inRegion, inHasFocus));
	return noErr;
}

void MacThreadedPainter::StrokeShape(COLORREF stroke, COLORREF fill, const CGPoint* points, size_t count)
{
	AddPainterTask(new StrokeShapePainterTask(m_native_painter, stroke, fill, points, count));
}

void MacThreadedPainter::SetPainter(MacOpPainter* painter)
{
	if (m_native_painter)
	{
		AddPainterTask(new RemoveClipRectPainterTask(m_native_painter));
		m_native_painter->DecrementReference();
	}
	m_native_painter = painter;
	if (painter) {
		painter->IncrementReference();
		painter->BeginPaint();
	}
}

#endif // THREADED_PAINTER
