/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr)
 */

#ifndef WIDGET_CONTEXT_H
#define WIDGET_CONTEXT_H

#include "adjunct/quick_toolkit/contexts/UiContext.h"

class WidgetReader;
class QuickWidget;
class TypedObjectCollection;

/** @brief a generic context for widgets
  * Derive from this class to make a context that can read and create custom widgets from the widgets.yml file
  */
class WidgetContext : public UiContext
{
public:

	static OP_STATUS CreateQuickWidget(const OpStringC8 & widget_name, QuickWidget* &widget, TypedObjectCollection &collection);

private:
	static OP_STATUS InitReader();
	static OpAutoPtr<WidgetReader> s_reader;
};

#endif //WIDGET_CONTEXT_H
