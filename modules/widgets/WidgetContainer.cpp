/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/display/vis_dev.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpButton.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpView.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/util/hash.h"
#include "modules/doc/frm_doc.h"

#ifdef QUICK
# include "adjunct/quick/Application.h"
# include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
# include "adjunct/quick/windows/DocumentDesktopWindow.h"
#endif

#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/pi/OpDragObject.h"
#endif // DRAG_SUPPORT

#ifdef NAMED_WIDGET
// == OpNamedWidgetsCollection ========================================

OpNamedWidgetsCollection::OpNamedWidgetsCollection()
{
	m_named_widgets.SetHashFunctions(&m_hash_function);
}

OpNamedWidgetsCollection::~OpNamedWidgetsCollection()
{
	OP_ASSERT(m_named_widgets.GetCount() == 0);
}
#endif // NAMED_WIDGET

// == RootWidget ========================================

class RootWidget : public OpWidget
#ifdef NAMED_WIDGET
				 , public OpNamedWidgetsCollection
#endif // NAMED_WIDGET
{
public:
	RootWidget(WidgetContainer* container)
	{
		SetWidgetContainer(container);
#ifdef QUICK
		SetSkinned(TRUE);
#endif
#ifdef SKIN_SUPPORT
		SetSkinIsBackground(TRUE);
#endif // SKIN_SUPPORT
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		AccessibilitySkipsMe(TRUE);
#endif
	}
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
	{
		if (GetWidgetContainer()->GetEraseBg())
		{
			if (!m_color.use_default_background_color)
			{
				vis_dev->SetColor32(m_color.background_color);
				vis_dev->FillRect(paint_rect);
			}
		}
	}
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
	{
		if (listener && button == MOUSE_BUTTON_1 && GetBounds().Contains(point))
			listener->OnClick(this, GetID());
	}

	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
	{
		if (listener && GetBounds().Contains(point))
			listener->OnMouseEvent(this, 0, point.x, point.y, button, TRUE, nclicks);
	}

#ifdef NAMED_WIDGET
	OpNamedWidgetsCollection *GetNamedWidgetsCollection()
	{
		return this;
	}
#endif // NAMED_WIDGET

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	Type GetType() {return WIDGET_TYPE_ROOT;}
#endif
};

#ifdef WIDGETS_IME_SUPPORT

// == WidgetInputMethodListener ========================================

WidgetInputMethodListener::WidgetInputMethodListener()
	: inputcontext(NULL)
{
}

IM_WIDGETINFO WidgetInputMethodListener::OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose)
{
	IM_WIDGETINFO widgetinfo;
	widgetinfo.rect.Set(0, 0, 0, 0);
	widgetinfo.font = NULL;
	widgetinfo.is_multiline = FALSE;

	OP_ASSERT(inputcontext == NULL);
	inputcontext = g_input_manager->GetKeyboardInputContext();
	if (inputcontext)
	{
		widgetinfo = inputcontext->OnStartComposing(imstring, compose);
	}
	return widgetinfo;
}

IM_WIDGETINFO WidgetInputMethodListener::OnCompose()
{
	IM_WIDGETINFO widgetinfo;
	widgetinfo.rect.Set(0, 0, 0, 0);
	widgetinfo.font = NULL;
	widgetinfo.is_multiline = FALSE;

	OP_ASSERT(inputcontext);
	if (inputcontext)
	{
		widgetinfo = inputcontext->OnCompose();
	}
	return widgetinfo;
}

void WidgetInputMethodListener::OnCommitResult()
{
	OP_ASSERT(inputcontext);
	if (inputcontext)
		inputcontext->OnCommitResult();
}

void WidgetInputMethodListener::OnStopComposing(BOOL canceled)
{
	OP_ASSERT(inputcontext);
	if (inputcontext)
		inputcontext->OnStopComposing(canceled);
	inputcontext = NULL;
}

#ifdef IME_RECONVERT_SUPPORT
void WidgetInputMethodListener::OnPrepareReconvert(const uni_char*& str, int& sel_start, int& sel_stop)
{
	if (inputcontext)
		inputcontext->OnPrepareReconvert(str,sel_start,sel_stop);
}

void WidgetInputMethodListener::OnReconvertRange(int sel_start, int sel_stop)
{
	OP_ASSERT(inputcontext);
	if (inputcontext)
		inputcontext->OnReconvertRange(sel_start,sel_stop);
}
#endif // IME_RECONVERT_SUPPORT

void WidgetInputMethodListener::OnCandidateShow(BOOL visible)
{
	OP_ASSERT(inputcontext);
	if (inputcontext)
		inputcontext->OnCandidateShow(visible);
}

