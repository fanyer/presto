/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_SKIN_ELEMENT_H
#define QUICK_SKIN_ELEMENT_H

#include "adjunct/quick_toolkit/widgets/QuickBackgroundWidget.h"
#include "adjunct/quick_toolkit/widgets/OpMouseEventProxy.h"


/** A container widget that pads a widget using skin information.
  *
  * @see QuickSkinWrap
  */
class QuickSkinElement : public QuickBackgroundWidget<OpMouseEventProxy>
{
	typedef QuickBackgroundWidget<OpMouseEventProxy> Base;
	IMPLEMENT_TYPEDOBJECT(Base);
public:
	QuickSkinElement() : Base(true) {}
};


/**
 * Utility function to apply skinning to a QuickWidget.
 *
 * @param widget the widget to apply skinning to.  Ownership is transfered.
 * @param skin_element the skin element to use
 * @param fallback the fallback skin element to use if skin_element is
 *		unavailable
 * @param a QuickSkinElement object wrapping @a widget, or @c NULL on error
 */
QuickSkinElement* QuickSkinWrap(QuickWidget* widget,
		const OpStringC8& skin_element, const OpStringC8& fallback = 0);

#endif // QUICK_SKIN_ELEMENT_H
