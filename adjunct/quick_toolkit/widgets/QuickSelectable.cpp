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
#include "adjunct/quick_toolkit/widgets/QuickSelectable.h"
#include "modules/skin/OpSkinManager.h"


QuickSelectable::~QuickSelectable()
{
	RemoveOpWidgetListener(*this);
}

OP_STATUS QuickSelectable::Init()
{
	RETURN_IF_ERROR(Base::Init());
	RETURN_IF_ERROR(ConstructSelectableQuickWidget());
	RETURN_IF_ERROR(AddOpWidgetListener(*this));
	return OpStatus::OK;
}

void QuickSelectable::SetParentOpWidget(OpWidget* widget)
{
	Base::SetParentOpWidget(widget);
	m_child->SetParentOpWidget(widget);
}

void QuickSelectable::SetChild(QuickWidget* new_child)
{
	new_child->SetParentOpWidget(GetOpWidget()->GetParent());
	m_child = new_child;
	m_child->SetEnabled(GetOpWidget()->GetValue());
	BroadcastContentsChanged();
}

void QuickSelectable::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	m_child->SetEnabled(widget->GetValue());
}

OP_STATUS QuickSelectable::FindImageSizes(const char* widget_type)
{
	INT32 dummy_height;
	RETURN_IF_ERROR(GetOpWidget()->GetSkinManager()->GetSize(widget_type, &m_image_width, &dummy_height));

	INT32 top, left, bottom, right;
	if (OpStatus::IsSuccess(GetOpWidget()->GetSkinManager()->GetMargin(widget_type, &left, &top, &right, &bottom)))
		GetOpWidget()->SetMargins(left, top, right, bottom);

	return OpStatus::OK; 
}

OP_STATUS QuickSelectable::Layout(const OpRect& rect)
{
	// selectable_rect defines the area of QuickSelectable that can toggle/change
	// the state of it via mouse clicks.
	OpRect selectable_rect = rect;
	selectable_rect.height = Base::GetDefaultMinimumHeight(rect.width);
	GetOpWidget()->SetRect(selectable_rect, TRUE, FALSE);

	OpRect child_rect = rect;
	const int dx = GetHorizontalSpacing();
	const int dy = selectable_rect.height + GetVerticalSpacing();
	child_rect.x += dx;
	child_rect.width -= dx;
	child_rect.y += dy;
	child_rect.height -= dy;

	child_rect.width = min(unsigned(child_rect.width), m_child->GetPreferredWidth()),
	child_rect.height = min(unsigned(child_rect.height), m_child->GetPreferredHeight());

	return m_child->Layout(child_rect, rect);
}

unsigned QuickSelectable::GetDefaultMinimumWidth()
{
	return max(Base::GetDefaultMinimumWidth(), GetHorizontalSpacing() + m_child->GetMinimumWidth());
}

unsigned QuickSelectable::GetDefaultMinimumHeight(unsigned width)
{
	return Base::GetDefaultMinimumHeight(width) + GetVerticalSpacing() + m_child->GetMinimumHeight(width);
}

unsigned QuickSelectable::GetDefaultNominalWidth()
{
	return max(Base::GetDefaultNominalWidth(), GetHorizontalSpacing() + m_child->GetNominalWidth());
}

unsigned QuickSelectable::GetDefaultNominalHeight(unsigned width)
{
	return Base::GetDefaultNominalHeight(width) + GetVerticalSpacing() + m_child->GetNominalHeight(width);
}

unsigned QuickSelectable::GetDefaultPreferredWidth()
{
	// This check is to avoid overflow caused by WidgetSizes::Infinity and WidgetSizes::Fill
	if (m_child->GetPreferredWidth() > WidgetSizes::UseDefault)
		return m_child->GetPreferredWidth();

#ifdef QUICK_TOOLKIT_SELECTABLE_FILLS_BY_DEFAULT
	return WidgetSizes::Fill;
#else
	return max(Base::GetDefaultPreferredWidth(), GetHorizontalSpacing() + m_child->GetPreferredWidth());
#endif
}

unsigned QuickSelectable::GetDefaultPreferredHeight(unsigned width)
{
	// This check is to avoid overflow caused by WidgetSizes::Infinity and WidgetSizes::Fill
	if (m_child->GetPreferredHeight(width) > WidgetSizes::UseDefault)
		return m_child->GetPreferredHeight(width);

	return Base::GetDefaultPreferredHeight(width) + GetVerticalSpacing() + m_child->GetPreferredHeight(width);
}

void QuickSelectable::GetDefaultMargins(WidgetSizes::Margins& margins)
{
	Base::GetDefaultMargins(margins);

	const WidgetSizes::Margins& child_margins = m_child->GetMargins();
	margins.right = max(margins.right, child_margins.right);
	margins.bottom = max(margins.bottom, child_margins.bottom);
}
