/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "OpZoomDropDown.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/windows/OpRichMenuWindow.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/managers/SpeedDialManager.h"

INT32 OpZoomDropDown::s_scales[] =
{	20, 30, 50, 70, 80,
	90, 100, 110, 120,
	150, 180, 200, 250,
	300, 400, 500, 600,
	700, 800, 900, 1000, 0
};


/***********************************************************************************
**
**	OpZoomDropDown
**
***********************************************************************************/
DEFINE_CONSTRUCT(OpZoomDropDown)

OpZoomDropDown::OpZoomDropDown() : OpDropDown(TRUE), DesktopWindowSpy(TRUE)
{
	RETURN_VOID_IF_ERROR(init_status);
	GetBorderSkin()->SetImage("Zoom Dropdown Skin", "Dropdown Skin");

	SetListener(this);
	edit->SetListener(this);
	SetSpyInputContext(this, FALSE);

	OpString ghost_text;

	g_languageManager->GetString(Str::S_ZOOM_GHOST_TEXT, ghost_text);
	edit->SetGhostText(ghost_text.CStr());
}

/***********************************************************************************
**
**	UpdateZoom
**
***********************************************************************************/

void OpZoomDropDown::UpdateZoom()
{
	UINT32 scale = 0;
	if(IsSpeedDial())
	{
		scale = g_speeddial_manager->GetIntegerThumbnailScale();
	}
	else
	{
		OpBrowserView *br_view = GetTargetBrowserView();
		if (br_view)
		{
			OpWindowCommander *win_comm = br_view->GetWindowCommander();
			scale = win_comm->GetScale();
		}
	}
	if(scale)
	{
		uni_char text[32];
		uni_snprintf(text, 31, UNI_L("%d%%"), scale);
		SetText(text);
	}

	GetForegroundSkin()->SetImage("Zoom");
}

BOOL OpZoomDropDown::IsSpeedDial()
{
	DesktopWindow* window = GetParentDesktopWindow();
	if (!window || window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
	{
		return FALSE;
	}
	// at this point we know that window's type is WINDOW_TYPE_DOCUEMNT so casting to DocumentDesktopWindow* is safe
	DocumentDesktopWindow* document_window = static_cast<DocumentDesktopWindow*>(window);
	return document_window->IsSpeedDialActive();
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpZoomDropDown::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_PREVIOUS_ITEM:
		case OpInputAction::ACTION_NEXT_ITEM:
			if (m_dropdown_window == NULL)
			{
				ShowMenu();
				return TRUE;
			}
			break;

		// Just so that we can Escape into the document
		case OpInputAction::ACTION_STOP:
		{
			if (action->IsKeyboardInvoked() && GetTargetBrowserView() && !GetTargetBrowserView()->GetWindowCommander()->IsLoading() && edit->IsFocused())
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);
				return TRUE;
			}
			break;
		}
	}

	return OpDropDown::OnInputAction(action);
}


void OpZoomDropDown::OnCharacterEntered( uni_char& character )
{
	if( (character >= '0' && character <= '9') || character == '%' )
	{
		// Ok
	}
	else
	{
		character = 0;
	}
}


/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void OpZoomDropDown::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == edit)
	{
		OpString text;
		GetText(text);
		INT32 zoom = (INT32) uni_atoi(text.CStr());

		g_input_manager->InvokeAction(OpInputAction::ACTION_ZOOM_TO, zoom);
	}
	else if (widget == this)
	{
		INTPTR zoom = (INTPTR) GetItemUserData(GetSelectedItem());
		g_input_manager->InvokeAction(OpInputAction::ACTION_ZOOM_TO, zoom);
	}
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void OpZoomDropDown::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == this)
	{
		((OpWidgetListener*)GetParent())->OnMouseEvent(this, pos, x, y, button, down, nclicks);
	}
	else
	{
		OpDropDown::OnMouseEvent(widget, pos, x, y, button, down, nclicks);
	}
}

/***********************************************************************************
**
**	OnDropdownMenu
**
***********************************************************************************/

void OpZoomDropDown::OnDropdownMenu(OpWidget *widget, BOOL show)
{
	OpString edittext;
	GetText(edittext);

	if (show)
	{
		INT32 i;
		for (i = 0; s_scales[i]; i++)
		{
			uni_char text[32];

			uni_snprintf(text, 31, UNI_L("%d%%"), s_scales[i]);
			AddItem(text, -1, NULL, s_scales[i]);
			if (uni_stri_eq(edittext.CStr(), text))
				SelectItem(i, TRUE);
		}
	}
	else
	{
		Clear();
	}
}

/***********************************************************************************
**
**	OnTargetDesktopWindowChanged
**
***********************************************************************************/

void OpZoomDropDown::OnTargetDesktopWindowChanged(DesktopWindow* target_window)
{
	UpdateZoom();
}

