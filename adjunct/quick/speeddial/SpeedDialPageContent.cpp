/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/speeddial/SpeedDialPageContent.h"

#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/speeddial/DesktopSpeedDial.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/widgets/DocumentView.h"
#include "modules/display/vis_dev.h"
#include "modules/widgets/OpButton.h"
#include "modules/url/url_man.h"

class SpeedDialPageContent::ContentButton : public OpButton
{
public:
	explicit ContentButton(const DesktopSpeedDial& entry);

	void SetBusy(bool busy);
	void SetImage(const Image& image);
	void SetThumbnailError(bool error){m_thumbnail_error = error;}

	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void OnResize(INT32* new_w, INT32* new_h);

	OP_STATUS RegenerateThumbnail(bool reload);

private:
	const DesktopSpeedDial* m_entry;
	bool m_first_load;
	bool m_busy;
	bool m_thumbnail_error;
};

SpeedDialPageContent::ContentButton::ContentButton(const DesktopSpeedDial& entry)
	: OpButton(TYPE_CUSTOM, STYLE_IMAGE)
	, m_entry(&entry)
	, m_first_load(true)
	, m_busy(false)
	, m_thumbnail_error(false)
{
	GetBorderSkin()->SetImage(SPEEDDIAL_THUMBNAIL_IMAGE_SKIN);
	GetForegroundSkin()->SetImage("Thumbnail Busy Image");
}

void SpeedDialPageContent::ContentButton::SetBusy(bool busy)
{
	m_busy = busy;
}

void SpeedDialPageContent::ContentButton::SetImage(const Image& image)
{
	GetForegroundSkin()->SetBitmapImage(const_cast<Image&>(image));
}

void SpeedDialPageContent::ContentButton::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (m_busy || m_thumbnail_error)
		return OpButton::OnPaint(widget_painter, paint_rect);

	INT32 left = 0, top = 0, right = 0, bottom = 0;
	GetPadding(&left, &top, &right, &bottom);

	const OpRect content_rect(left, top, rect.width - left - right, rect.height - top - bottom);

	OpWidgetImage* thumbnail = GetForegroundSkin();
	thumbnail->SetState(0, 0, 0); // turn off glow
	thumbnail->Draw(vis_dev, content_rect, &content_rect);
}

void SpeedDialPageContent::ContentButton::OnResize(INT32* new_w, INT32* new_h)
{
	// For initial thumbnail loading only.  Reloading on size change is
	// controlled higher up (through GenericThumbnailContent::Refresh()), because
	// we don't always want to reload on size change.
	if (m_first_load)
	{
		m_first_load = false;
		OpStatus::Ignore(RegenerateThumbnail(false));
	}
}

OP_STATUS SpeedDialPageContent::ContentButton::RegenerateThumbnail(bool reload)
{
	if (DocumentView::IsUrlRestrictedForViewFlag(m_entry->GetURL().CStr(), DocumentView::ALLOW_ADD_TO_SPEED_DIAL) &&
		DocumentView::GetType(m_entry->GetURL().CStr()) != DocumentView::DOCUMENT_TYPE_BROWSER)
	{
		Image img;
		OpWidgetImage* thumbnail = GetForegroundSkin();
		if (thumbnail)
		{
			DocumentView::GetThumbnailImage(m_entry->GetURL().CStr(), img);
			thumbnail->SetBitmapImage(img);
		}
		return OpStatus::OK;
	}

	int width  = g_speeddial_manager->GetThumbnailWidth();
	int height = g_speeddial_manager->GetThumbnailHeight();

	return m_entry->StartLoadingSpeedDial(reload, width, height);
}
SpeedDialPageContent::SpeedDialPageContent(const DesktopSpeedDial& entry)
	: m_entry(&entry)
	, m_button(NULL)
	, m_drag_handler(entry)
	, m_generic_handler(entry)
{
	g_thumbnail_manager->AddListener(this);
}

SpeedDialPageContent::~SpeedDialPageContent()
{
	if (m_button && !m_button->IsDeleted())
		m_button->Delete();

	g_thumbnail_manager->RemoveListener(this);
	m_image.DecVisible(this);
}

OP_STATUS SpeedDialPageContent::Init()
{
	m_button = OP_NEW(ContentButton, (*m_entry));
	RETURN_OOM_IF_NULL(m_button);
	m_button->SetBusy(true);

	return OpStatus::OK;
}

OpButton* SpeedDialPageContent::GetButton() const
{
	return m_button;
}

OP_STATUS SpeedDialPageContent::GetButtonInfo(ButtonInfo& info) const
{
	return m_generic_handler.GetButtonInfo(info);
}

const uni_char* SpeedDialPageContent::GetTitle() const
{
	return m_entry->GetTitle().HasContent() ? m_entry->GetTitle().CStr() : m_entry->GetDisplayURL().CStr();
}

