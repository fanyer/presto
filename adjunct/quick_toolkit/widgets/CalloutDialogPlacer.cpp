/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
 
#include "core/pch.h"
 
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "modules/skin/OpSkinManager.h"

namespace
{
	const int ANCHOR_OVERLAP = 4;
}


CalloutDialogPlacer::CalloutDialogPlacer(OpWidget& anchor_widget, Alignment alignment, const char* dialog_skin)
	: m_anchor_widget(&anchor_widget)
	, m_alignment(alignment)
	, m_dialog_skin(dialog_skin)

{
}

OpRect CalloutDialogPlacer::CalculatePlacement(const OpRect& bounds, const OpRect& dialog_size)
{
	INT32 up = 0, down = 0, left = 0, right = 0;

	// because we are using root coordinates here (to avoid problems with
	// scrolled containers), we also need to stay in sync with value
	// returned in UsesRootCoordinates()
	OpRect anchor = m_anchor_widget->GetRect(TRUE);

	if (m_alignment == CENTER)
	{
		// Adjust anchor position for padding, and add overlap.
		// But ensure the final rect is non-empty.
		m_anchor_widget->AddPadding(anchor);
		anchor = anchor.InsetBy(MIN(anchor.width/2 - 1, ANCHOR_OVERLAP), MIN(anchor.height/2 - 1, ANCHOR_OVERLAP));
	}
	else
	{
		anchor = anchor.InsetBy(0, MIN(anchor.height/2 - 1, ANCHOR_OVERLAP));
	}

	// Prefer aligning the dialog under the anchor, assume other positions are
	// not needed.
	down = bounds.Bottom() - anchor.Bottom();

	// If the dialog doesn't fit under the anchor, calculate space on
	// top of anchor. If it doesn't fit there either, calculate the
	// horizontal space.
	if (down < dialog_size.height)
	{
		up = anchor.Top() - bounds.Top();
		if (up < dialog_size.height)
		{
			left = anchor.Left() - bounds.Left();
			right = bounds.Right() - anchor.Right();
		}
	}

	// Align dialog where most space is available
	OpRect placement = dialog_size;
	if (MAX(down, up) > MAX(left, right))
	{
		placement.x = anchor.x;
		if (m_alignment == LEFT && m_dialog_skin)
		{
			INT32 p_left, p_right, p_top, p_bottom;
			if (OpStatus::IsSuccess(g_skin_manager->GetPadding(m_dialog_skin, &p_left, &p_top, &p_right, &p_bottom)))
			{
				placement.x -= p_left;
			}
			if (m_anchor_widget->GetRTL())
				placement.x += anchor.width + (anchor.x - placement.x) * 2 - placement.width;
		}
		else if (m_alignment == CENTER)
		{
			placement.x += (anchor.width - dialog_size.width) / 2;
		}
		if (down > up)
		{
			placement.y = anchor.Bottom();
		}
		else
		{
			placement.y = anchor.Top() - dialog_size.height;
		}
	}
	else
	{
		placement.y = anchor.y;
		if (m_alignment == LEFT && m_dialog_skin)
		{
			INT32 p_left, p_right, p_top, p_bottom;
			if (OpStatus::IsSuccess(g_skin_manager->GetPadding(m_dialog_skin, &p_left, &p_top, &p_right, &p_bottom)))
			{
				placement.y -= p_top;
			}
		}
		else if (m_alignment == CENTER)
		{
			placement.y += (anchor.height - dialog_size.height) / 2;
		}
		if (right > left)
		{
			placement.x = anchor.Right();
		}
		else
		{
			placement.x = anchor.Left() - dialog_size.width;
		}
	}

	return placement;
}


void CalloutDialogPlacer::PointArrow(OpWidgetImage& arrow_skin, const OpRect& placement)
{
	arrow_skin.PointArrow(placement, m_anchor_widget->GetRect(TRUE));
}
