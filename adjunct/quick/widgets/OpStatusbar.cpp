/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "OpStatusbar.h"

/***********************************************************************************
**
**	OpStatusbar
**
***********************************************************************************/

OP_STATUS OpStatusbarHeadTail::Construct(OpStatusbarHeadTail** obj)
{
	*obj = OP_NEW(OpStatusbarHeadTail, ());
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

void OpStatusbarHeadTail::OnContentSizeChanged()
{
	((OpStatusbar*)GetParent())->UpdateHeadTailSize();
	if (((OpStatusbar*)GetParent())->GetWidgetCount() > 0)
		GetParent()->Relayout();
}

OP_STATUS OpStatusbar::InitStatusbar()
{
	// Head
	RETURN_IF_ERROR(OpStatusbarHeadTail::Construct(&m_head_bar));
	m_head_bar->GetBorderSkin()->SetImage("Statusbar Head Skin");
	m_head_bar->SetName("Status Toolbar Head");
	m_head_bar->SetFixedHeight(OpToolbar::FIXED_HEIGHT_STRETCH);
	AddChild(m_head_bar, TRUE);

	// Tail
	RETURN_IF_ERROR(OpStatusbarHeadTail::Construct(&m_tail_bar));
	m_tail_bar->GetBorderSkin()->SetImage("Statusbar Tail Skin");
	m_tail_bar->SetName("Status Toolbar Tail");
	m_tail_bar->SetFixedHeight(OpToolbar::FIXED_HEIGHT_STRETCH);
	AddChild(m_tail_bar, TRUE);

	SetName("Status Toolbar");

	return OpStatus::OK;
}

OP_STATUS OpStatusbar::Construct(OpStatusbar** obj)
{
	OpAutoPtr<OpStatusbar> statusbar(OP_NEW(OpStatusbar, ()));

	if (!statusbar.get())
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(statusbar->init_status))
		return statusbar->init_status;

	RETURN_IF_ERROR(statusbar->InitStatusbar());

	*obj = statusbar.release();

	return OpStatus::OK;
}

OpStatusbar::OpStatusbar()
  : OpToolbar(PrefsCollectionUI::StatusbarAlignment),
  m_head_bar(NULL),
  m_tail_bar(NULL)
{
	GetBorderSkin()->SetImage("Statusbar Skin");
}

BOOL OpStatusbar::OnInputAction(OpInputAction* action)
{
	switch(action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_RESTORE_TO_DEFAULTS:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
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
			RestoreToDefaults();
			return TRUE;
		}
	}
	return OpToolbar::OnInputAction(action);
}

void OpStatusbar::SetHeadTailAlignmentFromStatusbar(Alignment statusbar_alignment)
{
	if (statusbar_alignment != ALIGNMENT_OFF)
	{
		if (m_head_bar->GetResultingAlignment() != ALIGNMENT_OFF)
		{
			if(statusbar_alignment == ALIGNMENT_LEFT || statusbar_alignment == ALIGNMENT_RIGHT)
			{
				m_head_bar->SetAlignment(ALIGNMENT_TOP, FALSE);
			}
			else
			{
				m_head_bar->SetAlignment(statusbar_alignment, FALSE);
			}
		}
		if (m_tail_bar->GetResultingAlignment() != ALIGNMENT_OFF)
		{
			if(statusbar_alignment == ALIGNMENT_LEFT || statusbar_alignment == ALIGNMENT_RIGHT)
			{
				m_tail_bar->SetAlignment(ALIGNMENT_TOP, FALSE);
			}
			else
			{
				m_tail_bar->SetAlignment(statusbar_alignment, FALSE);
			}
		}
	}
}

/***********************************************************************************
**
**	OnRelayout
**
***********************************************************************************/

void OpStatusbar::OnRelayout()
{
	BOOL target = IsTargetToolbar();

	OpBar::OnRelayout();

	Alignment alignment = GetResultingAlignment();

	// head and tail toolbars get the same alignment as pagebar in order to define
	// their skinning based on the pagebar's position
	if (alignment != ALIGNMENT_OFF)
	{
		SetHeadTailAlignmentFromStatusbar(alignment);
	}
	if(target)
	{
		UpdateTargetToolbar(TRUE);
	}
}

