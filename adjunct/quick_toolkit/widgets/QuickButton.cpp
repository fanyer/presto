/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickButton.h"

#include "modules/skin/OpSkinManager.h"
#include "modules/locale/oplanguagemanager.h"

OP_STATUS QuickButton::SetImage(const char* image_name)
{
	PrepareSkin()->SetImage(image_name);
	BroadcastContentsChanged();
	return OpStatus::OK;
}

OP_STATUS QuickButton::SetImage(Image image)
{
	PrepareSkin()->SetBitmapImage(image);
	BroadcastContentsChanged();
	return OpStatus::OK;
}

OpWidgetImage* QuickButton::PrepareSkin()
{
	GetOpWidget()->SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
	GetOpWidget()->SetFixedImage(TRUE);
	return GetOpWidget()->GetForegroundSkin();
}

OP_STATUS QuickButton::SetAction(const OpInputAction* action)
{
	if (!action)
	{
		GetOpWidget()->SetAction(0);
		return OpStatus::OK;
	}

	OpAutoPtr<OpInputAction> action_copy (OP_NEW(OpInputAction, ()));
	if (!action_copy.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(action_copy->Clone(const_cast<OpInputAction*>(action)));

	GetOpWidget()->SetAction(action_copy.release());
	return OpStatus::OK;
}

OpInputAction* QuickButton::GetAction()
{
	return GetOpWidget()->GetAction();
}

OP_STATUS QuickButton::SetContent(QuickWidget * content)
{
	OP_DELETE(m_content);
	
	m_content = content;
	if (m_content)
	{
		m_content->SetParentOpWidget(GetOpWidget());
	}

	MakeDescendantsNotRespondToInput(GetOpWidget());

	BroadcastContentsChanged();

	return OpStatus::OK;
}

void QuickButton::MakeDescendantsNotRespondToInput(OpWidget* widget)
{
	for (OpWidget* child = widget->GetFirstChild(); child; child = child->GetNextSibling())
	{
		child->SetIgnoresMouse(TRUE);
		MakeDescendantsNotRespondToInput(child);
	}
}

OP_STATUS QuickButton::Layout(const OpRect& rect)
{
	GetOpWidget()->SetRect(rect, TRUE, FALSE);

	if (m_content)
	{
		OpRect rect_with_margins;
		WidgetSizes::Margins margins = m_content->GetMargins();
		rect_with_margins.x = margins.left;
		rect_with_margins.width = rect.width - (margins.left + margins.right);
		rect_with_margins.y = margins.top;
		rect_with_margins.height = rect.height - (margins.top + margins.bottom);

		RETURN_IF_ERROR(m_content->Layout(rect_with_margins, rect));
	}

	return OpStatus::OK;
}

unsigned QuickButton::GetDefaultMinimumWidth()
{
	if (m_content)
		return m_content->GetMinimumWidth() + m_content->GetMargins().left + m_content->GetMargins().right;

	OpString text;
	OpStatus::Ignore(GetText(text));
	
	INT32 w = 0, h = 0;
	if (text.HasContent())
		GetOpWidget()->GetBorderSkin()->GetSize(&w, &h);
	return max(GetOpWidgetPreferredWidth(), (unsigned)w);
}

unsigned QuickButton::GetDefaultMinimumHeight(unsigned width)
{
	if (m_content)
		return m_content->GetMinimumHeight(width) + m_content->GetMargins().top + m_content->GetMargins().bottom;

	INT32 w = 0, h = 0;
	GetOpWidget()->GetBorderSkin()->GetSize(&w, &h);
	return max(GetOpWidgetPreferredHeight(), (unsigned)h);
}

unsigned QuickButton::GetDefaultPreferredWidth()
{
	if (m_content)
		return m_content->GetPreferredWidth() + m_content->GetMargins().left + m_content->GetMargins().right;
	else
		return GetDefaultMinimumWidth();
}

unsigned QuickButton::GetDefaultPreferredHeight(unsigned width)
{
	if (m_content)
		return m_content->GetPreferredHeight() + m_content->GetMargins().top + m_content->GetMargins().bottom;
	else
		return GetDefaultMinimumHeight(width);
}

QuickButton* QuickButton::ConstructButton(
		Str::LocaleString string_id, OpInputAction* action)
{
	OpAutoPtr<QuickButton> button(OP_NEW(QuickButton, ()));
	RETURN_VALUE_IF_NULL(button.get(), NULL);
	RETURN_VALUE_IF_ERROR(button->Init(), NULL);
	OpString text;
	RETURN_VALUE_IF_ERROR(g_languageManager->GetString(string_id, text), NULL);
	RETURN_VALUE_IF_ERROR(button->SetText(text), NULL);
	button->SetAction(action);
	return button.release();
}

QuickButton* QuickButton::ConstructButton(OpInputAction* action)
{
	OpAutoPtr<QuickButton> button(OP_NEW(QuickButton, ()));
	RETURN_VALUE_IF_NULL(button.get(), NULL);
	RETURN_VALUE_IF_ERROR(button->Init(), NULL);
	button->SetAction(action);
	return button.release();
}