IM_WIDGETINFO WidgetInputMethodListener::GetWidgetInfo()
{
	IM_WIDGETINFO widgetinfo;
	widgetinfo.rect.Set(0, 0, 0, 0);
	widgetinfo.font = NULL;
	widgetinfo.is_multiline = FALSE;
	OP_ASSERT(inputcontext);
	if (inputcontext)
	{
		widgetinfo = inputcontext->GetWidgetInfo();
	}
	return widgetinfo;
}

#endif // WIDGETS_IME_SUPPORT

#ifndef MOUSELESS

// == ContainerMouseListener ========================================

class ContainerMouseListener : public CoreViewMouseListener
{
public:
	ContainerMouseListener(WidgetContainer* widgetcontainer)
		: last_nclicks(1)
		, widgetcontainer(widgetcontainer)
#ifdef QUICK
		, m_button_down(false)
#endif //QUICK
	{
	}
	CoreView* GetMouseHitView(const OpPoint &point, CoreView* view)
	{
		return view->GetIntersectingChild(point.x, point.y);
	}
	OpPoint GetMousePos(const OpPoint& point)
	{
		INT32 xpos = widgetcontainer->GetRoot()->GetVisualDevice()->ScaleToDoc(point.x);
		INT32 ypos = widgetcontainer->GetRoot()->GetVisualDevice()->ScaleToDoc(point.y);
		return OpPoint(xpos, ypos);
	}
	void OnMouseMove(const OpPoint &point, ShiftKeyState keystate, CoreView* view)
	{
#ifdef PAN_SUPPORT
		OP_ASSERT(widgetcontainer->GetVisualDevice());
		OpWidget* widget = widgetcontainer->GetRoot();
		while (widget && !widget->IsScrollable(FALSE) && !widget->IsScrollable(TRUE))
			widget = widgetcontainer->GetNextFocusableWidget(widget, TRUE);
		// fallback to root widget
		if (!widget)
			widget = widgetcontainer->GetRoot();
		if (widget && widgetcontainer->GetVisualDevice()->PanMouseMove(view->ConvertToScreen(point), widget))
			return;
#endif // PAN_SUPPORT

		WidgetSafePointer captured_widget(g_widget_globals->captured_widget);
		widgetcontainer->GetRoot()->GenerateOnMouseMove(GetMousePos(point));

		// The widget may have been deleted at this point (on Mac).
		if (captured_widget.IsDeleted())
 			return;

		widgetcontainer->GetRoot()->GenerateOnSetCursor(GetMousePos(point));
#ifdef QUICK
		if (!m_button_down)
		{
			OpWidget* inner_widget = widgetcontainer->GetRoot()->GetWidget(GetMousePos(point), TRUE);
			if (g_application && inner_widget)
				g_application->SetToolTipListener(inner_widget);
		}
#endif // QUICK
	}
	void OnMouseLeave()
	{
#ifdef QUICK
		widgetcontainer->GetRoot()->GenerateOnMouseLeave();
		g_application->SetToolTipListener(NULL);
#endif // QUICK
	}
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks, ShiftKeyState keystate, CoreView* view)
	{
#ifdef QUICK
		m_button_down = true;
#endif // QUICK
		last_nclicks = nclicks;
#ifdef PAN_SUPPORT
		OP_ASSERT(widgetcontainer->GetVisualDevice());
		if (button == MOUSE_BUTTON_1)
			widgetcontainer->GetVisualDevice()->PanMouseDown(view->ConvertToScreen(point), keystate);
#endif // PAN_SUPPORT

		widgetcontainer->GetRoot()->GenerateOnMouseDown(GetMousePos(point), button, nclicks);
	}
	void OnMouseUp(const OpPoint &point, MouseButton button, ShiftKeyState keystate, CoreView* view)
	{
#ifdef QUICK
		m_button_down = false;
		g_application->SetToolTipListener(NULL);
#endif // QUICK


#ifdef PAN_SUPPORT
		OP_ASSERT(widgetcontainer->GetVisualDevice());
		if (button == MOUSE_BUTTON_1 && widgetcontainer->GetVisualDevice()->PanMouseUp())
			return;
#endif // PAN_SUPPORT

		widgetcontainer->GetRoot()->GenerateOnMouseUp(GetMousePos(point), button, last_nclicks);
	}
	BOOL OnMouseWheel(INT32 delta,BOOL vertical, ShiftKeyState keystate, CoreView* view)
	{
		BOOL handled = FALSE;

		if (g_widget_globals->hover_widget)
		{
			handled = g_widget_globals->hover_widget->GenerateOnMouseWheel(delta,vertical);
		}

		if (handled == FALSE && OpWidget::GetFocused())
		{
			handled = OpWidget::GetFocused()->GenerateOnMouseWheel(delta,vertical);
		}

		return handled;
	}
	void OnSetCursor() {}
private:
	UINT8 last_nclicks;
	WidgetContainer* widgetcontainer;
#ifdef QUICK
	bool m_button_down;
#endif //QUICK
};

#endif // !MOUSELESS

// == ContainerPaintListener ========================================

