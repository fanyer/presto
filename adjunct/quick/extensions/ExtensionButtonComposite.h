/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef EXTENSION_BUTTON_COMPOSITE_H
#define EXTENSION_BUTTON_COMPOSITE_H

#ifdef EXTENSION_SUPPORT

#include "adjunct/quick/extensions/ExtensionButtonComponent.h"
#include "modules/windowcommander/OpExtensionUIListener.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

class ExtensionButtonComposite
	: public ExtensionButtonComponent
{
	public:

		ExtensionButtonComposite();

		virtual ~ExtensionButtonComposite();

		typedef OpExtensionUIListener::ExtensionId ExtensionId;

		OP_STATUS CreateOpExtensionButton(ExtensionId id);

		OP_STATUS RemoveOpExtensionButton(ExtensionId id);

		OP_STATUS UpdateUIItem(OpExtensionUIListener::ExtensionUIItem *item);

		void Add(ExtensionButtonComponent *extension_button);

		void Remove(ExtensionButtonComponent *extension_button);

		void SetGadgetId(const uni_char *gadget_id) { m_gadget_id.Set(gadget_id); }
	
		const uni_char* GetGadgetId() { return m_gadget_id.CStr(); }

		const uni_char* GetPopupURL() const { return m_popup.href; }

		void GetPopupSize(UINT32& width, UINT32& height) const
			{ width = m_popup.width; height = m_popup.height; }

		BOOL GetEnabled() const;

		OpExtensionUIListener::ExtensionUIItemNotifications* GetCallbacks();

		BOOL HasComponents() { return m_components.GetCount() > 0; }


		//
		// ExtensionButtonComponent
		//

	private:

		virtual INT32 GetId();

		virtual OP_STATUS UpdateIcon(Image &image);

		virtual void SetBadgeBackgroundColor(COLORREF color);

		virtual void SetBadgeTextColor(COLORREF color);

		virtual void SetBadgeText(const OpStringC& text);

		virtual void SetDisabled(BOOL disabled);

		virtual void SetTitle(const OpStringC& title);

		//
		// ExtensionButtonComponent
		//

		void SetUIItem(OpExtensionUIListener::ExtensionUIItem item);

		//
		// members
		//
		
		COLORREF OpColorToColorref(OpWindowCommander::OpColor color);

		OpVector<ExtensionButtonComponent> m_components;

		Image m_icon;

		OpExtensionUIListener::ExtensionBadge m_badge;

		OpExtensionUIListener::ExtensionPopup m_popup;
	
		OpString m_gadget_id;

		OpExtensionUIListener::ExtensionUIItem m_item;
};

#endif // EXTENSION_SUPPORT
#endif // EXTENSION_BUTTON_COMPOSITE_H
