#ifndef MAC_THREADED_PAINTER_H
#define MAC_THREADED_PAINTER_H

#ifdef THREADED_PAINTER
#include "adjunct/desktop_util/painter/ThreadedPainter.h"

class RectRetainer
{
public:
	RectRetainer(const Rect* in_rect) { rect = *in_rect; }
	~RectRetainer() {}
	operator Rect* () { return &rect; }
protected:
	Rect rect;
};
class RegionRetainer
{
public:
	RegionRetainer(RgnHandle in_rgn) { rgn = NewRgn(); CopyRgn(in_rgn, rgn); }
	~RegionRetainer() { DisposeRgn(rgn); }
	operator RgnHandle () { return rgn; }
protected:
	RgnHandle rgn;
};
class HIRectRetainer
{
public:
	HIRectRetainer(const HIRect * in_rect) { rect = *in_rect; }
	~HIRectRetainer() {}
	operator HIRect* () { return &rect; }
protected:
	HIRect rect;
};
class ThemeButtonDrawInfoRetainer
{
public:
	ThemeButtonDrawInfoRetainer(const ThemeButtonDrawInfo * in_info) { info = *in_info; }
	~ThemeButtonDrawInfoRetainer() {}
	operator ThemeButtonDrawInfo* () { return &info; }
protected:
	ThemeButtonDrawInfo info;
};
class ThemeTrackDrawInfoRetainer
{
public:
	ThemeTrackDrawInfoRetainer(const ThemeTrackDrawInfo * in_info) { info = *in_info; }
	~ThemeTrackDrawInfoRetainer() {}
	operator ThemeTrackDrawInfo* () { return &info; }
protected:
	ThemeTrackDrawInfo info;
};
class HIThemeTabPaneDrawInfoRetainer
{
public:
	HIThemeTabPaneDrawInfoRetainer(const HIThemeTabPaneDrawInfo * in_info) { info = *in_info; }
	~HIThemeTabPaneDrawInfoRetainer() {}
	operator HIThemeTabPaneDrawInfo* () { return &info; }
protected:
	HIThemeTabPaneDrawInfo info;
};
class HIThemeBackgroundDrawInfoRetainer
{
public:
	HIThemeBackgroundDrawInfoRetainer(const HIThemeBackgroundDrawInfo * in_info) { info = *in_info; }
	~HIThemeBackgroundDrawInfoRetainer() {}
	operator HIThemeBackgroundDrawInfo* () { return &info; }
protected:
	HIThemeBackgroundDrawInfo info;
};
class HIThemeTabDrawInfoRetainer
{
public:
	HIThemeTabDrawInfoRetainer(const HIThemeTabDrawInfo * in_info) { info = *in_info; }
	~HIThemeTabDrawInfoRetainer() {}
	operator HIThemeTabDrawInfo* () { return &info; }
protected:
	HIThemeTabDrawInfo info;
};

#define MAKE_MAC_FUNCTION_CLASS_1(name, ext, t1, i1, p1) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1) : PainterTask(painter), p1(p1) {} \
		virtual void Execute() { ((MacOpPainter*)painter)->name(p1); } \
		i1 p1; \
	};

#define MAKE_MAC_FUNCTION_CLASS_2(name, ext, t1, i1, p1, t2, i2, p2) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1, t2 p2) : PainterTask(painter), p1(p1), p2(p2) {} \
		virtual void Execute() { ((MacOpPainter*)painter)->name(p1, p2); } \
		i1 p1; i2 p2; \
	};

#define MAKE_MAC_FUNCTION_CLASS_3(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1, t2 p2, t3 p3) : PainterTask(painter), p1(p1), p2(p2), p3(p3) {} \
		virtual void Execute() { ((MacOpPainter*)painter)->name(p1, p2, p3); } \
		i1 p1; i2 p2; i3 p3; \
	};

#define MAKE_MAC_FUNCTION_CLASS_4(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1, t2 p2, t3 p3, t4 p4) : PainterTask(painter), p1(p1), p2(p2), p3(p3), p4(p4) {} \
		virtual void Execute() { ((MacOpPainter*)painter)->name(p1, p2, p3, p4); } \
		i1 p1; i2 p2; i3 p3; i4 p4; \
	};

