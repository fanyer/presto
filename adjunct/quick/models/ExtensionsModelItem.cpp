/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "ExtensionsModelItem.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"


#include "modules/dochand/win.h"

ExtensionsModelItem::ExtensionsModelItem(OpGadget& gadget) : 
	m_gadget(gadget), 
	m_dev_mode(FALSE),
	m_update_available(FALSE),
	m_temporary(FALSE)
{
}

const uni_char* ExtensionsModelItem::GetExtensionId() const 
{ 
	return m_gadget.GetIdentifier(); 
}

const uni_char* ExtensionsModelItem::GetExtensionPath() const 
{ 
	return m_gadget.GetGadgetPath(); 
}

OP_STATUS ExtensionsModelItem::Init()
{
	if (!m_gadget.IsExtension())
		return OpStatus::ERR;
	
	LoadPersistentData();

	OpString update_file_path;
	RETURN_IF_ERROR(g_desktop_gadget_manager->GetUpdateFileName(m_gadget, update_file_path));

	if (update_file_path.HasContent())
	{
		OpFile update_file;
		RETURN_IF_ERROR(update_file.Construct(update_file_path));

		BOOL exists; 
		if (OpStatus::IsSuccess(update_file.Exists(exists)) && exists)
		{
			m_update_available = TRUE;
		}
	}

	if(ExtensionUtils::RequestsSpeedDialWindow(*m_gadget.GetClass()))
	{
		RETURN_IF_ERROR(SetExtendedName(m_gadget.GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE)));
	}

	return OpStatus::OK;
}

void ExtensionsModelItem::SetEnableOnStartup(BOOL enable)
{
	m_gadget.SetPersistentData(GADGET_ENABLE_ON_STARTUP,
			enable ? UNI_L("yes") : (const uni_char*) NULL);
}

BOOL ExtensionsModelItem::EnableOnStartup() const
{
	return m_gadget.GetPersistentData(GADGET_ENABLE_ON_STARTUP) != NULL;
}

void ExtensionsModelItem::AllowUserJSOnSecureConn(BOOL allow)
{
	m_gadget.SetExtensionFlag(OpGadget::AllowUserJSHTTPS, allow); 
	m_gadget.SetPersistentData(GADGET_RUN_ON_SECURE_CONN,
			allow ? UNI_L("yes") : (const uni_char*) NULL);
}

BOOL ExtensionsModelItem::IsUserJSAllowedOnSecureConn() const
{
	return m_gadget.GetExtensionFlag(OpGadget::AllowUserJSHTTPS);
}

void ExtensionsModelItem::AllowUserJSInPrivateMode(BOOL allow)
{
	// FIXME: Because bare OpGadget class is passed in many
	// places and lots of code does something with it, it may
	// happen that someone will do SetExtensionFlag and we won't
	// get a chance to synchronise data with disk. The same applies
	// to ExtensionsModelItem::AllosUserJSOnSecureConn.

	m_gadget.SetExtensionFlag(OpGadget::AllowUserJSPrivate, allow); 
	m_gadget.SetPersistentData(GADGET_RUN_IN_PRIVATE_MODE,
			allow ? UNI_L("yes") : (const uni_char*) NULL);
}

BOOL ExtensionsModelItem::IsUserJSAllowedInPrivateMode() const
{
	return m_gadget.GetExtensionFlag(OpGadget::AllowUserJSPrivate); 
}

void ExtensionsModelItem::LoadPersistentData()
{
	AllowUserJSInPrivateMode(
			m_gadget.GetPersistentData(GADGET_RUN_IN_PRIVATE_MODE) != NULL ?
				TRUE : FALSE);
	AllowUserJSOnSecureConn(
			m_gadget.GetPersistentData(GADGET_RUN_ON_SECURE_CONN) != NULL ?
				TRUE : FALSE);
	SetEnableOnStartup(
			m_gadget.GetPersistentData(GADGET_ENABLE_ON_STARTUP) != NULL ?
				TRUE : FALSE);
}

OP_STATUS ExtensionsModelItem::SetDeleted(BOOL deleted)
{
	return m_gadget.SetPersistentData(UNI_L("deleted"), deleted ? UNI_L("yes") : NULL);
}

BOOL ExtensionsModelItem::IsDeleted() const
{
	return OpStringC(m_gadget.GetPersistentData(UNI_L("deleted"))).HasContent();
}
