/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/widgets/OpPagebar.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/models/DesktopGroupModelItem.h"
#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/quick/widgets/OpToolbarMenuButton.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/widgets/OpTabGroupButton.h"
#include "adjunct/quick/widgets/OpThumbnailPagebar.h"
#include "adjunct/quick/widgets/PagebarButton.h"
#include "adjunct/desktop_util/file_utils/filenameutils.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/display/vis_dev.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/dragdrop/dragdrop_data_utils.h"
#include "modules/dragdrop/dragdrop_manager.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "adjunct/quick/managers/AnimationManager.h"
#endif

// Turn on to get debug pagebar by dumping it out
#ifdef _DEBUG	// Safeguard to avoid printfs in release
//#define PAGEBAR_DUMP_DEBUGGING
#endif // _DEBUG

/***********************************************************************************
**
**	OpPagebar
**
***********************************************************************************/

namespace {
	INT32 MINIMUM_BUTTON_WIDTH_SHOWING_CLOSE_BUTTON = 80;
}

OP_STATUS OpPagebar::CreateToolbar(OpToolbar** toolbar)
{
	return OpToolbar::Construct(toolbar);
}

OP_STATUS OpPagebar::InitPagebar()
{
	// Head
	RETURN_IF_ERROR(CreateToolbar(&m_head_bar));
	m_head_bar->SetContentListener(this);
	m_head_bar->GetBorderSkin()->SetImage("Pagebar Head Skin");
	m_head_bar->SetName("Pagebar Head");
#ifndef _MACINTOSH_
	// Set the head bar to not stretch when it can contain the enu button. 
	// Needed for compatibility with the aero integration where the head bar never grows
	// On mac it never contains the menu button, so leave it as growing
	m_head_bar->SetFixedHeight(FIXED_HEIGHT_NONE);
#endif // !_MACINTOSH_
	AddChild(m_head_bar, TRUE);

	// Tail
	RETURN_IF_ERROR(CreateToolbar(&m_tail_bar));
	m_tail_bar->GetBorderSkin()->SetImage("Pagebar Tail Skin");
	m_tail_bar->SetName("Pagebar Tail");
	AddChild(m_tail_bar, TRUE);

	// Floating
	RETURN_IF_ERROR(CreateToolbar(&m_floating_bar));
	m_floating_bar->GetBorderSkin()->SetImage("Pagebar Floating Skin");
	m_floating_bar->SetName("Pagebar Floating");
	AddChild(m_floating_bar);

	SetName("Pagebar");

	if(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu))
	{
		EnsureMenuButton(FALSE);
	}

	m_initialized = TRUE; // Must be the last statement

	return OpStatus::OK;
}

OP_STATUS OpPagebar::Construct(OpPagebar** obj, OpWorkspace* workspace)
{
	OpAutoPtr<OpThumbnailPagebar> pagebar(OP_NEW(OpThumbnailPagebar, (workspace->GetParentDesktopWindow()->GetToplevelDesktopWindow()->PrivacyMode())));

	if (!pagebar.get())
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(pagebar->init_status))
		return pagebar->init_status;

	// Workspace
	RETURN_IF_ERROR(pagebar->SetWorkspace(workspace));

	RETURN_IF_ERROR(pagebar->InitPagebar());

	*obj = pagebar.release();

	return OpStatus::OK;
}

OP_STATUS OpPagebar::SetWorkspace(OpWorkspace* workspace)
{
	if (m_workspace)
		m_workspace->RemoveListener(this);

	if(workspace)
		RETURN_IF_ERROR(workspace->AddListener(this));

	m_workspace = workspace;

	return OpStatus::OK;
}

OpPagebar::OpPagebar(BOOL privacy_mode)
  : OpToolbar(PrefsCollectionUI::PagebarAlignment, PrefsCollectionUI::PagebarAutoAlignment)
  ,m_head_bar(NULL)
  ,m_tail_bar(NULL)
  ,m_workspace(NULL)
  ,m_floating_bar(NULL)
  ,m_initialized(FALSE)
  ,m_is_handling_action(FALSE)
  ,m_allow_menu_button(TRUE)
  ,m_dragged_pagebar_button(-1)
  ,m_settings(NULL)
  ,m_extra_top_padding_head(0)
  ,m_extra_top_padding_normal(0)
  ,m_extra_top_padding_tail(0)
  ,m_extra_top_padding_tail_width(0)
{
	SetListener(this);
	SetWrapping(OpBar::WRAPPING_OFF);
	SetSelector(TRUE);
	SetFixedHeight(FIXED_HEIGHT_BUTTON);
	SetShrinkToFit(TRUE);
	SetFillToFit(TRUE);
	SetButtonType(OpButton::TYPE_PAGEBAR);
	SetStandardToolbar(FALSE);

#ifdef QUICK_FISHEYE_SUPPORT
	SetFisheyeShrink(TRUE);
#endif // QUICK_FISHEYE_SUPPORT

	GetBorderSkin()->SetImage("Pagebar Skin");

	SetFixedMaxWidth(GetSkinManager()->GetOptionValue("Pagebar max button width", 150));
	SetFixedMinWidth(GetSkinManager()->GetOptionValue("Pagebar min button width", 150));
}

void OpPagebar::OnDeleted()
{
	SetWorkspace(NULL);

	OpToolbar::OnDeleted();
}

INT32 OpPagebar::GetRowHeight()
{
	INT32 w, row_height;
	GetBorderSkin()->GetSize(&w, &row_height);
	return row_height;
}

INT32 OpPagebar::GetRowWidth()
{
	INT32 row_width, h;
	GetBorderSkin()->GetSize(&row_width, &h);
	return row_width;
}

void OpPagebar::SetHeadAlignmentFromPagebar(Alignment pagebar_alignment, BOOL force_on)
{
	if (pagebar_alignment != ALIGNMENT_OFF)
	{
		if (force_on || m_head_bar->GetResultingAlignment() != ALIGNMENT_OFF)
		{
			if(pagebar_alignment == ALIGNMENT_LEFT || pagebar_alignment == ALIGNMENT_RIGHT)
			{
				m_head_bar->SetAlignment(ALIGNMENT_TOP, TRUE);
			}
			else
			{
				m_head_bar->SetAlignment(pagebar_alignment, TRUE);
			}
		}
		// we still want to use the skin types from the page bar
		SkinType skin_type = GetBorderSkin()->GetType();
		if(m_head_bar->GetButtonSkinType() != skin_type)
		{
			m_head_bar->SetButtonSkinType(skin_type);
		}
	}
}

void OpPagebar::SetTailAlignmentFromPagebar(Alignment pagebar_alignment)
{
	if (pagebar_alignment != ALIGNMENT_OFF)
	{
		if (m_tail_bar->GetResultingAlignment() != ALIGNMENT_OFF)
		{
			if(pagebar_alignment == ALIGNMENT_LEFT || pagebar_alignment == ALIGNMENT_RIGHT)
			{
				// move it to just after head in the layout
				//				m_tail_bar->Remove();
				//				AddChild(m_tail_bar, FALSE, TRUE);
				//				m_head_bar->Remove();
				//				AddChild(m_head_bar, FALSE, TRUE);

				m_tail_bar->SetAlignment(ALIGNMENT_TOP, TRUE);
			}
			else
			{
				// move it back to the end
				//				m_tail_bar->Remove();
				//				AddChild(m_tail_bar);

				m_tail_bar->SetAlignment(pagebar_alignment, TRUE);
			}
		}
		// we still want to use the skin types from the page bar
		SkinType skin_type = GetBorderSkin()->GetType();

		if(m_tail_bar->GetButtonSkinType() != skin_type)
		{
			m_tail_bar->SetButtonSkinType(skin_type);
		}
	}
}

/***********************************************************************************
**
**	OnRelayout
**
***********************************************************************************/

void OpPagebar::OnRelayout()
{
	BOOL target = IsTargetToolbar();

	OpBar::OnRelayout();

	Alignment alignment = GetResultingAlignment();

	// head and tail toolbars get the same alignment as pagebar in order to define
	// their skinning based on the pagebar's position
	if (alignment != ALIGNMENT_OFF)
	{
		SetHeadAlignmentFromPagebar(alignment);

		if (m_floating_bar->GetResultingAlignment() != ALIGNMENT_OFF)
		{
			m_floating_bar->SetAlignment(alignment);
		}
		// we still want to use the skin types from the page bar
		SkinType skin_type = GetBorderSkin()->GetType();

		if(m_floating_bar->GetButtonSkinType() != skin_type)
		{
			m_floating_bar->SetButtonSkinType(skin_type);
		}
		if(GetButtonSkinType() != skin_type)
		{
			SetButtonSkinType(skin_type);
		}
		SetTailAlignmentFromPagebar(alignment);
	}
	if(target)
	{
		UpdateTargetToolbar(TRUE);
	}
}

