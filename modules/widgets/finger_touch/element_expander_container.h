/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ELEMENT_EXPANDER_CONTAINER_H
#define ELEMENT_EXPANDER_CONTAINER_H

#include "modules/widgets/WidgetContainer.h"

class ElementExpanderImpl;

class ElementExpanderContainer : public WidgetContainer
{
public:
	ElementExpanderContainer(ElementExpanderImpl *ee)
		: element_expander(ee) { }

protected:
	// from OpInputContext
	BOOL OnInputAction(OpInputAction* action);

private:
	ElementExpanderImpl *element_expander;
};

#endif // ELEMENT_EXPANDER_CONTAINER_H
