/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "OpInfobar.h"

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/widgets/OpStatusField.h"

/***********************************************************************************
**
**	OpInfobar
**
***********************************************************************************/

OP_STATUS OpInfobar::Construct(OpInfobar** obj)
{
	*obj = OP_NEW(OpInfobar, ());
	if (*obj == NULL || OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpInfobar::OpInfobar(PrefsCollectionUI::integerpref prefs_setting, PrefsCollectionUI::integerpref autoalignment_prefs_setting)
	: OpToolbar(prefs_setting, autoalignment_prefs_setting)
	, m_info_button(NULL)
	, m_wrap(FALSE)
{
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityPrunesMe(ACCESSIBILITY_PRUNE_WHEN_INVISIBLE);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
}

OpInfobar::~OpInfobar()
{
}

OP_STATUS OpInfobar::Init(const char *toolbar_name)
{
	SetStandardToolbar(FALSE);

	SetName(toolbar_name);

	GetBorderSkin()->SetImage("Infobar Toolbar Skin");

	SetSkinned(TRUE);

	m_wrap = TRUE;

	return OpStatus::OK;
}

OP_STATUS OpInfobar::CreateInfoButton()
{
	if(!m_info_button)
	{
		RETURN_IF_ERROR(OpButton::Construct(&m_info_button));
		AddChild(m_info_button, TRUE, TRUE);

		m_info_button->SetButtonTypeAndStyle(OpButton::TYPE_TOOLBAR, OpButton::STYLE_IMAGE);
		m_info_button->SetRestrictImageSize(FALSE);
		m_info_button->SetDead(TRUE);
	}
	return OpStatus::OK;
}

void OpInfobar::SetInfoImage(const char *image)
{
	RETURN_VOID_IF_ERROR(CreateInfoButton());

	if(m_info_button)
	{
		m_info_button->GetForegroundSkin()->SetImage(image);
	}
}

void OpInfobar::OnReadContent(PrefsSection *section)
{
	OpToolbar::OnReadContent(section);
	if (m_wrap)
		SetStatusTextWrap(TRUE);
	if (m_status_text.HasContent())
		SetStatusText(m_status_text);
	if (m_question_text.HasContent())
		SetQuestionText(m_question_text);
}

OP_STATUS OpInfobar::SetStatusText(OpStringC text)
{
	RETURN_IF_ERROR(m_status_text.Set(text));

	OpStatusField* label = NULL;

	// Allow fallback for older ini-files without named widget
	const char *name = GetStatusFieldName();
	if (name && *name)
		label = static_cast<OpStatusField*> (GetWidgetByName(name));
	else
		label = static_cast<OpStatusField*> (GetWidgetByType(OpTypedObject::WIDGET_TYPE_STATUS_FIELD, FALSE));

	if (label)
		return label->SetText(m_status_text);

	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS OpInfobar::SetQuestionText(OpStringC text)
{
	RETURN_IF_ERROR(m_question_text.Set(text));

	// No old ini file use this widget. Name is mandatory
	const char *name = GetQuestionFieldName();
	if (name && *name)
	{
		OpStatusField* label = static_cast<OpStatusField*> (GetWidgetByName(name));
		if (label)
		{
			label->SetHidden(FALSE);
			return label->SetText(text);
		}
	}

	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

void OpInfobar::SetStatusTextWrap(BOOL wrap)
{
	m_wrap = wrap;

	OpStatusField* label = 0;

	// Allow fallback for older ini-files without named widget
	const char *name = GetStatusFieldName();
	if (name && *name)
		label = static_cast<OpStatusField*> (GetWidgetByName(name));
	else
		label = static_cast<OpStatusField*> (GetWidgetByType(OpTypedObject::WIDGET_TYPE_STATUS_FIELD, FALSE));
	if (label)
		label->SetWrap(wrap);
}

void OpInfobar::ShowQuestionText(BOOL show)
{
	// No old ini file use this widget. Name is mandatory
	const char *name = GetQuestionFieldName();
	if (name && *name)
	{
		OpStatusField* label = static_cast<OpStatusField*> (GetWidgetByName(name));
		if (label)
			label->SetHidden(!show);
	}
}


OP_STATUS OpInfobar::SetInfoButtonActionString(OpString8& straction)
{
	RETURN_IF_ERROR(CreateInfoButton());

	OpInputAction *action;
	OP_STATUS s = OpInputAction::CreateInputActionsFromString(straction.CStr(), action);
	if(OpStatus::IsSuccess(s))
	{
		m_info_button->SetAction(action);
		m_info_button->SetDead(FALSE);
	}
	return s;
}