/***********************************************************************************
**
**	OnLayout - Calculate the nominal size needed for the pagebar using the standard
**			toolbar layout code, then add the extra spacing we might need
**
***********************************************************************************/
void OpPagebar::OnLayout()
{
	INT32 left, top, right, bottom;

	OpToolbar::GetPadding(&left, &top, &right, &bottom);

	// layout head and tail toolbars to be within the padding of this toolbar
	OpRect rect = GetBounds();
	rect.x += left;
	rect.y += top;
	rect.width -= left + right;
	rect.height -= top + bottom;

	INT32 row_height = GetRowHeight();
//	INT32 row_width = GetRowWidth();

	OpBar::OnLayout();

	OpRect head_rect;
	OpRect tail_rect;
	m_head_bar->GetRequiredSize(head_rect.width, head_rect.height);
	m_tail_bar->GetRequiredSize(tail_rect.width, tail_rect.height);

	// only show an empty tail-bar if it is customized at the very moment
	if (m_tail_bar->GetWidgetCount() == 0 && !g_application->IsCustomizingToolbars())
		tail_rect.width = 0;

	if (IsHorizontal())
	{
		if (GetWrapping() != WRAPPING_NEWLINE)
			row_height = rect.height;
		head_rect.x = rect.x;

		if (row_height-(INT32)m_extra_top_padding_head > head_rect.height)
			head_rect.height = row_height-m_extra_top_padding_head;

		int tail_padding = m_extra_top_padding_normal;
		if (m_extra_top_padding_tail > m_extra_top_padding_normal && tail_rect.height+(INT32)m_extra_top_padding_tail > row_height)
		{
			tail_rect.x = rect.x + rect.width - tail_rect.width - m_extra_top_padding_tail_width;
		}
		else
		{
			if (m_extra_top_padding_tail > m_extra_top_padding_normal)
				tail_padding = m_extra_top_padding_tail;
			tail_rect.x = rect.x + rect.width - tail_rect.width;
		}

		if (row_height-tail_padding > tail_rect.height)
			tail_rect.height = row_height-tail_padding;

		if (GetWrapping() == WRAPPING_NEWLINE)
		{
			// bottom-align tail-bar
			tail_rect.y = rect.y + rect.height - tail_rect.height;

			if (GetAlignment() == ALIGNMENT_TOP)
			{
				// top align head toolbar 
				head_rect.y = rect.y + m_extra_top_padding_head;
			}
			else
			{
				// top-align head-bar (close to hotlist)
				head_rect.y = rect.y;
			}
		}
		else // center-align head- and tail-bar
		{
			if (m_head_bar->HasFixedHeight())
				head_rect.y = rect.y + (rect.height - head_rect.height) / 2;
			else
				head_rect.y = rect.y + m_extra_top_padding_head;

			tail_rect.y = rect.y + tail_padding + (rect.height - tail_padding - tail_rect.height) / 2;
		}
	}
	else
	{
		head_rect.x = rect.x; // top-align head_rect
		head_rect.y = rect.y;
		tail_rect.x = rect.x; // bottom-align tail_rect
		//		tail_rect.y = rect.y + rect.height - tail_rect.height;
		tail_rect.y = rect.y;
	}
	LayoutChildToAvailableRect(m_head_bar, head_rect);
	if(IsHorizontal())
	{
		LayoutChildToAvailableRect(m_tail_bar, tail_rect);
	}
	if (m_floating_bar->IsOn())
	{
		// align "add tab" floating toolbar
		OpRect floating_rect;
		m_floating_bar->GetRequiredSize(floating_rect.width, floating_rect.height);

		INT32 pos = GetWidgetCount();
		OpWidget *last_button = pos ? GetWidget(pos - 1) : NULL;

		// position the floating bar after the last _visible_ tab always. With extender menu, 
		// the last tab might not be visible
		while(last_button && !last_button->IsVisible())
		{
			last_button = pos ? GetWidget(--pos) : NULL;
		}
		OpRect last_rect = head_rect;
		if (last_button)
			last_rect = last_button->IsFloating() ? last_button->GetOriginalRect() : last_button->GetRect();

		// get margins
		m_floating_bar->GetSkinMargins(&left, &top, &right, &bottom);

		if (IsHorizontal())
		{
			int top_padding = m_extra_top_padding_normal;
			if (m_extra_top_padding_tail > m_extra_top_padding_normal && tail_rect.height+(INT32)m_extra_top_padding_tail <= row_height && 
				floating_rect.height+(INT32)m_extra_top_padding_tail <= row_height)
				top_padding = m_extra_top_padding_tail;
			if (row_height-top_padding > floating_rect.height)
				floating_rect.height = row_height-top_padding;

			// if floating toolbar fits in between last button and tail-bar
			// (this is not the case when the extender menu is on)
			//			if (last_rect.x + last_rect.width + floating_rect.width <= tail_rect.x - right)
			{
				if (GetRTL())
					floating_rect.x = last_rect.x - floating_rect.width - left;
				else
					floating_rect.x = last_rect.x + last_rect.width + left;

				if (top_padding > (INT32)m_extra_top_padding_normal)
				{
					floating_rect.y = (rect.y + top_padding + (rect.height - top_padding - floating_rect.height) / 2);
				}
				else
				{
					// center the toolbar (needed when tabs are higher than this toolbar)
					floating_rect.y = (last_rect.y + (last_rect.height - floating_rect.height) / 2);
				}
			}
			m_floating_bar->LayoutToAvailableRect(floating_rect);
		}
		else
		{
			//			if (row_width > floating_rect.width)
			//				floating_rect.width = row_width;

			if(last_rect.y + last_rect.height + floating_rect.height <= rect.height)
			{
				floating_rect.y = last_rect.y + last_rect.height + top;
			}
			else
			{
				floating_rect.y = rect.height - floating_rect.height - top;
			}
			//			floating_rect.x = 0;
			floating_rect.x = (last_rect.x + (last_rect.width - floating_rect.width) / 2);

			// align on top of tail-bar
			//			floating_rect.x = rect.x;
			//			floating_rect.y = rect.y + rect.height - floating_rect.height;

			tail_rect.x = rect.width - tail_rect.width;

			LayoutChildToAvailableRect(m_floating_bar, floating_rect);
			LayoutChildToAvailableRect(m_tail_bar, tail_rect);
		}
	}
	OpBar::OnLayout();
}

INT32 OpPagebar::OnBeforeAvailableWidth(INT32 available_width) 
{ 
	/*
	if(GetWrapping() == WRAPPING_EXTENDER)
	{
	OpRect tail_rect = m_tail_bar->GetRect();

	return available_width - tail_rect.width;
	}
	*/
	return available_width; 
};

BOOL OpPagebar::OnBeforeExtenderLayout(OpRect& extender_rect)
{
	Alignment alignment = GetAlignment();
	BOOL overflow = FALSE;

	if(alignment == ALIGNMENT_TOP || alignment == ALIGNMENT_BOTTOM)
	{
		OpRect floating_rect = m_floating_bar->GetRect();
		OpRect tail_rect = m_tail_bar->GetRect();

		if (GetRTL())
		{
			extender_rect.x = floating_rect.x - extender_rect.width;
			if (extender_rect.x < tail_rect.Right())
			{
				extender_rect.x = tail_rect.Right();
				overflow = TRUE;
			}
			if (extender_rect.Right() > floating_rect.x)
				extender_rect.x = floating_rect.x - extender_rect.width;

			// floating_rect and tail_rect are already in RTL, and so is
			// extender_rect.  But OpToolbar will adjust extender_rect for RTL
			// later, because OpToolbar is more general.  Need to flip one more
			// time here so that the end result is RTL.
			extender_rect = AdjustForDirection(extender_rect);
		}
		else
		{
			extender_rect.x = floating_rect.Right();
			if (extender_rect.Right() > tail_rect.x)
			{
				extender_rect.x = tail_rect.x - extender_rect.width;
				overflow = TRUE;
			}
			extender_rect.x = MAX(extender_rect.x, floating_rect.Right());
		}
	}
	else if(alignment == ALIGNMENT_LEFT || alignment == ALIGNMENT_RIGHT)
	{
		OpRect floating_rect = m_floating_bar->GetRect();
		OpRect tail_rect = m_tail_bar->GetRect();
		extender_rect.y = floating_rect.Bottom();
		if (extender_rect.Bottom() > tail_rect.y)
		{
			extender_rect.y = tail_rect.y - extender_rect.height;
			overflow = TRUE;
		}
		extender_rect.y = MAX(extender_rect.y, floating_rect.Bottom());
	}
	return overflow;
}

BOOL OpPagebar::OnBeforeWidgetLayout(OpWidget *widget, OpRect& layout_rect)
{
	if(widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
	{
		PagebarButton *button = static_cast<PagebarButton *>(widget);
		if(button->IsCompactButton())
		{
			// Limit the height of the pinned button (make it look good when using thumbnail tabs)
			INT32 w, h;
			button->GetRequiredSize(w, h);

			if(GetAlignment() == ALIGNMENT_BOTTOM)
			{
				layout_rect.y = 0;
			}
			else
			{
				layout_rect.y = layout_rect.Bottom() - h;
			}
			layout_rect.height = h;
		}
	}
	return TRUE;
}

INT32 OpPagebar::GetPagebarButtonMin()
{
	INT32 min = 0;

	if (IsHorizontal() && GetRTL())
	{
		if (m_floating_bar->IsOn())
		{
			min = m_floating_bar->GetRect().Right();

			INT32 left = 0, top = 0, right = 0, bottom = 0;
			m_floating_bar->GetSkinMargins(&left, &top, &right, &bottom);
			min += right;
		}
		else if (m_tail_bar->IsOn())
		{
			min = m_tail_bar->GetRect().Right();
		}
	}
	else if (m_head_bar->IsOn())
	{
		if (IsHorizontal())
		{
			if (m_extra_top_padding_head + m_head_bar->GetRect().height >= m_extra_top_padding_normal)
			{
				min += m_head_bar->GetRect().width;
			}
		}
		else
		{
			min += m_head_bar->GetRect().height;	
		}
	}
	return min;
}

INT32 OpPagebar::GetPagebarButtonMax()
{
	INT32 max = IsHorizontal() ? GetRect().width : GetRect().height;

	if (IsHorizontal() && GetRTL())
	{
		if (m_extra_top_padding_head + m_head_bar->GetRect().height >= m_extra_top_padding_normal)
			max = m_head_bar->GetRect().x;
	}
	else
	{
		if (m_floating_bar->IsOn())
		{
			max = IsHorizontal() ? m_floating_bar->GetRect().x : m_floating_bar->GetRect().y;

			// take margins into account
			INT32 m_left = 0, m_top = 0, m_right = 0, m_bottom = 0;
			m_floating_bar->GetSkinMargins(&m_left, &m_top, &m_right, &m_bottom);
			max -= IsHorizontal() ? m_left : m_top;
		}
		else if (m_tail_bar->IsOn())
		{
			max -= IsHorizontal() ? m_tail_bar->GetRect().width : m_tail_bar->GetRect().height;
		}
	}
	return max;
}

/***********************************************************************************
**
**	GetPadding
**
***********************************************************************************/

void OpPagebar::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom)
{
	OpToolbar::GetPadding(left, top, right, bottom);

	// add to padding what head and tail (and sometimes add-button) is eating of space
	INT32 head_width = 0, head_height = 0;
	INT32 tail_width = 0, tail_height = 0;

	if (m_head_bar->IsOn())
	{
		m_head_bar->GetRequiredSize(head_width, head_height);
	}

	BOOL move_float_left = FALSE;
	INT32 row_height = GetRowHeight();
	if (GetWrapping() != WRAPPING_NEWLINE)
	{
		OpRect rect = GetBounds();
		rect.height -= *top + *bottom;
		row_height = rect.height;
	}

	int right_padding = m_extra_top_padding_tail_width;
	if (m_tail_bar->IsOn())
	{
		m_tail_bar->GetRequiredSize(tail_width, tail_height);
		if (m_extra_top_padding_tail > m_extra_top_padding_normal && tail_height+(INT32)m_extra_top_padding_tail > row_height)
		{
			move_float_left = TRUE;
			right_padding += tail_width;
		}
	}
	
	INT32 floating_height = 0;
	if (m_floating_bar->IsOn() && m_floating_bar->GetWidgetCount() > 0) // if the "add tab" button isn't auto-layouted
	{
		INT32 button_width = 0, button_height = 0;
		m_floating_bar->GetRequiredSize(button_width, button_height);

		// take margins into account
		INT32 marg_left = 0, marg_top = 0, marg_right = 0, marg_bottom = 0;
		m_floating_bar->GetSkinMargins(&marg_left, &marg_top, &marg_right, &marg_bottom);
		if (IsHorizontal())
		{
			tail_width += (marg_right+button_width);
		}
		else
		{
			floating_height = marg_bottom + button_height;
		}
	
		if (move_float_left)
		{
			right_padding += button_width+marg_right;
		}
		else if (m_extra_top_padding_tail > m_extra_top_padding_normal && button_height+(INT32)m_extra_top_padding_tail > row_height)
		{
			right_padding += button_width;
		}
	}

	if (IsHorizontal())
	{
		if(m_extra_top_padding_head + head_height >= m_extra_top_padding_normal)
		{
			*left += head_width;
		}
		*right += max(tail_width, right_padding);
		*top += m_extra_top_padding_normal;
	}
	else
	{
		*top += max(tail_height, head_height);
		*bottom += floating_height;
	}
}

/***********************************************************************************
**
**	SetSelected
**
***********************************************************************************/

BOOL OpPagebar::SetSelected(INT32 pos, BOOL invoke_listeners)
{
	// just return.. wait for proper OnDesktopWindowActivated
	return FALSE;
}

/***********************************************************************************
**
**	SetAlignment
**
***********************************************************************************/

