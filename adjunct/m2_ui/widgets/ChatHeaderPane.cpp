/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2_ui/widgets/ChatHeaderPane.h"
#include "adjunct/quick_toolkit/widgets/OpImageWidget.h"

/***********************************************************************************
**
**	ChatHeaderPane
**
***********************************************************************************/

ChatHeaderPane::ChatHeaderPane(const char* skin) :
	m_header_webimage(NULL),
	m_has_header_webimage(FALSE)
{
	OP_STATUS status = OpToolbar::Construct(&m_header_toolbar);
	CHECK_STATUS(status);
	AddChild(m_header_toolbar);

	status = OpToolbar::Construct(&m_toolbar);
	CHECK_STATUS(status);
	AddChild(m_toolbar);

	GetBorderSkin()->SetImage(skin);
	GetForegroundSkin()->SetForeground(FALSE);
	SetSkinned(TRUE);

	m_header_toolbar->GetBorderSkin()->SetImage(NULL);
	m_toolbar->GetBorderSkin()->SetImage(NULL);
}

void ChatHeaderPane::GetImageRect(OpRect& rect)
{
	if  (m_header_toolbar->GetResultingAlignment() == OpBar::ALIGNMENT_TOP && m_toolbar->GetResultingAlignment() == OpBar::ALIGNMENT_TOP && ((m_header_webimage && m_has_header_webimage) || GetForegroundSkin()->HasContent()))
	{
		INT32 default_width = 80;
		INT32 calculated_right = 0;
		INT32 left, top, bottom, right;

		GetForegroundSkin()->GetMargin(&left, &top, &right, &bottom);

		// compatibility workaround as the default allocated width for the image in the layout was always 80 previously
		if(right != 0)
		{
			calculated_right = default_width + right;
			if(calculated_right < 0)
			{
				calculated_right = 0;
			}
		}
		else
		{
			calculated_right = 80;
		}
		rect.Set(left, top, calculated_right, 60 + bottom);
	}
	else
		rect.Set(0,0,0,0);
}

INT32 ChatHeaderPane::GetHeightFromWidth(INT32 width)
{
	OpRect rect;

	GetImageRect(rect);

	width -= rect.x + rect.width;

	INT32 header_toolbar_height = m_header_toolbar->IsOn() ? m_header_toolbar->GetHeightFromWidth(width) : 0;
	INT32 toolbar_height = m_toolbar->IsOn() ? m_toolbar->GetHeightFromWidth(width) : 0;

	INT32 height = toolbar_height + header_toolbar_height;

	return rect.height > height ? rect.height : height;
}

void ChatHeaderPane::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	GetForegroundSkin()->Draw(vis_dev, OpPoint(0, 0));
}

void ChatHeaderPane::OnLayout()
{
	OpRect rect = GetBounds();
	OpRect image_rect;

	GetImageRect(image_rect);

	rect.x += image_rect.x + image_rect.width;
	rect.width -= image_rect.x + image_rect.width;

	rect = m_header_toolbar->LayoutToAvailableRect(rect);
	rect = m_toolbar->LayoutToAvailableRect(rect);

	image_rect.height = rect.y;

	if (m_header_webimage)
	{
		m_header_webimage->SetRect(image_rect.InsetBy(2,2));
		m_header_webimage->SetVisibility(!image_rect.IsEmpty());
	}
}

void ChatHeaderPane::SetHeaderToolbar(const char* header_toolbar_name)
{
	m_header_toolbar->SetName(header_toolbar_name);
	Relayout();
}

void ChatHeaderPane::SetToolbar(const char* toolbar_name)
{
	if (m_toolbar->GetName().Compare(toolbar_name) != 0)
	{
		m_toolbar->SetName(toolbar_name);
		Relayout();
	}
}

void ChatHeaderPane::SetImage(const char* skin_image, const uni_char* url_image)
{
	if (url_image && *url_image)
	{
		if (!m_header_webimage)
		{
			if (0 != (m_header_webimage = OP_NEW(OpWebImageWidget, (FALSE))))
				AddChild(m_header_webimage, TRUE, TRUE);
		}
		if (m_header_webimage)
		{
			m_header_webimage->SetImage(url_image);
			m_has_header_webimage = TRUE;
			GetForegroundSkin()->SetImage(NULL);
		}
	}
	else
	{
		GetForegroundSkin()->SetImage(skin_image);
		m_has_header_webimage = FALSE;
	}
	Relayout();
}


#endif // M2_SUPPORT