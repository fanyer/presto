/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/thumbnails/TabThumbnailContent.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"

#include "adjunct/desktop_util/widget_utils/LogoThumbnailClipper.h"
#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/quick_toolkit/widgets/OpToolTipListener.h"
#include "modules/display/vis_dev.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpButton.h"


namespace
{

class ContentButton : public OpButton
{
public:
	explicit ContentButton(OpToolTipThumbnailPair& data);

	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

private:
	OpToolTipThumbnailPair* m_data;
};

ContentButton::ContentButton(OpToolTipThumbnailPair& data)
	: OpButton(TYPE_CUSTOM, STYLE_IMAGE)
	, m_data(&data)
{
	GetBorderSkin()->SetImage("Thumbnail Image Skin");
	GetForegroundSkin()->SetBitmapImage(m_data->thumbnail);
}

void ContentButton::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	INT32 left = 0, top = 0, right = 0, bottom = 0;
	GetPadding(&left, &top, &right, &bottom);

	OpRect content_rect(left, top, rect.width - left - right, rect.height - top - bottom);
	OpRect clip_rect = content_rect;

	if (m_data->is_fixed_image)
	{
		OpRect src_rect(0, 0, m_data->thumbnail.Width(), m_data->thumbnail.Height());
		OpRect dst_rect(content_rect);

		SmartCropScaling crop_scaling;

		crop_scaling.SetOriginalSize(content_rect.width, content_rect.height);
		crop_scaling.GetCroppedRects(src_rect, dst_rect);

		if(dst_rect.width > src_rect.width && dst_rect.height > src_rect.height)
		{
			// we're not scaling the original image up, so we will center it instead
			dst_rect.width = src_rect.width;
			dst_rect.height = src_rect.height;

			dst_rect.x = content_rect.Center().x - (dst_rect.width / 2);
			dst_rect.y = content_rect.Center().y - (dst_rect.height / 2);
		}
		vis_dev->BeginClipping(content_rect);
		vis_dev->ImageOut(m_data->thumbnail, src_rect, dst_rect);
		vis_dev->EndClipping();
	}
	else
	{
		//  Crop off the bottom of our thumbnail, if necessary.

		// As paint_rect might not be the entire rect, we ignore it and just redraw the whole thing, using rect's dimensions.

		if (content_rect.width * 3 > content_rect.height * 4)
		{
			content_rect.height = content_rect.width * 3 / 4;
		}

		OpWidgetImage* thumbnail = GetForegroundSkin();
		thumbnail->SetState(0, 0, 0); // turn off glow
		thumbnail->Draw(vis_dev, content_rect, &clip_rect);
	}
}

} // anonymous namespace


TabThumbnailContent::TabThumbnailContent()
	: m_data(NULL)
	, m_button(NULL)
{
}

TabThumbnailContent::~TabThumbnailContent()
{
	if (m_button && !m_button->IsDeleted())
		m_button->Delete();

	OP_DELETE(m_data);
}

OP_STATUS TabThumbnailContent::Init(OpToolTipThumbnailPair& data)
{
	m_data = OP_NEW(OpToolTipThumbnailPair, ());
	RETURN_OOM_IF_NULL(m_data);
	RETURN_IF_ERROR(m_data->title.Set(data.title));
	m_data->thumbnail = data.thumbnail;
	m_data->is_fixed_image = data.is_fixed_image;
	m_data->window_id = data.window_id;
	m_data->show_close_button = data.show_close_button;
	m_data->can_activate_button = data.can_activate_button;

	m_button = OP_NEW(ContentButton, (*m_data));
	RETURN_OOM_IF_NULL(m_button);

	return OpStatus::OK;
}

OP_STATUS TabThumbnailContent::GetButtonInfo(ButtonInfo& info) const
{
	if (m_data->show_close_button || m_data->can_activate_button)
	{
		info.m_action = OpInputAction::ACTION_ACTIVATE_GROUP_WINDOW;
		info.m_action_data = m_data->window_id;
	}

	info.m_accessibility_text.Empty();
	RETURN_IF_ERROR(info.m_accessibility_text.AppendFormat(
				UNI_L("Thumbnail %d"), m_data->window_id));

	return OpStatus::OK;
}

const uni_char* TabThumbnailContent::GetTitle() const
{
	return m_data->title.CStr();
}

OP_STATUS TabThumbnailContent::GetCloseButtonInfo(ButtonInfo& info) const
{
	if (!m_data->show_close_button)
		return OpStatus::OK;

	RETURN_IF_ERROR(info.m_name.Set(WIDGET_NAME_THUMBNAIL_BUTTON_CLOSE));

	info.m_action = OpInputAction::ACTION_THUMB_CLOSE_PAGE;
	info.m_action_data = m_data->window_id;

	info.m_accessibility_text.Empty();
	RETURN_IF_ERROR(info.m_accessibility_text.AppendFormat(
				UNI_L("Thumbnail close %d"), m_data->window_id));

	return OpStatus::OK;
}

bool TabThumbnailContent::HandleDragStart(DesktopDragObject& drag_object) const
{
	drag_object.AddID(m_data->window_id);
	drag_object.SetTitle(GetTitle());

	DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(m_data->window_id);
	if (desktop_window && desktop_window->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
	{
		drag_object.SetURL(static_cast<DocumentDesktopWindow *>(desktop_window)->GetWindowCommander()->GetCurrentURL(FALSE));
	}
	return true;
}
