/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_MULTILINE_LABEL_H
#define QUICK_MULTILINE_LABEL_H

#include "adjunct/quick_toolkit/widgets/QuickMultilineEdit.h"

/** @brief A label that can contain multiple lines of text
  */
class QuickMultilineLabel : public QuickMultilineEdit
{
	IMPLEMENT_TYPEDOBJECT(QuickMultilineEdit);
public:
	virtual OP_STATUS Init();
	virtual BOOL HeightDependsOnWidth() { return TRUE; }

protected:
	// Override QuickWidget
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width) { return GetDefaultMinimumHeight(width); }
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalWidth() { return GetDefaultMinimumWidth(); }
	virtual unsigned GetDefaultNominalHeight(unsigned width) { return GetDefaultMinimumHeight(width); }
};

#endif // QUICK_MULTILINE_EDIT_H