class ContainerPaintListener : public CoreViewPaintListener
{
private:
	ContainerPaintListener(WidgetContainer* widgetcontainer)
		: widgetcontainer(widgetcontainer)
		, vd(NULL)
	{}

public:

	static ContainerPaintListener* Create(WidgetContainer* widgetcontainer)
	{
		ContainerPaintListener* cpl = OP_NEW(ContainerPaintListener, (widgetcontainer));
		if (cpl == NULL)
			return NULL;
	
		// Initialise the visual device
		cpl->vd = OP_NEW(VisualDevice, ());
		if (cpl->vd)
        {
			cpl->vd->SetView(widgetcontainer->GetView());
			return cpl;
		}
		else
		{
			OP_DELETE(cpl);
			return NULL;
		}
	}

	~ContainerPaintListener()
	{
		if (vd)
		{
			vd->painter = NULL;
			vd->SetView(NULL);
		}
		OP_DELETE(vd);
	}


	BOOL BeforePaint()
	{
		widgetcontainer->GetRoot()->GenerateOnBeforePaint();
# ifdef QUICK // SyncLayout isn't a method of OpWidget otherwise ...
		widgetcontainer->GetRoot()->SyncLayout();
# endif // QUICK
		return TRUE;
	}

	void OnPaint(const OpRect &crect, OpPainter* painter, CoreView* view, int translate_x, int translate_y)
	{
		OpRect doc_rect = vd->ScaleToDoc(crect);
		OpRect view_rect;
		view->GetSize(&view_rect.width, &view_rect.height);

		AffinePos view_ctm;
		view->GetTransformToContainer(view_ctm);
		view_ctm.AppendTranslation(translate_x, translate_y);

		vd->BeginPaint(painter, view_rect, doc_rect, view_ctm);

		widgetcontainer->GetRoot()->GenerateOnPaint(doc_rect);

		vd->EndPaint();

	}
public:
	WidgetContainer* widgetcontainer;
	VisualDevice* vd;
};

#ifdef TOUCH_EVENTS_SUPPORT

class ContainerTouchListener : public CoreViewTouchListener
{
public:
	ContainerTouchListener(WidgetContainer* widgetcontainer, ContainerMouseListener* mouselistener)
		: m_widgetcontainer(widgetcontainer)
		, m_mouselistener(mouselistener)
		, m_scroll_widget(NULL)
	{
	}
	CoreView* GetTouchHitView(const OpPoint &point, CoreView* view)
	{
		return m_mouselistener->GetMouseHitView(point, view);
	}
	OpPoint GetTouchPos(const OpPoint& point)
	{
		return m_mouselistener->GetMousePos(point);
	}
	virtual void OnTouchDown(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data)
	{
		m_start_point = point;
		m_last_move_point = point;

		// for compatibility, send a mouse down now
		view->MouseDown(point, MOUSE_BUTTON_1, 1, modifiers);

		// simulate the button being down
		view->SetMouseButton(MOUSE_BUTTON_1, true);

		// with touch move, we only do something for scrollable widgets
		m_scroll_widget = g_widget_globals->captured_widget;
		if (m_scroll_widget)
		{
			if(!m_scroll_widget->IsScrollable(FALSE) && !m_scroll_widget->IsScrollable(TRUE))
			{
				m_scroll_widget = NULL;
			}
		}
	}
	virtual void OnTouchUp(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data)
	{
		// for compatibility, send a mouse up now
		if(FramesDocument::CheckMovedTooMuchForClick(point, m_start_point))
		{
			view->MouseUp(point, MOUSE_BUTTON_1, modifiers);
		}
		else
		{
			// we have moved too much from the initial position to consider this a "click".
			view->MouseLeave();
		}
		view->SetMouseButton(MOUSE_BUTTON_1, false);

		m_scroll_widget = NULL;
	}
	virtual void OnTouchMove(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, CoreView* view, void* user_data)
	{
		BOOL scrolled = FALSE;
		if (m_scroll_widget)
		{
			INT32 delta_x = m_last_move_point.x - point.x;
			INT32 delta_y = m_last_move_point.y - point.y;
			if (delta_y)
				scrolled |= m_scroll_widget->GenerateOnScrollAction(delta_y, TRUE);
			if (delta_x)
				scrolled |= m_scroll_widget->GenerateOnScrollAction(delta_x, FALSE);

			m_widgetcontainer->GetRoot()->GenerateOnSetCursor(GetTouchPos(point));
			m_last_move_point = point;

		}
		if(!scrolled && !m_scroll_widget)
		{
			// for compatibility, send a mouse move now if we didn't scroll
			view->MouseMove(point, modifiers);
		}
	}

private:
	OpPoint m_last_move_point;			// last point of the move
	OpPoint m_start_point;				// the touch down initial point
	WidgetContainer* m_widgetcontainer;
	ContainerMouseListener* m_mouselistener;
	OpWidget* m_scroll_widget;
};

