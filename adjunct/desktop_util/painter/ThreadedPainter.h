#ifndef THREADED_PAINTER_H
#define THREADED_PAINTER_H

#include "modules/pi/OpPainter.h"
#include "adjunct/desktop_pi/DesktopTaskQueue.h"
#include "adjunct/desktop_pi/DesktopPainter.h"
#include "adjunct/desktop_util/bitmap/desktop_opbitmap.h"

/* This class implements a threaded OpPainter. All you need is pthread support.
 * It DOES make the assumtion that the painter can respond to GetDPI, IsUsingOffscreenbitmap,
 * GetOffscreenBitmapRect & Supports out-of-line and from a different thread than the other calls.
 * (this is so that these calls don't have to wait for the task queue to flush)
 * It does not require that the painter is reentrant.
 * BE CAREFUL IF YOU ARE TOUCHING GLOBALS IN YOUR PAINTER. YOU WILL BE RUNNING ON A THREAD.
 * It also requires refcounting bitmaps (see rc_bitmap.h).
 *
 * Procedure is pretty straightforward: You create a native painter, and construct a ThreadedPainter with that.
 * When you destroy the ThreadedPainter, it will perform remaining drawing tasks and delete your native painter.
 * You should NOT create this painter if you are on a single-core system: You will just slow things down.
 * Also, if your graphics system is already threaded, this will probably not help.
 *
 * There may be problems with SetFont & InvertBorderPolygon (parameter data pointer may go away).
 */

class PainterTask : public OpTask
{
public:
	PainterTask(DesktopPainter* painter)
		: painter(painter)
		{ painter->IncrementReference(); }
	~PainterTask() { painter->DecrementReference(); }
	DesktopPainter* painter;
};

class BitmapRetainer
{
public:
	BitmapRetainer(const OpBitmap* bitmap) : m_bitmap(bitmap ? ((DesktopOpBitmap*)bitmap)->Clone() : NULL) { }
	~BitmapRetainer() { OP_DELETE(m_bitmap); }
	operator const OpBitmap* () { return m_bitmap; }
protected:
	DesktopOpBitmap* m_bitmap;
};

class StringRetainer
{
public:
	StringRetainer(const uni_char* string) { m_string.Set(string); }
	~StringRetainer() {}
	operator uni_char* () { return m_string.CStr(); }
protected:
	OpString m_string;
};

#define MAKE_FUNCTION_CLASS_0(name, ext) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter) : PainterTask(painter) {} \
		virtual void Execute() { painter->name(); } \
	};
