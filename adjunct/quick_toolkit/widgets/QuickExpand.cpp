/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickExpand.h"

QuickExpand::QuickExpand()
	: m_is_visible(TRUE)
{
}

QuickExpand::~QuickExpand()
{
	m_expand_button.GetOpWidget()->RemoveListener(this);
}

OP_STATUS QuickExpand::Init()
{
	RETURN_IF_ERROR(m_expand_button.Init());
	m_expand_button.SetContainer(this);
	DesktopToggleButton* button = m_expand_button.GetOpWidget();
	
	button->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
	button->SetTabStop(TRUE);
	RETURN_IF_ERROR(button->AddToggleState(OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_EXPAND)), UNI_L(""), "Disclosure Triangle Skin", 0));
	RETURN_IF_ERROR(button->AddToggleState(OP_NEW(OpInputAction, (OpInputAction::ACTION_CLOSE_EXPAND)), UNI_L(""), "Disclosure Triangle Skin.selected", 1));
	RETURN_IF_ERROR(button->AddListener(this));
	button->GetBorderSkin()->SetImage("Expand Hover Skin");

	return OpStatus::OK;
}

OP_STATUS QuickExpand::SetText(const OpStringC& expand_text, const OpStringC& collapse_text)
{
	DesktopToggleButton* button = m_expand_button.GetOpWidget();
	button->SetToggleStateText(expand_text,   0);
	button->SetToggleStateText(collapse_text, 1);

	return OpStatus::OK;
}


void QuickExpand::SetContent(QuickWidget* content)
{
	m_content = content;
	m_content->SetParentOpWidget(m_expand_button.GetOpWidget()->GetParent());
	m_content->SetContainer(this);
	UpdateContent();
	BroadcastContentsChanged();
}

void QuickExpand::SetName(const OpStringC8& name)
{
	m_expand_button.GetOpWidget()->SetName(name);
}

OP_STATUS QuickExpand::Layout(const OpRect& rect)
{
	OpRect button_rect = rect;
	button_rect.height = m_expand_button.GetMinimumHeight(rect.width);
	m_expand_button.GetOpWidget()->SetRect(button_rect, TRUE, FALSE);

	if (m_content->IsVisible())
	{
		OpRect content_rect = rect;
		const int dy = button_rect.height + GetVerticalSpacing();
		content_rect.y += dy;
		content_rect.height -= dy;

		content_rect.width  = min(unsigned(content_rect.width ), m_content->GetPreferredWidth ()),
		content_rect.height = min(unsigned(content_rect.height), m_content->GetPreferredHeight());

		RETURN_IF_ERROR(m_content->Layout(content_rect, rect));
	}

	return OpStatus::OK;
}

void QuickExpand::SetParentOpWidget(class OpWidget* parent)
{
	m_expand_button.SetParentOpWidget(parent);
	m_content->SetParentOpWidget(parent);
}

void QuickExpand::Show()
{
	m_is_visible = TRUE;

	m_expand_button.Show();
	m_content->Show();
}

void QuickExpand::Hide()
{
	m_is_visible = FALSE;

	m_expand_button.Hide();
	m_content->Hide();
}

void QuickExpand::SetEnabled(BOOL enabled)
{
	m_expand_button.SetEnabled(enabled);
	m_content->SetEnabled(enabled);
}

unsigned QuickExpand::GetDefaultMinimumWidth()
{
	return max(m_expand_button.GetMinimumWidth(), m_content->GetMinimumWidth());
}

unsigned QuickExpand::GetDefaultMinimumHeight(unsigned width)
{
	unsigned height = m_expand_button.GetMinimumHeight(width);
	if (m_content->IsVisible())
		height += GetVerticalSpacing() + m_content->GetMinimumHeight(width);

	return height;
}

unsigned QuickExpand::GetDefaultNominalWidth()
{
	return max(m_expand_button.GetNominalWidth(), m_content->GetNominalWidth());
}

unsigned QuickExpand::GetDefaultNominalHeight(unsigned width)
{
	unsigned height = m_expand_button.GetNominalHeight(width);
	if (m_content->IsVisible())
		height += GetVerticalSpacing() + m_content->GetNominalHeight(width);

	return height;
}

unsigned QuickExpand::GetDefaultPreferredWidth()
{
	return max(m_expand_button.GetPreferredWidth(), m_content->GetPreferredWidth());
}

unsigned QuickExpand::GetDefaultPreferredHeight(unsigned width)
{
	unsigned height = m_expand_button.GetPreferredHeight(width);
	if (m_content->IsVisible())
		height += GetVerticalSpacing() + m_content->GetPreferredHeight(width);

	return height;
}

void QuickExpand::GetDefaultMargins(WidgetSizes::Margins& margins)
{
	margins = m_expand_button.GetMargins();
	if (m_content->IsVisible())
		margins.bottom = m_content->GetMargins().bottom;
}

int QuickExpand::GetVerticalSpacing()
{
	return max(m_expand_button.GetMargins().bottom, m_content->GetMargins().top);
}

void QuickExpand::UpdateContent()
{
	if (IsExpanded())
	{
		m_content->Show();
	}
	else
	{
		// Prevent the expand button from shrinking when collapsing.
		m_expand_button.SetMinimumWidth(WidgetSizes::UseDefault);
		m_expand_button.SetNominalWidth(WidgetSizes::UseDefault);
		m_expand_button.SetPreferredWidth(WidgetSizes::UseDefault);

		m_expand_button.SetMinimumWidth(max(m_expand_button.GetMinimumWidth(), m_content->GetMinimumWidth()));
		m_expand_button.SetNominalWidth(max(m_expand_button.GetNominalWidth(), m_content->GetNominalWidth()));
		m_expand_button.SetPreferredWidth(max(m_expand_button.GetPreferredWidth(), m_content->GetPreferredWidth()));

		m_content->Hide();
	}
}
