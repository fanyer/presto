#include "adjunct/desktop_util/painter/ThreadedPainter.h"

ThreadedPainter::ThreadedPainter(DesktopPainter* native_painter)
	: m_task_queue(NULL)
	, m_native_painter(native_painter)
{
	if (m_native_painter)
		m_native_painter->IncrementReference();
	DesktopTaskQueue::Make(&m_task_queue);
	current_color = 0;
	current_clip.width = UINT_MAX;
	current_clip.height = UINT_MAX;
}

ThreadedPainter::~ThreadedPainter()
{
	OP_DELETE(m_task_queue);
	m_native_painter->DecrementReference();
}

void ThreadedPainter::GetDPI(UINT32* horizontal, UINT32* vertical)
{
	switch (m_native_painter->GetAsyncClassOfFunction(DesktopPainter::PAINTER_FUNCTION_GET_DPI))
	{
		case DesktopPainter::ASYNC_CLASS_REENTRANT:
			m_native_painter->GetDPI(horizontal, vertical);
			break;
		case DesktopPainter::ASYNC_CLASS_OUT_OF_ORDER:
			m_task_queue->HandleSynchronousTask(OP_NEW(GetDPIPainterTask, (m_native_painter, horizontal, vertical)));
			break;
		case DesktopPainter::ASYNC_CLASS_IN_ORDER:
			m_task_queue->HandleSynchronousTaskWhenDone(OP_NEW(GetDPIPainterTask, (m_native_painter, horizontal, vertical)));
			break;
	}
}

void ThreadedPainter::SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha)
{
	AddPainterTask(OP_NEW(SetColorPainterTask, (m_native_painter, red, green, blue, alpha)));
	current_color = red | (green << 8) | (blue << 16) | (alpha << 24);
}

void ThreadedPainter::SetFont(OpFont* font)
{
	AddPainterTask(OP_NEW(SetFontPainterTask, (m_native_painter, font)));
}

OP_STATUS ThreadedPainter::SetClipRect(const OpRect & rect)
{
	AddPainterTask(OP_NEW(SetClipRectPainterTask, (m_native_painter, rect)));
	clip_stack.Add(OP_NEW(OpRect, (current_clip)));
	current_clip.IntersectWith(rect);
	return OpStatus::OK;
}

void ThreadedPainter::RemoveClipRect()
{
	AddPainterTask(OP_NEW(RemoveClipRectPainterTask, (m_native_painter)));
	current_clip = *clip_stack.Get(clip_stack.GetCount() - 1);
	clip_stack.Delete(clip_stack.GetCount() - 1);
}

void ThreadedPainter::GetClipRect(OpRect* rect)
{
	*rect = current_clip;
}

OpBitmap* ThreadedPainter::CreateBitmapFromBackground(const OpRect& rect)
{
	// Hmmm...
	OpBitmap* off = m_native_painter->CreateBitmapFromBackground(rect);
	return off;
}

BOOL ThreadedPainter::IsUsingOffscreenbitmap()
{
	BOOL off = false;
	switch (m_native_painter->GetAsyncClassOfFunction(DesktopPainter::PAINTER_FUNCTION_IS_USING_OFFSCREEN_BITMAP))
	{
		case DesktopPainter::ASYNC_CLASS_REENTRANT:
			off = m_native_painter->IsUsingOffscreenbitmap();
			break;
		case DesktopPainter::ASYNC_CLASS_OUT_OF_ORDER:
			m_task_queue->HandleSynchronousTask(OP_NEW(IsUsingOffscreenbitmapPainterTask, (m_native_painter, &off)));
			break;
		case DesktopPainter::ASYNC_CLASS_IN_ORDER:
			m_task_queue->HandleSynchronousTaskWhenDone(OP_NEW(IsUsingOffscreenbitmapPainterTask, (m_native_painter, &off)));
			break;
	}
	return off;
}

OpBitmap* ThreadedPainter::GetOffscreenBitmap()
{
	OpBitmap* off = NULL;
	switch (m_native_painter->GetAsyncClassOfFunction(DesktopPainter::PAINTER_FUNCTION_GET_OFFSCREEN_BITMAP))
	{
		case DesktopPainter::ASYNC_CLASS_REENTRANT:
			off = m_native_painter->GetOffscreenBitmap();
			break;
		case DesktopPainter::ASYNC_CLASS_OUT_OF_ORDER:
			m_task_queue->HandleSynchronousTask(OP_NEW(GetOffscreenBitmapPainterTask, (m_native_painter, &off)));
			break;
		case DesktopPainter::ASYNC_CLASS_IN_ORDER:
			m_task_queue->HandleSynchronousTaskWhenDone(OP_NEW(GetOffscreenBitmapPainterTask, (m_native_painter, &off)));
			break;
	}
	return off;
}

void ThreadedPainter::GetOffscreenBitmapRect(OpRect* rect)
{
	switch (m_native_painter->GetAsyncClassOfFunction(DesktopPainter::PAINTER_FUNCTION_GET_OFFSCREEN_BITMAP_RECT))
	{
		case DesktopPainter::ASYNC_CLASS_REENTRANT:
			m_native_painter->GetOffscreenBitmapRect(rect);
			break;
		case DesktopPainter::ASYNC_CLASS_OUT_OF_ORDER:
			m_task_queue->HandleSynchronousTask(OP_NEW(GetOffscreenBitmapRectPainterTask, (m_native_painter, rect)));
			break;
		case DesktopPainter::ASYNC_CLASS_IN_ORDER:
			m_task_queue->HandleSynchronousTaskWhenDone(OP_NEW(GetOffscreenBitmapRectPainterTask, (m_native_painter, rect)));
			break;
	}
}

BOOL ThreadedPainter::Supports(SUPPORTS supports)
{
	BOOL sup = m_native_painter->Supports(supports);
	switch (m_native_painter->GetAsyncClassOfFunction(DesktopPainter::PAINTER_FUNCTION_SUPPORTS))
	{
		case DesktopPainter::ASYNC_CLASS_REENTRANT:
			sup = m_native_painter->Supports(supports);
			break;
		case DesktopPainter::ASYNC_CLASS_OUT_OF_ORDER:
			m_task_queue->HandleSynchronousTask(OP_NEW(SupportsPainterTask, (m_native_painter, supports, &sup)));
			break;
		case DesktopPainter::ASYNC_CLASS_IN_ORDER:
			m_task_queue->HandleSynchronousTaskWhenDone(OP_NEW(SupportsPainterTask, (m_native_painter, supports, &sup)));
			break;
	}
	return sup;
}

UINT32 ThreadedPainter::GetColor()
{
	return current_color;
}

void ThreadedPainter::AddPainterTask(PainterTask* task)
{
	if (m_task_queue)
		m_task_queue->AddTask(task);
}