/***********************************************************************************
**
**	OnLayout - Calculate the nominal size needed for the statusbar using the standard
**			toolbar layout code, then add the extra spacing we might need
**
***********************************************************************************/

void OpStatusbar::OnLayout()
{
	OpBar::OnLayout();
	UpdateHeadTailSize();
	OpBar::OnLayout();
}

void OpStatusbar::UpdateHeadTailSize()
{
	INT32 left, top, right, bottom;

	OpToolbar::GetPadding(&left, &top, &right, &bottom);

	// layout head and tail toolbars to be within the padding of this toolbar
	OpRect rect = GetBounds();
	rect.x += left;
	rect.y += top;
	rect.width -= left + right;
	rect.height -= top + bottom;

	OpRect head_rect;
	OpRect tail_rect;

	m_head_bar->GetRequiredSize(head_rect.width, head_rect.height);
	m_tail_bar->GetRequiredSize(tail_rect.width, tail_rect.height);

	if (IsHorizontal())
	{
		head_rect.x = rect.x;
		tail_rect.x = rect.x + rect.width - tail_rect.width;

		head_rect.y = rect.y;
		tail_rect.y = rect.y;

		head_rect.height = rect.height;
		tail_rect.height = rect.height;

		if (head_rect.width+tail_rect.width > rect.width)
		{
			head_rect.width = rect.width-tail_rect.width;
		}
	}
	else
	{
		head_rect.x = rect.x;
		head_rect.y = rect.y;

		tail_rect.x = rect.x;
		tail_rect.y = rect.y + rect.height - tail_rect.height;

		head_rect.width = rect.width;
		tail_rect.width = rect.width;
		if (head_rect.height+tail_rect.height > rect.height)
		{
			head_rect.height = rect.height-tail_rect.height;
		}
	}
	LayoutChildToAvailableRect(m_head_bar, head_rect);
	LayoutChildToAvailableRect(m_tail_bar, tail_rect);
}

/***********************************************************************************
**
**	GetPadding
**
***********************************************************************************/

void OpStatusbar::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom)
{
	OpToolbar::GetPadding(left, top, right, bottom);

	// add to padding what head and tail (and sometimes add-button) is eating of space
	INT32 head_width = 0, head_height = 0;
	INT32 tail_width = 0, tail_height = 0;

	if (m_head_bar->IsOn())
	{
		m_head_bar->GetRequiredSize(head_width, head_height);
	}

	if (m_tail_bar->IsOn())
	{
		m_tail_bar->GetRequiredSize(tail_width, tail_height);
	}
	if (IsHorizontal())
	{
		*left += head_width;
		*right += tail_width;

		if (GetWidgetCount() == 0)
		{
			// Make sure there is space for head and tail
			*top += max(head_height, tail_height);
		}
	}
	else
	{
		*top += head_height;
		*bottom += tail_height;

		if (GetWidgetCount() == 0)
		{
			// Make sure there is space for head and tail
			*left += max(head_width, tail_width);
		}
	}
}

/***********************************************************************************
**
**	SetAlignment
**
***********************************************************************************/

BOOL OpStatusbar::SetAlignment(Alignment alignment, BOOL write_to_prefs)
{
	// head and tail toolbars get the same alignment as pagebar in order to define
	// their skinning based on the pagebar's position
	if (alignment != ALIGNMENT_OFF)
	{
		SetHeadTailAlignmentFromStatusbar(alignment);
	}
	BOOL rc = OpBar::SetAlignment(alignment, write_to_prefs);

	return rc;
}

void OpStatusbar::EnableTransparentSkin(BOOL enable)
{
	if (enable)
	{
	    GetBorderSkin()->SetImage("Statusbar Transparent Skin", "Statusbar Skin");
	    m_head_bar->GetBorderSkin()->SetImage("Statusbar Head Transparent Skin", "Statusbar Head Skin");
	    m_tail_bar->GetBorderSkin()->SetImage("Statusbar Tail Transparent Skin", "Statusbar Tail Skin");
	}
	else
	{
		GetBorderSkin()->SetImage("Statusbar Skin");
		m_head_bar->GetBorderSkin()->SetImage("Statusbar Head Skin");
		m_tail_bar->GetBorderSkin()->SetImage("Statusbar Tail Skin");
	}
}

