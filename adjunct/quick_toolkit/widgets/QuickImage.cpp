/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickImage.h"

#include "modules/skin/OpWidgetImage.h"


OP_STATUS QuickImage::Init()
{
	RETURN_IF_ERROR((QuickOpWidgetWrapper<OpButton>::Init()));

	GetOpWidget()->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);

	return OpStatus::OK;
}


OP_STATUS QuickImage::SetImage(Image& image)
{
	GetOpWidget()->GetForegroundSkin()->SetBitmapImage(image);

	return OpStatus::OK;
}