#endif // TOUCH_EVENTS_SUPPORT
// == WidgetContainer ========================================

WidgetContainer::WidgetContainer(DesktopWindow* parent_desktop_window) :
	 root(NULL)
	, view(NULL)
	, erase_bg(FALSE)
	, is_focusable(FALSE)
	, m_parent_desktop_window(parent_desktop_window)
	, paint_listener(NULL)
	, mouse_listener(NULL)
#ifdef TOUCH_EVENTS_SUPPORT
	, touch_listener(NULL)
#endif // TOUCH_EVENTS_SUPPORT
	, m_has_default_button(TRUE)
{
}

OP_STATUS WidgetContainer::Init(const OpRect &rect, OpWindow* window, CoreView* parentview)
{
	this->window = window;

	if (parentview)
		RETURN_IF_ERROR(CoreView::Create(&view, parentview));
	else
		RETURN_IF_ERROR(CoreViewContainer::Create(&view, window, NULL, NULL));

	AffinePos pos(rect.x, rect.y);
	view->SetPos(pos);
	view->SetSize(rect.width, rect.height);
	view->SetVisibility(TRUE);

	paint_listener = ContainerPaintListener::Create(this);
#ifndef MOUSELESS
	mouse_listener = OP_NEW(ContainerMouseListener, (this));
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
	touch_listener = OP_NEW(ContainerTouchListener, (this, mouse_listener));
#endif // TOUCH_EVENTS_SUPPORT
	if (paint_listener == NULL
#ifndef MOUSELESS
		|| mouse_listener == NULL
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
		|| touch_listener == NULL
#endif // TOUCH_EVENTS_SUPPORT
		|| paint_listener->vd == NULL)
	{
		OP_DELETE(paint_listener);
		paint_listener = NULL;
#ifdef TOUCH_EVENTS_SUPPORT
		OP_DELETE(touch_listener);
		touch_listener = NULL;
#endif // TOUCH_EVENTS_SUPPORT
#ifndef MOUSELESS
		OP_DELETE(mouse_listener);
		mouse_listener = NULL;
#endif // !MOUSELESS
		return OpStatus::ERR_NO_MEMORY;
	}
	view->SetParentInputContext(this);

#ifdef WIDGETS_IME_SUPPORT
	view->GetOpView()->SetInputMethodListener(g_im_listener);
#endif
	view->SetPaintListener(paint_listener);
#ifndef MOUSELESS
	view->SetMouseListener(mouse_listener);
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
	view->SetTouchListener(touch_listener);
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
	view->SetDragListener(this);
#endif

	// Init the root widget
	root = OP_NEW(RootWidget, (this));
	if (root == NULL)
	{
		OP_DELETE(paint_listener);
		paint_listener = NULL;
#ifdef TOUCH_EVENTS_SUPPORT
		OP_DELETE(touch_listener);
		touch_listener = NULL;
#endif // TOUCH_EVENTS_SUPPORT
#ifndef MOUSELESS
		OP_DELETE(mouse_listener);
		mouse_listener = NULL;
#endif // !MOUSELESS
		return OpStatus::ERR_NO_MEMORY;
	}

	if (OpStatus::IsMemoryError(root->init_status))
	{
		OP_STATUS stat = root->init_status;
		root->Delete();
		root = NULL;
		OP_DELETE(paint_listener);
		paint_listener = NULL;
#ifdef TOUCH_EVENTS_SUPPORT
		OP_DELETE(touch_listener);
		touch_listener = NULL;
#endif // TOUCH_EVENTS_SUPPORT
#ifndef MOUSELESS
		OP_DELETE(mouse_listener);
		mouse_listener = NULL;
#endif // !MOUSELESS

		return stat;
	}		

	root->SetParentInputContext(this);

	root->SetVisualDevice(paint_listener->vd);

	root->SetRect(paint_listener->vd->ScaleToDoc(OpRect(0, 0, rect.width, rect.height)));

	AffinePos doc_pos;
	root->SetPosInDocument(doc_pos);

	OP_STATUS status;
	TRAP(status, g_pcdisplay->RegisterListenerL(this));
	OpStatus::Ignore(status);

	return OpStatus::OK;
}

WidgetContainer::~WidgetContainer()
{
	g_pcdisplay->UnregisterListener(this);
	if (root)
	{
		root->Delete();
	}
	if (view)
	{
		view->SetPaintListener(NULL);
#ifdef TOUCH_EVENTS_SUPPORT
		view->SetTouchListener(NULL);
#endif // TOUCH_EVENTS_SUPPORT
#ifndef MOUSELESS
		view->SetMouseListener(NULL);
#endif // !MOUSELESS
	}
	OP_DELETE(paint_listener);
#ifdef TOUCH_EVENTS_SUPPORT
	OP_DELETE(touch_listener);
#endif // TOUCH_EVENTS_SUPPORT
#ifndef MOUSELESS
	OP_DELETE(mouse_listener);
#endif // !MOUSELESS
	OP_DELETE(view);
}