class MacThreadedPainter : public ThreadedPainter
{
public:
	MacThreadedPainter(MacOpPainter* native_painter);

	// need to match the DesktopPainter vtable offset. temphack
#ifdef THREADED_PAINTER
	virtual void DummyDecrementReference() { }
	virtual void DummyIncrementReference() { }
	virtual void DummyGetAsyncClassOfFunction() { }
#endif

	// paint tasks needed
	MAKE_MAC_FUNCTION_CLASS_1(SetGraphicsContext,, CGrafPtr, CGrafPtr, port);
	MAKE_MAC_FUNCTION_CLASS_1(SetIsPrinter,, Boolean, Boolean, isPrinter);
	MAKE_MAC_FUNCTION_CLASS_3(PainterHIThemeDrawTabPane,, const HIRect *, HIRectRetainer, inRect, const HIThemeTabPaneDrawInfo *, HIThemeTabPaneDrawInfoRetainer, info, HIThemeOrientation, HIThemeOrientation, inOrientation);
	MAKE_MAC_FUNCTION_CLASS_3(PainterHIThemeDrawBackground,, const HIRect *, HIRectRetainer, inRect, const HIThemeBackgroundDrawInfo *, HIThemeBackgroundDrawInfoRetainer, info, HIThemeOrientation, HIThemeOrientation, inOrientation);
	MAKE_MAC_FUNCTION_CLASS_3(PainterHIThemeDrawTab,, const HIRect *, HIRectRetainer, inRect, const HIThemeTabDrawInfo *, HIThemeTabDrawInfoRetainer, info, HIThemeOrientation, HIThemeOrientation, inOrientation);

	MAKE_MAC_FUNCTION_CLASS_4(PainterDrawThemePopupArrow,, const Rect *, RectRetainer, rect, ThemeArrowOrientation, ThemeArrowOrientation, orientation,
		ThemePopupArrowSize, ThemePopupArrowSize, size, ThemeDrawState, ThemeDrawState, state)
	MAKE_MAC_FUNCTION_CLASS_2(PainterDrawThemeEditTextFrame,, const Rect *, RectRetainer, rect, ThemeDrawState, ThemeDrawState, state)
	MAKE_MAC_FUNCTION_CLASS_2(PainterDrawThemeListBoxFrame,, const Rect *, RectRetainer, rect, ThemeDrawState, ThemeDrawState, state)
	MAKE_MAC_FUNCTION_CLASS_2(PainterDrawThemeTabPane,, const Rect *, RectRetainer, rect, ThemeDrawState, ThemeDrawState, state)
	MAKE_MAC_FUNCTION_CLASS_3(PainterDrawThemeTab,, const Rect *, RectRetainer, rect, ThemeTabStyle, ThemeTabStyle, style, ThemeTabDirection, ThemeTabDirection, inDirection)
	MAKE_MAC_FUNCTION_CLASS_2(PainterDrawThemeWindowHeader,, const Rect *, RectRetainer, rect, ThemeDrawState, ThemeDrawState, state)
	MAKE_MAC_FUNCTION_CLASS_2(PainterDrawThemePlacard,, const Rect *, RectRetainer, rect, ThemeDrawState, ThemeDrawState, state)
	MAKE_MAC_FUNCTION_CLASS_2(PainterDrawThemePrimaryGroup,, const Rect *, RectRetainer, rect, ThemeDrawState, ThemeDrawState, state)
	MAKE_MAC_FUNCTION_CLASS_4(PainterDrawThemeButton,, const Rect *, RectRetainer, rect, ThemeButtonKind, ThemeButtonKind, kind, const ThemeButtonDrawInfo *, ThemeButtonDrawInfoRetainer, info,
		const ThemeButtonDrawInfo *, ThemeButtonDrawInfoRetainer, info2)
	MAKE_MAC_FUNCTION_CLASS_1(PainterDrawThemeTrack,, const ThemeTrackDrawInfo *, const ThemeTrackDrawInfo *, drawInfo)
	MAKE_MAC_FUNCTION_CLASS_2(PainterDrawThemeFocusRect,, const Rect *, RectRetainer, rect, Boolean, Boolean, inHasFocus)
	MAKE_MAC_FUNCTION_CLASS_2(PainterDrawThemeFocusRegion,, RgnHandle, RegionRetainer, inRegion, Boolean, Boolean, inHasFocus)
	class StrokeShapePainterTask : public PainterTask
	{
	public:
		StrokeShapePainterTask(DesktopPainter* painter, COLORREF stroke, COLORREF fill, const CGPoint* points, size_t count) : PainterTask(painter), stroke(stroke), fill(fill), count(count)
		{
			mpoints = new CGPoint[count];
			memcpy(mpoints, points, count * sizeof(CGPoint));
		}
		virtual ~StrokeShapePainterTask() { delete [] mpoints; }
		virtual void Execute() { ((MacOpPainter*)painter)->StrokeShape(stroke, fill, mpoints, count); }
		COLORREF stroke;
		COLORREF fill;
		CGPoint* mpoints;
		size_t count;
	};


