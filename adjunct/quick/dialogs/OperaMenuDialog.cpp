/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "adjunct/quick/dialogs/OperaMenuDialog.h"

#ifdef QUICK_NEW_OPERA_MENU

#include "adjunct/quick/managers/AnimationManager.h"
#include "modules/softcore/inputmanager/opinputmanager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/panels/HotlistPanel.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/widgets/OpTabs.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpSlider.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick/controller/factories/ButtonFactory.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpStateButton.h"
#include "adjunct/desktop_util/sessions/opsession.h"

/***********************************************************************************
**
**  OperaMenuDialog
**
***********************************************************************************/

OperaMenuDialog::OperaMenuDialog()
	: m_tabs(NULL)
	, m_hovered_index(0)
	, m_window_animation(NULL)
{
	m_timer.SetTimerListener(this);
}

OperaMenuDialog::~OperaMenuDialog()
{
	// stop listening to desktopwindow (that is a browserdektopwindow)
	if (GetParentDesktopWindow() && GetParentDesktopWindow()->GetParentDesktopWindow())
			GetParentDesktopWindow()->GetParentDesktopWindow()->RemoveListener(this);
	OP_DELETE(m_window_animation);
}

/***********************************************************************************
**
**  OnInit
**
***********************************************************************************/

void OperaMenuDialog::OnInit()
{ 
	int i;

	//don't save position, the dialog will be aligned automatically
	SetSavePlacementOnClose(FALSE);	

	// Listen to parent desktop window to get notified about movements
	if (GetParentDesktopWindow() && GetParentDesktopWindow()->GetParentDesktopWindow())
			GetParentDesktopWindow()->GetParentDesktopWindow()->AddListener(this);

	m_tabs =  static_cast<OpTabs *> (GetWidgetByType(WIDGET_TYPE_TABS));
	m_tabs->SetButtonStyle(OpButton::STYLE_IMAGE);
	m_tabs->SetXResizeEffect(RESIZE_FIXED);
	m_tabs->SetYResizeEffect(RESIZE_SIZE);

	OpWidget *w = (OpWidget*) m_tabs->childs.First();
	while (w)
	{
		w->SetListener(this);
		OpInputAction *action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_MENU_SECTION));
		if (w->GetType() == WIDGET_TYPE_BUTTON)
			((OpButton*)w)->SetAction(action);
		w = (OpWidget*) w->Suc();
	}

	// Basic layout and initialization
	for(i = 0; i < (int)GetPageCount(); i++)
	{
		OpWidget *page = GetPageByNumber(i);
		OpWidget *w = page->GetFirstChild();
		int y = 0, row_add = 0;
		while (w)
		{
			if (w->GetType() == WIDGET_TYPE_BUTTON)
			{
				OpButton *b = static_cast<OpButton*> (w);
				b->SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
				b->SetButtonType(OpButton::TYPE_OMENU);
				b->SetFixedImage(FALSE);
				if (b->GetBounds().width >= 300)
					b->SetShowShortcut(TRUE);
			}
			// test of some really basic vertical layout so we don't have to do that by hand in ini.
			/*if (page->GetName().Compare("Menu Page") == 0 ||
				page->GetName().Compare("Menu Tools") == 0 ||
				page->GetName().Compare("Menu Closed Tabs") == 0)*/
			{
				int x = w->GetRect().x;
				int height = w->GetRect().height;

				if (x == 0)
					y += row_add;

				w->SetRect(OpRect(x, y, w->GetRect().width, height));
				w->SetOriginalRect(w->GetRect());
				row_add = height;
			}
			w = (OpWidget *) w->Suc();
		}
	}

	if (OpSlider *slider = static_cast<OpSlider*> (GetWidgetByName("zoom_slider")))
	{
		int zoom = 100;
		slider->SetMin(20);
		slider->SetMax(300);
		slider->SetValue(zoom);
		slider->SetStep(10);
		slider->ShowTickLabels(TRUE);

		OpSlider::TICK_VALUE tick_values[5] = {	{20, FALSE },
												{50, FALSE },
												{100, TRUE },
												{200, TRUE },
												{300, FALSE } };
		slider->SetTickValues(5, tick_values, 10);
	}

	// Create window animation for slider fade
	m_window_animation = OP_NEW(QuickAnimationWindowObject, ());
	if (!m_window_animation || OpStatus::IsError(m_window_animation->Init(this, NULL)))
	{
		OP_DELETE(m_window_animation);
		m_window_animation = NULL;
	}
}

