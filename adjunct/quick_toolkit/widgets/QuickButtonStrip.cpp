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
#include "adjunct/quick_toolkit/widgets/QuickButtonStrip.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "modules/pi/ui/OpUiInfo.h"

QuickButtonStrip::QuickButtonStrip(Order order)
	: m_order(order == SYSTEM ?
		(g_op_ui_info->DefaultButtonOnRight() ? REVERSE : NORMAL) : order)
	, m_content(0)
	, m_primary(0)
{
}

QuickButtonStrip::~QuickButtonStrip() 
{ 
	OP_DELETE(m_content); // this also deletes m_primary automatically
}

OP_STATUS QuickButtonStrip::Init()
{
	m_content = OP_NEW(QuickStackLayout, (QuickStackLayout::HORIZONTAL));
	RETURN_OOM_IF_NULL(m_content);
	RETURN_IF_ERROR(m_content->InsertWidget((QuickWidget*)0)); // secondary content
	RETURN_IF_ERROR(m_content->InsertEmptyWidget(0,0,WidgetSizes::Fill,WidgetSizes::Fill)); // middle spacer

	m_primary = OP_NEW(QuickStackLayout, (QuickStackLayout::HORIZONTAL));
	if (!m_primary)
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_content->InsertWidget(m_primary)); // primary content, within a stacklayout

	return OpStatus::OK;
}

OP_STATUS QuickButtonStrip::SetSecondaryContent(QuickWidget* widget)
{
	m_content->RemoveWidget(0);
	RETURN_IF_ERROR(m_content->InsertWidget(widget,0));
	BroadcastContentsChanged();
	return OpStatus::OK;
}

void QuickButtonStrip::SetContainer(QuickWidgetContainer* container)
{
	QuickWidget::SetContainer(container);
	m_content->SetContainer(container);
}

void QuickButtonStrip::SetParentOpWidget(class OpWidget* parent)
{
	m_content->SetParentOpWidget(parent);
}

void QuickButtonStrip::Show()
{
	m_content->Show();
}

void QuickButtonStrip::Hide()
{
	m_content->Hide();
}

BOOL QuickButtonStrip::IsVisible()
{
	return m_content->IsVisible();
}

void QuickButtonStrip::SetEnabled(BOOL enabled)
{
	m_content->SetEnabled(enabled);
}

OP_STATUS QuickButtonStrip::InsertIntoPrimaryContent(QuickWidget* widget)
{
	int pos = -1; // normally, add to the tail
	if (m_order == REVERSE)
		pos = 0; // but in reverse order, add to the head.
	RETURN_IF_ERROR(m_primary->InsertWidget(widget,pos));
	BroadcastContentsChanged();
	return OpStatus::OK;
}

OP_STATUS QuickButtonStrip::Layout(const OpRect& rect)
{
	return m_content->Layout(rect);
}

unsigned QuickButtonStrip::GetDefaultMinimumWidth()
{
	return m_content->GetMinimumWidth();
}

unsigned QuickButtonStrip::GetDefaultMinimumHeight(unsigned width)
{
	return m_content->GetMinimumHeight(width);
}

unsigned QuickButtonStrip::GetDefaultPreferredWidth()
{
	return m_content->GetPreferredWidth();
}

unsigned QuickButtonStrip::GetDefaultPreferredHeight(unsigned width)
{
	return m_content->GetPreferredHeight(width);
}

void QuickButtonStrip::GetDefaultMargins(WidgetSizes::Margins& margins)
{
	margins = m_content->GetMargins();
}