CoreView* WidgetContainer::GetView()
{
	return view;
}

OpView* WidgetContainer::GetOpView()
{
	return view->GetOpView();
}

void WidgetContainer::SetPos(const AffinePos& pos)
{
	view->SetPos(pos);
}

void WidgetContainer::SetSize(INT32 width, INT32 height)
{
	view->SetSize(width, height);
	if (paint_listener && paint_listener->vd)
		root->SetRect(paint_listener->vd->ScaleToDoc(OpRect(0, 0, width, height)));
}

VisualDevice* WidgetContainer::GetVisualDevice()
{
	return paint_listener ? paint_listener->vd : NULL;
}

BOOL WidgetContainer::OnInputAction(OpInputAction* action)
{
#ifdef WIDGETS_UI
	switch (action->GetAction())
	{
#ifdef BUTTON_GROUP_SUPPORT
		case OpInputAction::ACTION_FOCUS_NEXT_RADIO_WIDGET:
		case OpInputAction::ACTION_FOCUS_PREVIOUS_RADIO_WIDGET:
		{
			OpWidget* widget = OpWidget::GetFocused();

			if(widget && widget->GetType() == OpTypedObject::WIDGET_TYPE_RADIOBUTTON && static_cast<OpRadioButton*>(widget)->m_button_group)
			{
				widget = action->GetAction() == OpInputAction::ACTION_FOCUS_NEXT_RADIO_WIDGET ? widget->GetNextSibling() : widget->GetPreviousSibling();

				while (widget && (widget->GetType() != OpTypedObject::WIDGET_TYPE_RADIOBUTTON || !widget->IsInputContextAvailable(FOCUS_REASON_OTHER)))
				{
					widget = action->GetAction() == OpInputAction::ACTION_FOCUS_NEXT_RADIO_WIDGET ? widget->GetNextSibling() : widget->GetPreviousSibling();
				}

				if (widget && (widget->GetType() == OpTypedObject::WIDGET_TYPE_RADIOBUTTON
#ifdef WIDGETS_TABS_AND_TOOLBAR_BUTTONS_ARE_TAB_STOP
					|| (widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON && (static_cast<OpButton*>(widget)->GetButtonType() == OpButton::TYPE_TAB || static_cast<OpButton*>(widget)->GetButtonType() == OpButton::TYPE_PAGEBAR))
#endif
					))
				{
					GetRoot()->SetHasFocusRect(TRUE);
					static_cast<OpRadioButton*>(widget)->SetValue(TRUE);
					static_cast<OpRadioButton*>(widget)->SetFocus(FOCUS_REASON_KEYBOARD);

					// invoke listener

					if (widget->GetListener())
					{
						widget->GetListener()->OnClick(widget, widget->GetID()); // Invoke!
					}
					return TRUE;
				}
			}
			return FALSE;
		}
#endif // BUTTON_GROUP_SUPPORT

		case OpInputAction::ACTION_FOCUS_NEXT_WIDGET:
		case OpInputAction::ACTION_FOCUS_PREVIOUS_WIDGET:
		{
			BOOL forward = action->GetAction() == OpInputAction::ACTION_FOCUS_NEXT_WIDGET;

			// Move focus

			OpWidget* focus_widget = OpWidget::GetFocused();

			if (!focus_widget)
			{
				// if we don't have a previous focus, try so check if current input context
				// exist below us

				OpInputContext* input_context = g_input_manager->GetKeyboardInputContext();

				while (input_context && !focus_widget)
				{
					OpWidget* widget = GetRoot();

					while (widget)
					{
						if (input_context == widget)
						{
							focus_widget = widget;
							break;
						}

						if (widget->childs.First())
						{
							widget = (OpWidget*) widget->childs.First();
						}
						else
						{
							while (widget->Suc() == NULL && widget->parent)
							{
								widget = widget->parent;
							}

							widget = (OpWidget*) widget->Suc();
						}
					}

					input_context = input_context->GetParentInputContext();
				}
			}

			if (focus_widget)
			{
				focus_widget = GetNextFocusableWidget(focus_widget, forward);
			}

#ifdef QUICK
			if (GetParentDesktopWindow())
			{
				DocumentDesktopWindow* doc_parent = GetParentDesktopWindow()->GetType() == WINDOW_TYPE_DOCUMENT ?
					static_cast<DocumentDesktopWindow*>(GetParentDesktopWindow()) :
					NULL;

				/* This speed dial check was introduced to fix DSK-331040. 
				   In order to cycle back to the top when tabbing, we want the speed dial to get the
				   next focusable widget from the root, and not from the parent workspace,
				   so it works like it does with forms on web pages.*/
				bool is_speed_dial = doc_parent && doc_parent->IsSpeedDialActive();

				if (!focus_widget && !is_speed_dial && GetParentDesktopWindow()->GetParentWorkspace())
				{
					focus_widget = GetNextFocusableWidget(GetParentDesktopWindow()->GetParentWorkspace(), forward);

					if (!focus_widget)
					{
						focus_widget = GetNextFocusableWidget(GetParentDesktopWindow()->GetParentWorkspace()->GetWidgetContainer()->GetRoot(), forward);
					}

				}
			}
#endif

			if (!focus_widget)
			{
				focus_widget = GetNextFocusableWidget(GetRoot(), forward);
			}

			if (focus_widget)
			{
				GetRoot()->SetHasFocusRect(TRUE);
				focus_widget->SetFocus(FOCUS_REASON_KEYBOARD);
			}

			return TRUE;
		}

		case OpInputAction::ACTION_CLICK_DEFAULT_BUTTON:
		{
			// find default push button and click it

			OpWidget* widget = GetRoot();

			while (widget)
			{
				if (widget->GetType() == WIDGET_TYPE_BUTTON)
				{
					OpButton* button = (OpButton*) widget;

					if (button->HasDefaultLook())
					{
#ifdef QUICK
						button->Click();
#endif
						return TRUE;
					}
				}

				if (widget->IsVisible() && widget->childs.First())
				{
					widget = (OpWidget*) widget->childs.First();
				}
				else
				{
					while (widget->Suc() == NULL && widget->parent)
					{
						widget = widget->parent;
					}

					widget = (OpWidget*) widget->Suc();
				}
			}

			return FALSE;
		}
		default:
			break;
	}
#endif // WIDGETS_UI

	return FALSE;
}

