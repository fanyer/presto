/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "modules/widgets/OpMultiEdit.h"
#include "adjunct/quick/widgets/OpPrivateBrowsingBar.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/widgets/OpButton.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

DEFINE_CONSTRUCT(OpPrivateBrowsingBar)

OpPrivateBrowsingBar::OpPrivateBrowsingBar()
{
	m_checkbox = NULL;

	// OpBar has to have a name to perform actions! what?
	SetName("Private Mode Bar");

	if(OpStatus::IsSuccess(OpMultilineEdit::Construct(&m_text)))
	{
		m_text->SetLabelMode();
		AddChild(m_text);
		//m_text->SetText(UNI_L("Private browsing is enabled in this tab. Opera will forget everything you did when you close the tab."));
	}

	if(OpStatus::IsSuccess(OpButton::Construct(&m_close)))
	{
		OpInputAction* close = OP_NEW(OpInputAction, (OpInputAction::ACTION_SET_ALIGNMENT, ALIGNMENT_OFF, 0, 0, 0, Str::NOT_A_STRING, "Caption Close"));
		m_close->SetAction(close);
		m_close->GetForegroundSkin()->SetImage("Caption Close");
		m_close->SetButtonStyle(OpButton::STYLE_IMAGE);
		AddChild(m_close);
	}
	
	if(OpStatus::IsSuccess(OpButton::Construct(&m_more_info)))
	{
		OpInputAction* close				= OP_NEW(OpInputAction,(OpInputAction::ACTION_SET_ALIGNMENT, ALIGNMENT_OFF));
		OpInputAction* go_to_privacy_page	= OP_NEW(OpInputAction,(OpInputAction::ACTION_GO_TO_PRIVACY_INTRO_PAGE));
		close->SetActionOperator(OpInputAction::OPERATOR_AND);
		close->SetNextInputAction(go_to_privacy_page);
		m_more_info->SetAction(close);
		m_more_info->SetText(UNI_L("More information"));
		AddChild(m_more_info);
	}	

	if (OpStatus::IsSuccess(OpButton::Construct(&m_checkbox)))
	{
		m_checkbox->SetButtonType(OpButton::TYPE_CHECKBOX);
		m_checkbox->SetFixedTypeAndStyle(TRUE);
		m_checkbox->SetText(UNI_L("Do not show this again"));
		AddChild(m_checkbox);
	}

	SetAlignment(OpBar::ALIGNMENT_OFF);

	GetBorderSkin()->SetImage("Content Block Toolbar Skin");
}

void OpPrivateBrowsingBar::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if(widget == m_checkbox)
	{
#ifdef PREFS_CAP_SHOW_PRIVATE_BROWSING_BAR
		TRAPD(err,g_pcui->WriteIntegerL(PrefsCollectionUI::ShowPrivateBrowsingBar, !widget->GetValue()));
#endif
	}
}


void OpPrivateBrowsingBar::OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height)
{
	m_text->SetText(UNI_L("Private browsing is enabled in this tab. Opera will forget everything you did when you close the tab."));
	
	// calculate the room for text
	INT32 close_width, close_height,
		more_info_width, more_info_height,
		check_box_width, check_box_height,
		height,offset_y = 0,
		padding_top, padding_bottom, padding_left, padding_right, padding_vertical, padding_horizontal, 
		dummy;
	
	GetPadding(&padding_left, &padding_top, &padding_right, &padding_bottom);

	padding_vertical = padding_top + padding_bottom;
	padding_horizontal = padding_left + padding_right;

	padding_top += 2;

	m_close->GetPreferedSize(&close_width, &close_height, 0, 0);
	m_more_info->GetPreferedSize(&more_info_width, &more_info_height, 0, 0);
	m_checkbox->GetPreferedSize(&check_box_width, &check_box_height, 0, 0);
	
	height = MAX(check_box_height,MAX(close_height,more_info_height)) + 2;
	
	used_height = height*2 + padding_bottom + padding_top;
	used_width = available_width;

	// animation
	if(IsAnimating())
	{
		double p = m_animation_in ? m_animation_progress : 1.0 - m_animation_progress;
		offset_y = used_height*(p-1);
		used_height *= p;
	}

	if(!compute_size_only)
	{
		INT32 label_2row_height;
		m_text->GetPreferedSize(&dummy, &label_2row_height, 1, 2);


		INT32 text_width = available_width - MAX(check_box_width, (close_width + more_info_width)) - padding_horizontal;
		if(text_width <= 0)
		{
			m_text->SetVisibility(FALSE);
		}
		else
		{
			//m_text->SetWrap(TRUE);
			m_text->SetRect(OpRect(padding_left, padding_top + offset_y, text_width,label_2row_height));
			m_text->SetVisibility(TRUE);
		}

		// Buttons
		m_more_info->SetRect(OpRect(available_width - close_width - more_info_width - padding_right,	padding_top + offset_y,			more_info_width,	more_info_height));
		m_close->SetRect	(OpRect(available_width - close_width - padding_right,						padding_top + offset_y,			close_width,		close_height));
		m_checkbox->SetRect (OpRect(available_width - check_box_width - padding_right,					height + padding_top + offset_y, check_box_width,	check_box_height));
	}
}
