/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** OpAccessibilityAdapter.h -- abstract class for interfacing with screenreaders and other accessibility tools.
*/

#ifndef OPACCESSIBILITYADAPTER_H
#define OPACCESSIBILITYADAPTER_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/accessibilityenums.h"
class OpWindow;
class OpAccessibleItem;

/** @short Platform listener for accessibility events.
 * It mainly warns the platform about events and should provide an interface supported by the
 * platform to allow it to obtain information from the accessible item associated to it.
 */

class OpAccessibilityAdapter
{
public:

	/** Create an OpAccessibilityAdapter.
	 *
	 * @param adapter The new adapter to be created and returned.
	 * @param accessible_item The accessible item this adapter should report for.
	 *
	 * @return OK on success; otherwise, caller must ignore *adapter and return indicates what went wrong.
	 */
	static OP_STATUS Create(OpAccessibilityAdapter** adapter,
			OpAccessibleItem* accessible_item);

	/** Send an accessibility event on the behalf of an accessible item.
	 *
	 * The sender is guaranteed to be ready to handle accessibility requests when this is called.
	 *
	 * @param sender The accessible item sending the event
	 * @param evt The accessibility event to send on the behalf of the item
	 *
	 * @return Whether the event was sent successfully or not
	 */
	static OP_STATUS SendEvent(OpAccessibleItem* sender, Accessibility::Event evt);

	/**
	 * The platform is responsible for deleting these objects, for details;
	 * see the documentation of OpAccessibilityAdapter::AccessibleItemRemoved().
	 */
	virtual ~OpAccessibilityAdapter() {}

	/** Get the accessible item represented by this adapter
	 *
	 * @return the accessible item
	 */
	virtual OpAccessibleItem* GetAccessibleItem() = 0;

	/**
	 * Removal notification.
	 *
	 * Called by core just before it deletes the OpAccessibleItem the OpAccessibilityAdapter has been handling;
	 * the OpAccessibilityAdapter must not reference the item after returning from this method.  Core will make
	 * no further reference to this OpAccessibilityAdapter, so the platform is at liberty to delete it once this
	 * method has been called.
	 */
	virtual void AccessibleItemRemoved() = 0;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // OPACCESSIBILITYEXT_H
