/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */
 
#ifndef QUICK_LABEL_H
#define QUICK_LABEL_H
 
#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"
#include "adjunct/quick_toolkit/widgets/DesktopLabel.h"

class QuickLabel : public QuickTextWidgetWrapper<DesktopLabel>
{
	typedef QuickTextWidgetWrapper<DesktopLabel> Base;
	IMPLEMENT_TYPEDOBJECT(Base);

public:
	virtual OP_STATUS Init();

	static QuickLabel* ConstructLabel(const OpStringC& text);

protected:
	// QuickWidget
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultNominalWidth() { return GetDefaultMinimumWidth(); }
	virtual unsigned GetDefaultPreferredWidth() { return GetDefaultMinimumWidth(); }
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalHeight(unsigned width) { return GetDefaultMinimumHeight(width); }
	virtual unsigned GetDefaultPreferredHeight(unsigned width) { return GetDefaultMinimumHeight(width); }
};

#endif //QUICK_LABEL_H