BOOL OpPagebar::SetAlignment(Alignment alignment, BOOL write_to_prefs)
{
	SkinType skin_type = GetBorderSkin()->GetType();

	// head and tail toolbars get the same alignment as pagebar in order to define
	// their skinning based on the pagebar's position
	if (alignment != ALIGNMENT_OFF)
	{
		SetHeadAlignmentFromPagebar(alignment);

		if (m_floating_bar->GetResultingAlignment() != ALIGNMENT_OFF)
		{
			m_floating_bar->SetAlignment(alignment);
		}
		m_floating_bar->SetButtonSkinType(skin_type);

		SetTailAlignmentFromPagebar(alignment);
	}

	OpString8 style_name;
	style_name.Set(GetName());
	style_name.Append(".style");

	if (alignment != GetAlignment() && alignment != ALIGNMENT_OFF && GetAlignment() != ALIGNMENT_OFF && alignment != ALIGNMENT_OLD_VISIBLE)
	{
		// NOTE: When alignment changes from horizontal (top or bottom) to vertical (left or right) alignment we force
		// wrapping to be WRAPPING_EXTENDER and from vertical to horizontal alignment we force wrapping to be WRAPPING_OFF.
		if(alignment == ALIGNMENT_LEFT || alignment == ALIGNMENT_RIGHT)
		{
			// we need to show the expander on the left or right as we have no other way
			// to ensure we can show all tabs.
			OpToolbar::SetWrapping(WRAPPING_EXTENDER);
		}
		else
		{
			OpToolbar::SetWrapping(WRAPPING_OFF);
		}

		// Update pref if necessary
		if (!g_application->IsCustomizingToolbars() && IsInitialized() && !m_settings)
		{
		 	PrefsFile* prefs_file = g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP, TRUE);
		 	TRAPD(err, prefs_file->ClearSectionL(style_name.CStr()));
		 	OnWriteStyle(prefs_file, style_name.CStr());
		}
	}
	else if (m_settings && m_settings->IsChanged(SETTINGS_CUSTOMIZE_END_CANCEL))
	{
		PrefsSection *style_section = NULL;
		TRAPD(err, style_section = g_setup_manager->GetSectionL(style_name.CStr(), OPTOOLBAR_SETUP, NULL, FALSE));
		if(style_section)
			OnReadStyle(style_section);
	}

	BOOL rc = OpBar::SetAlignment(alignment, write_to_prefs);
	SetButtonSkinType(skin_type);
	return rc;
}

OpSkinElement* OpPagebar::GetGroupBackgroundSkin(BOOL use_thumbnails)
{
	return g_skin_manager->GetSkinElement(use_thumbnails ? "Tab Thumbnail Group Group Expanded Background Skin" : "Tab Group Expanded Background Skin", GetBorderSkin()->GetType());
}

INT32 OpPagebar::GetGroupSpacing(BOOL use_thumbnails)
{
	OpSkinElement *elm = GetGroupBackgroundSkin(use_thumbnails);
	if(elm)
	{
		INT32 spacing;
		if (OpStatus::IsSuccess(elm->GetSpacing(&spacing, 0)))
			return spacing;
	}
	return 0;
}

void OpPagebar::PaintGroupRect(OpRect &overlay_rect, BOOL use_thumbnails)
{
	OpSkinElement *elm = GetGroupBackgroundSkin(use_thumbnails);
	if(elm)
	{
		INT32 left, right, top, bottom;

		elm->GetMargin(&left, &top, &right, &bottom, 0);

		overlay_rect.x += left;
		overlay_rect.y += top;
		overlay_rect.width -= left + right;
		overlay_rect.height -= top + bottom;

		elm->Draw(GetVisualDevice(), overlay_rect, 0, 0, NULL);
	}
}

void OpPagebar::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpToolbar::OnPaint(widget_painter, paint_rect);

	BOOL use_thumbnails = FALSE;
	OpRect overlay_rect;
	UINT32 current_group = 0;
	int last_x = 0;
	int count = GetWidgetCount();
	for(int i = 0; i < count; i++)
	{
		OpWidget *widget = GetWidget(i);

		UINT32 this_group = 0;
		if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
		{
			PagebarButton *button = static_cast<PagebarButton *>(widget);
			this_group = button->GetGroupNumber();
			use_thumbnails = button->CanUseThumbnails();
		}
		else if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
		{
			this_group = current_group;
		}

		if (this_group != current_group && !overlay_rect.IsEmpty())
		{
			// We have the whole group, paint it!
			PaintGroupRect(overlay_rect, use_thumbnails);
			overlay_rect.Empty();
		}

		if (this_group && !widget->IsHidden())
		{
			// Use the original rect if it's floating, except for collapsed groups
			OpRect rect = widget->IsFloating() ? widget->GetOriginalRect() : widget->GetRect();
			if (!IsGroupExpanded(this_group))
				rect = widget->GetRect();

			if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
			{
				// Need to be sure the group button doesn't extend the height of the group area.
				// This may happen because of misalignment in skin and cause paint artifacts.
				if (IsHorizontal())
				{
					rect.y = overlay_rect.y;
					rect.height = overlay_rect.height;
				}
				else
				{
					rect.x = overlay_rect.x;
					rect.width = overlay_rect.width;
				}
			}

			bool wrapped = false;
			if (!overlay_rect.IsEmpty())
				wrapped = IsHorizontal() && GetRTL() ? rect.x > last_x : rect.x < last_x;

			if (wrapped)
			{
				// We must have wrapped to a new line. Flush what we have and begin a new group.
				PaintGroupRect(overlay_rect, use_thumbnails);
				overlay_rect.Empty();
			}

			last_x = rect.x;
			overlay_rect.UnionWith(rect);
		}
		current_group = this_group;
	}
	if(!overlay_rect.IsEmpty())
	{
		// We had a pending paint for a group, paint it!
		PaintGroupRect(overlay_rect, use_thumbnails);
	}
}

int OpPagebar::FindActiveTab(int position, UINT32 group_number)
{
	if (group_number)
	{
		for (int i = position; i < GetWidgetCount(); i++)
		{
			OpWidget* widget = GetWidget(i);
			if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON) &&
				static_cast<PagebarButton*>(widget)->GetGroupNumber() == group_number)
			{
				if (static_cast<PagebarButton*>(widget)->IsActiveTabForGroup())
					return i;
			}
		}
	}
	return -1;
}

int OpPagebar::FindFirstGroupTab(INT32 position)
{
	OpWidget* widget = GetWidget(position);
	if (widget && IsInGroup(position))
	{
		unsigned int group_number = static_cast<PagebarButton*>(widget)->GetGroupNumber();
		if (!IsGroupExpanded(group_number, NULL))
		{
			for (int i = position-1; i >=0; i--)
			{
				widget = GetWidget(i);
				if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
				{
					if (static_cast<PagebarButton*>(widget)->GetGroupNumber() != group_number)
						return i+1;
				}
				else if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
				{
					return i+1;
				}
			}
			return 0;
		}
	}
	return position;
}

int OpPagebar::FindLastGroupTab(INT32 position)
{
	OpWidget* widget = GetWidget(position);
	if (widget && IsInGroup(position))
	{
		unsigned int group_number = static_cast<PagebarButton*>(widget)->GetGroupNumber();
		if (IsGroupExpanded(group_number, NULL))
		{
			return position+1;
		}
		else
		{
			for (int i = position+1; i < GetWidgetCount(); i++)
			{
				widget = GetWidget(i);
				if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
				{
					if (static_cast<PagebarButton*>(widget)->GetGroupNumber() != group_number)
						return i;
				}
			}
			return GetWidgetCount();
		}
	}
	return position;
}

int OpPagebar::FindFirstTab(INT32 position, UINT32 group_number)
{
	if (group_number)
	{
		for (int i = position; i < GetWidgetCount(); i++)
		{
			OpWidget* widget = GetWidget(i);
			if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON) &&
				static_cast<PagebarButton*>(widget)->GetGroupNumber() == group_number)
			{
				return i;
			}
		}
	}
	return -1;
}

bool OpPagebar::IsInExpandedGroup(INT32 position)
{
	OpWidget* widget = GetWidget(position);
	if (widget && IsInGroup(position))
	{
		unsigned int group_number = static_cast<PagebarButton*>(widget)->GetGroupNumber();
		return !!IsGroupExpanded(group_number, NULL);
	}
	return false;
}

bool OpPagebar::IsInGroup(INT32 position)
{
	OpWidget* widget = GetWidget(position);
	if (widget)
	{
		if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
			return true;
		else if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
			return !!static_cast<PagebarButton *>(widget)->IsGrouped();
	}
	return false;
}

INT32 OpPagebar::GetActiveTabInGroup(INT32 position)
{
	OpWidget* widget = GetWidget(position);
	if (widget && widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
	{
		int group_number = static_cast<PagebarButton*>(widget)->GetGroupNumber();
		if (group_number > 0 && !IsGroupExpanded(group_number, NULL))
		{
			int start = FindFirstTab(0, group_number);
			if (start != -1)
			{
				int active = FindActiveTab(start, group_number);
				if (active != -1)
					return active;
			}
		}
	}
	return position;
}

void OpPagebar::IncDropPosition(int& position, bool step_out_of_group)
{
	OpWidget* widget = GetWidget(position);
	if (widget)
	{
		if (IsInGroup(position))
		{
			unsigned int group_number = static_cast<PagebarButton*>(widget)->GetGroupNumber();
			if (!IsGroupExpanded(group_number, NULL))
			{
				for (int i = position; i < GetWidgetCount(); i++)
				{
					widget = GetWidget(i);
					if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
					{
						position = i + (step_out_of_group ? 1 : 0);
						return;
					}
				}
			}
		}
		position ++; // Default / fallback behavior
	}
}

void OpPagebar::OnAlignmentChanged()
{
	// Fixed height will not work well with expand/collapse buttons in vertical mode. They should only use as much space they need.
	SetFixedHeight(IsHorizontal() ? FIXED_HEIGHT_BUTTON : FIXED_HEIGHT_NONE);

	OpToolbar::OnAlignmentChanged();
}


BOOL OpPagebar::OnReadWidgetType(const OpString8& type)
{
	if (!m_allow_menu_button && type.CompareI("MenuButton") == 0)
		return FALSE;
	return TRUE;
}




/***********************************************************************************
**
**	IsCloseButtonVisible
**
***********************************************************************************/

BOOL OpPagebar::IsCloseButtonVisible(DesktopWindow* desktop_window, BOOL check_for_space_only)
{
	if (!check_for_space_only && desktop_window && !desktop_window->IsClosableByUser())
		return FALSE;

	if (!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowCloseButtons))
		return FALSE;

	if (!IsHorizontal())
		return TRUE;

	if (GetWrapping() != OpBar::WRAPPING_OFF)
		return TRUE;

	if (desktop_window && desktop_window->IsActive())
		return TRUE;

	INT32 visible_width = GetVisibleButtonWidth(desktop_window);

	if (check_for_space_only && visible_width < 0)
		return TRUE;

	if (visible_width > MINIMUM_BUTTON_WIDTH_SHOWING_CLOSE_BUTTON)
		return TRUE;

	return FALSE;
}

/***********************************************************************************
**
**	OnMouseDown
**
***********************************************************************************/

void OpPagebar::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	OpToolbar::OnMouseDown(point, button, nclicks);

	if ((button == MOUSE_BUTTON_1 && nclicks == 2) || button == MOUSE_BUTTON_3)
	{
		OpString text;
		if( button == MOUSE_BUTTON_3 )
		{
			INT32 midclickaction = g_pcui->GetIntegerPref(PrefsCollectionUI::PagebarOpenURLOnMiddleClick);
			// Default for UNIX for get data from selection buffer
#if defined(_X11_SELECTION_POLICY_)
			if (midclickaction == 1)
			{
				g_desktop_clipboard_manager->GetText(text, true);
			}
#endif
			// All platforms will get data from clipboard buffer. Fallback for UNIX
			if (midclickaction == 1 && text.IsEmpty())
				g_desktop_clipboard_manager->GetText(text);
		}

		// Normally, we would use g_application->IsOpeningInBackgroundPreferred()
		// but expected behaviour clashes with tweak configuration on Mac.
		// That's why we prefer to use default tweak value here.
		// see DSK-338217
		BOOL open_in_background = (g_op_system_info->GetShiftKeyState() == SHIFTKEY_CTRL);

		BOOL saved_open_page_setting = g_pcui->GetIntegerPref(PrefsCollectionUI::OpenPageNextToCurrent);
		TRAPD(rc,g_pcui->WriteIntegerL(PrefsCollectionUI::OpenPageNextToCurrent, FALSE));

		if( text.IsEmpty() )
		{
			g_application->GetBrowserDesktopWindow(
 					FALSE, // force new window
 					open_in_background,
 					TRUE,  // create new page
					NULL, NULL, 0, 0, TRUE, FALSE,
 					TRUE   // ignore modifier keys - since we already passed our own open_in_background
 				   	);
		}
		else
		{
			g_application->OpenURL( text, NO, YES, open_in_background ? YES : NO );
		}

		TRAP(rc,g_pcui->WriteIntegerL(PrefsCollectionUI::OpenPageNextToCurrent, saved_open_page_setting));
	}
}