/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void OpZoomDropDown::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	DesktopDragObject* drag_object = GetDragObject(OpTypedObject::DRAG_TYPE_ZOOM_DROPDOWN, x, y);

	if (drag_object)
	{
		drag_object->SetObject(this);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

/***********************************************************************************
**
**	OnScaleChanged
**
***********************************************************************************/

void OpZoomDropDown::OnPageScaleChanged(OpWindowCommander* commander, UINT32 newScale)
{
	UpdateZoom();
}

/***********************************************************************************
**
**	OpZoomSlider
**
***********************************************************************************/
const double OpZoomSlider::MinimumZoomLevel = 0.2;
const double OpZoomSlider::MaximumZoomLevel = 3;
const double OpZoomSlider::ScaleDelta = 0.1;

const OpSlider::TICK_VALUE OpZoomSlider::TickValues[] = 
											{{MinimumZoomLevel, FALSE },
											 {1, TRUE },
											 {2, FALSE },
											 {MaximumZoomLevel, FALSE }};

DEFINE_CONSTRUCT(OpZoomSlider)

OpZoomSlider::OpZoomSlider()
: DesktopWindowSpy(FALSE)
{
	RETURN_VOID_IF_ERROR(init_status);
	RETURN_VOID_IF_ERROR(g_speeddial_manager->AddConfigurationListener(this));
	SetSpyInputContext(this, FALSE);
	SetTabStop(TRUE);
	SetName("Zoom Slider");
	SetUpdateNeededWhen(UPDATE_NEEDED_WHEN_VISIBLE);

	// Initialize slider to default zoom settings.
	SetValues();
}

void OpZoomSlider::SetValues()
{
	// Get default settings
	double min = MinimumZoomLevel;
	double max = MaximumZoomLevel;
	double step = ScaleDelta;
	UINT8 num_tick_values = ARRAY_SIZE(TickValues);
	const OpSlider::TICK_VALUE* tick_values = TickValues;

	/* This is a hook to override slider's default settings. It is need to due 
	 * lack of common settings of slider between the speeddial page and html-document.
	 */
	DesktopWindow *dtw = GetTargetDesktopWindow();
	if (dtw)
		dtw->QueryZoomSettings(min, max, step, tick_values, num_tick_values);

	// Set value
	SetMin(min);
	SetMax(max);
	SetStep(step);
	SetTickValues(num_tick_values, tick_values, step);

	UpdateZoom();
}

void OpZoomSlider::OnDeleted()
{
	RETURN_VOID_IF_ERROR(g_speeddial_manager->RemoveConfigurationListener(this));
	SetSpyInputContext(NULL, FALSE);
	OpSlider::OnDeleted();
}

void OpZoomSlider::HandleOnChange()
{
	OpSlider::HandleOnChange();
	g_input_manager->InvokeAction(OpInputAction::ACTION_ZOOM_TO, int(GetNumberValue() * 100));
}

void OpZoomSlider::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	OpSlider::OnMouseDown(point, button, nclicks);

	DesktopWindow *dtw = GetTargetDesktopWindow();
	if (dtw)
		dtw->OnMouseEvent(this, 0, point.x, point.y, button, TRUE, nclicks);
}

void OpZoomSlider::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	OpSlider::OnMouseUp(point, button, nclicks);

	DesktopWindow *dtw = GetTargetDesktopWindow();
	if (dtw)
		dtw->OnMouseEvent(this, 0, point.x, point.y, button, FALSE, nclicks);
}

void OpZoomSlider::OnMouseMove(const OpPoint &point)
{
	OpSlider::OnMouseMove(point);
	UpdateStatusText();
}

void OpZoomSlider::OnMouseLeave()
{
	OpSlider::OnMouseLeave();

	m_status_text.Empty();
	DesktopWindow *window = GetParentDesktopWindow();
	if (window)
		window->SetStatusText(m_status_text);
}

void OpZoomSlider::UpdateZoom()
{
	DesktopWindow *dw = GetTargetDesktopWindow();
	if (dw)
	{
		SetValue(dw->GetWindowScale(), TRUE);
	}
}

void OpZoomSlider::UpdateZoom(UINT32 scale)
{
	SetValue(scale * 0.01, TRUE);
	UpdateStatusText();
}

void OpZoomSlider::UpdateStatusText()
{
	OpString text;
	OpString str;
	if (OpStatus::IsSuccess(g_languageManager->GetString(Str::S_ZOOM_MENU_BUTTON, str)) &&
		OpStatus::IsSuccess(text.AppendFormat(str.CStr(), int(GetNumberValue() * 100))))
	{
		if (m_status_text.Compare(text) != 0)
		{
			m_status_text.Set(text);
			DesktopWindow *window = GetParentDesktopWindow();
			if (window)
				window->SetStatusText(text);
		}
	}
}

void OpZoomSlider::OnUpdateActionState()
{
	OpInputAction* action = OpWidget::GetAction();
	if (!action)
		return;

	SetEnabled(!!(g_input_manager->GetActionState(action, this) & OpInputAction::STATE_ENABLED));
}

/***********************************************************************************
**
**	OpZoomMenuButton - button in the status bar
**
***********************************************************************************/

DEFINE_CONSTRUCT(OpZoomMenuButton)

