/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_MULTILINE_EDIT_H
#define QUICK_MULTILINE_EDIT_H

#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"
#include "modules/widgets/OpMultiEdit.h"

#include "adjunct/desktop_util/widget_utils/WidgetUtils.h"

/** @brief An edit field that can have multiple lines of text and a scrollbar
  */
class QuickMultilineEdit : public QuickEditableTextWidgetWrapper<OpMultilineEdit>
{
	typedef QuickEditableTextWidgetWrapper<OpMultilineEdit> Base;
	IMPLEMENT_TYPEDOBJECT(Base);
public:
	virtual OP_STATUS SetText(const OpStringC& text);
	OP_STATUS SetText(const OpStringC8& text8) { return Base::SetText(text8); }
	OP_STATUS SetText(Str::LocaleString id) { return Base::SetText(id); }

protected:
	// Override QuickWidget
	virtual unsigned GetDefaultMinimumHeight(unsigned width)
		{ return GetCharHeight(GetOpWidget(), MinimumLines); }
	virtual unsigned GetDefaultNominalHeight(unsigned width)
		{ return GetCharHeight(GetOpWidget(), NominalLines); }
	virtual unsigned GetDefaultPreferredWidth()
		{ return WidgetSizes::Infinity; }
	virtual unsigned GetDefaultPreferredHeight(unsigned width)
		{ return WidgetSizes::Infinity; }

private:
	static const unsigned MinimumLines = 2; ///< Lines visible at default minimum size
	static const unsigned NominalLines = 4; ///< Lines visible at default nominal size
};

#endif // QUICK_MULTILINE_EDIT_H