/***********************************************************************************
**
**	OnSettingsChanged
**
***********************************************************************************/

void OpPagebar::OnSettingsChanged(DesktopSettings* settings)
{
	m_settings = settings;
	OpToolbar::OnSettingsChanged(settings);
	m_settings = NULL;

	if(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu))
	{
		EnsureMenuButton(FALSE);
	}
	if (settings->IsChanged(SETTINGS_SKIN))
	{
		SetFixedMaxWidth(GetSkinManager()->GetOptionValue("Pagebar max button width", 150));
		SetFixedMinWidth(GetSkinManager()->GetOptionValue("Pagebar min button width", 150));

		PrefsSection* pagebar_section = NULL;
		TRAPD(err, pagebar_section = g_setup_manager->GetSectionL("Pagebar.style", OPTOOLBAR_SETUP));
		BOOL found_style_entry = pagebar_section && pagebar_section->FindEntry(UNI_L("Button style"));

		if(!found_style_entry && g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
		{
			SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_ON_LEFT);
		}
		else
		{
			SetButtonStyle(m_default_button_style);
		}

		OP_DELETE(pagebar_section);

		// Need to check the "Inverted Pagebar Icons" skin option again
		OpWidget* child = (OpWidget*) childs.First();
		while(child)
		{
			if(child->GetType() == WIDGET_TYPE_BUTTON)
			{
				OpButton *button = (OpButton *)child;

				OpWidgetImage *image = button->GetForegroundSkin();

				if(image)
				{
					OpString8 image_name;
					image_name.Set(image->GetImage());

					if(g_skin_manager->GetOptionValue("Inverted Pagebar Icons", 0))
					{
						if(image_name.FindI(" Inverted") == KNotFound)
						{
							if(OpStatus::IsSuccess(image_name.Append(" Inverted")))
							{
								if(g_skin_manager->GetSkinElement(image_name.CStr()))
								{
									button->GetForegroundSkin()->SetImage(image_name.CStr());
								}
							}
						}
					}
					else
					{
						int pos = 0;
						if((pos = image_name.FindI(" Inverted")) > KNotFound)
						{
							image_name.Delete(pos, op_strlen(" Inverted"));
							if(g_skin_manager->GetSkinElement(image_name.CStr()))
							{
								button->GetForegroundSkin()->SetImage(image_name.CStr());
							}
						}
					}
				}
			}
			child = (OpWidget*) child->Suc();
		}
	}
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void OpPagebar::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if ( widget == this)
	{
		BOOL shift = vis_dev->GetView()->GetShiftKeys() & SHIFTKEY_SHIFT;

		DesktopWindow* window = (DesktopWindow*) GetUserData(pos);

		if (window)
		{
			if( down && button == MOUSE_BUTTON_1 && shift )
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_CLOSE_PAGE, 1, NULL, this);
			}
			else if( down && (button == MOUSE_BUTTON_3 || ( g_pcui->GetIntegerPref(PrefsCollectionUI::DoubleclickToClose) && button == MOUSE_BUTTON_1 && nclicks == 2 )) )
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_CLOSE_PAGE, 1, NULL, this);
			}
		}
	}
	else
	{
		OpToolbar::OnMouseEvent(widget, pos, x, y, button, down, nclicks);
	}
}

/***********************************************************************************
**
**	GetDragSourcePos
**
***********************************************************************************/

INT32 OpPagebar::GetDragSourcePos(DesktopDragObject* drag_object)
{
	if (drag_object->GetType() == DRAG_TYPE_WINDOW)
	{
		DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(drag_object->GetID(0));

		return FindWidgetByUserData(desktop_window);
	}

	return -1;
}

void OpPagebar::StartDrag(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON && !IsExtenderButton(widget) )
	{
		OpButton* button = (OpButton*) widget;

		DesktopWindow* window = (DesktopWindow*) button->GetUserData();

		if (!window)
			return;

		DesktopDragObject* drag_object = widget->GetDragObject(OpTypedObject::DRAG_TYPE_WINDOW, x, y);

		if (drag_object)
		{

			drag_object->AddID(window->GetID());

			if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
			{
				drag_object->SetID(window->GetID());
				m_dragged_pagebar_button = drag_object->GetID();
			}

			if (window->GetType() == WINDOW_TYPE_DOCUMENT)
			{
				drag_object->SetURL(((DocumentDesktopWindow*)window)->GetWindowCommander()->GetCurrentURL(FALSE));
				drag_object->SetTitle(((DocumentDesktopWindow*)window)->GetWindowCommander()->GetCurrentTitle());

				OpString description;
				WindowCommanderProxy::GetDescription(window->GetWindowCommander(), description);
				if( description.HasContent() )
				{
					ReplaceEscapes( description.CStr(), description.Length(), TRUE );
					DragDrop_Data_Utils::SetText(drag_object, description.CStr());
				}
			}

			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
		}
	}
}

/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/

void OpPagebar::OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	if (widget->IsOfType(WIDGET_TYPE_BUTTON))
	{
		OpWidget* parent = widget->GetParent();
		if (parent && parent->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
		{
			// We are on top of the 'x' button in the pagebar button
			OpRect rect = widget->GetRect(FALSE);
			widget = parent;
			x += rect.x;
			y += rect.y;
		}
	}

	if (drag_object->GetType() == DRAG_TYPE_WINDOW || drag_object->GetType() == DRAG_TYPE_THUMBNAIL)
		return OnDragDropOfPreviouslyDraggedPagebarButton(widget, drag_object, pos, x, y);

	if (widget != this)
	{
		// Triggers a recursive call to OpPagebar::OnDragDrop()
		OpToolbar::OnDragDrop(widget, static_cast<DesktopDragObject*>(drag_object), pos, x, y);
	}
	else
	{
		OnDrop(widget, static_cast<DesktopDragObject*>(drag_object), pos, x, y);

		RemoveAnyHighlightOnTargetButton();
	}
}

void OpPagebar::OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	if (widget->IsOfType(WIDGET_TYPE_BUTTON))
	{
		OpWidget* parent = widget->GetParent();
		if (parent && parent->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
		{
			// We are on top of the 'x' button in the pagebar button
			OpRect rect = widget->GetRect(FALSE);
			widget = parent;
			x += rect.x;
			y += rect.y;
		}
	}

	DesktopDragObject* desktop_drag_object = static_cast<DesktopDragObject *>(drag_object);

	if (desktop_drag_object->GetID() == m_dragged_pagebar_button)
		return OnDragMoveOfPreviouslyDraggedPagebarButton(widget, drag_object, pos, x, y);

	// Convert to pagebar button position
	if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON) && drag_object->GetSource() != widget)
	{
		OnDragOnButton(static_cast<PagebarButton*>(widget), desktop_drag_object, x, y);

		// Coordinates in pagebar scope needed for OnDragMove()
		OpRect rect = widget->GetRect(FALSE);
		OpToolbar::OnDragMove(desktop_drag_object, OpPoint(rect.x + x, rect.y + y));
		return;
	}
	else if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
	{
		OpRect rect = widget->GetRect(FALSE);
		OpToolbar::OnDragMove(desktop_drag_object, OpPoint(rect.x + x, rect.y + y));
		return;
	}

	OnDrag(widget, desktop_drag_object, pos, x, y);
}

void OpPagebar::OnDrag(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	if (drag_object->GetType() == DRAG_TYPE_WINDOW ||
		drag_object->GetType() == DRAG_TYPE_THUMBNAIL)
	{
		if (pos >= GetWidgetCount() && drag_object->GetID() != m_dragged_pagebar_button)
		{
			pos = GetWidgetCount() - 1;
		}
		INT32 id = GetWidget(pos) ? GetWidget(pos)->GetID() : 0;
		DesktopWindowCollection& model = g_application->GetDesktopWindowCollection();
		model.OnDragWindows(drag_object, model.GetItemByID(id));
	}
	else if (drag_object->GetType() == DRAG_TYPE_RESIZE_SEARCH_DROPDOWN)
	{
		drag_object->SetDesktopDropType(DROP_NONE);
	}
	else
	{
		drag_object->SetDesktopDropType(DROP_COPY);
	}
}

