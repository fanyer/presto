/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "OpHelptooltip.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/widgets/OpButton.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkinManager.h"

DEFINE_CONSTRUCT(OpHelpTooltip);

OpHelpTooltip::OpHelpTooltip()
	: m_close_button(NULL)
	, m_more_button(NULL)
{
	SetSkinned(TRUE);
	GetBorderSkin()->SetImage("Help Tooltip Skin");
	SetHasCssBorder(FALSE);
}

OP_STATUS OpHelpTooltip::Init()
{
	RETURN_IF_ERROR(OpButton::Construct(&m_close_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	m_close_button->SetName("Close help tooltip");
	m_close_button->SetTabStop(TRUE);
	m_close_button->GetBorderSkin()->SetImage("Help Tooltip Close Button Skin");
	m_close_button->GetForegroundSkin()->SetImage("Speeddial Close");

	OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_CLOSE_HELP_TOOLTIP));
	if(!action)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_close_button->SetAction(action);
	AddChild(m_close_button);

	RETURN_IF_ERROR(OpButton::Construct(&m_more_button, OpButton::TYPE_LINK, OpButton::STYLE_IMAGE));
	m_more_button->SetName("Go to more help");

	OpString str;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_GET_MORE_HELP, str));
	m_more_button->SetText(str.CStr());
	m_more_button->SetTabStop(TRUE);

	m_more_button->SetVisibility(FALSE);
	m_more_button->SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);
	AddChild(m_more_button);
	RETURN_IF_ERROR(OpLabel::Construct(&m_label));
	m_label->SetWrap(TRUE);
	AddChild(m_label);

	//julienp: Fix for bug 255366. Set the buttons to the same color as the text (Not perfect, but it works)
	UINT32 color;

	if (GetSkinManager()->GetTextColor(GetBorderSkin()->GetImage(), &color, 0, GetBorderSkin()->GetType(), GetBorderSkin()->GetSize(), GetBorderSkin()->IsForeground()) == OpStatus::OK)
	{
		m_more_button->SetForegroundColor(color);
	}

	return OpStatus::OK;
}

OpHelpTooltip::~OpHelpTooltip()
{
}

OP_STATUS OpHelpTooltip::SetText(const uni_char* text)
{
	if (m_label)
		return m_label->SetText(text);
	else
		return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS OpHelpTooltip::GetText(OpString& text)
{
	if (m_label)
		return m_label->GetText(text);
	else
		return OpStatus::ERR_NULL_POINTER;
}

OP_STATUS OpHelpTooltip::SetHelpTopic(const OpStringC& topic,
		BOOL force_new_window)
{
	OpInputAction* action = OP_NEW(OpInputAction,(OpInputAction::ACTION_SHOW_HELP));
	RETURN_OOM_IF_NULL(action);
	action->SetActionData(!force_new_window);
	action->SetActionDataString(topic.CStr());
	return SetHelpAction(action);
}

OP_STATUS OpHelpTooltip::SetHelpUrl(const OpStringC& url_name)
{
	OpInputAction* action = OP_NEW(OpInputAction,(OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW));
	RETURN_OOM_IF_NULL(action);
	// FALSE for "URL didn't come from address field"
	action->SetActionData(FALSE);
	action->SetActionDataString(url_name.CStr());
	return SetHelpAction(action);
}

OP_STATUS OpHelpTooltip::SetHelpAction(OpInputAction* action)
{
	RETURN_OOM_IF_NULL(action);
	if (NULL == action->GetActionDataString()
			|| '\0' == action->GetActionDataString()[0])
	{
		m_more_button->SetVisibility(FALSE);
		OP_DELETE(action);
		return OpStatus::OK;
	}

	m_more_button->SetAction(action);
	m_more_button->SetVisibility(TRUE);
	return OpStatus::OK;
}

void OpHelpTooltip::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	if (!m_label || !m_close_button || !m_more_button)
	{
		*w = 0;
		*h = 0;
		return;
	}

	INT32 tmp_w, tmp_h;
	m_close_button->GetRequiredSize(tmp_w, tmp_h);
	m_more_button->GetRequiredSize(*w, *h);

	*w = max( *w, tmp_w );
	*h += tmp_h + 5;

	m_label->GetRequiredSize(tmp_w, tmp_h);

	*w += tmp_w + 3;
	*h = max( *h, tmp_h );

}

INT32 OpHelpTooltip::GetHeightForWidth(INT32 width)
{
	if (!m_label || !m_close_button || !m_more_button)
		return 0;
	INT32 tmp_w, tmp_h;
	INT32 btn_width, height;
	m_close_button->GetRequiredSize(tmp_w, tmp_h);
	m_more_button->GetRequiredSize(btn_width, height);

	btn_width = max( btn_width, tmp_w );
	height += tmp_h + 5;

	INT32 left, right, top, bottom;

	GetPadding(&left, &top, &right, &bottom);

	m_label->SetRect(OpRect(left, top, width-(btn_width+3), 0));

	return top+bottom+max(height, m_label->GetTextHeight());

}

void OpHelpTooltip::OnLayout()
{

	if (!m_label || !m_close_button || !m_more_button)
		return;

	OpRect bounds = GetBounds();

	INT32 close_w = 0, close_h = 0, more_w = 0, more_h = 0;

	if (m_close_button->IsVisible())
		m_close_button->GetRequiredSize(close_w, close_h);

	if (m_more_button->IsVisible())
		m_more_button->GetRequiredSize(more_w, more_h);

	INT32 left, right, top, bottom;

	GetPadding(&left, &top, &right, &bottom);
	bounds.width -= (left+right);
	bounds.height -= (top+bottom);


	m_label->SetRect(OpRect(left, top, bounds.width-(max( close_w, more_w )+3), bounds.height));
	m_close_button->SetRect(OpRect(left + bounds.width - close_w, top, close_w, close_h));
	m_more_button->SetRect(OpRect(left + bounds.width - more_w, top + bounds.height - more_h, more_w, more_h));

	//send event to listener
	OpWidget::OnLayout();
}

void OpHelpTooltip::OnFocus(BOOL focus, FOCUS_REASON reason)
{
	if (focus)
		m_close_button->SetFocus(reason);
}

void OpHelpTooltip::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if( old_input_context->GetParentInputContext() && 
		old_input_context->GetParentInputContext()->GetInputContextName() && 
		!uni_strcmp(old_input_context->GetParentInputContext()->GetInputContextName(), UNI_L("Help tooltip widget")))
	{
		SetVisibility(FALSE);
	}
}

BOOL OpHelpTooltip::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CLOSE_HELP_TOOLTIP:
		{
			SetVisibility(FALSE);
			return TRUE;
		}
		default:
			break;
	}

	return OpWidget::OnInputAction(action);
}
