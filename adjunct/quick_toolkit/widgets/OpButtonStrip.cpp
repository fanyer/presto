/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpButtonStrip.h"

#include "modules/img/img_module.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/widgets/OpButton.h"

OpButtonStrip::OpButtonStrip(BOOL reverse_buttons)
	: m_reverse_buttons(reverse_buttons)
	, m_width(100)
	, m_height(23)
	, m_resize_buttons(TRUE)
	, m_centered_buttons(FALSE)
	, m_back_forward_buttons(FALSE)
	, m_button_rects(0)
	, m_total_width(0)
	, m_button_margins(SKIN_DEFAULT_MARGIN, SKIN_DEFAULT_MARGIN, SKIN_DEFAULT_MARGIN, SKIN_DEFAULT_MARGIN)
	, m_button_padding(SKIN_DEFAULT_PADDING, SKIN_DEFAULT_PADDING, SKIN_DEFAULT_PADDING, SKIN_DEFAULT_PADDING)
	, m_dynamic_space(0)
	, m_default_button_ratio(1.0f)
	, m_is_custom_size(FALSE)
{
}

OpButtonStrip::~OpButtonStrip()
{
	OP_DELETEA(m_button_rects);
}

OP_STATUS OpButtonStrip::SetButtonCount(int button_count)
{
	m_buttons.DeleteAll();
	OP_DELETEA(m_button_rects);
	m_button_rects = 0;

	for (int i = 0; i < button_count; i++)
	{
		RETURN_IF_ERROR(AddButton());
	}

	m_button_rects = OP_NEWA(OpRect, button_count);
	if (!m_button_rects)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS OpButtonStrip::AddButton()
{
	OpButton* button;
	RETURN_IF_ERROR(OpButton::Construct(&button));

	AddChild(button);
	RETURN_IF_ERROR(m_buttons.Add(button));

	button->SetID(m_buttons.Find(button));

	button->SetXResizeEffect(RESIZE_MOVE);
	button->SetYResizeEffect(RESIZE_MOVE);

	OpWidget* border = OP_NEW(OpWidget, ());
	if (!border)
		return OpStatus::ERR_NO_MEMORY;

	AddChild(border);
	button->SetUserData(border);

	border->SetSkinned(TRUE);
	border->GetBorderSkin()->SetImage(m_border_skin.CStr());
	border->SetXResizeEffect(RESIZE_MOVE);
	border->SetYResizeEffect(RESIZE_MOVE);

	// Hide the help button if kiosk mode is set
	// TODO
	/* if (KioskManager::GetInstance()->GetEnabled() && !uni_strcmp(text, GetHelpText()))
		button->SetVisibility(FALSE); */

	return OpStatus::OK;
}

OP_STATUS OpButtonStrip::SetButtonInfo(int id, OpInputAction* action, const OpStringC& text, BOOL enabled, BOOL visible, const OpStringC8& name)
{
	OP_ASSERT(0 <= id && (unsigned)id < m_buttons.GetCount());

	SetButtonAction(id, action);
	RETURN_IF_ERROR(SetButtonText(id, text));
	EnableButton(id, enabled);
	ShowButton(id, visible);
	SetButtonName(id, name);

	return OpStatus::OK;
}

void OpButtonStrip::SetDefaultButton(int id, float default_button_size /* = 1.0f */)
{
	OP_ASSERT(0 <= id && (unsigned)id < m_buttons.GetCount());
	m_buttons.Get(id)->SetDefaultLook(TRUE);
	m_default_button_ratio = default_button_size;
}

int OpButtonStrip::CalculatePositions()
{

	// paddings & margins to be added to raw button width/height
	INT32 buttonwidth_padding_margin = m_button_padding.left + m_button_padding.right + m_button_margins.left + m_button_margins.right;
	INT32 buttonheight_padding_margin = m_button_padding.top + m_button_padding.bottom + m_button_margins.top + m_button_margins.bottom;
	
	INT32 current_button_width; // width of the button currently looked at
	UINT32 num_buttons = m_buttons.GetCount();
	INT32 prefered_width = m_width;
	m_total_width = 0; // total with of dialog buttons

	UINT32 i;
	// Take preferred size, if larger than default size
	if (m_resize_buttons)
	{
		INT32 w, h;
		for (i = 0; i < num_buttons; i++)
		{
			m_buttons.Get(i)->GetPreferedSize(&w, &h, 0, 0);

			// If this is a default button, then don't compare the text width with 
			// m_width, but the actual default button width, which might be bigger.
			if (m_buttons.Get(i)->HasDefaultLook())
				prefered_width = MAX(prefered_width, w / m_default_button_ratio);
			else
				prefered_width = MAX(prefered_width, w);
		}
	}

	// Set default positions/sizes
	for (i = 0; i < num_buttons; i++)
	{
		// If this is the default button. Add the extra button size.
		if (m_buttons.Get(i)->HasDefaultLook())
			current_button_width = prefered_width * m_default_button_ratio;
		else
			current_button_width = prefered_width;

		m_button_rects[i].x = 0;
		m_button_rects[i].y = 0;
		m_button_rects[i].width = current_button_width + buttonwidth_padding_margin;
		m_button_rects[i].height = m_height + buttonheight_padding_margin;

		m_total_width += m_button_rects[i].width;
	}

	return m_total_width;
}

void OpButtonStrip::PlaceButtons()
{
	// place dialog buttons
	OpRect rect = GetBounds();
	INT32 width_used = 0;

	INT32 x_space_left = MAX(rect.width - m_total_width, 0);
	INT32 end_pos = m_centered_buttons ? (m_total_width + rect.width) / 2 : rect.width;
	unsigned num_buttons = m_buttons.GetCount();

	for (unsigned i = 0; i < num_buttons; i++)
	{
		if (m_reverse_buttons) // platforms with reversed buttons
		{
			// don't reverse back and forward
			if (m_back_forward_buttons && i <= 1)
			{
				m_button_rects[i].x = end_pos - m_button_rects[i].width;
				if (i == 0)
				{
					m_button_rects[i].x -= m_button_rects[1].width;
				}
			}
			else
			{
				m_button_rects[i].x = end_pos - width_used - m_button_rects[i].width;
			}
		}
		else
		{
			m_button_rects[i].x = end_pos - m_total_width + width_used;
		}

		if (m_dynamic_space > i)
			m_button_rects[i].x -= x_space_left;

		width_used += m_button_rects[i].width;
		PlaceDialogButton(i, m_button_rects[i]);
	}
}

void OpButtonStrip::PlaceDialogButton(INT32 button_index, const OpRect& rect)
{
	OpButton* button = m_buttons.Get(button_index);
	OpWidget* border = reinterpret_cast<OpWidget*>(button->GetUserData());

	OpRect border_rect;
	border_rect.x = rect.x + m_button_margins.left;
	border_rect.y = rect.y + m_button_margins.top;
	border_rect.width = rect.width - (m_button_margins.left + m_button_margins.right);
	border_rect.height = rect.height - (m_button_margins.top + m_button_margins.bottom);

	SetChildRect(border, border_rect, FALSE, FALSE);

	OpRect button_rect;
	button_rect.x = border_rect.x + m_button_padding.left;
	button_rect.y = border_rect.y + m_button_padding.top;
	button_rect.width = border_rect.width - (m_button_padding.left + m_button_padding.right);
	button_rect.height = border_rect.height - (m_button_padding.top + m_button_padding.bottom);

	SetChildRect(button, button_rect, FALSE, FALSE);
}

int OpButtonStrip::GetButtonId(OpWidget* widget)
{
	for (unsigned i = 0; i < m_buttons.GetCount(); i++)
	{
		if (m_buttons.Get(i) == widget)
			return i;
	}

	return -1;
}

void OpButtonStrip::ShowButton(int id, BOOL show)
{
	OpButton *button = m_buttons.Get(id);
	button->SetVisibility(show);
	reinterpret_cast<OpWidget*>(button->GetUserData())->SetVisibility(show);
}