OpZoomMenuButton::OpZoomMenuButton() 
: OpButton(TYPE_TOOLBAR)
, DesktopWindowSpy(FALSE)
, m_zoom_window(NULL)
{
	RETURN_VOID_IF_ERROR(init_status);

	SetSpyInputContext(this, FALSE);

	OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_ZOOM_POPUP_MENU));
	if (!action)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	action->SetActionDataString(UNI_L("Zoom Popup Menu"));
	SetAction(action);

	if (GetAction())
		GetAction()->SetActionImage("Zoom");

	g_speeddial_manager->AddConfigurationListener(this);

	SetTabStop(TRUE);
}

void OpZoomMenuButton::OnDeleted()
{
	if (m_zoom_window)
		m_zoom_window->CloseMenu();

	SetSpyInputContext(NULL, FALSE);
	g_speeddial_manager->RemoveConfigurationListener(this);
	OpButton::OnDeleted();
}

void OpZoomMenuButton::OnMenuClosed()
{
	m_zoom_window = NULL;
	OnMouseLeave();
	TogglePushedState(false);
}

void OpZoomMenuButton::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	// Set the focus rect, core doesn't do it for us
	SetHasFocusRect(TRUE);

	OpButton::OnKeyboardInputGained(new_input_context, old_input_context, reason);
}

void OpZoomMenuButton::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	// Set the focus rect, core doesn't do it for us
	SetHasFocusRect(FALSE);

	OpButton::OnKeyboardInputLost(new_input_context, old_input_context, reason);
}

void OpZoomMenuButton::Click(BOOL plus_action)
{
	OP_ASSERT(!IsSpeedDial());
	if (IsSpeedDial())
		return;

	if (GetTargetDesktopWindow() && !m_zoom_window)
	{
		m_zoom_window = OP_NEW(OpRichMenuWindow, (this, GetTargetDesktopWindow()->GetParentDesktopWindow()));
		if (!m_zoom_window)
			return;
		
		if (OpStatus::IsSuccess(m_zoom_window->Construct("Zoom Toolbar", OpWindow::STYLE_NOTIFIER)))
		{
			OpZoomSlider* slider = static_cast<OpZoomSlider*>(m_zoom_window->GetToolbarWidgetOfType(WIDGET_TYPE_ZOOM_SLIDER));
			if (slider)
				slider->SetValues();
		
			m_zoom_window->Show(*this);
			TogglePushedState(true);
		}
		else
		{
			OP_DELETE(m_zoom_window);
			m_zoom_window = NULL;
		}
	}
}

void OpZoomMenuButton::UpdateZoom(INT32 zoom)
{
	UINT32 scale = 0;
	BOOL is_speeddial = IsSpeedDial();
	if (is_speeddial)
	{
		if (g_speeddial_manager->IsScaleAutomatic())
			scale = zoom;
		else
			scale = g_speeddial_manager->GetIntegerThumbnailScale();
	}
	else
	{
		OpBrowserView *br_view = GetTargetBrowserView();
		if (br_view)
		{
			OpWindowCommander *win_comm = br_view->GetWindowCommander();
			scale = win_comm->GetScale();
		}
	}
	if (scale)
	{
		OpString text;
		OpString str;
		g_languageManager->GetString(Str::S_ZOOM_MENU_BUTTON, str);
		text.AppendFormat(str.CStr(), scale);
		SetText(text.CStr());

		if (GetParent())
		{
			ResetRequiredSize();
			GetParent()->OnContentSizeChanged();
		}
	}
}

BOOL OpZoomMenuButton::IsSpeedDial()
{
	DesktopWindow* window = GetParentDesktopWindow();
	if (!window || window->GetType() != OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		return FALSE;
	}
	// at this point we know that window's type is WINDOW_TYPE_BROWSER so casting to BrowserDesktopWindow* is safe
	DocumentDesktopWindow* document_window = static_cast<BrowserDesktopWindow*>(window)->GetActiveDocumentDesktopWindow();
	if (!document_window)
	{
		return FALSE;
	}
	return document_window->IsSpeedDialActive();
}

void OpZoomMenuButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	// Can't let baseclass OpButton do this as it will send WIDGET_TYPE_ZOOM_MENU_BUTTON to
	// core, which it doesn't know anything about.
	string.NeedUpdate();
#ifdef QUICK
	UpdateActionStateIfNeeded();
#endif
	GetInfo()->GetPreferedSize(this, OpTypedObject::WIDGET_TYPE_BUTTON, w, h, cols, rows);
}

void OpZoomMenuButton::TogglePushedState(bool pushed)
{
	if (m_org_img.IsEmpty())
		RETURN_VOID_IF_ERROR(m_org_img.Set(GetBorderSkin()->GetImage()));

	OpString8 skin;
	RETURN_VOID_IF_ERROR(skin.Set(m_org_img));
	if (pushed)
	{
		RETURN_VOID_IF_ERROR(skin.Append(".selected"));
		if (IsMiniSize())
			RETURN_VOID_IF_ERROR(skin.Append(".mini"));
	}

	GetBorderSkin()->SetImage(skin);
}
