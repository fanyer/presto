/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/widgets/OpThumbnailPagebar.h"
#include "adjunct/quick/widgets/OpTabThumbnailChecker.h"
#include "adjunct/quick/widgets/OpIndicatorButton.h"
#include "adjunct/desktop_util/widget_utils/LogoThumbnailClipper.h"

#include "modules/skin/OpSkinManager.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/display/vis_dev.h"

#define DRAGGED_HEIGHT_TRIGGER	15					// if the pagebar becomes X pixels higher from dragging, switch to thumbnail mode
#define PAGEBAR_DEFAULT_DRAGHEIGHT	0				// default height of the pagebar on startup
#define PAGEBAR_THUMBNAIL_GENERATION_DELAY	20		// delay in ms after loading is done until the thumbnail is generated
#define THUMBNAIL_BUTTON_BORDER		7
#define THUMBNAIL_MAX_HEIGHT		150
#define THUMBNAIL_MAX_HEIGHT_SIDES	140
#define THUMBNAIL_MIN_SIDE_HEIGHT	40				// minimum size when placed on the sides
#define THUMBNAIL_SIDE_MIN_WIDTH	100				// minimum width of tabs on the side
#define THUMBNAIL_MAX_WIDTH_FACTOR	(double)1.5

/***********************************************************************************
**
**	PagebarHeadTail
**
***********************************************************************************/

OP_STATUS PagebarHeadTail::Construct(PagebarHeadTail** obj,
									 PrefsCollectionUI::integerpref prefs_setting,
									 PrefsCollectionUI::integerpref autoalignment_prefs_setting)
{
	OpAutoPtr<PagebarHeadTail> bar (OP_NEW(PagebarHeadTail, (prefs_setting, autoalignment_prefs_setting)));
	if (!bar.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(bar->init_status);

	*obj = bar.release();

	return OpStatus::OK;
}

PagebarHeadTail::PagebarHeadTail(PrefsCollectionUI::integerpref prefs_setting,
								 PrefsCollectionUI::integerpref autoalignment_prefs_setting )
	: OpToolbar(prefs_setting, autoalignment_prefs_setting)
{
	SetFixedHeight(FIXED_HEIGHT_STRETCH);
	SetYResizeEffect(RESIZE_SIZE);
	SetXResizeEffect(RESIZE_SIZE);
	m_use_thumbnails = FALSE;
	m_type = PAGEBAR_UNKNOWN;
}

OP_STATUS PagebarHeadTail::CreateButton(OpButton **button)
{
	RETURN_IF_ERROR(OpButton::Construct(button));

	(*button)->SetYResizeEffect(RESIZE_SIZE);
	(*button)->SetXResizeEffect(RESIZE_SIZE);

	return OpStatus::OK;
}

void PagebarHeadTail::UpdateButtonSkins(const char *name, const char *fallback_name)
{
	OpWidget* child = (OpWidget*) childs.First();
	while(child)
	{
		// any button inheriting from OpButton except the menu button
		if(child->IsOfType(WIDGET_TYPE_BUTTON) && child->GetType() != WIDGET_TYPE_TOOLBAR_MENU_BUTTON)
		{
			OpButton *button = (OpButton *)child;

			button->GetBorderSkin()->SetImage(name, fallback_name);
		}
		child = (OpWidget*) child->Suc();
	}
}

void PagebarHeadTail::OnShowThumbnails(BOOL force_update)
{
	if(!m_use_thumbnails || force_update)
	{
		switch(m_type)
		{
			case PAGEBAR_HEAD:
				GetBorderSkin()->SetImage("Pagebar Thumbnail Head Skin", "Pagebar Head Skin");
				UpdateButtonSkins("Pagebar Thumbnail Head Button Skin", "Pagebar Head Button Skin");
				break;

			case PAGEBAR_TAIL:
				GetBorderSkin()->SetImage("Pagebar Thumbnail Tail Skin", "Pagebar Tail Skin");
				UpdateButtonSkins("Pagebar Thumbnail Tail Button Skin", "Pagebar Tail Button Skin");
				break;

			case PAGEBAR_FLOATING:
				GetBorderSkin()->SetImage("Pagebar Thumbnail Floating Skin", "Pagebar Floating Skin");
				UpdateButtonSkins("Pagebar Thumbnail Floating Button Skin", "Pagebar Floating Button Skin");
				break;

			default:
				OP_ASSERT(!"Should never happen!");
				break;
		}
		m_use_thumbnails = TRUE;
	}
}

void PagebarHeadTail::OnHideThumbnails(BOOL force_update)
{
	if(m_use_thumbnails || force_update)
	{
		switch(m_type)
		{
		case PAGEBAR_HEAD:
			GetBorderSkin()->SetImage("Pagebar Head Skin");
			UpdateButtonSkins("Pagebar Head Button Skin", NULL);
			break;

		case PAGEBAR_TAIL:
			GetBorderSkin()->SetImage("Pagebar Tail Skin");
			UpdateButtonSkins("Pagebar Tail Button Skin", NULL);
			break;

		case PAGEBAR_FLOATING:
			GetBorderSkin()->SetImage("Pagebar Floating Skin");
			UpdateButtonSkins("Pagebar Floating Button Skin", NULL);
			break;

		default:
			OP_ASSERT(!"Should never happen!");
			break;
		}
		m_use_thumbnails = FALSE;
	}
}

void PagebarHeadTail::OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height)
{
	OpToolbar::OnLayout(compute_size_only, available_width, available_height, used_width, used_height);

	// needed just in case the pagebar is on the sides and all buttons on this toolbar has been removed.
	// Fixes DSK-261726
	if(m_type == PAGEBAR_HEAD || m_type == PAGEBAR_TAIL)
	{
		OpRect rect = GetRect();

		if(used_height < 10 || !IsVisible())
		{
			used_height = 30;
			rect.height = used_height;
		}
		if(!compute_size_only)
		{
			SetRect(rect);
		}
	}
}

