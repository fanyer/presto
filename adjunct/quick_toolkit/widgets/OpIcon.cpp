/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * Author: Adam Minchinton
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpIcon.h"

/***********************************************************************************
**
**	OpIcon
**
***********************************************************************************/

DEFINE_CONSTRUCT(OpIcon)

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OpIcon::OpIcon()
	: m_allow_scaling(false)
{
	SetSkinned(TRUE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OpIcon::~OpIcon()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpIcon::SetImage(const char* image_name, BOOL foreground)
{
	if (!image_name)
		return;

	if (foreground)
		GetForegroundSkin()->SetImage(image_name);
	else
		GetBorderSkin()->SetImage(image_name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpIcon::SetBitmapImage(Image& image, BOOL foreground)
{
	// Set the image if it's passed in
	if (foreground)
		GetForegroundSkin()->SetBitmapImage(image);
	else
		GetBorderSkin()->SetBitmapImage(image);
}

void OpIcon::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	if (GetBorderSkin()->HasContent())
		GetBorderSkin()->GetSize(w, h);
	else if (GetForegroundSkin()->HasContent())
		GetForegroundSkin()->GetSize(w, h);
	else
		*w = *h = 0;
}

void OpIcon::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (GetForegroundSkin()->HasContent())
	{
		OpRect rect = GetBounds();
		INT32 imgw, imgh;
		GetForegroundSkin()->GetSize(&imgw, &imgh);

		if (m_allow_scaling)
		{
			imgh = MIN(imgh, rect.height);
			imgw = MIN(imgw, rect.width);
		}

		GetForegroundSkin()->Draw(GetVisualDevice(), OpRect((rect.width - imgw)/2, (rect.height - imgh)/2, imgw, imgh));
	}
}
