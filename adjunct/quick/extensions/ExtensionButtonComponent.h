/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef EXTENSION_BUTTON_COMPONENT_H
#define EXTENSION_BUTTON_COMPONENT_H

#include "modules/windowcommander/OpWindowCommander.h"

class Image;

/**
 * Component interface for extension buttons.
 *
 * Composite pattern for this class connects ExtensionButtonComposite with
 * OpExtensionButton (which is component).
 */
class ExtensionButtonComponent
{
	public:

		ExtensionButtonComponent() : m_connected(FALSE) {}

		virtual INT32 GetId() = 0;

		virtual OP_STATUS UpdateIcon(Image &icon) = 0;

		virtual void SetBadgeBackgroundColor(COLORREF color) = 0;

		virtual void SetBadgeTextColor(COLORREF color) = 0;

		virtual void SetBadgeText(const OpStringC& text) = 0;

		/**
		 * Set connection status between composite and component.
		 *
		 * OpExtensionButton should do callbacks only if it is connected
		 * to composite.
		 */
		virtual void SetConnected(BOOL connected) { m_connected = connected; }

		virtual void SetDisabled(BOOL disabled) = 0;

		virtual void SetTitle(const OpStringC& title) = 0;

	protected:

		BOOL m_connected;
};

#endif // EXTENSION_BUTTON_COMPONENT_H