void OpPagebar::OnDrop(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
	{
		PagebarButton *button = static_cast<PagebarButton *>(widget);
		button->CancelDelayedActivation();
	}

	// This fixes the jumping cursor problem when there is space between buttons
	if (pos > GetWidgetCount() || pos == -1)
		pos = GetWidgetPosition( x, y );

	switch (drag_object->GetType())
	{
	case DRAG_TYPE_WINDOW: /* fallthrough */
	case DRAG_TYPE_THUMBNAIL:
	{

		int widget_count = GetWidgetCount();
		INT32 button_pos;
		if (!widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON) && !widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
			button_pos= GetWidgetPosition(x, y );
		else
			button_pos = GetWidgetPos(widget);
		if (button_pos >= widget_count)
			button_pos = widget_count > 0 ? widget_count-1 : 0;

		OpWidget *button_widget = GetWidget(button_pos);
		PagebarButton* button = NULL;
		DesktopWindowCollectionItem* button_item = NULL;

		// Ungroup
		for (INT32 i = drag_object->GetIDCount() - 1; i >= 0; i--)
		{
			if (drag_object->GetType() == DRAG_TYPE_WINDOW)
			{
				DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(drag_object->GetID(i));
				DesktopWindowCollectionItem* item = g_application->GetDesktopWindowCollection().GetItemByID(drag_object->GetID(i));
				DesktopWindowCollectionItem* item_parent_group = item ? item->GetParentItem() : NULL;
				OpWidget *drag_widget = desktop_window ? desktop_window->GetWidgetByTypeAndId(OpTypedObject::WIDGET_TYPE_BUTTON,  drag_object->GetID(i)) : NULL;
				if (drag_widget && drag_widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
				{
					PagebarButton *button = static_cast<PagebarButton *>(drag_widget);
					if (button && button->IsGrouped() && item_parent_group && item_parent_group->GetChildCount() <= 2)
					{
						g_application->GetDesktopWindowCollection().UnGroup(item_parent_group);
					}
				}
			}
		}

		if (button_widget && button_widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
		{
			button = button_widget ? static_cast<PagebarButton*>(button_widget) : NULL;
			if (button)
				button_item = button->GetModelItem();
		}
		DesktopWindowCollection& model = g_application->GetDesktopWindowCollection();
		INT32 id = button ? button->GetID() : 0;
		if (id == 0)
		{
			id = button_widget ? button_widget->GetID() : 0;
			button_item = model.GetItemByID(id);
		}

		model.OnDropWindows(drag_object, button_item);
		break;
	}
	case DRAG_TYPE_BOOKMARK:
		OnDropBookmark(widget, drag_object, pos, x, y);
		break;
	case DRAG_TYPE_CONTACT:
		OnDropContact(widget, drag_object, pos, x, y);
		break;
	case DRAG_TYPE_HISTORY:
		OnDropHistory(widget, drag_object, pos, x, y);
		break;
	default:
		OnDropURL(widget, drag_object, pos, x, y);
		break;
	}
}

BOOL OpPagebar::OnDragOnButton(PagebarButton* button, DesktopDragObject* drag_object, INT32 x, INT32 y)
{
	button->SetUseHoverStackOverlay(FALSE);
	INT32 drop_area = IsHorizontal() ? button->GetDropArea(x, -1) : button->GetDropArea(-1, y);

	if (!(drop_area & DROP_CENTER))
		return TRUE;

	if (drag_object->GetType() == DRAG_TYPE_WINDOW)
	{
		for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
		{
			// don't allow the tab to be dropped on itself
			if (button->GetDesktopWindow()->GetID() == drag_object->GetID(i))
			{
				// But activate tab. We can still drop in on a text field etc to use the address.
				if (!button->HasScheduledDelayedActivation())
					button->DelayedActivation(TAB_DELAYED_ACTIVATION_TIME);
				return FALSE;
			}
		}
	}

	// Activate page that is associated with the tab
	if (!button->HasScheduledDelayedActivation())
		button->DelayedActivation(TAB_DELAYED_ACTIVATION_TIME);

	button->SetUseHoverStackOverlay(TRUE);
	return TRUE;
}

void OpPagebar::OnDropBookmark(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	if (!drag_object->GetIDCount())
		return;

	DesktopWindow* parent_window = GetParentDesktopWindow();
	if (parent_window)
		parent_window->Activate();

	// We can not use the provided position if opening within a group. Recalculate position
	bool closed_group = false;
	pos = CalculateDropPosition(x, y, closed_group);

	INT32 window_pos = 0;
	DesktopWindowCollectionItem* parent = GetDropWindowPosition(pos, window_pos, closed_group);
	DesktopWindow* target_window = GetTargetWindow(pos, x, y);

	BOOL3 new_page = target_window ? MAYBE : YES;
	g_hotlist_manager->OpenUrls(drag_object->GetIDs(), NO, new_page, MAYBE, target_window, window_pos, parent);
}

void OpPagebar::OnDropContact(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	if (!drag_object->GetIDCount())
		return;

	DesktopWindow* parent = GetParentDesktopWindow();
	if (parent)
		parent->Activate();

	DesktopWindow* target_window = GetTargetWindow(pos, x, y);
	BOOL3 new_page = target_window ? MAYBE : YES;
	INT32 window_pos = 0;

	GetDropWindowPosition(pos, window_pos, TRUE);

	g_hotlist_manager->OpenContacts(drag_object->GetIDs(), NO, new_page, MAYBE, target_window, window_pos);
}

void OpPagebar::OnDropHistory(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopWindow* parent_window = GetParentDesktopWindow();
	if (parent_window)
		parent_window->Activate();

	// We can not use the provided position if opening within a group. Recalculate position
	bool closed_group = false;
	pos = CalculateDropPosition(x, y, closed_group);

	INT32 window_pos = 0;
	DesktopWindowCollectionItem* parent = GetDropWindowPosition(pos, window_pos, closed_group);
	DesktopWindow* target_window = GetTargetWindow(pos, x, y);

	DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();
	for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
	{
		HistoryModelItem* history_item = history_model->FindItem(drag_object->GetID(i));

		if (history_item)
		{
			OpString address;
			RETURN_VOID_IF_ERROR(history_item->GetAddress(address));
			OpenURL(address, target_window, parent, window_pos);
			// Let link 2,3... open in new tabs to the right
			target_window = NULL;
			window_pos ++;
		}
	}
}

void OpPagebar::OnDropURL(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopWindow* parent_window = GetParentDesktopWindow();
	if (parent_window)
		parent_window->Activate();

	// We can not use the provided position if opening within a group. Recalculate position
	bool closed_group = false;
	pos = CalculateDropPosition(x, y, closed_group);

	INT32 window_pos = 0;
	DesktopWindowCollectionItem* parent = GetDropWindowPosition(pos, window_pos, closed_group);
	DesktopWindow* target_window = GetTargetWindow(pos, x, y);

	if( drag_object->GetURLs().GetCount() > 0 )
	{
		for( UINT32 i=0; i<drag_object->GetURLs().GetCount(); i++ )
		{
			OpenURL(*drag_object->GetURLs().Get(i), target_window, parent, window_pos);
			// Let link 2,3... open in new tabs to the right
			target_window = NULL;
			window_pos ++;
		}
	}
	else if (drag_object->GetURL())
	{
		PagebarButton* drop_button = FindDropButton();
		if (drop_button)
			target_window = drop_button->GetDesktopWindow();
		OpenURL(drag_object->GetURL(), target_window, parent, window_pos);
	}
	else
	{
		OpDragDataIterator& iter = drag_object->GetDataIterator();
		if (iter.First())
		{
			do
			{
				if (iter.IsFileData())
				{
					const OpFile* file = iter.GetFileData();
					if (file)
					{
						OpString url_string;
						if (OpStatus::IsSuccess(ConvertFullPathtoURL(url_string, file->GetFullPath())))
							OpenURL(url_string, target_window, parent, window_pos);
					}
				}
			} while (iter.Next());
		}
	}
}

void OpPagebar::OpenURL(const OpStringC& url, DesktopWindow* target_window, DesktopWindowCollectionItem* parent, INT32 pos)
{
	OpenURLSetting setting;
	RETURN_VOID_IF_ERROR(setting.m_address.Set(url));
	setting.m_new_window = NO;
	setting.m_new_page = target_window ? MAYBE : YES;
	setting.m_in_background = g_pcui->GetIntegerPref(PrefsCollectionUI::OpenDraggedLinkInBackground) ? YES : MAYBE;
	setting.m_target_window = target_window;
	setting.m_target_position = pos;
	setting.m_target_parent = parent;

	g_application->OpenURL( setting );
}


int OpPagebar::CalculateDropPosition(int x, int y, bool& closed_group)
{
	closed_group = false;

	int position = GetWidgetPosition(x, y);

	OpWidget* widget = GetWidget(position);
	if (widget)
	{
		OpRect rect = widget->GetRect(FALSE);
		int wx = x - rect.x;
		int wy = y - rect.y;
		if (IsHorizontal())
			wx = MAX(wx, 0);
		else
			wy = MAX(wy, 0);
		PagebarDropLocation loc = CalculatePagebarDropLocation(widget, wx, wy);

		if (IsInGroup(position))
		{
			closed_group = !IsInExpandedGroup(position);
			if (loc == NEXT)
				position = FindLastGroupTab(position);
			else if (loc == PREVIOUS)
				position = FindFirstGroupTab(position);
		}
		else
		{
			closed_group = true;
			if (loc == NEXT)
				position ++;
		}
	}

	return position;
}





DesktopWindowCollectionItem* OpPagebar::GetDropWindowPosition(INT32 pos, INT32& window_pos, BOOL ignore_groups)
{
	if (pos < 0 || pos >= GetWidgetCount())
	{
		window_pos = pos < 0 ? 0 : m_workspace->GetModelItem()->GetChildCount();
		return m_workspace->GetModelItem();
	}

	// If the position refers to the last (rightmost) button in a group of
	// tabs then that is the expand/collapse button. In order to iterate over
	// the tabs in the group we have to step back to the last tab in this group
	// and start from there.
	int offset = 0;
	if (pos > 0)
	{
		OpWidget* widget = GetWidget(pos);
		if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
			offset = 1;
	}

	DesktopWindowCollectionItem* item = g_application->GetDesktopWindowCollection().GetItemByID(GetWidget(pos-offset)->GetID());
	if (!item)
		return NULL;

	if (ignore_groups)
	{
		// go up to the workspace level, ignoring all groups in between
		while (item->GetParentItem() && item->GetParentItem()->GetType() != WINDOW_TYPE_BROWSER)
			item = item->GetParentItem();
	}

	// Get the position by counting all items in the tree at the same level before this one
	for (window_pos = offset; item->GetPreviousItem(); window_pos++)
		item = item->GetPreviousItem();

	return item->GetParentItem();
}

DesktopWindow* OpPagebar::GetTargetWindow(INT32 pos, INT32 x, INT32 y)
{
	if (pos >= GetWidgetCount())
		return NULL;

	OpButton* button = static_cast<OpButton*>(GetWidget(pos));
	if (button)
	{
		OpRect rect = button->GetRect(FALSE);
		int wx = x - rect.x;
		int wy = y - rect.y;
		if (IsHorizontal())
			wx = MAX(wx, 0);
		else
			wy = MAX(wy, 0);

		INT32 drop_area = IsHorizontal() ? button->GetDropArea(wx, -1) : button->GetDropArea(-1, wy);

		if (drop_area & DROP_CENTER)
			return static_cast<DesktopWindow*>(button->GetUserData());
	}

	return NULL;
}

void OpPagebar::OnNewDesktopGroup(OpWorkspace* workspace, DesktopGroupModelItem& group)
{
	DesktopWindowCollectionItem* first = group.GetChildItem();
	DesktopWindowCollectionItem* last = group.GetLastChildItem();

	if (first && last)
	{
		INT32 main_pos = FindPagebarButton(first->GetDesktopWindow(), 0);
		PagebarButton* main_button = static_cast<PagebarButton*>(GetWidget(main_pos));
		INT32 second_pos = FindPagebarButton(last->GetDesktopWindow(), 0);
		PagebarButton* second_button = static_cast<PagebarButton*>(GetWidget(second_pos));

		if (!main_button || !second_button)
			return;

		INT32 group_no = group.GetID();

		for (DesktopWindowCollectionItem* child = group.GetChildItem(); child; child = child->GetSiblingItem())
		{
			PagebarButton* btn =  child->GetDesktopWindow() ? child->GetDesktopWindow()->GetPagebarButton() : NULL;
			if (btn)
				btn->SetGroupNumber(group_no);
		}

		SetGroupCollapsed(group_no, group.IsCollapsed());
		AddTabGroupButton(MAX(main_pos, second_pos) + 1, group);
	}
	OpStatus::Ignore(group.AddListener(this));
}

void OpPagebar::OnDragMoveOfPreviouslyDraggedPagebarButton(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	// drop == FALSE
	DesktopDragObject* drag_object = static_cast<DesktopDragObject*>(op_drag_object);

	if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
	{
		// If OnDragOnButton says we can't drop, don't go any further
		if (!OnDragOnButton(static_cast<PagebarButton*>(widget), drag_object, x, y))
		{
			drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
			return;
		}
	}

	pos = m_widgets.Find(widget);
	if (pos == KNotFound)
	{
		// It may be between buttons or after the last
		int widget_count = m_widgets.GetCount();
		pos = GetWidgetPosition(x + widget->GetRect().x, y + widget->GetRect().y);
		if (pos >= widget_count)
			pos = widget_count > 0 ? widget_count-1 : 0;
		widget = GetWidget(pos);
		if (!widget)
			return;
	}

	PagebarDropLocation drop_location = CalculatePagebarDropLocation(widget, x, y);
	if (drop_location == PREVIOUS)
		drag_object->SetInsertType(DesktopDragObject::INSERT_BEFORE);
	else if (drop_location == NEXT)
		drag_object->SetInsertType(DesktopDragObject::INSERT_AFTER);

	bool show_drop_marker = true;
	if (pos != -1 && pos < GetWidgetCount())
	{
		OpWidget* widget = GetWidget(pos);
		if (widget)
		{
			int drop_area = IsHorizontal() ? widget->GetDropArea(x,-1) : widget->GetDropArea(-1,y);
			if (drop_area & DROP_CENTER)
				show_drop_marker = false;
			else if (drop_area & (DROP_RIGHT|DROP_BOTTOM))
				IncDropPosition(pos, true);
		}
	}

	OnDrag(widget, drag_object, pos, x, y);
	int src_pos = GetDragSourcePos(drag_object);
	bool noDrop = (pos == src_pos);
	if (drop_location != STACK)
		noDrop |= (pos == src_pos+1);

	if (noDrop)
	{
		drag_object->SetDesktopDropType(DROP_NONE);
		show_drop_marker = false;
	}
	else
	{
		drag_object->SetDesktopDropType(DROP_MOVE);
	}

	UpdateDropPosition(show_drop_marker ? pos : -1);
}

void OpPagebar::OnDragDropOfPreviouslyDraggedPagebarButton(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	if (!widget)
		return;

	// drop == TRUE
	DesktopDragObject* drag_object = static_cast<DesktopDragObject*>(op_drag_object);
	int widget_count = m_widgets.GetCount();
	pos = GetWidgetPosition(x + widget->GetRect().x, y + widget->GetRect().y);
	if (pos >= widget_count)
		pos = widget_count > 0 ? widget_count-1 : 0;

	PagebarDropLocation drop_location = CalculatePagebarDropLocation(widget, x, y);
	if (drop_location == PREVIOUS)
		drag_object->SetInsertType(DesktopDragObject::INSERT_BEFORE);
	else if (drop_location == NEXT)
		drag_object->SetInsertType(DesktopDragObject::INSERT_AFTER);

	bool drop_center = false;
	if (pos != -1 && pos < GetWidgetCount())
	{
		OpWidget* widget = GetWidget(pos);
		if (widget)
		{
			int drop_area = IsHorizontal() ? widget->GetDropArea(x,-1) : widget->GetDropArea(-1,y);
			if (drop_area & DROP_CENTER)
			{
				drop_center = true;
			}
			else if (drop_area & (DROP_RIGHT|DROP_BOTTOM))
				IncDropPosition(pos, true);
		}
	}

	// The position is the marker position. So pos = 1 is left with the second
	// button etc. Modify settings so that behavior matches with other kind of drops

	if (pos >= GetWidgetCount())
	{
		// Allows drop after all buttons on the bar
		pos = GetWidgetCount() - 1;
		drag_object->SetInsertType(DesktopDragObject::INSERT_AFTER);
	}
	else if (drop_location == NEXT)
	{
		if (IsInGroup(pos) && !IsInExpandedGroup(pos) && drop_center)
		{
			drag_object->SetInsertType(DesktopDragObject::INSERT_INTO);
		}
		else
		{
			drag_object->SetInsertType(DesktopDragObject::INSERT_AFTER);
		}
	}
	else
	{
		if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
		{
			// Allows drop inside a group when dropped on an expander button
			drag_object->SetInsertType(DesktopDragObject::INSERT_AFTER);
		}
		else if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON) && drop_center)
		{
			// Allow dropping into a tab that's either not in a group, or in a non-expanded group
			if (!IsInGroup(pos) || (IsInGroup(pos) && !IsInExpandedGroup(pos)))
				drag_object->SetInsertType(DesktopDragObject::INSERT_INTO);
		}
	}

	OnDrop(widget, drag_object, pos, x, y);
	RemoveAnyHighlightOnTargetButton();
	// This removes drop position indicator from the pagebar.
	OpToolbar::OnDragLeave(drag_object);
}

