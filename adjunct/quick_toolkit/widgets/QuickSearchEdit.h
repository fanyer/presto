/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen
 */

#ifndef QUICK_SEARCH_EDIT_H
#define QUICK_SEARCH_EDIT_H

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"
#include "adjunct/quick/widgets/OpSearchEdit.h"

class QuickSearchEdit : public QuickOpWidgetWrapper<OpSearchEdit>
{
	IMPLEMENT_TYPEDOBJECT(QuickOpWidgetWrapper<OpSearchEdit>);

protected:
	virtual unsigned GetDefaultMinimumWidth() { return 100; }
	virtual unsigned GetDefaultPreferredWidth() { return 200; }
};


#endif // QUICK_SEARCH_EDIT_H