int SpeedDialPageContent::GetNumber() const
{
	return m_generic_handler.GetNumber();
}

OP_STATUS SpeedDialPageContent::GetCloseButtonInfo(ButtonInfo& info) const
{
	 return m_generic_handler.GetCloseButtonInfo(info);
}

BOOL SpeedDialPageContent::IsBusy() const
{
	return m_entry->GetReloading() || m_entry->GetLoading();
}

SpeedDialPageContent::MenuInfo SpeedDialPageContent::GetContextMenuInfo() const
{
	return m_generic_handler.GetContextMenuInfo("Speed Dial Thumbnail Popup Menu");
}

bool SpeedDialPageContent::AcceptsDragDrop(DesktopDragObject& drag_object) const
{
	return IsEntryManaged() && (m_drag_handler.AcceptsDragDrop(drag_object) ||
			drag_object.GetType() == OpTypedObject::DRAG_TYPE_SPEEDDIAL);
}

bool SpeedDialPageContent::HandleDragStart(DesktopDragObject& drag_object) const
{
	return m_drag_handler.HandleDragStart(drag_object);
}

void SpeedDialPageContent::HandleDragDrop(DesktopDragObject& drag_object)
{
	OP_ASSERT(AcceptsDragDrop(drag_object));
	if (m_drag_handler.HandleDragDrop(drag_object))
		return;

	const int target_pos = g_speeddial_manager->FindSpeedDial(m_entry);
	if (target_pos < 0)
		return;

	if (drag_object.GetType() == OpTypedObject::DRAG_TYPE_SPEEDDIAL)
		g_speeddial_manager->MoveSpeedDial(drag_object.GetID(), target_pos);
}

OP_STATUS SpeedDialPageContent::HandleMidClick()
{
	return m_generic_handler.HandleMidClick();
}

bool SpeedDialPageContent::IsEntryManaged() const
{
	return g_speeddial_manager->FindSpeedDial(m_entry) >= 0;
}

OP_STATUS SpeedDialPageContent::Refresh()
{
	return m_button->RegenerateThumbnail(true);
}

OP_STATUS SpeedDialPageContent::Zoom()
{
	if (DocumentView::IsUrlRestrictedForViewFlag(m_entry->GetURL().CStr(), DocumentView::ALLOW_ADD_TO_SPEED_DIAL) &&
		DocumentView::GetType(m_entry->GetURL().CStr()) != DocumentView::DOCUMENT_TYPE_BROWSER)
	{
		DocumentView::GetThumbnailImage(m_entry->GetURL().CStr(), m_image);
		return OpStatus::OK;
	}

	// Only react on zoom requests from active speeddial, this fixes the flickering due
	// to requesting thumbnails in different sizes from invisible speeddials (DSK-332305).
	if (GetButton() && GetButton()->GetParentDesktopWindow() && !GetButton()->GetParentDesktopWindow()->IsActive())
		return OpStatus::OK;

	const OpRect thumb_rect = m_button->GetRect();
	int width = thumb_rect.width - m_button->GetPaddingLeft() - m_button->GetPaddingRight();
	int height = thumb_rect.height - m_button->GetPaddingTop() - m_button->GetPaddingBottom();
	return m_entry->StartLoadingSpeedDial(FALSE, width, height);
}

bool SpeedDialPageContent::IsMyUrl(const URL& url) const
{
	return m_entry->GetCoreUrlID() == url.Id();
}

void SpeedDialPageContent::OnThumbnailRequestStarted(const URL& url, BOOL reload)
{
	if (!IsMyUrl(url))
		return;

	m_button->SetBusy(true);
	m_button->SetThumbnailError(false);

}

void SpeedDialPageContent::OnThumbnailReady(const URL& url, const Image& thumbnail, const uni_char *title, long preview_refresh)
{
	if (!IsMyUrl(url))
		return;

	m_button->SetBusy(false);
	m_button->SetThumbnailError(false);

	m_image.DecVisible(this);
	m_image = thumbnail;
	m_image.IncVisible(this);
	OnPortionDecoded();
}

void SpeedDialPageContent::OnThumbnailFailed(const URL& url, OpLoadingListener::LoadingFinishStatus status)
{
	if (!IsMyUrl(url))
		return;

	m_button->SetBusy(false);
	m_button->SetThumbnailError(true);
	m_image.DecVisible(this);
	m_image.Empty();
}

void SpeedDialPageContent::OnInvalidateThumbnails()
{
	m_button->SetImage(Image());
	m_image.DecVisible(this);
	m_image.Empty();
}

void SpeedDialPageContent::OnPortionDecoded()
{
	if (!m_image.ImageDecoded())
		return;

	OpBitmap* buffer = m_image.GetBitmap(this);
	OP_ASSERT(buffer != NULL);
	m_button->SetImage(m_image);
	if (buffer != NULL)
		m_image.ReleaseBitmap();
}
