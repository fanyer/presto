/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VIS_DEV_LISTENERS_H
#define VIS_DEV_LISTENERS_H

#include "modules/display/vis_dev.h"
#include "modules/dom/domeventtypes.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpScrollbar.h"

class OpView;
class Window;
class DocumentManager;
class FramesDocument;
class HTML_Element;

void ShowHotclickMenu(VisualDevice* vis_dev, Window* window, CoreView* view, DocumentManager* doc_man, FramesDocument* doc, BOOL reset_pos = FALSE);

#ifdef DISPLAY_CLICKINFO_SUPPORT

class MouseListenerClickInfo
{
public:
	MouseListenerClickInfo();
	void Reset();
	void SetLinkElement( FramesDocument* document, HTML_Element* link_element );
	void SetTitleElement( FramesDocument* document, HTML_Element* element );
	void SetImageElement( HTML_Element* image_element );
	void GetLinkTitle( OpString &title );
	HTML_Element* GetImageElement() const { return m_image_element; }
	HTML_Element* GetLinkElement() const { return m_link_element; }

private:
	FramesDocument* m_document;
	HTML_Element* m_image_element;
	HTML_Element* m_link_element;
};

#endif // DISPLAY_CLICKINFO_SUPPORT

class PaintListener : public CoreViewPaintListener
{
public:
	VisualDevice* 	vis_dev;
					PaintListener(VisualDevice* vis_dev);
	BOOL			BeforePaint();
	void 			OnPaint(const OpRect &rect, OpPainter* painter, CoreView* view, int translate_x, int translate_y);
	void 			OnMoved();
};

#ifndef MOUSELESS

class MouseListener : public CoreViewMouseListener, public OpTimerListener
{
public:
	OpPoint mousepos;
	OpPoint docpos;
	VisualDevice* vis_dev;
	BOOL in_editable_element;
	MouseListener(VisualDevice* vis_dev);
	OP_STATUS Init();
	~MouseListener();
	CoreView* GetMouseHitView(const OpPoint &point, CoreView* view);
	void UpdateOverlappedStatus(CoreView* view);
	void OnMouseMove(const OpPoint &point, ShiftKeyState keystate, CoreView* view);
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks, ShiftKeyState keystate, CoreView* view);
	void OnMouseUp(const OpPoint &point, MouseButton button, ShiftKeyState keystate, CoreView* view);
	void OnMouseLeave();
	void OnSetCursor();
	BOOL OnMouseWheel(INT32 delta, BOOL vertical, ShiftKeyState keystate, CoreView* view);

	// from OpTimerListener:
	void OnTimeOut(OpTimer* timer);

#ifdef DISPLAY_CLICKINFO_SUPPORT
	static MouseListenerClickInfo& GetClickInfo() { return *g_opera->display_module.mouse_click_info; }
#endif // DISPLAY_CLICKINFO_SUPPORT

private:
	void OnMouseLeft3Clk(CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, ShiftKeyState keystate);
	void OnMouseLeft4Clk(CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, ShiftKeyState keystate);
	BOOL haveActivatedALink; //< if first click in a doubleclick activated a link

	void OnMouseLeftDown  (CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate);
	void OnMouseLeftUp    (CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate);
	void OnMouseLeftDblClk(CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, ShiftKeyState keystate);

	void OnMouseRightDown  (CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate);
	void OnMouseRightUp    (CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate);
	void OnMouseRightDblClk(CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, ShiftKeyState keystate);

	void OnMouseMiddleDown  (CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate);
	void OnMouseMiddleUp    (CoreView* view, Window* window, DocumentManager* doc_man, FramesDocument* doc, UINT8 nclicks, ShiftKeyState keystate);

	void PropagateMouseEvent(CoreView* view, Window* window, FramesDocument* doc, MouseButton button, UINT8 nclicks, ShiftKeyState keystate);

#ifdef DRAG_SUPPORT
	BOOL3 is_dragging;
	OpPoint drag_point;
	OpTimer* dragTimer;

	void StopDragging();
#endif // DRAG_SUPPORT

	BOOL right_key_is_down;
	BOOL left_key_is_down;
	BOOL left_while_right_down;
};

#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
class TouchListener : public CoreViewTouchListener
{
public:
	TouchListener(VisualDevice* vis_dev);

	void OnTouchDown(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data);
	void OnTouchUp(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data);
	void OnTouchMove(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data);

	void OnTouch(DOM_EventType type, int id, const OpPoint &point, int radius, ShiftKeyState modifiers, void* user_data);

	CoreView* GetTouchHitView(const OpPoint &point, CoreView* view);
	void UpdateOverlappedStatus(CoreView* view);

private:
	VisualDevice* vis_dev;
	BOOL in_editable_element;
};
#endif // TOUCH_EVENTS_SUPPORT

class ScrollListener :
	public OpSmoothScroller,
	public OpWidgetListener
{
public:
	VisualDevice* vis_dev;
	ScrollListener(VisualDevice* vis_dev);

	/** OpSmoothScroller::OnSmoothScroll - Happens when smoothscroll is running */
	BOOL OnSmoothScroll(INT32 value);

	/** OpWidgetListener::OnScroll - Happens when the OpScrollbar has changed value. */
	void OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input);
};

#ifdef DRAG_SUPPORT

class DragListener : public CoreViewDragListener
{
public:
	VisualDevice* m_vis_dev;
	DragListener(VisualDevice* vis_dev) : m_vis_dev(vis_dev) {}
	virtual ~DragListener() {}
	// CoreViewDragListener's interface
	virtual void OnDragStart(CoreView* view, const OpPoint& start_point, ShiftKeyState modifiers, const OpPoint& current_point);
	virtual void OnDragEnter(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers);
	virtual void OnDragCancel(CoreView* view, OpDragObject* drag_object, ShiftKeyState modifiers);
	virtual void OnDragLeave(CoreView* view, OpDragObject* drag_object, ShiftKeyState modifiers);
	virtual void OnDragMove(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers);
	virtual void OnDragDrop(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers);
	virtual void OnDragEnd(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers);
};

#endif

#endif // VIS_DEV_LISTENERS_H