void PagebarHeadTail::OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	// don't allow the resizeable search field to be dropped
	if (drag_object->GetType() == DRAG_TYPE_RESIZE_SEARCH_DROPDOWN)
	{
		static_cast<DesktopDragObject*>(drag_object)->SetDesktopDropType(DROP_NONE);
		return;
	}
	OpToolbar::OnDragMove(widget, drag_object, pos, x, y);
}

/***********************************************************************************
**
**	PagebarImage
**
***********************************************************************************/

OP_STATUS PagebarImage::Construct(PagebarImage** obj, PagebarButton* parent)
{
	if (!(*obj = OP_NEW(PagebarImage, (parent))))
		return OpStatus::ERR_NO_MEMORY;
	
	return OpStatus::OK;
}


PagebarImage::PagebarImage(PagebarButton* parent)
	: m_parent(parent)
	, m_icon_button(0)
	, m_title_button(0)
	, m_indicator_button(NULL)
	, m_fixed_image(FALSE)
{
	OP_ASSERT(m_parent);

	OP_STATUS status = OpButton::Construct(&m_icon_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
	CHECK_STATUS(status);
		
	AddChild(m_icon_button);

	status = OpButton::Construct(&m_title_button);
	CHECK_STATUS(status);

	m_title_button->SetEllipsis(g_pcui->GetIntegerPref(PrefsCollectionUI::EllipsisInCenter) == 1 ?
								ELLIPSIS_CENTER :
								ELLIPSIS_END);

	m_title_button->font_info.weight = 7;	// bold
	m_title_button->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_TEXT);
	// This call has to be after SetButtonTypeAndStyle as if the type is OpButton::TYPE_CUSTOM
	// the justification is forced to be in the centre
	m_title_button->SetJustify(JUSTIFY_LEFT, FALSE);

	AddChild(m_title_button);

	GetBorderSkin()->SetImage("Pagebar Thumbnail Skin");

	m_title_button->GetBorderSkin()->SetImage("Pagebar Thumbnail Title Skin");

	m_title_button->SetIgnoresMouse(TRUE);

	SetIgnoresMouse(TRUE);
}

void PagebarImage::ShowThumbnailImage(Image& image, const OpPoint &logoStart, BOOL fixed_image)
{
	m_fixed_image = fixed_image;
	m_logoStart = logoStart;
	m_thumbnail = image;

	// center this when we have a custom image
	if(m_fixed_image)
	{
		m_title_button->SetJustify(JUSTIFY_CENTER, FALSE);
	}
}

INT32 PagebarImage::GetTitleHeight()
{
	INT32 width  = 0;
	INT32 height = 0;

	m_title_button->GetPreferedSize(&width, &height, 1, 1);

	if(width && height)
		return height;

	return 0;
}