bool OpPagebar::IsDropAvailable(DesktopDragObject* drag_object, int x, int y)
{
	if (drag_object->GetType() != DRAG_TYPE_WINDOW && drag_object->GetType() != DRAG_TYPE_THUMBNAIL)
		return OpToolbar::IsDropAvailable(drag_object, x, y);

	if (drag_object->GetID() != m_dragged_pagebar_button)
		return OpToolbar::IsDropAvailable(drag_object, x, y);

	int src_pos = GetDragSourcePos(drag_object);
	if (src_pos <= -1)
		return OpToolbar::IsDropAvailable(drag_object, x, y);

	int widget_count = m_widgets.GetCount();
	if (widget_count == 0)
		return OpToolbar::IsDropAvailable(drag_object, x, y);

	INT32 target_pos = GetWidgetPosition(x, y);

	if (target_pos >= widget_count)
	{
		// After/below all tabs. Allow if source is not the last tab
		return src_pos + 1 >= widget_count ? false : true;
	}
	else
	{
		OpWidget* widget = GetWidget(target_pos);
		if (!widget)
			return OpToolbar::IsDropAvailable(drag_object, x, y);

		OpRect rect = widget->GetRect(FALSE);
		int wx = x - rect.x;
		int wy = y - rect.y;
		if (IsHorizontal())
			wx = MAX(wx, 0);
		else
			wy = MAX(wy, 0);

		PagebarDropLocation drop_location = CalculatePagebarDropLocation(widget, wx, wy);

		int marker_position = target_pos;
		if (drop_location == NEXT)
			marker_position ++;
		else if (drop_location == STACK)
			marker_position = -1;
		if (marker_position == src_pos || marker_position == (src_pos+1))
			marker_position = -1;

		return marker_position != -1;
	}
}

void OpPagebar::UpdateDropAction(DesktopDragObject* drag_object, bool accepted)
{
	if (drag_object->GetType() == DRAG_TYPE_WINDOW)
		drag_object->SetDropType(accepted ? DROP_MOVE : DROP_NONE);
	else
		drag_object->SetDropType(accepted ? DROP_COPY : DROP_NONE);
}


OpPagebar::PagebarDropLocation OpPagebar::CalculatePagebarDropLocation(OpWidget* widget, INT32 x, INT32 y)
{
	INT32 drop_area = IsHorizontal() ? widget->GetDropArea(x,-1) : widget->GetDropArea(-1,y);
	if (drop_area & (DROP_LEFT | DROP_TOP))
		return PREVIOUS;
	if (drop_area & DROP_CENTER)
		return STACK;
	if (drop_area & (DROP_RIGHT | DROP_BOTTOM))
		return NEXT;
	return PREVIOUS;
}

void OpPagebar::RemoveAnyHighlightOnTargetButton()
{
	PagebarButton* target_button = FindDropButton();
	if (target_button)
		target_button->SetUseHoverStackOverlay(FALSE);
}

void OpPagebar::OnDragLeave(OpWidget* widget, OpDragObject* drag_object)
{
	if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
	{
		PagebarButton *button = static_cast<PagebarButton *>(widget);
		button->CancelDelayedActivation();
		button->SetUseHoverStackOverlay(FALSE);
	}

	OpToolbar::OnDragLeave(widget, (DesktopDragObject*)drag_object);
}


void OpPagebar::OnDragCancel(OpWidget* widget, OpDragObject* drag_object)
{
	if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
	{
		PagebarButton *button = static_cast<PagebarButton *>(widget);
		button->CancelDelayedActivation();
		button->SetUseHoverStackOverlay(FALSE);
	}

	OpToolbar::OnDragCancel(widget, (DesktopDragObject*)drag_object);
}


/***********************************************************************************
 **
 **	Group Functions
 **
 ***********************************************************************************/

void OpPagebar::HideGroupOpTabGroupButton(PagebarButton* button, BOOL hide)
{
	for (INT32 pos = GetWidgetPos(button) + 1; pos < GetWidgetCount(); pos++)
	{
		if (!GetWidget(pos)->IsOfType(WIDGET_TYPE_PAGEBAR_ITEM))
			continue;

		OpPagebarItem* item = static_cast<OpPagebarItem*>(GetWidget(pos));
		if (item->GetGroupNumber() != button->GetGroupNumber())
			return;

		if (item->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
		{
			if (item->IsHidden() == hide)
				Relayout();
			item->SetHidden(hide);
			return;
		}
	}
}



BOOL OpPagebar::IsGroupExpanded(UINT32 group_number, PagebarButton* exclude)
{
	// return TRUE if there is no group number since it should
	// behave like and expanded group
	if (!group_number)
		return TRUE;

	INT32 count = GetWidgetCount();

	for(INT32 pos = 0; pos < count; pos++)
	{
		OpWidget* widget = GetWidget(pos);
		if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
		{
			OpTabGroupButton* button = static_cast<OpTabGroupButton*>(widget);
			if (button->GetGroupNumber() == group_number)
				return !button->GetValue(); // value == 1 <==> collapsed
		}
	}

	return TRUE;
}

void OpPagebar::SetGroupCollapsed(UINT32 group_number, BOOL collapsed)
{
	if (!group_number)
		return;

	for (int i = 0; i < GetWidgetCount(); i++)
	{
		OpWidget* widget = GetWidget(i);
		if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON) &&
			static_cast<PagebarButton*>(widget)->GetGroupNumber() == group_number)
		{
			PagebarButton* button = static_cast<PagebarButton*>(widget);
			button->SetHidden(collapsed && !button->IsActiveTabForGroup());
		}
	}
}

BOOL OpPagebar::IsGroupCollapsing(UINT32 group_number)
{
	if (IsGroupExpanded(group_number, NULL))
	{
		INT32 i, count = GetWidgetCount();
		for (i = 0; i < count; i++)
		{
			OpWidget *widget = GetWidget(i);
			if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
			{
				OpTabGroupButton* button = static_cast<OpTabGroupButton*>(widget);
				if (button->GetValue() && group_number == button->GetGroupNumber())
					return TRUE;
			}
		}
	}
	return FALSE;
}

void OpPagebar::AddTabGroupButton(INT32 pos, DesktopGroupModelItem& group)
{
	OpTabGroupButton* group_button;
	RETURN_VOID_IF_ERROR(OpTabGroupButton::Construct(&group_button, group));
	group_button->SetName(WIDGET_NAME_TABGROUP_BUTTON);
	AddButton(group_button, NULL, NULL, NULL, NULL, pos);
}

void OpPagebar::SetOriginalGroupNumber(UINT32 group_number)
{
	for (INT32 i = 0; i < GetWidgetCount(); i++)
	{
		if (GetWidget(i)->GetType() != WIDGET_TYPE_PAGEBAR_BUTTON)
			continue;

		PagebarButton* btn = static_cast<PagebarButton*>(GetWidget(i));
		if (btn->GetGroupNumber() == group_number)
			btn->SetOriginalGroupNumber(group_number);
	}
}

void OpPagebar::ResetOriginalGroupNumber()
{
	for (INT32 i = 0; i < GetWidgetCount(); i++)
	{
		if (GetWidget(i)->GetType() == WIDGET_TYPE_PAGEBAR_BUTTON)
			static_cast<PagebarButton*>(GetWidget(i))->SetOriginalGroupNumber(-1);
	}
}

void OpPagebar::AnimateAllWidgetsToNewRect(INT32 ignore_pos)
{
	if (!g_animation_manager->GetEnabled())
		return;

	AnimateWidgets(0, GetWidgetCount() - 1, ignore_pos, TAB_GROUP_ANIMATION_DURATION);

	// Layout of the floating bar is not done by the OpToolbar layout so we have to create a animation for it explicitly.
	QuickAnimationParams params(m_floating_bar);

	params.duration  = TAB_GROUP_ANIMATION_DURATION / 1000.0;
	params.curve     = ANIM_CURVE_SLOW_DOWN;
	params.move_type = ANIM_MOVE_RECT_TO_ORIGINAL;

	g_animation_manager->startAnimation(params);
}

/***********************************************************************************
**
**	OnContextMenu
**
***********************************************************************************/

BOOL OpPagebar::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	if (widget != this)
	{
		// try to find button clicked

		child_index = FindWidget(widget, TRUE);

		if (child_index == -1)
			return FALSE;
	}

	DesktopWindow* window = child_index == -1 ? 0 : (DesktopWindow*) GetUserData(child_index);
	const PopupPlacement at_cursor = PopupPlacement::AnchorAtCursor();

	if(widget && widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON) && static_cast<PagebarButton *>(widget)->IsGrouped())
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Pagebar Item Group Popup Menu", at_cursor, 0, keyboard_invoked);
	}
	else if (window && m_workspace && m_workspace->GetActiveDesktopWindow() == window)
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Pagebar Item Popup Menu", at_cursor, 0, keyboard_invoked);
	}
	else if( window )
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Pagebar Inactive Item Popup Menu", at_cursor, 0, keyboard_invoked);
	}

	else
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Pagebar Popup Menu", at_cursor, 0, keyboard_invoked);
	}
	return TRUE;
}

/***********************************************************************************
**
**	CreateButton
**
***********************************************************************************/