#define MAKE_FUNCTION_TASK_0(name, ext, return_t, return_v) \
	MAKE_FUNCTION_CLASS_0(name, ext) \
	virtual return_t name() { AddPainterTask(new name ## PainterTask ## ext(m_native_painter)); return return_v; }


#define MAKE_FUNCTION_CLASS_1(name, ext, t1, i1, p1) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1) : PainterTask(painter), p1(p1) {} \
		virtual void Execute() { painter->name(p1); } \
		i1 p1; \
	};
#define MAKE_FUNCTION_TASK_1(name, ext, t1, i1, p1, return_t, return_v) \
	MAKE_FUNCTION_CLASS_1(name, ext, t1, i1, p1) \
	virtual return_t name(t1 p1) { AddPainterTask(new name ## PainterTask ## ext(m_native_painter, p1)); return return_v; }


#define MAKE_FUNCTION_CLASS_2(name, ext, t1, i1, p1, t2, i2, p2) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1, t2 p2) : PainterTask(painter), p1(p1), p2(p2) {} \
		virtual void Execute() { painter->name(p1, p2); } \
		i1 p1; i2 p2; \
	};
#define MAKE_FUNCTION_TASK_2(name, ext, t1, i1, p1, t2, i2, p2, return_t, return_v) \
	MAKE_FUNCTION_CLASS_2(name, ext, t1, i1, p1, t2, i2, p2) \
	virtual return_t name(t1 p1, t2 p2) { AddPainterTask(new name ## PainterTask ## ext(m_native_painter, p1, p2)); return return_v; }


#define MAKE_FUNCTION_CLASS_3(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1, t2 p2, t3 p3) : PainterTask(painter), p1(p1), p2(p2), p3(p3) {} \
		virtual void Execute() { painter->name(p1, p2, p3); } \
		i1 p1; i2 p2; i3 p3; \
	};
#define MAKE_FUNCTION_TASK_3(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, return_t, return_v) \
	MAKE_FUNCTION_CLASS_3(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3) \
	virtual return_t name(t1 p1, t2 p2, t3 p3) { AddPainterTask(new name ## PainterTask ## ext(m_native_painter, p1, p2, p3)); return return_v; }


#define MAKE_FUNCTION_CLASS_4(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1, t2 p2, t3 p3, t4 p4) : PainterTask(painter), p1(p1), p2(p2), p3(p3), p4(p4) {} \
		virtual void Execute() { painter->name(p1, p2, p3, p4); } \
		i1 p1; i2 p2; i3 p3; i4 p4; \
	};
#define MAKE_FUNCTION_TASK_4(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4, return_t, return_v) \
	MAKE_FUNCTION_CLASS_4(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4) \
	virtual return_t name(t1 p1, t2 p2, t3 p3, t4 p4) { AddPainterTask(new name ## PainterTask ## ext(m_native_painter, p1, p2, p3, p4)); return return_v; }


#define MAKE_FUNCTION_CLASS_5(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4, t5, i5, p5) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) : PainterTask(painter), p1(p1), p2(p2), p3(p3), p4(p4), p5(p5) {} \
		virtual void Execute() { painter->name(p1, p2, p3, p4, p5); } \
		i1 p1; i2 p2; i3 p3; i4 p4; i5 p5; \
	};
#define MAKE_FUNCTION_TASK_5(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4, t5, i5, p5, return_t, return_v) \
	MAKE_FUNCTION_CLASS_5(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4, t5, i5, p5) \
	virtual return_t name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) { AddPainterTask(new name ## PainterTask ## ext(m_native_painter, p1, p2, p3, p4, p5)); return return_v; }


#define MAKE_FUNCTION_CLASS_6(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4, t5, i5, p5, t6, i6, p6) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) : PainterTask(painter), p1(p1), p2(p2), p3(p3), p4(p4), p5(p5), p6(p6) {} \
		virtual void Execute() { painter->name(p1, p2, p3, p4, p5, p6); } \
		i1 p1; i2 p2; i3 p3; i4 p4; i5 p5; i6 p6; \
	};
#define MAKE_FUNCTION_TASK_6(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4, t5, i5, p5, t6, i6, p6, return_t, return_v) \
	MAKE_FUNCTION_CLASS_6(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4, t5, i5, p5, t6, i6, p6) \
	virtual return_t name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) { AddPainterTask(new name ## PainterTask ## ext(m_native_painter, p1, p2, p3, p4, p5, p6)); return return_v; }

#define MAKE_FUNCTION_CLASS_7(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4, t5, i5, p5, t6, i6, p6, t7, i7, p7) \
	class name ## PainterTask ## ext : public PainterTask \
	{ \
	public: \
		name ## PainterTask ## ext(DesktopPainter* painter, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) : PainterTask(painter), p1(p1), p2(p2), p3(p3), p4(p4), p5(p5), p6(p6), p7(p7) {} \
		virtual void Execute() { painter->name(p1, p2, p3, p4, p5, p6, p7); } \
		i1 p1; i2 p2; i3 p3; i4 p4; i5 p5; i6 p6; i7 p7; \
	};
#define MAKE_FUNCTION_TASK_7(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4, t5, i5, p5, t6, i6, p6, t7, i7, p7, return_t, return_v) \
	MAKE_FUNCTION_CLASS_7(name, ext, t1, i1, p1, t2, i2, p2, t3, i3, p3, t4, i4, p4, t5, i5, p5, t6, i6, p6, t7, i7, p7) \
	virtual return_t name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) { AddPainterTask(new name ## PainterTask ## ext(m_native_painter, p1, p2, p3, p4, p5, p6, 7)); return return_v; }

class ThreadedPainter : public OpPainter
{
public:
	ThreadedPainter(DesktopPainter* native_painter);
	virtual ~ThreadedPainter();
	MAKE_FUNCTION_CLASS_2(GetDPI,, UINT32*, UINT32*, horizontal, UINT32*, UINT32*, vertical)
	void GetDPI(UINT32* horizontal, UINT32* vertical);
	MAKE_FUNCTION_CLASS_4(SetColor,, UINT8, UINT8, red, UINT8, UINT8, green, UINT8, UINT8, blue, UINT8, UINT8, alpha)
	virtual void SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha);
	MAKE_FUNCTION_CLASS_1(SetFont,, OpFont*, OpFont*, font)
	virtual void SetFont(OpFont* font);
	MAKE_FUNCTION_CLASS_1(SetClipRect,, const OpRect &, OpRect, rect)
	virtual OP_STATUS SetClipRect(const OpRect & rect);
	MAKE_FUNCTION_CLASS_0(RemoveClipRect,)
	virtual void RemoveClipRect();
	virtual void GetClipRect(OpRect* rect);
	MAKE_FUNCTION_TASK_1(DrawRect,, const OpRect &, OpRect, rect, void,)
	MAKE_FUNCTION_TASK_1(FillRect,, const OpRect &, OpRect, rect, void,)
	MAKE_FUNCTION_TASK_1(DrawEllipse,, const OpRect &, OpRect, rect, void,)
	MAKE_FUNCTION_TASK_1(FillEllipse,, const OpRect &, OpRect, rect, void,)
	MAKE_FUNCTION_TASK_4(DrawLine,, const OpPoint &, OpPoint, from, UINT32, UINT32, length, BOOL, BOOL, horizontal, UINT32, UINT32, width, void,);
	MAKE_FUNCTION_TASK_3(DrawLine,2, const OpPoint &, OpPoint, from, const OpPoint &, OpPoint, to, UINT32, UINT32, width, void,);
	MAKE_FUNCTION_TASK_4(DrawString,, const OpPoint &, OpPoint, pos, uni_char*, StringRetainer, text, UINT32, UINT32, len, INT32, INT32, extra_char_spacing, void,);
	MAKE_FUNCTION_TASK_1(InvertRect,, const OpRect &, OpRect, rect, void,)
	MAKE_FUNCTION_TASK_2(InvertBorderRect,, const OpRect &, OpRect, rect, int, int, border, void,)
	MAKE_FUNCTION_TASK_2(InvertBorderEllipse,, const OpRect &, OpRect, rect, int, int, border, void,)
	MAKE_FUNCTION_TASK_3(InvertBorderPolygon,, const OpPoint*, const OpPoint*, point_array, int, int, points, int, int, border, void,)
	MAKE_FUNCTION_TASK_3(DrawBitmapClipped,, const OpBitmap*, BitmapRetainer, bitmap, const OpRect &, OpRect, source, OpPoint, OpPoint, p, void,)
	MAKE_FUNCTION_TASK_3(DrawBitmapClippedTransparent,, const OpBitmap*, BitmapRetainer, bitmap, const OpRect &, OpRect, source, OpPoint, OpPoint, p, void,)
	MAKE_FUNCTION_TASK_3(DrawBitmapClippedAlpha,, const OpBitmap*, BitmapRetainer, bitmap, const OpRect &, OpRect, source, OpPoint, OpPoint, p, void,)
	MAKE_FUNCTION_TASK_4(DrawBitmapClippedOpacity,, const OpBitmap*, BitmapRetainer, bitmap, const OpRect &, OpRect, source, OpPoint, OpPoint, p, int, int, opacity, BOOL, TRUE)
	MAKE_FUNCTION_TASK_3(DrawBitmapScaled,, const OpBitmap*, BitmapRetainer, bitmap, const OpRect &, OpRect, source, const OpRect &, OpRect, dest, void,)
	MAKE_FUNCTION_TASK_3(DrawBitmapScaledTransparent,, const OpBitmap*, BitmapRetainer, bitmap, const OpRect &, OpRect, source, const OpRect &, OpRect, dest, void,)
	MAKE_FUNCTION_TASK_3(DrawBitmapScaledAlpha,, const OpBitmap*, BitmapRetainer, bitmap, const OpRect &, OpRect, source, const OpRect &, OpRect, dest, void,)
	MAKE_FUNCTION_TASK_4(DrawBitmapTiled,, const OpBitmap*, BitmapRetainer, bitmap, const OpPoint&, OpPoint, offset, const OpRect&, OpRect, dest, INT32, INT32, scale, OP_STATUS, OpStatus::OK)
	MAKE_FUNCTION_TASK_6(DrawBitmapTiled,2, const OpBitmap*, BitmapRetainer, bitmap, const OpPoint&, OpPoint, offset, const OpRect&, OpRect, dest, INT32, INT32, scale, UINT32, UINT32, bitmapWidth, UINT32, UINT32, bitmapHeight, OP_STATUS, OpStatus::OK)


	virtual OpBitmap* CreateBitmapFromBackground(const OpRect& rect);
	class IsUsingOffscreenbitmapPainterTask : public PainterTask
	{
	public:
		IsUsingOffscreenbitmapPainterTask(DesktopPainter* painter, BOOL* is_offscreen) : PainterTask(painter), is_offscreen(is_offscreen) {}
		virtual void Execute() { *is_offscreen = painter->IsUsingOffscreenbitmap(); }
		BOOL* is_offscreen;
	};
	virtual BOOL IsUsingOffscreenbitmap();
	class GetOffscreenBitmapPainterTask : public PainterTask
	{
	public:
		GetOffscreenBitmapPainterTask(DesktopPainter* painter, OpBitmap** offscreen) : PainterTask(painter), offscreen(offscreen) {}
		virtual void Execute() { *offscreen = painter->GetOffscreenBitmap(); }
		OpBitmap** offscreen;
	};
	virtual OpBitmap* GetOffscreenBitmap();
	class GetOffscreenBitmapRectPainterTask : public PainterTask
	{
	public:
		GetOffscreenBitmapRectPainterTask(DesktopPainter* painter, OpRect* rect) : PainterTask(painter), rect(rect) {}
		virtual void Execute() { painter->GetOffscreenBitmapRect(rect); }
		OpRect* rect;
	};
	virtual void GetOffscreenBitmapRect(OpRect* rect);
	class SupportsPainterTask : public PainterTask
	{
	public:
		SupportsPainterTask(DesktopPainter* painter, SUPPORTS supports, BOOL* result) : PainterTask(painter), supports(supports), result(result) {}
		virtual void Execute() { *result = painter->Supports(supports); }
		SUPPORTS supports;
		BOOL* result;
	};
	virtual BOOL Supports(SUPPORTS supports);
	MAKE_FUNCTION_TASK_2(BeginOpacity,, const OpRect &, OpRect, rect, UINT8, UINT8, opacity, OP_STATUS, OpStatus::OK)
	MAKE_FUNCTION_TASK_0(EndOpacity,, void,)
	virtual UINT32 GetColor();

protected:
	void AddPainterTask(PainterTask* task);

	DesktopTaskQueue* m_task_queue;
	DesktopPainter* m_native_painter;
	OpRect current_clip;
	OpVector<OpRect> clip_stack;
	UINT32 current_color;
};

#endif // THREADED_PAINTER_H