BOOL PagebarImage::GetTextRect(OpRect &text_rect)
{
	INT32 height = GetTitleHeight();
	if(!height)
		return FALSE;

	INT32 left = 0, top = 0, right = 0, bottom = 0;
	OpRect content_rect = GetContentRect();

	m_title_button->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
	
	INT32 icon_width = 0, icon_height = 0;

	// don't paint the fav icon if we have a custom image
	if(!m_fixed_image)
	{
		m_icon_button->GetForegroundSkin()->GetRestrictedSize(&icon_width, &icon_height);
	}

	if(g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
	{
		text_rect.x = content_rect.x + left + m_close_button_rect.width;
		text_rect.width = content_rect.width - left - right - icon_width;
	}
	else
	{
		text_rect.x = content_rect.x + left + icon_width + left;
		text_rect.width = content_rect.width - left - right - m_close_button_rect.width - icon_width - left;
	}

	if (m_indicator_button->IsVisible())
	{
		text_rect.width -= m_indicator_button->GetPreferredWidth();
	}

    text_rect.y = content_rect.y + content_rect.height - height - top - bottom;
	text_rect.height = height + top + bottom;

	return TRUE;
}

BOOL PagebarImage::GetIconRect(OpRect &icon_rect)
{
	INT32 height = GetTitleHeight();
	if(!height)
		return FALSE;
	
	OpRect content_rect = GetContentRect();
	
	INT32 tleft = 0, ttop = 0, tright = 0, tbottom = 0;
	m_title_button->GetBorderSkin()->GetPadding(&tleft, &ttop, &tright, &tbottom);
	
	INT32 ileft = 0, itop = 0, iright = 0, ibottom = 0;
	m_icon_button->GetBorderSkin()->GetPadding(&ileft, &itop, &iright, &ibottom);

	INT32 icon_width, icon_height;
	m_icon_button->GetForegroundSkin()->GetRestrictedSize(&icon_width, &icon_height);

	icon_rect.width  = icon_width  + ileft + iright;
	icon_rect.height = icon_height + itop  + ibottom;
	icon_rect.y = content_rect.y + content_rect.height - icon_height - itop - ttop - tbottom;

	if(g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
	{
		icon_rect.x = content_rect.x + content_rect.width - icon_width - iright;
	}
	else
	{
		icon_rect.x = content_rect.x + tleft - ileft;
	}

	return TRUE;
}

void PagebarImage::OnLayout()
{
	OpRect text_rect;

	if (!GetTextRect(text_rect))
		return;

	SetChildRect(m_title_button, text_rect);

	OpRect loading_rect = GetContentRect();
	RemoveTextHeight(loading_rect, text_rect);

	OpRect icon_rect;
	if (!GetIconRect(icon_rect))
		return;

	SetChildRect(m_icon_button, icon_rect);
}

OP_STATUS PagebarImage::SetTitle(const uni_char* title)
{
	OpString old_text;

	OpStatus::Ignore(m_title_button->GetText(old_text));
	if(old_text.Compare(title))
	{
		return m_title_button->SetText(title);
	}
	return OpStatus::OK;
}

OpRect PagebarImage::GetContentRect()
{
	OpRect rect = GetBounds();
	INT32 left = 0, top = 0, right = 0, bottom = 0;

	GetPadding(&left, &top, &right, &bottom);

	return OpRect(rect.x + left, rect.y + top, rect.width - left - right, rect.height - top - bottom);
}

void PagebarImage::SetSkinState(OpWidget* parent)
{
	OP_ASSERT(parent != NULL);

	if(!parent)
		return;

	m_title_button->GetBorderSkin()->SetState(parent->GetBorderSkin()->GetState(),
											  -1,
											  parent->GetBorderSkin()->GetHoverValue());

	m_title_button->GetForegroundSkin()->SetState(parent->GetForegroundSkin()->GetState(),
												  -1,
												  parent->GetForegroundSkin()->GetHoverValue());

	SkinType skin_type = parent->GetBorderSkin()->GetType();

	GetBorderSkin()->SetType(skin_type);

	m_title_button->GetBorderSkin()->SetType(skin_type);
}

void PagebarImage::AddBorder(OpRect& content_rect, int border)
{
	content_rect.x += border;
	content_rect.y += border;
	content_rect.height -= border;
	content_rect.width -= (border * 2);
}

void PagebarImage::RemoveTextHeight(OpRect& content_rect, const OpRect& text_rect)
{
	content_rect.height -= text_rect.height;
}

void PagebarImage::DrawSkin(VisualDevice* visual_device, OpRect& content_rect)
{
	GetBorderSkin()->Draw(visual_device, content_rect);
}

void PagebarImage::DrawThumbnail(VisualDevice* vd, OpRect& content_rect)
{
	if(m_thumbnail.IsEmpty())
		return;

	OpRect src_rect(0, 0, m_thumbnail.Width(), m_thumbnail.Height());
    int defaultButtonWidth = GetSkinManager()->GetOptionValue("Pagebar max button width", 150);

	OpRect dst_rect(content_rect);

	if(m_fixed_image)
	{
		SmartCropScaling crop_scaling;

		crop_scaling.SetOriginalSize(content_rect.width, content_rect.height);
		crop_scaling.GetCroppedRects(src_rect, dst_rect);

		vd->BeginClipping(content_rect);
		vd->ImageOut(m_thumbnail, src_rect, dst_rect);
		vd->EndClipping();
	}
	else
	{
		// Set the target rectangle tall enough for the entire image
		dst_rect.width = MAX(defaultButtonWidth, dst_rect.width);
		dst_rect.height = (dst_rect.width * src_rect.height) / src_rect.width;

		LogoThumbnailClipper     clipper;
		clipper.SetOriginalSize(dst_rect.width, dst_rect.height);
		clipper.SetClippedSize(content_rect.width, content_rect.height);

		clipper.SetLogoStart( m_logoStart );

		OpRect clipRect = clipper.ClipRect();
		dst_rect.OffsetBy(-clipRect.x, -clipRect.y);

		clipRect.x = content_rect.x;
		clipRect.y = content_rect.y;

		vd->BeginClipping(clipRect);
		vd->ImageOut(m_thumbnail, src_rect, dst_rect);
		if(m_parent->ShowsPageLoading())
		{
			OpSkinElement* skin_element = g_skin_manager->GetSkinElement("Pagebar Thumbnail Loading Overlay");
			if(skin_element)
			{
				skin_element->Draw(vd, dst_rect, 0);
			}
			else
			{
				vd->SetColor(255, 255, 255, 160);
				vd->FillRect(dst_rect);
			}
		}
		vd->EndClipping();
	}
}

void PagebarImage::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpRect content_rect = GetContentRect();

	SetSkinState(GetParent());

	RemoveTextHeight(content_rect, m_title_button->GetRect());

	DrawSkin(GetVisualDevice(), content_rect);

	DrawThumbnail(GetVisualDevice(), content_rect);
}

/***********************************************************************************
**
**	OpThumbnailPagebarButton
**
***********************************************************************************/

OpThumbnailPagebarButton::OpThumbnailPagebarButton(OpThumbnailPagebar* pagebar, DesktopWindow* desktop_window)
	: PagebarButton(desktop_window)
	, m_pagebar(pagebar)
	, m_thumbnail_button(NULL)
	, m_use_thumbnail(FALSE)
{
	OP_STATUS status = g_main_message_handler->SetCallBack(this, MSG_QUICK_UPDATE_PAGEBAR_THUMBNAIL, (MH_PARAM_1)this);
	CHECK_STATUS(status);

	if (m_pagebar)
	{
		status = m_pagebar->AddThumbnailStatusListener(this);
		CHECK_STATUS(status);
	}
}

void OpThumbnailPagebarButton::OnDeleted()
{
	if (m_pagebar)
	{
		m_pagebar->RemoveThumbnailStatusListener(this);
		m_pagebar = NULL;
	}
	PagebarButton::OnDeleted();
}

OpThumbnailPagebarButton::~OpThumbnailPagebarButton()
{
	// OnDeleted() not called when destroyed due to error, so doing it here again
	if (m_pagebar)
		m_pagebar->RemoveThumbnailStatusListener(this);

	g_main_message_handler->UnsetCallBack(this, MSG_QUICK_UPDATE_PAGEBAR_THUMBNAIL, (MH_PARAM_1)this);
}

/* static */
OP_STATUS OpThumbnailPagebarButton::Construct(OpThumbnailPagebarButton** obj, OpThumbnailPagebar* pagebar, DesktopWindow* desktop_window)
{
	OpAutoPtr<OpThumbnailPagebarButton> new_button (OP_NEW(OpThumbnailPagebarButton, (pagebar, desktop_window)));
	if (!new_button.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_button->InitPagebarButton());
	*obj = new_button.release();

	return OpStatus::OK;
}

OP_STATUS OpThumbnailPagebarButton::InitPagebarButton()
{
	RETURN_IF_ERROR(PagebarButton::InitPagebarButton());

	RETURN_IF_ERROR(PagebarImage::Construct(&m_thumbnail_button, this));

	m_thumbnail_button->SetIndicatorButton(m_indicator_button);

	AddChild(m_thumbnail_button);
	m_thumbnail_button->SetVisibility(FALSE);
	m_thumbnail_button->SetIgnoresMouse(TRUE);

	SetYResizeEffect(RESIZE_SIZE);
	SetXResizeEffect(RESIZE_SIZE);

	if (m_pagebar && m_pagebar->CanUseThumbnails())
	{
		OnShowThumbnails();
	}
	return OpStatus::OK;
}

OP_STATUS OpThumbnailPagebarButton::GetTitle(OpString& title)
{
	if(CanUseThumbnails())
		return m_thumbnail_button->GetTitle(title);

	return PagebarButton::GetTitle(title);
}

OP_STATUS OpThumbnailPagebarButton::SetTitle(const OpStringC& title)
{
	if(CanUseThumbnails())
		return m_thumbnail_button->SetTitle(title.CStr());

	return PagebarButton::SetTitle(title);
}

OpWidgetImage* OpThumbnailPagebarButton::GetIconSkin()
{
	if(CanUseThumbnails())
		return m_thumbnail_button->GetIconButton()->GetForegroundSkin();

	return PagebarButton::GetIconSkin();
}

void OpThumbnailPagebarButton::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_QUICK_UPDATE_PAGEBAR_THUMBNAIL)
		UpdateThumbnail();

	PagebarButton::HandleCallback(msg, par1, par2);
}