	virtual void		SetGraphicsContext(CGrafPtr inPort);
	virtual CGrafPtr	GetGraphicsContext();
	virtual float		GetWinHeight();
	virtual void		GetPortBoundingRect(Rect &bounds);
	virtual void		SetIsPrinter(Boolean isPrinter);
	virtual Boolean		IsPrinter();

	virtual void		DrawBitmap(const DesktopOpBitmap* effect_bitmap, const MacOpBitmap* platform_bitmap, const OpRect& source, const OpRect& dest) {}
	virtual OP_STATUS	TileBitmap(const DesktopOpBitmap* effect_bitmap, const MacOpBitmap* platform_bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale, UINT32 bitmapWidth, UINT32 bitmapHeight) { return OpStatus::ERR; }

	virtual OSStatus PainterHIThemeDrawTabPane(const HIRect * inRect, const HIThemeTabPaneDrawInfo * inDrawInfo, HIThemeOrientation inOrientation);
	virtual OSStatus PainterHIThemeDrawBackground(const HIRect * inBounds, const HIThemeBackgroundDrawInfo * inDrawInfo, HIThemeOrientation inOrientation);
	virtual OSStatus PainterHIThemeDrawTab(const HIRect * inRect, const HIThemeTabDrawInfo * inDrawInfo, HIThemeOrientation inOrientation);

	virtual OSStatus PainterDrawThemePopupArrow(const Rect * bounds, ThemeArrowOrientation orientation, ThemePopupArrowSize size, ThemeDrawState state);
	virtual OSStatus PainterDrawThemeEditTextFrame(const Rect * inRect, ThemeDrawState inState);
	virtual OSStatus PainterDrawThemeListBoxFrame(const Rect * inRect, ThemeDrawState inState);
	virtual OSStatus PainterDrawThemeTabPane(const Rect * inRect, ThemeDrawState inState);
	virtual OSStatus PainterDrawThemeTab(const Rect * inRect, ThemeTabStyle inStyle, ThemeTabDirection inDirection);
	virtual OSStatus PainterDrawThemeWindowHeader(const Rect * inRect, ThemeDrawState inState);
	virtual OSStatus PainterDrawThemePlacard(const Rect * inRect, ThemeDrawState inState);
	virtual OSStatus PainterDrawThemePrimaryGroup(const Rect * inRect, ThemeDrawState inState);
	virtual OSStatus PainterDrawThemeButton(const Rect * inBounds, ThemeButtonKind inKind, const ThemeButtonDrawInfo * inNewDrawInfo, const ThemeButtonDrawInfo * inPrevInfo);
	virtual OSStatus PainterDrawThemeTrack(const ThemeTrackDrawInfo * drawInfo);
	virtual OSStatus PainterDrawThemeFocusRect(const Rect * inRect, Boolean inHasFocus);
	virtual OSStatus PainterDrawThemeFocusRegion(RgnHandle inRegion, Boolean inHasFocus);

	virtual void StrokeShape(COLORREF stroke, COLORREF fill, const CGPoint* points, size_t count);

	// Not really paint tasks, but the vtable might need them.
#ifdef UNIT_TEXT_OUT
	virtual void	FlushText() {}
#endif
	virtual void	Sync() {}
	virtual void	ClearRect() const { }

	void SetPainter(MacOpPainter* painter);

protected:
	CGrafPtr m_graf;
	Boolean mIsPrinter;
};

#endif // THREADED_PAINTER

#endif // MAC_THREADED_PAINTER_H