OP_STATUS OpPagebar::CreatePagebarButton(PagebarButton*& button, DesktopWindow* desktop_window)
{
	return PagebarButton::Construct(&button, desktop_window);
}

/***********************************************************************************
**
**	SetupButton
**
***********************************************************************************/

void OpPagebar::SetupButton(PagebarButton* button)
{
	button->SetRestrictImageSize(TRUE);
	button->UpdateTextAndIcon(true, true, false);
}

/***********************************************************************************
**
**	OnDesktopWindowAdded
**
***********************************************************************************/

void OpPagebar::OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	INT32 widget_pos = GetWindowPos(desktop_window);

	PagebarButton* button;
	RETURN_VOID_IF_ERROR(CreatePagebarButton(button, desktop_window));

	OpInputAction* button_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_ACTIVATE_WINDOW));
	if (!button_action)
		return;
	button_action->SetActionData(desktop_window->GetID());
	AddButton(button, desktop_window->GetTitle(), NULL, button_action, desktop_window, widget_pos);
	SetupButton(button);

	// move our button to the end again
	INT32 orgpos = FindWidget(m_floating_bar);
	if(orgpos != -1)
	{
		MoveWidget(orgpos, GetWidgetCount());
	}

	DesktopWindowCollectionItem& item = desktop_window->GetModelItem();
	if (!item.GetParentItem())
	{
		OP_ASSERT(!"Desktop window item does not have a parent");
		return;
	}
	else
	{
		if (item.GetParentItem()->GetType() == OpTypedObject::WINDOW_TYPE_GROUP )
		{
			DesktopGroupModelItem* group = static_cast<DesktopGroupModelItem*>(item.GetParentItem());
			if (group)
			{
				INT32 button_index = FindTabGroupButton(group->GetID(), 0);
				if (button_index < 0)
					AddTabGroupButton(widget_pos + 1, *group);
			}
		}

		// Restore group order, since newly added window might be grouped
		RestoreOrder(workspace->GetModelItem(), 0, 0);
	}
}

INT32 OpPagebar::GetWindowPos(DesktopWindow* desktop_window)
{
	DesktopWindowCollectionItem* after = desktop_window->GetModelItem().GetPreviousItem();
	if (!after)
		return 0;

	for (INT32 pos = 0; pos < GetWidgetCount(); pos++)
	{
		if (GetWidget(pos)->GetID() == after->GetID())
			return pos + 1;
	}

	OP_ASSERT(!"Can't find previous item, should not be possible");
	return -1;
}

/***********************************************************************************
**
**	OnDesktopWindowRemoved
**
***********************************************************************************/

void OpPagebar::OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	INT32 widget_id = FindWidgetByUserData(desktop_window);
	if (widget_id < 0)
		return;

	// RemoveWidget delays the deletion of the PagebarButton,
	// so make sure that the PagebarButton doesn't get a timed tooltip query before it's
	// really deleted, as this will cause it to access the deleted DesktopWindow

	OpWidget *widget = GetWidget(widget_id);
	if (widget == g_application->GetToolTipListener())
		g_application->SetToolTipListener(NULL);

	RemoveWidget(widget_id);
}

/***********************************************************************************
**
**	OnDesktopWindowOrderChanged
**
***********************************************************************************/
void OpPagebar::OnDesktopWindowOrderChanged(OpWorkspace* workspace)
{
	DesktopWindowCollectionItem* parent = workspace->GetModelItem();
	if (!parent)
		return;

	// Order all widgets in this pagebar to represent the children of the workspace
	INT32 widget_pos = RestoreOrder(parent, 0, 0);

	// All widgets before widget_pos now represent the correct widgets for this
	// workspace. Remove the rest, they were no longer in this workspace
	for (INT32 i = GetWidgetCount() - 1; i >= widget_pos; i--)
		RemoveWidget(i);
}

INT32 OpPagebar::RestoreOrder(DesktopWindowCollectionItem* item, UINT32 group_id, INT32 widget_pos)
{
	if (item->GetDesktopWindow() && !item->GetDesktopWindow()->VisibleInWindowList())
		return widget_pos;

	for (DesktopWindowCollectionItem* child = item->GetChildItem(); child; child = child->GetSiblingItem())
	{
		INT32 child_group_id = item->GetType() == WINDOW_TYPE_GROUP && !group_id ? item->GetID() : group_id;
		widget_pos = RestoreOrder(child, child_group_id, widget_pos);
	}

	OpWidget* widget = GetWidget(widget_pos);
	if (!widget)
		return widget_pos;

	switch (item->GetType())
	{
	case WINDOW_TYPE_BROWSER:
		return widget_pos;

	case WINDOW_TYPE_GROUP:
		if (!widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON) ||
			static_cast<OpTabGroupButton*>(widget)->GetGroupNumber() != (UINT32)item->GetID())
		{
			INT32 button_pos = FindTabGroupButton(item->GetID(), widget_pos);
			OpTabGroupButton* button = static_cast<OpTabGroupButton*>(GetWidget(button_pos));
			if (!button)
				return widget_pos;

			MoveWidget(button_pos, widget_pos);
		}
		return widget_pos + 1;

	default:
		PagebarButton* button = static_cast<PagebarButton*>(widget);
		if (!widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON) ||
			button->GetGroupNumber() != group_id ||
			button->GetDesktopWindow() != item->GetDesktopWindow() ||
			button->IsHidden() != (button->GetGroup() && button->GetGroup()->IsCollapsed() && !button->IsActiveTabForGroup()))
		{
			INT32 button_pos = FindPagebarButton(item->GetDesktopWindow(), widget_pos);
			button = static_cast<PagebarButton*>(GetWidget(button_pos));
			if (!button)
				return widget_pos;

			MoveWidget(button_pos, widget_pos);
			button->SetGroupNumber(group_id);
			button->SetHidden(button->GetGroup() && button->GetGroup()->IsCollapsed() && !button->IsActiveTabForGroup());
		}
		return widget_pos + 1;
	}
}

INT32 OpPagebar::FindPagebarButton(DesktopWindow* window, INT32 start_pos)
{
	for (INT32 i = start_pos; i < GetWidgetCount(); i++)
	{
		OpWidget* widget = GetWidget(i);
		if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON) &&
			static_cast<PagebarButton*>(widget)->GetDesktopWindow() == window)
			return i;
	}

	return -1;
}

INT32 OpPagebar::FindTabGroupButton(INT32 group_no, INT32 start_pos)
{
	for (INT32 i = start_pos; i < GetWidgetCount(); i++)
	{
		OpWidget* widget = GetWidget(i);
		if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON) &&
			static_cast<OpTabGroupButton*>(widget)->GetGroupNumber() == (UINT32)group_no)
			return i;
	}

	return -1;
}

OpTabGroupButton* OpPagebar::FindTabGroupButtonByGroupNumber(INT32 group_no)
{
	INT32 index = FindTabGroupButton(group_no, 0);
	return index < 0 ? NULL : static_cast<OpTabGroupButton*>(GetWidget(index));
}


/***********************************************************************************
**
**	OnDesktopWindowActivated
**
***********************************************************************************/

void OpPagebar::OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate)
{
	INT32 pos = FindWidgetByUserData(desktop_window);
	if (pos < 0)
		return;

	if (activate)
	{
		OpToolbar::SetSelected(pos, FALSE);
	}
	else if (pos == GetSelected())
	{
		OpToolbar::SetSelected(-1, FALSE);
	}
}

/***********************************************************************************
**
**	OnDesktopWindowChanged
**
***********************************************************************************/

void OpPagebar::OnDesktopWindowChanged(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	INT32 pos = FindWidgetByUserData(desktop_window);
	if (pos < 0)
		return;

	OpWidget* widget = GetWidget(pos);
	if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
	{
		desktop_window->UpdateUseFixedImage();

		static_cast<PagebarButton *>(widget)->UpdateTextAndIcon(true, true, true);
	}
	else
		OP_ASSERT(FALSE); // does this ever happen?
}

/***********************************************************************************
**
**	OnDesktopGroupCreated
**
***********************************************************************************/

void OpPagebar::OnDesktopGroupCreated(OpWorkspace* workspace, DesktopGroupModelItem& group)
{
	OnNewDesktopGroup(workspace, group);
}

void OpPagebar::OnDesktopGroupAdded(OpWorkspace* workspace, DesktopGroupModelItem& group)
{
	OnNewDesktopGroup(workspace, group);
}

void OpPagebar::OnDesktopGroupRemoved(OpWorkspace* workspace, DesktopGroupModelItem& group)
{
	group.RemoveListener(this);
}

/***********************************************************************************
**
**	OnWorkspaceDeleted
**
***********************************************************************************/

void OpPagebar::OnWorkspaceDeleted(OpWorkspace* workspace)
{
	m_workspace = NULL;
}

/***********************************************************************************
**
**	GetFocusedButton
**
***********************************************************************************/

PagebarButton* OpPagebar::GetFocusedButton()
{
	if (GetFocused() == -1)
		return NULL;

	OpWidget *focused = GetWidget(GetFocused());
	if (!focused->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
		return NULL;

	PagebarButton *button = static_cast<PagebarButton *>(focused);

	if(button->GetType() != WIDGET_TYPE_BUTTON)
		return NULL;

	return button;
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpPagebar::OnInputAction(OpInputAction* action)
{
	// Fix for bug 329748
	if(action->GetAction() == OpInputAction::ACTION_SET_BUTTON_STYLE)
	{
		if((OpButton::ButtonStyle)action->GetActionData() == OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT && g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
		{
			action->SetActionData((INTPTR)OpButton::STYLE_IMAGE_AND_TEXT_ON_LEFT);
		}
	}

	if (OpToolbar::OnInputAction(action))
		return TRUE;

	PagebarButton *button = GetFocusedButton();

	if(!button)
		return FALSE;

	switch(action->GetAction())
	{
	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			BOOL is_locked = button->IsLockedByUser();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_LOCK_PAGE:
				{
					if (button->GetDesktopWindow()->GetWindowCommander() &&
						button->GetDesktopWindow()->GetWindowCommander()->GetGadget())
					
					{
						child_action->SetSelected(FALSE);
						child_action->SetEnabled(FALSE);
						return TRUE;
					}
					
					child_action->SetSelected(is_locked);
					return TRUE;

				}
			case OpInputAction::ACTION_UNLOCK_PAGE:
				{
					if (button->GetDesktopWindow()->GetWindowCommander() &&
						button->GetDesktopWindow()->GetWindowCommander()->GetGadget())
						
					{
						child_action->SetSelected(FALSE);
						child_action->SetEnabled(FALSE);
						return TRUE;
					}
					
					child_action->SetSelected(!is_locked);
					return TRUE;
				}
			case OpInputAction::ACTION_SHOW_POPUP_MENU:
				{
					if(child_action->GetActionDataString() && !uni_strcmp(child_action->GetActionDataString(), UNI_L("Browser Button Menu Bar")))
					{
						child_action->SetEnabled(TRUE);

						BOOL show_menu = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu);

						if(show_menu)
						{
							EnsureMenuButton(FALSE);
						}
						return TRUE;
					}
					// we don't handle other menus
					return FALSE;
				}
				case OpInputAction::ACTION_RESTORE_TO_DEFAULTS:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
			}
			break;
		}

	case OpInputAction::ACTION_UNLOCK_PAGE:
	case OpInputAction::ACTION_LOCK_PAGE:
		{
			DesktopWindow* tab = button->GetDesktopWindow();
			if (tab)
			{
				tab->SetLockedByUser(!button->IsLockedByUser());
				if(button->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
				{
					tab->UpdateUserLockOnPagebar();
				}
				else
				{
					button->Relayout(TRUE, TRUE);
				}
				g_input_manager->UpdateAllInputStates();
				return TRUE;
			}
			break;
		}
	case OpInputAction::ACTION_RESTORE_TO_DEFAULTS:
		{
			if(m_head_bar)
			{
				m_head_bar->RestoreToDefaults();
			}
			if(m_tail_bar)
			{
				m_tail_bar->RestoreToDefaults();
			}
			if(m_floating_bar)
			{
				m_floating_bar->RestoreToDefaults();
			}
			RestoreToDefaults();
			return TRUE;
		}
	case OpInputAction::ACTION_DISSOLVE_TAB_GROUP:
		{
			if (button->IsGrouped())
			{
				g_application->GetDesktopWindowCollection().UnGroup(button->GetGroup());
				Relayout();
			}
			return TRUE;
		}
	case OpInputAction::ACTION_CLOSE_TAB_GROUP:
		{
			if (button->IsGrouped())
			{
				g_application->GetDesktopWindowCollection().DestroyGroup(button->GetGroup());
				Relayout();
			}
			return TRUE;
		}
	}
	if (!m_is_handling_action)
	{
		m_is_handling_action = TRUE;
		BOOL handled = button->WindowOnInputAction(action);
		m_is_handling_action = FALSE;
		return handled;
	}

	return FALSE;
}

/***********************************************************************************
**
**	OnReadStyle
**
***********************************************************************************/

void OpPagebar::OnReadStyle(PrefsSection *section)
{
	OpToolbar::OnReadStyle(section);

	m_default_button_style = GetButtonStyle();

	if(!section->FindEntry(UNI_L("Button style")) && g_skin_manager->GetOptionValue("PageCloseButtonOnLeft", 0))
	{
		SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_ON_LEFT);
	}
}