void OpThumbnailPagebarButton::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if(IsCompactButton() || !CanUseThumbnails())
	{
		PagebarButton::OnPaint(widget_painter, paint_rect);
	}
}

void OpThumbnailPagebarButton::OnLayout()
{
	UpdateCompactStatus();

	if (m_use_thumbnail)
	{
		bool is_close_button_visible = IsCloseButtonVisible();
		OpRect rect = GetBounds();

		// Update the rect of the thumbnail itself to match this one
		SetChildRect(m_thumbnail_button, rect);

		GetBorderSkin()->AddPadding(rect);


		OpRect text_rect;
		m_thumbnail_button->GetTextRect(text_rect);

		INT32 width, height;
		m_close_button->GetBorderSkin()->GetSize(&width, &height);
		m_close_button->UpdateActionStateIfNeeded();
		m_close_button->GetBorderSkin()->AddMargin(rect);

		rect.height = height;

		// Line up the close button and/or indicator with the text!
		if(text_rect.height > rect.height)
		{
			rect.y = text_rect.y + (text_rect.height - rect.height) / 2;
		}
		else
		{
			rect.y = text_rect.y;
		}

		if (is_close_button_visible)
		{
			OpRect close_button_rect;
			if(!g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
			{
				rect.x += rect.width;
				close_button_rect = rect;
				close_button_rect.width = width;
				close_button_rect.x -= width;
				rect.x -= width;
			}
			else
			{
				close_button_rect = rect;
				close_button_rect.width = width;
				rect.x += rect.width;
			}

			SetChildRect(m_close_button, close_button_rect);
			if(m_thumbnail_button)
			{
				m_thumbnail_button->SetCloseButtonRect(close_button_rect);
			}
		}
		else
		{
			rect.x += rect.width;
		}

		if (m_indicator_button->IsVisible())
		{
			m_indicator_button->ShowSeparator(is_close_button_visible);
			OpRect indicator_rect = rect;
			indicator_rect.width = m_indicator_button->GetPreferredWidth();
			indicator_rect.x -= indicator_rect.width;
			SetChildRect(m_indicator_button, indicator_rect);
		}

		UpdateLockedTab();
	}
	else
	{
		PagebarButton::OnLayout();
	}
}

void OpThumbnailPagebarButton::OnStartLoading(DocumentDesktopWindow* document_window)
{
	PagebarButton::OnStartLoading(document_window);

	if (CanUseThumbnails())
		m_thumbnail_button->Invalidate(m_thumbnail_button->GetBounds(), true, false);

}


void OpThumbnailPagebarButton::OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user)
{
	PagebarButton::OnLoadingFinished(document_window, status, was_stopped_by_user);

	if (CanUseThumbnails())
		m_thumbnail_button->Invalidate(m_thumbnail_button->GetBounds(), true, false);
}

void OpThumbnailPagebarButton::ClearRegularFields()
{
	// clear the non-thumbnail parts right away

	GetForegroundSkin()->SetImage(NULL);
	if(GetForegroundSkin()->HasBitmapImage())
	{
		Image image;
		GetForegroundSkin()->SetBitmapImage(image, TRUE);
	}
	SetText(UNI_L(""));
}

void OpThumbnailPagebarButton::SetUseThumbnail(BOOL use_thumbnail)
{
	if(m_use_thumbnail != use_thumbnail)
	{
		const char* icon = GetIconSkin()->GetImage(); // do this before setting m_use_thumbnail
		m_use_thumbnail = use_thumbnail;

		// the switched view needs to have the correct loading spinner set
		// (what GetIconSkin() returns depends on m_use_thumbnail)
		if (ShowsPageLoading())
			GetIconSkin()->SetImage(icon);

		m_thumbnail_button->SetVisibility(m_use_thumbnail);

		if(m_use_thumbnail)
		{
			RequestUpdateThumbnail();
			ClearRegularFields();
		}
		UpdateTextAndIcon(true, true);
	}
	SetSkin();
}

void OpThumbnailPagebarButton::SetSkin()
{
	if(m_use_thumbnail)
	{
		GetBorderSkin()->SetImage("Pagebar Thumbnail Button Skin");
	}
	else
	{
		GetBorderSkin()->SetImage("Pagebar Button Skin");
	}
}

