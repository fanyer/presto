/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen
 */

#ifndef QUICK_ADDRESS_DROPDOWN_H
#define QUICK_ADDRESS_DROPDOWN_H

#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "modules/widgets/OpEdit.h"

/** @brief An autocompleting dropdown for urls */
class QuickAddressDropDown : public QuickEditableTextWidgetWrapper<OpAddressDropDown>
{
	typedef QuickEditableTextWidgetWrapper<OpAddressDropDown> Base;
	IMPLEMENT_TYPEDOBJECT(Base);

public:
	virtual ~QuickAddressDropDown() { RemoveOpWidgetListener(*GetOpWidget()); }
	// The wrapped OpAddressDropDown needs to listen to itself to be able to
	// show the menu window.
	virtual OP_STATUS Init() { RETURN_IF_ERROR(Base::Init()); return AddOpWidgetListener(*GetOpWidget()); }

	// Implement QuickTextWidget
	virtual OP_STATUS SetText(const OpStringC& text) { return Base::GetOpWidget()->edit->SetText(text.CStr()); }

protected:
	virtual unsigned GetDefaultMinimumWidth() { return Base::GetCharWidth(GetOpWidget(),70); }
	virtual unsigned GetDefaultPreferredWidth() { return WidgetSizes::Infinity; }
};

#endif // QUICK_ADDRESS_DROPDOWN_H
