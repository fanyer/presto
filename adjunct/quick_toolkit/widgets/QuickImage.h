/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_IMAGE_H
#define QUICK_IMAGE_H

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"
#include "modules/widgets/OpButton.h"

class QuickImage : public QuickOpWidgetWrapper<OpButton>
{
	IMPLEMENT_TYPEDOBJECT(QuickOpWidgetWrapper<OpButton>);
public:
	virtual OP_STATUS Init();

	OP_STATUS SetImage(Image& image);
};

#endif // QUICK_IMAGE_H