OpWidget* WidgetContainer::GetNextFocusableWidget(OpWidget* widget, BOOL forward)
{
#ifdef WIDGETS_UI
	if (widget == NULL)
	{
		return NULL;
	}

#ifdef BUTTON_GROUP_SUPPORT
	OpWidget* old_parent = widget->parent;
#endif // BUTTON_GROUP_SUPPORT
	OpRadioButton* fallback_radio_button = NULL;

	BOOL next = FALSE;

	do
	{
		if (forward)
		{
			if (widget->IsVisible() && widget->childs.First() && !fallback_radio_button)
			{
				widget = (OpWidget*) widget->childs.First();
			}
			else
			{
				if (fallback_radio_button && !widget->Suc())
				{
					widget = fallback_radio_button;
					break;
				}

				while (widget->Suc() == NULL && widget->parent)
				{
					widget = widget->parent;
				}

				widget = (OpWidget*) widget->Suc();
			}
		}
		else
		{
			if (widget == root)
			{
				while (widget->IsVisible() && widget->childs.Last())
				{
					widget = (OpWidget*) widget->childs.Last();
				}
			}
			else if (widget->Pred())
			{
				widget = (OpWidget*) widget->Pred();

				while (!fallback_radio_button && widget->IsVisible() && widget->childs.Last())
				{
					widget = (OpWidget*) widget->childs.Last();
				}
			}
			else
			{
				if (fallback_radio_button)
				{
					widget = fallback_radio_button;
					break;
				}

				widget = widget->parent;
			}
		}

		next = FALSE;

#ifdef BUTTON_GROUP_SUPPORT
		if(widget && ((widget->GetType() == OpTypedObject::WIDGET_TYPE_RADIOBUTTON && static_cast<OpRadioButton*>(widget)->m_button_group)
#ifdef WIDGETS_TABS_AND_TOOLBAR_BUTTONS_ARE_TAB_STOP
			|| (widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON && (((OpButton*)widget)->GetButtonType() == OpButton::TYPE_TAB || ((OpButton*)widget)->GetButtonType() == OpButton::TYPE_PAGEBAR))
#endif
			))
		{
			next = old_parent == widget->parent;
			
			if (!next)
			{
				next = ((OpRadioButton*)widget)->GetValue() == FALSE;

				if (next && !fallback_radio_button && widget->IsEnabled() && widget->IsAllVisible() && widget->IsTabStop())
				{
					fallback_radio_button = (OpRadioButton*) widget;
				}
			}
		}
		else if (fallback_radio_button)
		{
			next = TRUE;
		}
#endif // BUTTON_GROUP_SUPPORT

	} while (widget && widget != root && (!widget->IsEnabled() || !widget->IsAllVisible() || !widget->IsTabStop() || next));

	if (widget == root)
	{
		widget = NULL;
	}

	return widget;
#else // WIDGETS_UI
	OP_ASSERT(NULL); // You need WIDGETS_UI
	return NULL;
#endif
}

/***********************************************************************************
**
**	UpdateDefaultPushButton
**
***********************************************************************************/

