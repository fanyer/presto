/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickIcon.h"

unsigned QuickIcon::GetWidth()
{
	INT32 width, height;
	GetOpWidget()->GetForegroundSkin()->GetSize(&width, &height);
	return width;
}

unsigned QuickIcon::GetHeight()
{
	INT32 width, height;
	GetOpWidget()->GetForegroundSkin()->GetSize(&width, &height);
	return height;
}
