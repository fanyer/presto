/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickWidget.h"

#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"


QuickWidget::QuickWidget()
	: m_minimum_width(WidgetSizes::UseDefault)
	, m_minimum_height(WidgetSizes::UseDefault)
	, m_nominal_width(WidgetSizes::UseDefault)
	, m_nominal_height(WidgetSizes::UseDefault)
	, m_preferred_width(WidgetSizes::UseDefault)
	, m_preferred_height(WidgetSizes::UseDefault)
	, m_margins(WidgetSizes::UseDefault)
	, m_container(0)
{
}

unsigned QuickWidget::GetMinimumWidth()
{
	if (m_minimum_width != WidgetSizes::UseDefault)
		return m_minimum_width;

	return GetDefaultMinimumWidth();
}

unsigned QuickWidget::GetMinimumHeight(unsigned width)
{
	if (m_minimum_height != WidgetSizes::UseDefault)
		return m_minimum_height;

	return GetDefaultMinimumHeight(width);
}

unsigned QuickWidget::GetNominalWidth()
{
	if (m_nominal_width != WidgetSizes::UseDefault)
		return max(m_nominal_width, GetMinimumWidth());

	return max(GetDefaultNominalWidth(), GetMinimumWidth());
}

unsigned QuickWidget::GetNominalHeight(unsigned width)
{
	if (m_nominal_height != WidgetSizes::UseDefault)
		return max(m_nominal_height, GetMinimumHeight(width));

	return max(GetDefaultNominalHeight(width), GetMinimumHeight(width));
}

unsigned QuickWidget::GetPreferredWidth()
{
	if (m_preferred_width != WidgetSizes::UseDefault)
		return max(m_preferred_width, GetNominalWidth());

	return max(GetDefaultPreferredWidth(), GetNominalWidth());
}

unsigned QuickWidget::GetPreferredHeight(unsigned width)
{
	if (m_preferred_height != WidgetSizes::UseDefault)
		return max(m_preferred_height, GetNominalHeight(width));

	return max(GetDefaultPreferredHeight(width), GetNominalHeight(width));
}

WidgetSizes::Margins QuickWidget::GetMargins()
{
	WidgetSizes::Margins margins;
	GetDefaultMargins(margins);

	if (m_margins.left != WidgetSizes::UseDefault)
		margins.left = m_margins.left;
	if (m_margins.right != WidgetSizes::UseDefault)
		margins.right = m_margins.right;
	if (m_margins.top != WidgetSizes::UseDefault)
		margins.top = m_margins.top;
	if (m_margins.bottom != WidgetSizes::UseDefault)
		margins.bottom = m_margins.bottom;

	return margins;
}

OP_STATUS QuickWidget::Layout(const OpRect& rect, const OpRect& container_rect)
{
	OpRect adapted_rect = rect;
	if (UiDirection::Get() == UiDirection::RTL)
		adapted_rect.x = container_rect.x + (container_rect.Right() - rect.Right());
	return Layout(adapted_rect);
}

void QuickWidget::BroadcastContentsChanged()
{
	if (m_container)
		return m_container->OnContentsChanged();
}
