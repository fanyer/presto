/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "adjunct/quick/widgets/OpWandStoreBar.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/wand/wandmanager.h"
#include "adjunct/quick/managers/AnimationManager.h"

#define WANDSTOREBAR_AUTOHIDE_DELAY 15000 // Time until the toolbar will hide automatically.

DEFINE_CONSTRUCT(OpWandStoreBar)

OpWandStoreBar::OpWandStoreBar()
	: m_info(NULL)
	, m_timer(NULL)
{
	SetListener(this);
	SetWrapping(OpBar::WRAPPING_OFF);
	SetShrinkToFit(TRUE);
	//SetFixedHeight(FIXED_HEIGHT_BUTTON_AND_EDIT);
	SetStandardToolbar(FALSE);
	GetBorderSkin()->SetImage("Infobar Toolbar Skin");
	SetName("Wand Store Toolbar");
}

OpWandStoreBar::~OpWandStoreBar()
{
	OP_DELETE(m_timer);
	if (m_info)
		m_info->ReportAction(WAND_DONT_STORE);
}

void OpWandStoreBar::Show(WandInfo* info)
{
	if (m_info && m_info != info)
		m_info->ReportAction(WAND_DONT_STORE);

	m_info = info;
	SetAlignmentAnimated(OpBar::ALIGNMENT_OLD_VISIBLE);

	if (!m_timer)
		m_timer = OP_NEW(OpTimer, ());
	RestartHideTimer();
}

void OpWandStoreBar::RestartHideTimer()
{
	if (m_timer)
	{
		m_timer->SetTimerListener(this);
		m_timer->Start(WANDSTOREBAR_AUTOHIDE_DELAY);
	}
}

void OpWandStoreBar::Hide(BOOL focus_page)
{
	if (m_timer)
		m_timer->Stop();
	SetAlignmentAnimated(OpBar::ALIGNMENT_OFF);

	if (focus_page)
		g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);
}

void OpWandStoreBar::OnAlignmentChanged()
{
	if (GetAlignment() == ALIGNMENT_OFF)
	{
		if (m_info)
		{
			m_info->ReportAction(WAND_DONT_STORE);
			m_info = NULL;
		}
	}
}

BOOL OpWandStoreBar::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CANCEL:
			Hide();
			break;
		case OpInputAction::ACTION_WAND_SAVE:
		case OpInputAction::ACTION_WAND_NEVER_SAVE:
		{
			if (m_info)
			{
#ifdef WAND_ECOMMERCE_SUPPORT
				// If this trigs, this method is now implemented in core and we should show the ConfirmWandECommerceDialog dialog like we used to do!
				OP_ASSERT(!m_info->page->HasChangedECommerceData());
#endif

				BOOL entire_server = TRUE;
				if (OpWidget *checkbox = GetWidgetByType(OpTypedObject::WIDGET_TYPE_CHECKBOX, FALSE))
					entire_server = checkbox->GetValue() ? FALSE : TRUE;

				if (action->GetAction() == OpInputAction::ACTION_WAND_SAVE)
					m_info->ReportAction(entire_server ? WAND_STORE_ENTIRE_SERVER : WAND_STORE);
				else
					m_info->ReportAction(entire_server ? WAND_NEVER_STORE_ON_ENTIRE_SERVER : WAND_NEVER_STORE_ON_THIS_PAGE);
				m_info = NULL;
			}
			Hide();
			break;
		}
	};
	return OpToolbar::OnInputAction(action);
}

void OpWandStoreBar::OnTimeOut(OpTimer* timer)
{
	if (timer == m_timer)
	{
		if (IsFocusedOrHovered())
			// If focused or hovered we probably shouldn't hide it now. Restart the timer.
			RestartHideTimer();
		else
			Hide(FALSE);
	}
	else
		OpToolbar::OnTimeOut(timer);
}