void OperaMenuDialog::GetPlacement(OpRect& initial_placement)
{
	OpWidget *menu_button = GetWidgetByType(WIDGET_TYPE_TOOLBAR_MENU_BUTTON);
	if (menu_button)
	{
		OpRect rect = menu_button->GetScreenRect();
		initial_placement.x = rect.x;
		initial_placement.y = rect.y + rect.height;
	}
}

BOOL OperaMenuDialog::OnInputAction(OpInputAction* action)
{
	// Must post some actions that trigger a modal dialog for later processing. That way we avoid spawning a nested message loop.
	if (action->GetAction() == OpInputAction::ACTION_PRINT_DOCUMENT)
	{
		OpInputAction *new_action = OP_NEW(OpInputAction, ());
		if (new_action && OpStatus::IsSuccess(new_action->Clone(action)))
			g_main_message_handler->PostDelayedMessage(MSG_QUICK_DELAYED_ACTION, (MH_PARAM_1)new_action, 0, 0);
		else
			OP_DELETE(new_action);
		return TRUE;
	}
	return FALSE;
}

void OperaMenuDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_tabs && m_tabs->GetSelected() != -1)
	{
		SetCurrentPage(m_tabs->GetSelected());
	}
	else if (widget == GetWidgetByName("zoom_slider"))
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_ZOOM_TO, widget->GetValue());
	}
}

void OperaMenuDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (m_window_animation && widget == GetWidgetByName("zoom_slider"))
	{
		if (down)
			m_window_animation->animateOpacity(1.0, 0.8);
		else
			m_window_animation->animateOpacity(0.8, 1.0);
		g_animation_manager->abortAnimation(m_window_animation);
		g_animation_manager->startAnimation(m_window_animation, QuickAnimationManager::ANIMATION_BEZIER);
	}
}

void OperaMenuDialog::OnMouseMove(OpWidget *widget, const OpPoint &point)
{
	if (widget->GetParent() == m_tabs)
	{
		int index = m_tabs->FindWidget(widget);
		if (index != GetCurrentPage())
		{
			m_hovered_index = index;
			m_timer.Start(200);
		}
	}
}

void OperaMenuDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if (widget->GetParent() == m_tabs)
		SetCurrentPage(m_hovered_index);
	else if (widget->GetType() == WIDGET_TYPE_BUTTON)
	{
		if (widget->GetAction() && widget->GetAction()->GetAction() == OpInputAction::ACTION_SHOW_POPUP_MENU)
			return;
		Show(FALSE);
		CloseDialog(FALSE);
	}
}

void OperaMenuDialog::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_timer)
	{
		if (m_tabs->GetWidget(m_hovered_index) == g_widget_globals->hover_widget)
			SetCurrentPage(m_hovered_index);
	}
	else
		Dialog::OnTimeOut(timer);
}

void OperaMenuDialog::OnActivate(BOOL activate, BOOL first_time)
{
	Dialog::OnActivate(activate, first_time);
	if (!activate)
		CloseDialog(FALSE);
}

INT32 OperaMenuDialog::GetButtonCount()
{
	return 1;
}

void OperaMenuDialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	is_enabled = TRUE;
	is_default = FALSE;
	action = OP_NEW(OpInputAction, (OpInputAction::ACTION_EXIT, 0, 0));
	g_languageManager->GetString(Str::MI_IDM_Exit, text);
	name.Set(WIDGET_NAME_BUTTON_OK);
}

#endif // QUICK_NEW_OPERA_MENU
