/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef EXTENSIONS_MODEL_ITEM_H
#define EXTENSIONS_MODEL_ITEM_H

#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetClass.h"

#define GADGET_ENABLE_ON_STARTUP UNI_L("GadgetEnabledOnStartup")
#define GADGET_RUN_IN_PRIVATE_MODE UNI_L("GadgetRunInPrivateMode")
#define GADGET_RUN_ON_SECURE_CONN UNI_L("GadgetRunOnSecureConn")

class ExtensionsModelItem
{
	public:

		ExtensionsModelItem(OpGadget& gadget);

		OP_STATUS Init();

		const uni_char* GetExtensionId() const;
		const uni_char* GetExtensionPath() const;

		OpGadget* GetGadget() const { return &m_gadget; }
		OpGadgetClass* GetGadgetClass() const { return m_gadget.GetClass(); }

		void SetEnableOnStartup(BOOL enable);
		BOOL EnableOnStartup() const;

		BOOL IsEnabled() const { return m_gadget.IsRunning(); }
		BOOL IsDisabled() const { return !IsEnabled(); }

		void AllowUserJSOnSecureConn(BOOL allow);
		BOOL IsUserJSAllowedOnSecureConn() const;

		void AllowUserJSInPrivateMode(BOOL allow);
		BOOL IsUserJSAllowedInPrivateMode() const;

		void SetIsInDevMode(BOOL dev_mode) { m_dev_mode = dev_mode; }
		BOOL IsInDevMode() const { return m_dev_mode; }

		void SetIsUpdateAvailable(BOOL available) { m_update_available = available; }
		BOOL IsUpdateAvailable() const { return m_update_available; }

		void SetTemporary(BOOL temporary) { m_temporary= temporary; }
		BOOL IsTemporary() const { return m_temporary; }
		
		OP_STATUS SetExtendedName(const OpStringC& name){return m_extended_name.Set(name);}
		const uni_char* GetExtendedName()const {return m_extended_name.CStr();}

		OP_STATUS SetDeleted(BOOL deleted);
		BOOL IsDeleted() const;

	private:

		OpGadget& m_gadget;
		BOOL m_dev_mode;
		BOOL m_update_available;
		BOOL m_temporary;
		OpString m_extended_name;

		void LoadPersistentData();
};

#endif // EXTENSIONS_MODEL_ITEM_H
