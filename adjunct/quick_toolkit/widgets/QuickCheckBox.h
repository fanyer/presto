/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */

#ifndef QUICK_CHECK_BOX_H
#define QUICK_CHECK_BOX_H

#include "adjunct/quick_toolkit/widgets/QuickSelectable.h"

class QuickCheckBox : public QuickSelectable
{
	IMPLEMENT_TYPEDOBJECT(QuickSelectable);
public:
	virtual OP_STATUS ConstructSelectableQuickWidget();
};

#endif // QUICK_CHECK_BOX_H
