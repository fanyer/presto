/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_WIDGET_CONTAINER_H
#define QUICK_WIDGET_CONTAINER_H

/** @brief An object that contains one or more widgets
  * Derive from this class to receive a message when contents change
  * @see QuickWidget::SetContainer()
  */
class QuickWidgetContainer
{
public:
	virtual ~QuickWidgetContainer() {}

	/** Called when contents in this container have changed */
	virtual void OnContentsChanged() = 0;
};

#endif // QUICK_WIDGET_CONTAINER_H