void WidgetContainer::UpdateDefaultPushButton()
{
#ifdef WIDGETS_UI
#ifdef SKIN_SUPPORT
	if (m_has_default_button)
	{
		// if window is inactive, noone should have default look
		// if window is active, a focused push button should have the look, otherwise
		// the one with packed2.has_defaultlook should have it, as long as it isn't disabled

		OpWidget* widget = GetRoot();

		OpButton* focused_button = NULL;
			
		if (g_input_manager->GetKeyboardInputContext() && g_input_manager->GetKeyboardInputContext()->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
		{
			focused_button = (OpButton*) g_input_manager->GetKeyboardInputContext();
		}

#ifdef _MACINTOSH_	
		// Note: MacOS never changes default button it's always the same! 
		// So if this function is attempting to change the default button using the 
		// currently focused button then just jump out!
	    if (focused_button != NULL) 
		{
        	WidgetContainer *wc = focused_button->GetWidgetContainer();
         
         	// The focused button that we have grabbed must be in the same widget container (i.e. Dialog)
        	// This was added since sometimes we have a dialog come over another dialog
        	if (wc != this)
            	focused_button = NULL;
         	else
            	return;
     	}
#endif

		while (widget)
		{
			if (widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
			{
				OpButton* button = (OpButton*) widget;
				if (button->GetButtonType() == OpButton::TYPE_PUSH_DEFAULT || button->GetButtonType() == OpButton::TYPE_PUSH)
				{
					BOOL in_active_win = TRUE;
#ifdef QUICK
#ifndef _MACINTOSH_	
					DesktopWindow* parent_desktop = GetParentDesktopWindow();
					in_active_win = parent_desktop && parent_desktop->IsActive();
#endif
#endif
					if (in_active_win && ((!focused_button && button->HasDefaultLook() && button->IsEnabled()) || (focused_button == button)))
					{
						button->SetButtonTypeAndStyle(OpButton::TYPE_PUSH_DEFAULT, button->m_button_style, TRUE);
					}
					else
					{
						button->SetButtonTypeAndStyle(OpButton::TYPE_PUSH, button->m_button_style, TRUE);
					}
				}
			}

			if (widget->IsVisible() && widget->childs.First())
			{
				widget = (OpWidget*) widget->childs.First();
			}
			else
			{
				while (widget->Suc() == NULL && widget->parent)
				{
					widget = widget->parent;
				}

				widget = (OpWidget*) widget->Suc();
			}
		}
	} // end if (m_has_default_button)
#endif // SKIN_SUPPORT
#endif // WIDGETS_UI
}

#ifdef DRAG_SUPPORT
void WidgetContainer::OnDragStart(CoreView* view, const OpPoint& start_point, ShiftKeyState modifiers, const OpPoint& current_point)
{
	OpWidget* root = GetRoot();
	OpPoint start_scaled_point = root->GetVisualDevice()->ScaleToDoc(start_point);
	root->GenerateOnDragStart(start_scaled_point);
}

void WidgetContainer::OnDragEnter(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers)
{
	g_drag_manager->OnDragEnter();
	OpPoint scaled_point = root->GetVisualDevice()->ScaleToDoc(point);
	OnDragMove(view, drag_object, scaled_point, modifiers);
}

void WidgetContainer::OnDragCancel(CoreView* view, OpDragObject* drag_object, ShiftKeyState modifiers)
{
	GetRoot()->GenerateOnDragCancel(drag_object);
	// Notify a source that d'n'd has been cancelled.
	CoreViewDragListener* dl = g_drag_manager->GetOriginDragListener();
	if (dl && dl != this)
		dl->OnDragCancel(view, drag_object, modifiers);
	else
		g_drag_manager->StopDrag(TRUE);
}

/***********************************************************************************
**
**	OnDragLeave
**
***********************************************************************************/

void WidgetContainer::OnDragLeave(CoreView* view, OpDragObject* drag_object, ShiftKeyState modifiers)
{
	GetRoot()->GenerateOnDragLeave(drag_object);
	// The current target (the one we enters instead) should change this.
	drag_object->SetDropType(DROP_NONE);
	drag_object->SetVisualDropType(DROP_NONE);
	GetWindow()->SetCursor(CURSOR_NO_DROP);
}

/***********************************************************************************
**
**	OnDragMove
**
***********************************************************************************/

void WidgetContainer::OnDragMove(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers)
{
	if (!drag_object->IsInProtectedMode())
	{
		OpWidget* root = GetRoot();
		OpPoint scaled_point = root->GetVisualDevice()->ScaleToDoc(point);
		root->GenerateOnDragMove(drag_object, scaled_point);
	}
	else
	{
		drag_object->SetDropType(DROP_NONE);
		drag_object->SetVisualDropType(DROP_NONE);
		GetWindow()->SetCursor(CURSOR_NO_DROP);
	}
}

/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/

void WidgetContainer::OnDragDrop(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers)
{
	OpWidget* drag_widget = g_widget_globals->drag_widget;
	OpPoint scaled_point = root->GetVisualDevice()->ScaleToDoc(point);
	if (drag_object->GetDropType() != DROP_NONE && !drag_object->IsInProtectedMode())
	{
		OpWidget* root = GetRoot();
		root->GenerateOnDragDrop(drag_object, scaled_point);
	}
	else
		OnDragLeave(view, drag_object, modifiers);

	// Notify a source that d'n'd has ended.
	CoreViewDragListener* dl = g_drag_manager->GetOriginDragListener();
	if (dl)
		dl->OnDragEnd(view, drag_object, point, modifiers);
	else
		OnDragEnd(view, drag_object, point, modifiers);

	if (drag_widget)
	{
		OpRect rect = drag_widget->GetRect(TRUE);
		OpPoint translated_point(scaled_point.x - rect.x, scaled_point.y - rect.y);
		drag_widget->OnSetCursor(translated_point);
	}
}

void WidgetContainer::OnDragEnd(CoreView* view, OpDragObject* drag_object, const OpPoint& point, ShiftKeyState modifiers)
{
	g_drag_manager->StopDrag();
}

#endif // DRAG_SUPPORT

void WidgetContainer::PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue)
{
#ifdef CSS_SCROLLBARS_SUPPORT
	if (id == OpPrefsCollection::Display && pref == PrefsCollectionDisplay::EnableScrollbarColors)
	{
		GetRoot()->InvalidateAll();
	}
#endif
}

#ifdef NAMED_WIDGET

/***********************************************************************************
**
**	Hash (NAMED_WIDGET API) (Copied from OpGenericString8HashTable)
**
***********************************************************************************/
UINT32 OpNamedWidgetsCollection::StringHash::Hash(const void* key)
{
	return static_cast<UINT32>(djb2hash_nocase(static_cast<const char*>(key)));
}

/***********************************************************************************
**
**	KeysAreEqual (NAMED_WIDGET API) (Copied from OpGenericString8HashTable)
**
***********************************************************************************/
BOOL OpNamedWidgetsCollection::StringHash::KeysAreEqual(const void* key1, const void* key2)
{
	const char* string1 = (const char*) key1;
	const char* string2 = (const char*) key2;

	return !op_stricmp(string1, string2);
}

/***********************************************************************************
**
**	NameChanging (NAMED_WIDGET API)
**
***********************************************************************************/
OP_STATUS OpNamedWidgetsCollection::ChangeName(OpWidget * widget, const OpStringC8 & name)
{
	OP_ASSERT(widget);

	if(!widget)
		return OpStatus::ERR;

	if (widget->packed.added_to_name_collection)
		RETURN_IF_ERROR(WidgetRemoved(widget));

	RETURN_IF_ERROR(widget->m_name.Set(name));

	if (widget->packed.added_to_name_collection)
		RETURN_IF_ERROR(WidgetAdded(widget));

	return OpStatus::OK;
}

/***********************************************************************************
**
**	WidgetAdded (NAMED_WIDGET API)
**
***********************************************************************************/
OP_STATUS OpNamedWidgetsCollection::WidgetAdded(OpWidget * child)
{
	OP_ASSERT(child);

	if(!child)
		return OpStatus::ERR;

	OP_STATUS status = OpStatus::OK;

	if(child->HasName())
	{
		status = m_named_widgets.Add((void *) child->GetName().CStr(), (void *) child);

#ifdef DEBUG_ENABLE_OPASSERT
		if (status == OpStatus::ERR)
		{
			OpWidget * widget = 0;
			m_named_widgets.GetData((void *) child->GetName().CStr(), (void**) &widget);
			if (widget)
				widget->m_has_duplicate_named_widget = TRUE;
		}
#endif // DEBUG_ENABLE_OPASSERT
	}

	return status;
}

/***********************************************************************************
**
**	WidgetRemoved (NAMED_WIDGET API)
**
***********************************************************************************/
OP_STATUS OpNamedWidgetsCollection::WidgetRemoved(OpWidget * child)
{
	OP_ASSERT(child);

	if(!child)
		return OpStatus::ERR;

	OP_STATUS status = OpStatus::OK;

	if(child->HasName())
	{
		OpWidget * widget = 0;
		status = m_named_widgets.Remove((void *) child->GetName().CStr(), (void**) &widget);
	}

	return status;
}

/***********************************************************************************
**
**	GetWidgetByName (NAMED_WIDGET API)
**
***********************************************************************************/
OpWidget* OpNamedWidgetsCollection::GetWidgetByName(const OpStringC8 & name)
{
	OpWidget * widget = 0;

	// Calling GetWidgetByName with an empty name is an error
	OP_ASSERT(name.HasContent());

	if(name.HasContent())
		m_named_widgets.GetData((void *) name.CStr(), (void**) &widget);
	
	OP_ASSERT(!widget || !widget->m_has_duplicate_named_widget);
	
	return widget;
}

#endif // NAMED_WIDGET
