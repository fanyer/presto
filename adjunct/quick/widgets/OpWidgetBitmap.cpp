/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/pi/OpBitmap.h"

#include "OpWidgetBitmap.h"
#include "modules/img/image.h"

/***********************************************************************************
**
**	OpWidgetBitmap
**
***********************************************************************************/

OpWidgetBitmap::OpWidgetBitmap(OpBitmap* bitmap)
	: m_image()
	, m_refcounter(0)
{
	m_image = imgManager->GetImage(bitmap);
}

OpWidgetBitmap::~OpWidgetBitmap()
{
}

void OpWidgetBitmap::Release()
{
	m_refcounter--;

	if (m_refcounter <= 0)
		OP_DELETE(this);
}
