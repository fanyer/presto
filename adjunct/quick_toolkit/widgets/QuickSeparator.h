/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Kupczyk (tkupczyk)
 */
 
#ifndef QUICK_SEPARATOR_H
#define QUICK_SEPARATOR_H
 
#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"
#include "adjunct/quick_toolkit/widgets/OpSeparator.h"

class QuickSeparator : public QuickOpWidgetWrapper<OpSeparator>
{
	typedef QuickOpWidgetWrapper<OpSeparator> Base;
	IMPLEMENT_TYPEDOBJECT(Base);

public:
	static QuickSeparator* ConstructSeparator(const OpStringC8& skin_element = NULL);
	
protected:
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);
};

#endif //QUICK_SEPARATOR_H
