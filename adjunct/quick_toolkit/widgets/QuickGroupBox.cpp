/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */
 
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickGroupBox.h"

#include "modules/skin/OpSkinManager.h"

QuickGroupBox::QuickGroupBox() 
	: m_visible(TRUE)
	, m_left_padding(0)
{
}

OP_STATUS QuickGroupBox::Init()
{
	RETURN_IF_ERROR(m_label.Init());
	m_label.SetContainer(this);

	INT32 left,top,right,bottom; //temporary variables
	
	// Take the left padding information from the skin manager.
	g_skin_manager->GetPadding("Group Box Skin",&left, &top, &right, &bottom);
	m_left_padding = left;
	
	return OpStatus::OK;
}

void QuickGroupBox::SetContent(QuickWidget* widget)
{
	m_content = widget;
	m_content->SetContainer(this);
	
	BroadcastContentsChanged();
}

void QuickGroupBox::SetParentOpWidget(class OpWidget* parent)
{
	m_label.SetParentOpWidget(parent);
	m_content->SetParentOpWidget(parent);
}

void QuickGroupBox::Show()
{
	m_label.Show();
	m_content->Show();
}

void QuickGroupBox::Hide()
{
	m_label.Hide();
	m_content->Hide();
}

BOOL QuickGroupBox::IsVisible()
{
	return m_visible;
}

void QuickGroupBox::SetEnabled(BOOL enabled)
{
	m_label.SetEnabled(enabled);
	m_content->SetEnabled(enabled);
}

OP_STATUS QuickGroupBox::Layout(const OpRect& rect)
{
	// Label should be placed left top of the group box
	OpRect label_rect = rect;
	label_rect.height = m_label.GetMinimumHeight();
	RETURN_IF_ERROR(m_label.Layout(label_rect));

	OpRect content_rect;
	content_rect.x = rect.x + m_left_padding; // the content must be indented
	content_rect.y = rect.y + label_rect.height
			+ max(m_label.GetMargins().bottom, m_content->GetMargins().top);
	content_rect.height = rect.height - (content_rect.y - rect.y);
	content_rect.width	= rect.width - m_left_padding;

	return m_content->Layout(content_rect, rect);
}

unsigned QuickGroupBox::GetDefaultMinimumWidth()
{
	return max(m_content->GetMinimumWidth() + m_left_padding, m_label.GetMinimumWidth());
}

unsigned QuickGroupBox::GetDefaultMinimumHeight(unsigned width)
{
	return m_content->GetMinimumHeight(width)
			+ max(m_label.GetMargins().bottom, m_content->GetMargins().top)
			+ m_label.GetMinimumHeight();
}

unsigned QuickGroupBox::GetDefaultNominalWidth()
{
	return max(m_content->GetNominalWidth() + m_left_padding, m_label.GetNominalWidth());
}

unsigned QuickGroupBox::GetDefaultNominalHeight(unsigned width)
{
	return m_content->GetNominalHeight(width)
			+ max(m_label.GetMargins().bottom, m_content->GetMargins().top)
			+ m_label.GetNominalHeight();
}

unsigned QuickGroupBox::GetDefaultPreferredWidth()
{
	const unsigned preferred_width = m_content->GetPreferredWidth();
	// to keep away from overflowing
	if (preferred_width > WidgetSizes::UseDefault) 
		return preferred_width;
		
	return max(preferred_width + m_left_padding, m_label.GetPreferredWidth());
}

unsigned QuickGroupBox::GetDefaultPreferredHeight(unsigned width)
{
	const unsigned preferred_height = m_content->GetPreferredHeight(width);
	// to keep away from overflowing
	if (preferred_height > WidgetSizes::UseDefault) 
		return preferred_height;

	return preferred_height
			+ max(m_label.GetMargins().bottom, m_content->GetMargins().top)
			+ m_label.GetPreferredHeight();
}

void QuickGroupBox::GetDefaultMargins(WidgetSizes::Margins& margins)
{
	margins = m_content->GetMargins();

	const WidgetSizes::Margins label_margins = m_label.GetMargins();
	margins.top = label_margins.top;
	margins.left = label_margins.left;
}
