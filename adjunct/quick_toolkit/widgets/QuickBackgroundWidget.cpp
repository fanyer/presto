/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickBackgroundWidget.h"


OP_STATUS GenericQuickBackgroundWidget::Init()
{
	OpWidget* opwidget = CreateOpWidget();
	RETURN_OOM_IF_NULL(opwidget);
	SetOpWidget(opwidget);

	return OpStatus::OK;
}

void GenericQuickBackgroundWidget::SetContent(QuickWidget* content)
{
	m_content.SetContent(content);
	m_content->SetContainer(this);
	m_content->SetParentOpWidget(GetOpWidget());
}

OP_STATUS GenericQuickBackgroundWidget::Layout(const OpRect& rect)
{
	RETURN_IF_ERROR(GenericQuickOpWidgetWrapper::Layout(rect));

	const OpRect absolute_rect(0, 0, rect.width, rect.height);
	OpRect content_rect = absolute_rect;
	if (m_add_padding)
	{
		GetOpWidget()->AddPadding(content_rect);
		content_rect.width = max(0, content_rect.width);
		content_rect.height = max(0, content_rect.height);

		if (m_dynamic_padding)
		{
			unsigned preferred_width = m_content->GetPreferredWidth();
			if (preferred_width < (unsigned)rect.width)
			{
				content_rect.width = max(content_rect.width, (int)preferred_width);
				content_rect.x  = min (content_rect.x, (rect.width - content_rect.width) / 2);
			}
			else
			{
				content_rect.width = rect.width;
				content_rect.x = 0;
			}

			unsigned preferred_height = m_content->GetPreferredHeight(content_rect.width);
			if (preferred_height < (unsigned)rect.height)
			{
				content_rect.height = max(content_rect.height, (int)preferred_height);
				content_rect.y  = min (content_rect.y, (rect.height - content_rect.height) / 2);
			}
			else
			{
				content_rect.height = rect.height;
				content_rect.y = 0;
			}
		}
	}

	return m_content->Layout(content_rect, absolute_rect);
}

unsigned GenericQuickBackgroundWidget::GetDefaultPreferredWidth()
{
	unsigned width = m_content->GetPreferredWidth();
	if (width >= WidgetSizes::UseDefault)
		return width;

	return width + GetHorizontalPadding();
}

unsigned GenericQuickBackgroundWidget::GetDefaultPreferredHeight(unsigned width)
{
	unsigned height = m_content->GetPreferredHeight(GetContentWidth(width));
	if (height >= WidgetSizes::UseDefault)
		return height;

	return height + GetVerticalPadding();
}