void OpThumbnailPagebarButton::UpdateThumbnail()
{
	if(!CanUseThumbnails() || IsDocumentLoading())
		return;

	if (GetDesktopWindow())
	{
		OpPoint logoStart;
		Image image = GetDesktopWindow()->GetThumbnailImage(logoStart);

		if(!image.IsEmpty())
		{
			m_thumbnail_button->ShowThumbnailImage(image, logoStart, GetDesktopWindow()->HasFixedThumbnailImage());
			m_thumbnail_button->Relayout(TRUE, TRUE);
		}
	}
}

void OpThumbnailPagebarButton::RequestUpdateThumbnail()
{
	g_main_message_handler->RemoveDelayedMessage(MSG_QUICK_UPDATE_PAGEBAR_THUMBNAIL, (MH_PARAM_1)this, 0);
	g_main_message_handler->PostDelayedMessage(MSG_QUICK_UPDATE_PAGEBAR_THUMBNAIL, (MH_PARAM_1)this, (MH_PARAM_2)0, PAGEBAR_THUMBNAIL_GENERATION_DELAY);
}

void OpThumbnailPagebarButton::OnCompactChanged()
{
	SetUseThumbnail(m_use_thumbnail && !IsCompactButton());
}

BOOL OpThumbnailPagebarButton::ShouldShowFavicon()
{
	if (CanUseThumbnails())
	{
		// we have a fixed image, don't show a favicon
		if(GetDesktopWindow()->HasFixedThumbnailImage())
		{
			return FALSE;
		}
	}
	return TRUE;
}

void OpThumbnailPagebarButton::UpdateTextAndIcon(bool update_icon, bool update_text, bool delay_update)
{
	if(CanUseThumbnails() && update_icon)
	{
		OpRect icon_rect;
		if (m_thumbnail_button->GetIconRect(icon_rect))
			Invalidate(icon_rect);
	}

	PagebarButton::UpdateTextAndIcon(update_icon, update_text, delay_update);
}

void OpThumbnailPagebarButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	OpButton::GetPreferedSize(w, h, cols, rows);
	INT32 left, top, right, bottom;
	GetPadding(&left, &top, &right, &bottom);
	*w = GetMaxWidth() + left + right;
	if (m_indicator_button->IsVisible())
	{
		*w -= m_indicator_button->GetPreferredWidth();
	}
}

void OpThumbnailPagebarButton::GetRequiredSize(INT32& width, INT32& height)
{
	if(IsCompactButton())
	{
		OpButton::GetRequiredSize(width, height);

		INT32 left, top, right, bottom;
		OpButton::GetPadding(&left, &top, &right, &bottom);
		width += op_abs(left - right);

		if (m_indicator_button->IsVisible())
		{
			m_indicator_button->GetPadding(&left, &top, &right, &bottom);
			width += op_abs(left - right);
		}

		// Fix for DSK-339432: Make sure the button has the same height as if it had text
		INT32 text_width, text_height;
		GetInfo()->GetPreferedSize(this, WIDGET_TYPE_BUTTON, &text_width, &text_height, 0, 0);
		INT32 tmp_height = MAX(height, text_height + top + bottom);
		height = MIN(tmp_height, height);
	}
	else
	{
		OpButton::GetRequiredSize(width, height);
	}
}

/***********************************************************************************
**
**	OpThumbnailPagebar
**
***********************************************************************************/

OP_STATUS OpThumbnailPagebar::CreateToolbar(OpToolbar** toolbar)
{
	PagebarHeadTail* headtail = NULL;
	RETURN_IF_ERROR(PagebarHeadTail::Construct(&headtail));
	*toolbar = headtail;
	return OpStatus::OK;
}

OpThumbnailPagebar::OpThumbnailPagebar(BOOL privacy)
	: OpPagebar(privacy),
	m_dragged_height(g_pcui->GetIntegerPref(PrefsCollectionUI::PagebarHeight)),
#if defined(DU_CAP_PREFS)
	  m_dragged_width(g_pcui->GetIntegerPref(PrefsCollectionUI::PagebarWidth)),
#else
	  m_dragged_width(0),
#endif // DU_CAP_PREFS
	  m_thumbnails_enabled(g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInsideTabs)),
	  m_first_layout(TRUE),
	  m_minimal_height(0),
	  m_maximum_width(0),
	  m_minimum_width(THUMBNAIL_SIDE_MIN_WIDTH),
	  m_hide_tab_thumbnails(FALSE)
{

}

OP_STATUS OpThumbnailPagebar::InitPagebar()
{
	RETURN_IF_ERROR(OpPagebar::InitPagebar());

	// set up the child toolbars as listeners
	RETURN_IF_ERROR(AddThumbnailStatusListener((PagebarHeadTail *)m_head_bar));
	RETURN_IF_ERROR(AddThumbnailStatusListener((PagebarHeadTail *)m_tail_bar));
	RETURN_IF_ERROR(AddThumbnailStatusListener((PagebarHeadTail *)m_floating_bar));

	((PagebarHeadTail *)m_head_bar)->SetHeadTailType(PagebarHeadTail::PAGEBAR_HEAD);
	((PagebarHeadTail *)m_tail_bar)->SetHeadTailType(PagebarHeadTail::PAGEBAR_TAIL);
	((PagebarHeadTail *)m_floating_bar)->SetHeadTailType(PagebarHeadTail::PAGEBAR_FLOATING);

	SetYResizeEffect(RESIZE_SIZE);
	SetXResizeEffect(RESIZE_SIZE);

	return OpStatus::OK;
}

void OpThumbnailPagebar::OnDeleted()
{
	RemoveThumbnailStatusListener((PagebarHeadTail *)m_head_bar);
	RemoveThumbnailStatusListener((PagebarHeadTail *)m_tail_bar);
	RemoveThumbnailStatusListener((PagebarHeadTail *)m_floating_bar);

	OpPagebar::OnDeleted();
}

