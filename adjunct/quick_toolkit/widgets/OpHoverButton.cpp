/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpHoverButton.h"


/***********************************************************************************
 ** Construction function
 **
 ** OpHoverButton::Construct
 **
 ***********************************************************************************/
OP_STATUS OpHoverButton::Construct(OpHoverButton** obj, UINT32 hover_color, ButtonType button_type, ButtonStyle button_style)
{
	*obj = OP_NEW(OpHoverButton, (hover_color, button_type, button_style));

	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Called when mouse is moved on this widget
 **
 ** OpHoverButton::OnMouseMove
 **
 ***********************************************************************************/
void OpHoverButton::OnMouseMove(const OpPoint & point)
{
	if (m_color.use_default_foreground_color)
	{
		m_color.use_default_foreground_color = FALSE;
		m_color.foreground_color = m_hover_color;

		InvalidateAll();
	}
}


/***********************************************************************************
 ** Called when mouse leaves this widget
 **
 ** OpHoverButton::OnMouseLeave
 **
 ***********************************************************************************/
void OpHoverButton::OnMouseLeave()
{
	if (!m_color.use_default_foreground_color)
	{
		m_color.use_default_foreground_color = TRUE;

		InvalidateAll();
	}
}