/***********************************************************************************
**
**	OnWriteStyle
**
***********************************************************************************/

void OpPagebar::OnWriteStyle(PrefsFile* prefs_file, const char* name)
{
	OpToolbar::OnWriteStyle(prefs_file, name);
	m_default_button_style = GetButtonStyle();
}

/***********************************************************************************
**
**	EnsureMenuButton - Ensure that the menu button is available in the head toolbar
**
***********************************************************************************/
OP_STATUS OpPagebar::EnsureMenuButton(BOOL show)
{
	BOOL found = FALSE;

	// This button should never ever show on Mac See DSK-258242
#ifdef _MACINTOSH_
	show = FALSE;
#else
	if (!m_allow_menu_button)
		show = FALSE;
#endif

	OpWidget* child = (OpWidget*) m_head_bar->childs.First();
	while(child)
	{
		if(child->GetType() == WIDGET_TYPE_TOOLBAR_MENU_BUTTON)
		{
			found = TRUE;
			if(!show)
			{
				m_head_bar->RemoveWidget(m_head_bar->FindWidget(child));
			}
			break;
		}
		child = (OpWidget*) child->Suc();
	}
	if(found && child && show)
	{
		BOOL needs_visibility = m_head_bar->GetWidgetCount() < 1;	// assumes the menu button is the only button on the toolbar by default, change to 2 if the panel button returns there

		if(needs_visibility)
		{
			SetHeadAlignmentFromPagebar(GetResultingAlignment(), TRUE);
		}
		if(!child->IsVisible())
		{
			child->SetVisibility(TRUE);
		}
	}
	else if(!found)
	{
		if(show)
		{
			OpToolbarMenuButton *button;

			RETURN_IF_ERROR(OpToolbarMenuButton::Construct(&button));

			BOOL needs_visibility = m_head_bar->GetWidgetCount() == 0;

			m_head_bar->AddWidget(button, 0);

			if(needs_visibility)
			{
				SetHeadAlignmentFromPagebar(GetResultingAlignment(), TRUE);
			}
			m_head_bar->WriteContent();
		}
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	ShowMenuButtonMenu - Show the menu attached to the menu button at the right position
**
***********************************************************************************/
void OpPagebar::ShowMenuButtonMenu()
{
#ifdef QUICK_NEW_OPERA_MENU
	g_input_manager->InvokeAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_MENU, 0)));
#else
	BOOL found = FALSE;
	OpRect show_rect;

	OpWidget* child = (OpWidget*) m_head_bar->childs.First();
	while(child)
	{
		if(child->GetType() == WIDGET_TYPE_TOOLBAR_MENU_BUTTON && child->IsVisible())
		{
			found = TRUE;
			show_rect = child->GetScreenRect();
			break;
		}
		child = (OpWidget*) child->Suc();
	}

	if(!found)
	{
		show_rect = GetScreenRect();
	}

	g_application->GetMenuHandler()->ShowPopupMenu("Browser Button Menu Bar", PopupPlacement::AnchorBelow(show_rect));
#endif
}

void OpPagebar::ActivateNext(BOOL forwards)
{
	if (!m_workspace->GetActiveDesktopWindow())
		return;

	int start = forwards ? 0 : GetWidgetCount() - 1;
	int end = forwards ? GetWidgetCount() : -1;
	bool activate_next = false;

	for (int i = start; true; forwards ? ++i : --i)
	{
		if (i == end)
		{
			// cycle back to the start when we're at the end
			activate_next = true;
			i = start;
		}

		if (!GetWidget(i)->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
			continue;

		PagebarButton* button = static_cast<PagebarButton*>(GetWidget(i));
		if (button->IsHiddenOrHiding())
			continue;

		if (activate_next)
		{
			button->GetDesktopWindow()->Activate();
			break;
		}

		if (button->GetDesktopWindow()->IsActive())
			activate_next = true;
	}
}

void OpPagebar::EnableTransparentSkin(BOOL enable)
{
	if (enable)
	{
	    GetBorderSkin()->SetImage("Pagebar Transparent Skin");
	}
	else
	{
		GetBorderSkin()->SetImage("Pagebar Skin");
	}
}

void OpPagebar::UpdateIsTopmost(BOOL top)
{
	int height = GetRect(TRUE).height;
	if (top != m_reported_on_top || (top && height != m_reported_height))
	{
		m_reported_on_top = top;
		m_reported_height = height;
		m_workspace->GetOpWindow()->OnPagebarMoved(top, height);
	}
}

void OpPagebar::SetExtraTopPaddings(unsigned int head_top_padding, unsigned int normal_top_padding, unsigned int tail_top_padding, unsigned int tail_top_padding_width)
{
	m_extra_top_padding_head = head_top_padding;
	m_extra_top_padding_normal = normal_top_padding;
	m_extra_top_padding_tail = tail_top_padding;
	m_extra_top_padding_tail_width = tail_top_padding_width;
}

void OpPagebar::OnLockedByUser(PagebarButton *button, BOOL locked)
{
	button->UpdateLockedTab();

	// Pinned compact tabs only occur when non-grouped and horizontal
	if (button->IsGrouped() || !IsHorizontal() || locked == button->IsCompactButton())
		return;

	button->SetIsCompactButton(locked);
	UpdateWidget(button);

	// Find the last non-grouped locked tab on the left
	DesktopWindowCollectionItem& button_item = button->GetDesktopWindow()->GetModelItem();
	DesktopWindowCollectionItem* last_locked = NULL;
	DesktopWindowCollectionItem* parent = GetWorkspace()->GetModelItem();
	for (DesktopWindowCollectionItem* item = parent->GetChildItem(); item; item = item->GetSiblingItem())
	{
		if (item == &button_item)
			continue;
		if (item->IsContainer() || !item->GetDesktopWindow() || !item->GetDesktopWindow()->IsLockedByUser())
			break;
		last_locked = item;
	}

	// Move the tab to the new position
	g_application->GetDesktopWindowCollection().ReorderByItem(button_item, parent, last_locked);

	// Since we change size on this tab, make sure all widgets in the toolbar animate to their new positions
	AnimateAllWidgetsToNewRect();
}

void OpPagebar::ClearAllHoverStackOverlays()
{
	for (int i = 0; i < GetWidgetCount(); i++)
	{
		if (GetWidget(i)->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
			static_cast<PagebarButton*>(GetWidget(i))->SetUseHoverStackOverlay(FALSE);
	}
}

PagebarButton* OpPagebar::FindDropButton()
{
	for (int i = 0; i < GetWidgetCount(); i++)
	{
		if (!GetWidget(i)->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
			continue;

		PagebarButton* button = static_cast<PagebarButton*>(GetWidget(i));
		if (button->GetUseHoverStackOverlay())
			return button;
	}

	return NULL;
}

PagebarButton* OpPagebar::GetPagebarButton(const OpPoint& point)
{
	BOOL horizontal = IsHorizontal();

	for (INT32 i = 0; i < GetWidgetCount(); i++)
	{
		if (GetWidget(i)->IsFloating() || !GetWidget(i)->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
			continue;

		PagebarButton* button = static_cast<PagebarButton*>(GetWidget(i));
		OpRect button_rect = button->GetRectWithoutMargins();

		if (horizontal && button_rect.Left() <= point.x && point.x < button_rect.Right())
			return button;
		if (!horizontal && button_rect.Top() <= point.y && point.y < button_rect.Bottom())
			return button;
	}

	return NULL;
}

INT32 OpPagebar::GetVisibleButtonWidth(DesktopWindow *window)
{
	INT32 width = 0;
	OpRect rect;

	if(window->GetPagebarButton())
	{
		// the fast, but maybe not available code path
		rect = window->GetPagebarButton()->GetRect();
		width = rect.width;
	}
	else
	{
		INT32 i, count = GetWidgetCount();

		for (i = 0; i < count; ++i)
		{
			OpWidget *widget = GetWidget(i);
			if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON) && !widget->IsHidden())
			{
				PagebarButton *button = static_cast<PagebarButton *>(widget);
				if(button->GetDesktopWindow() && button->GetDesktopWindow() == window)
				{
					rect = widget->GetRect();
					width = rect.width;
					break;
				}
			}
		}
	}

	if (rect.x == -10000 && rect.y == -10000 && rect.width == 0)
	{
		// this means, that button was not laid out yet
		return -1;
	}

	return width;
}

INT32 OpPagebar::GetPosition(PagebarButton* button)
{
	INT32 count = GetWidgetCount();
	int num = 0;

	for (INT32 i = 0; i < count; ++i)
	{
		OpWidget *widget = GetWidget(i);
		if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
		{
			if (widget == button)
				return num;

			num++;
		}
	}

	return -1;
}


INT32 OpPagebar::GetWidgetPos(OpWidget* widget)
{
	INT32 i;
	INT32 count = GetWidgetCount();

	for (i = 0; i < count; i++)
	{
		if (GetWidget(i) == widget)
		{
			return i;
		}
	}
	return -1;
}

#ifdef PAGEBAR_DUMP_DEBUGGING

void OpPagebar::DumpPagebar()
{
	INT32	i, count = GetWidgetCount();
	
	// Loop all the widgets in reverse and just add/remove the OpTabGroupButton as required
	for (i = 0; i < count; i++)
	{
		OpWidget	*widget = GetWidget(i);
		if (widget->IsOfType(WIDGET_TYPE_PAGEBAR_BUTTON))
		{
			printf("Group %d, PagebarButton, Hidden %d, Active: %d\n", static_cast<PagebarButton *>(widget)->GetGroupNumber(), static_cast<PagebarButton *>(widget)->IsHiddenOrHiding(), static_cast<PagebarButton *>(widget)->IsActiveTabForGroup());
		}
		else if (widget->IsOfType(WIDGET_TYPE_TAB_GROUP_BUTTON))
		{
			printf("Group %d, OpTabGroupButton, Hidden %d\n", static_cast<OpTabGroupButton *>(widget)->GetGroupNumber(), static_cast<OpTabGroupButton *>(widget)->IsHidden());
		}
		else if (widget->IsOfType(WIDGET_TYPE_SHRINK_ANIMATION))
		{
			printf("WIDGET_TYPE_SHRINK_ANIMATION\n");
		}
		else
		{
			printf("Unknown\n");
		}
	}
}

#endif // PAGEBAR_DUMP_DEBUGGING