void OpThumbnailPagebar::SetupDraggedHeight(INT32 used_height)
{
	if(!m_dragged_height)
	{
		m_dragged_height = used_height;

		if (IsDragScrollbarEnabled() && IsHorizontal())
			m_dragged_height += PAGEBAR_DEFAULT_DRAGHEIGHT;
	}
}

void OpThumbnailPagebar::SetupDraggedWidth(INT32 used_width)
{
	if(!m_dragged_width)
	{
		m_dragged_width = used_width;
	}
}

void OpThumbnailPagebar::OnRelayout()
{
	NotifyButtons();

	OpPagebar::OnRelayout();
}

/***********************************************************************************
**
**	OnLayout - Calculate the nominal size needed for the pagebar using the standard
**			toolbar layout code, then add the extra spacing we might need
**
***********************************************************************************/

void OpThumbnailPagebar::OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height)
{
	OpToolbar::OnLayout(compute_size_only, available_width, available_height, used_width, used_height);

	if (!HasFixedHeight() && m_first_layout)
		// Isn't it great? Fixed _height_ on toolbars in vertical mode enforce a default width on the toolbar even if empty.
		// Since things are not relayouted when we get the tabs added and have width 0, we would dissapear on startup if we don't give it a default width here.
		used_width = 190;

	if(IsDragScrollbarEnabled())
	{
		Alignment alignment = GetResultingAlignment();
		INT32 margin_left, margin_right, margin_top, margin_bottom;

		if(alignment == ALIGNMENT_LEFT || alignment == ALIGNMENT_RIGHT)
		{
			OpRect floating_rect;

			if(m_floating_bar->IsOn())
			{
				floating_rect = m_floating_bar->GetRect();
			}
			// different layout code if tt is enabled or not
			if(IsThumbnailsEnabled())
			{
				INT32 extra;
				INT32 remains = 0;
				UINT32 last_y = 0;
				OpWidget* child;
				INT32 count = 0;
				INT32 calculated_height = 0;
				OpRect tail_rect, head_rect;

				if(m_tail_bar->IsOn())
				{
					tail_rect = m_tail_bar->GetRect();
				}
				if(m_head_bar->IsOn())
				{
					head_rect = m_head_bar->GetRect();
				}

				if(m_first_layout || !m_dragged_width)
				{
					SetupDraggedWidth(used_width);
					m_maximum_width = used_width * THUMBNAIL_MAX_WIDTH_FACTOR;
				}

				// find out the max y position for any tab
//				INT32 max_y = min(floating_rect.y, tail_rect.y);	// tail is at the bottom
				INT32 max_y = rect.height - floating_rect.height;	// tail is at the top

				if(head_rect.width + tail_rect.width < m_maximum_width)
				{
					m_minimum_width = head_rect.width + tail_rect.width;

					// we want the minimum width to not be wider than the optimal width
					if(m_minimum_width > used_width)
					{
						m_minimum_width = used_width;
					}
				}
				// propagate the new height to the callee
				used_width = m_dragged_width;

				max_y -= head_rect.height;

				INT32 max_height = used_height - (used_height - max_y);

				// count the number of real buttons that are visible
				for (INT32 i = 0; i < GetWidgetCount(); i++)
				{
					child = GetWidget(i);

					if(child && child->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
					{
						// The tab group button height isn't affected by the following layout and should be excluded from max_height.
						// Also reduce the group spacing used between groups.
						OpRect rect = child->IsFloating() ? child->GetOriginalRect() : child->GetRect();
						max_height -= GetGroupSpacing(TRUE) + rect.height;
					}
					else if(child && child->GetType() == WIDGET_TYPE_BUTTON && child->IsVisible() && !child->IsHidden())
					{
						count++;
					}
				}
				if (!count)
				{
					m_first_layout = FALSE;
					return;
				}
				BOOL first = TRUE;
				for (INT32 i = 0; i < GetWidgetCount(); i++)
				{
					child = GetWidget(i);

					extra = 0;

					if(child->GetType() == WIDGET_TYPE_BUTTON && child->IsVisible() && !child->IsHidden())
					{
						OpRect rect = child->IsFloating() ? child->GetOriginalRect() : child->GetRect();

						child->GetSkinMargins(&margin_left, &margin_top, &margin_right, &margin_bottom);

						if(first)	// first button
						{
							first = FALSE;
							// figure out what the "normal" height would be and if they will fit with that height
							max_height = max_height - count * margin_top;
							calculated_height = (max_height / count) - 1;

							remains = (max_height % count) - 1;

							calculated_height = min(calculated_height, THUMBNAIL_MAX_HEIGHT_SIDES);
							if(calculated_height > THUMBNAIL_MAX_HEIGHT_SIDES)
							{
								remains = 0;
							}
							if(calculated_height < THUMBNAIL_MIN_SIDE_HEIGHT || IsExtenderActive())
							{
								m_hide_tab_thumbnails = TRUE;
							}
							else if (!child->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
							{
								m_hide_tab_thumbnails = FALSE;

								extra = remains-- > 0 ? 1 : 0;
								INT32 new_height = calculated_height + extra;
								if (child->GetAnimation())
								{
									INT32 left_margin, top_margin, right_margin, bottom_margin;
									child->GetSkinMargins(&left_margin, &top_margin, &right_margin, &bottom_margin);

									new_height += top_margin + bottom_margin;

									INT32 dummy_width = 0;
									child->GetAnimation()->GetCurrentValue(dummy_width, new_height);

									new_height -= top_margin + bottom_margin;
								}
								rect.height = new_height;
							}
							rect.width = m_dragged_width;
							rect.width -= (margin_left + margin_right);
						}
						else
						{
							if(!m_hide_tab_thumbnails)
							{
								if (!child->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
								{
									extra = remains-- > 0 ? 1 : 0;
									INT32 new_height = calculated_height + extra;
									if (child->GetAnimation())
									{
										INT32 left_margin, top_margin, right_margin, bottom_margin;
										child->GetSkinMargins(&left_margin, &top_margin, &right_margin, &bottom_margin);

										new_height += top_margin + bottom_margin;

										INT32 dummy_width = 0;
										child->GetAnimation()->GetCurrentValue(dummy_width, new_height);

										new_height -= top_margin + bottom_margin;
									}
									rect.height = new_height;
								}
								rect.y = last_y + margin_top;
							}
							rect.width = m_dragged_width;
							rect.width -= (margin_left + margin_right);
						}
						last_y = rect.y + rect.height;

						if(!compute_size_only)
						{
							if (child->IsFloating())
								SetChildOriginalRect(child, rect);
							else
								SetChildRect(child, rect);
						}
					}
				}
			}
			else
			{
				OpWidget* child;

				if(m_first_layout || !m_dragged_width)
				{
					SetupDraggedWidth(used_width);
					m_maximum_width = used_width * THUMBNAIL_MAX_WIDTH_FACTOR;
				}
				used_width = m_dragged_width;

				for (INT32 i = 0; i < GetWidgetCount(); i++)
				{
					child = GetWidget(i);

					if(child->GetType() == WIDGET_TYPE_BUTTON && child->IsVisible() && !child->IsHidden())
					{
						OpRect rect = child->IsFloating() ? child->GetOriginalRect() : child->GetRect();

						child->GetSkinMargins(&margin_left, &margin_top, &margin_right, &margin_bottom);

						rect.width = m_dragged_width;
						rect.width -= (margin_left + margin_right);

						if(!compute_size_only)
						{
							if (child->IsFloating())
								SetChildOriginalRect(child, rect);
							else
								SetChildRect(child, rect);
						}
					}
				}
			}
		}
		if(alignment == ALIGNMENT_TOP || alignment == ALIGNMENT_BOTTOM)
		{
			if(m_first_layout)
			{
				SetupDraggedHeight(used_height);
				m_minimal_height = used_height;
			}
			// If we don't want to have "live size update" when the bar is too small
			// to show tab thumbnails, enable following if check.
			// if(CanUseThumbnails())
			{
				// propagate the new height to the callee
				used_height = m_dragged_height;
			}
		}
		NotifyButtons();
	}
	// if first layout is still set here (and it will be if the dragbar is not enabled, eg. when wrapping), we must clear it
	m_first_layout = FALSE;
}

// reset all cached layout values if the configuration changes
void OpThumbnailPagebar::ResetLayoutCache()
{
	m_first_layout = TRUE;
	m_minimal_height = 0;
	m_maximum_width = 0;
	m_minimum_width = THUMBNAIL_SIDE_MIN_WIDTH;
}

INT32 OpThumbnailPagebar::GetRowHeight()
{
	if (CanUseThumbnails())
		return m_dragged_height;
	else
		return OpPagebar::GetRowHeight();
}

INT32 OpThumbnailPagebar::GetRowWidth()
{
	if (CanUseThumbnails())
		return m_dragged_width;
	else
		return OpPagebar::GetRowWidth();
}

void OpThumbnailPagebar::OnSettingsChanged(DesktopSettings* settings)
{
	OpPagebar::OnSettingsChanged(settings);

	if (settings->IsChanged(SETTINGS_SKIN))
	{
		// notify the sub toolbars of possible size changes
		ResetLayoutCache();
		NotifyButtons();
	}
	else if (settings->IsChanged(SETTINGS_CUSTOMIZE_END_CANCEL)) // Used by the Appearance dialog
	{
		ResetLayoutCache();
		RestoreThumbnailsEnabled();
		NotifyButtons();
	}
	else if (settings->IsChanged(SETTINGS_CUSTOMIZE_END_OK)) // Used by the Appearance dialog
	{
		ResetLayoutCache();
		CommitThumbnailsEnabled();
		NotifyButtons();
	}
	else if (settings->IsChanged(SETTINGS_TOOLBAR_SETUP)) // Used by the Preferences dialog
	{
		ResetLayoutCache();
		RestoreThumbnailsEnabled();
		NotifyButtons();
	}
}

BOOL OpThumbnailPagebar::SetAlignment(Alignment alignment, BOOL write_to_prefs)
{
	BOOL retval = OpPagebar::SetAlignment(alignment, write_to_prefs);

	// when alignment changes, reset the layout cache so we get accurate layout based on the new alignment
	ResetLayoutCache();
//	NotifyButtons();

	return retval;
}

// Implementing the OpDragScrollbarTarget interface

void OpThumbnailPagebar::SizeChanged(const OpRect& rect)
{
	if (GetRect().Equals(rect))
		return;

	SetRect(rect);

	Alignment alignment = GetResultingAlignment();

	if(alignment == ALIGNMENT_TOP || alignment == ALIGNMENT_BOTTOM)
	{
		m_dragged_height = rect.height;
		TRAPD(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::PagebarHeight, m_dragged_height-m_extra_top_padding_normal));
	}
	else
	{
		m_dragged_width = rect.width;
#if defined(DU_CAP_PREFS)
		TRAPD(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::PagebarWidth, m_dragged_width));
#endif // DU_CAP_PREFS
	}
	GetParent()->Relayout();
}

void OpThumbnailPagebar::EndDragging()
{
}

BOOL OpThumbnailPagebar::IsDragScrollbarEnabled()
{
	OpDragbarChecker checker(GetAlignment(), GetWrapping(), m_thumbnails_enabled);

	return checker.IsDragbarAllowed();
}

void OpThumbnailPagebar::GetMinMaxWidth(INT32& min_width, INT32& max_width)
{
	Alignment alignment = GetResultingAlignment();

	if(alignment == ALIGNMENT_LEFT || alignment == ALIGNMENT_RIGHT)
	{
		if(m_maximum_width && m_minimum_width)
		{
			max_width = m_maximum_width;
			min_width = m_minimum_width;
		}
		else
		{
			OpRect rect;
			INT32 width, height;

			rect = GetWidgetContainer()->GetRoot()->GetRect();

			OpToolbar::OnLayout(TRUE, rect.width, rect.height, width, height);

			m_maximum_width = width * THUMBNAIL_MAX_WIDTH_FACTOR;
		}
	}
}

INT32 OpThumbnailPagebar::GetMinHeightSnap()
{
	INT32 min_height = 0, max_height = 0;
	GetMinMaxHeight(min_height, max_height);
	return min_height + DRAGGED_HEIGHT_TRIGGER;
}

void OpThumbnailPagebar::GetMinMaxHeight(INT32& min_height, INT32& max_height)
{
	max_height = THUMBNAIL_MAX_HEIGHT;			// hardcoded size of our normal thumbnails

	Alignment alignment = GetResultingAlignment();

	if(alignment == ALIGNMENT_TOP || alignment == ALIGNMENT_BOTTOM)
	{
		if(m_minimal_height)
		{
			min_height = m_minimal_height;
		}
		else
		{
			OpRect rect;
			INT32 width, height;

			rect = GetWidgetContainer()->GetRoot()->GetRect();

			OpToolbar::OnLayout(TRUE, rect.width, rect.height, width, height);

			m_minimal_height = height;
		}
	}
	else
	{
		max_height = THUMBNAIL_MAX_HEIGHT_SIDES;			// hardcoded size of our normal thumbnails
	}
}

void OpThumbnailPagebar::SetExtraTopPaddings(unsigned int head_top_padding, unsigned int normal_top_padding, unsigned int tail_top_padding, unsigned int tail_top_padding_width)
{
	int delta = (int)normal_top_padding-(int)m_extra_top_padding_normal;
	OpPagebar::SetExtraTopPaddings(head_top_padding, normal_top_padding, tail_top_padding, tail_top_padding_width);
	if (delta)
	{
		m_dragged_height += delta;
		Alignment alignment = GetResultingAlignment();
		if (alignment == ALIGNMENT_TOP || alignment == ALIGNMENT_BOTTOM)
		{
			OpRect rect;
			INT32 width, height;

			rect = GetWidgetContainer()->GetRoot()->GetRect();

			OpToolbar::OnLayout(TRUE, rect.width, rect.height, width, height);

			m_minimal_height = height;

			if (m_dragged_height < m_minimal_height+DRAGGED_HEIGHT_TRIGGER)
				m_dragged_height = m_minimal_height;
		}
	}
}

void OpThumbnailPagebar::NotifyButtons(BOOL force_update)
{
	BOOL show_thumbnails = CanUseThumbnails();

	for (OpListenersIterator iterator(m_thumbnail_status_listeners); m_thumbnail_status_listeners.HasNext(iterator);)
	{
		if (show_thumbnails)
			m_thumbnail_status_listeners.GetNext(iterator)->OnShowThumbnails(force_update);
		else
			m_thumbnail_status_listeners.GetNext(iterator)->OnHideThumbnails(force_update);
	}
}

OP_STATUS OpThumbnailPagebar::CreatePagebarButton(PagebarButton*& button, DesktopWindow* desktop_window)
{
	OpThumbnailPagebarButton* new_button;
	RETURN_IF_ERROR(OpThumbnailPagebarButton::Construct(&new_button, this, desktop_window));
	button = new_button;
	return OpStatus::OK;
}

void OpThumbnailPagebar::ToggleThumbnailsEnabled()
{
	m_thumbnails_enabled = !m_thumbnails_enabled;

	Wrapping wrapping = GetWrapping();
	if(wrapping == WRAPPING_NEWLINE)
	{
		SetWrapping(WRAPPING_OFF);
	}
	NotifyButtons();
	Relayout();
}

BOOL OpThumbnailPagebar::OnInputAction(OpInputAction* action)
{
	if (OpPagebar::OnInputAction(action))
		return TRUE;

	switch(action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_ENABLE_TAB_THUMBNAILS:
				case OpInputAction::ACTION_TOGGLE_TAB_THUMBNAILS:
				{
					OpTabThumbnailChecker checker(GetAlignment(), GetWrapping());

					child_action->SetSelected(checker.IsThumbnailAllowed() && m_thumbnails_enabled);

					// always enable them, then we'll deal with switching off wrapping when we need to
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_ENABLE_TAB_THUMBNAILS:
		{
			ToggleThumbnailsEnabled();
			CommitThumbnailsEnabled();
			return FALSE;	// propagate the action up to the browser window so it can do the relayout needed
		}
		case OpInputAction::ACTION_TOGGLE_TAB_THUMBNAILS:
		{
			ToggleThumbnailsEnabled();
			return TRUE;
		}
	}
	return FALSE;
}

void OpThumbnailPagebar::CommitThumbnailsEnabled()
{
	OpStatus::Ignore(g_pcui->WriteIntegerL(PrefsCollectionUI::UseThumbnailsInsideTabs, m_thumbnails_enabled));
}

void OpThumbnailPagebar::RestoreThumbnailsEnabled()
{
	m_thumbnails_enabled = g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInsideTabs);
}

BOOL OpThumbnailPagebar::HeightAllowsThumbnails()
{
	Alignment alignment = GetResultingAlignment();

	if(alignment == ALIGNMENT_LEFT || alignment == ALIGNMENT_RIGHT)
	{
		return m_hide_tab_thumbnails ? FALSE : TRUE;
	}
	else
	{
		return (m_minimal_height + DRAGGED_HEIGHT_TRIGGER) < m_dragged_height;
	}
}

BOOL OpThumbnailPagebar::CanUseThumbnails()
{
	OpTabThumbnailChecker checker(GetAlignment(), GetWrapping());

	return m_thumbnails_enabled && checker.IsThumbnailAllowed() && HeightAllowsThumbnails();
}

void OpThumbnailPagebar::SetupButton(PagebarButton* button)
{
	if(CanUseThumbnails())
	{
		button->GetBorderSkin()->SetImage("Pagebar Thumbnail Button Skin");
	}
	OpPagebar::SetupButton(button);
}
