/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpProgress.h"
#include "modules/display/vis_dev.h"
#ifdef SKIN_SUPPORT
#include "modules/skin/OpSkinManager.h"
#endif // SKIN_SUPPORT

// == OpProgress ===========================================================

DEFINE_CONSTRUCT(OpProgress)

OpProgress::OpProgress()
	: m_type(TYPE_PROGRESS)
	, m_progress(0)
{
}

OpProgress::~OpProgress()
{
}

void OpProgress::SetType(TYPE type)
{
	if (type != m_type)
	{
		m_type = type;
		InvalidateAll();
	}
}

void OpProgress::SetProgress(float progress)
{
	OP_ASSERT((progress >= 0 && progress <= 1) || progress == -1);
	if (progress != m_progress)
	{
		m_progress = progress;
		InvalidateAll();
	}
}

/* virtual */
void OpProgress::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	*w = 160;
	*h = 20;
	return;
}

void OpProgress::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	// If everything is default (not styled by CSS) we can use the skin. Otherwise paint with colors.
	BOOL default_bg = GetColor().use_default_background_color;
#ifdef SKIN_SUPPORT
	BOOL use_skin = !g_widgetpaintermanager->NeedCssPainter(this);
	const char *skin_name_bg;
	const char *skin_name_fg;
	if (m_type == TYPE_PROGRESS)
	{
		skin_name_bg = "Progress Background Skin";
		skin_name_fg = "Progress Indicator Skin";
	}
	else
	{
		skin_name_bg = "Meter Background Skin";
		if (m_type == TYPE_METER_GOOD_VALUE)
			skin_name_fg = "Meter Indicator Good Skin";
		else if (m_type == TYPE_METER_BAD_VALUE)
			skin_name_fg = "Meter Indicator Bad Skin";
		else
			skin_name_fg = "Meter Indicator Worst Skin";
	}
#endif

	COLORREF bg_color = default_bg ? OP_RGB(255, 255, 255) : GetColor().background_color;
	COLORREF fg_color = m_type == TYPE_METER_BAD_VALUE ? OP_RGB(255, 216, 0) : OP_RGB(144, 190, 41);

	INT32 width = GetBounds().width;
	INT32 height = GetBounds().height;

	// Draw it

#ifdef SKIN_SUPPORT
	if (use_skin)
	{
		OpSkinElement *skin_elm = g_skin_manager->GetSkinElement(skin_name_bg);
		if (skin_elm)
		{
			// Draw background
			OpStatus::Ignore(g_skin_manager->DrawElement(vis_dev, skin_name_bg, OpRect(0, 0, width, height)));

			// Draw indicator (inset with background skin padding)
			if (m_progress != -1)
			{
				INT32 padding_left, padding_top, padding_right, padding_bottom;
				skin_elm->GetPadding(&padding_left, &padding_top, &padding_right, &padding_bottom, 0);
				const INT32 progress_width = INT32(m_progress * (width - padding_left - padding_right));
				OpRect rect(padding_left, padding_top, progress_width, height - padding_top - padding_bottom);
				if (GetRTL())
					rect.x = width - padding_right - progress_width;
				OpStatus::Ignore(g_skin_manager->DrawElement(vis_dev, skin_name_fg, rect));
			}
		}
		else
			// We had no skin so paint nonskinned below.
			use_skin = FALSE;
	}
	if (!use_skin)
#endif
	{
		if (default_bg)
		{
			vis_dev->SetColor(bg_color);
			vis_dev->FillRect(OpRect(0, 0, width, height));
		}
		if (m_progress != -1)
		{
			vis_dev->SetColor(fg_color);
			const INT32 progress_width = INT32(m_progress * width);
			OpRect rect(0, 0, progress_width, height);
			if (GetRTL())
				rect.x = width - progress_width;
			vis_dev->FillRect(rect);
		}
	}
}
