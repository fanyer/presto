/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */

#ifndef QUICK_NUMBER_EDIT_H
#define QUICK_NUMBER_EDIT_H

#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"
#include "modules/widgets/OpNumberEdit.h"

class QuickNumberEdit : public QuickEditableTextWidgetWrapper<OpNumberEdit>
{
	IMPLEMENT_TYPEDOBJECT(QuickOpWidgetWrapper<OpNumberEdit>);

protected:
	virtual unsigned GetDefaultPreferredWidth() {  return WidgetSizes::Infinity; }
};

#endif // QUICK_NUMBER_EDIT_H
