/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/device_api/device_api_module.h"

#ifdef DAPI_JIL_FILESYSTEM_SUPPORT
# include "modules/device_api/jil/JILFSMgr.h"
#endif // DAPI_JIL_FILESYSTEM_SUPPORT
#ifdef DAPI_JIL_NETWORKRESOURCES_SUPPORT
# include "modules/device_api/jil/JILNetworkResources.h"
#endif // DAPI_JIL_NETWORKRESOURCES_SUPPORT
#ifdef DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
# include "modules/device_api/SystemResourceSetter.h"
#endif // DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
#ifdef DAPI_JIL_ADDRESSBOOK_SUPPORT
# include "modules/device_api/jil/JILAddressBook.h"
#endif // DAPI_JIL_ADDRESSBOOK_SUPPORT
#include "modules/device_api/OrientationManager.h"


#ifdef DAPI_VCARD_SUPPORT
# include "modules/device_api/src/VCardGlobals.h"
#endif // DAPI_VCARD_SUPPORT

DeviceApiModule::DeviceApiModule()
	: OperaModule()	// Just to use up the colon and allow for different combinations of the initializers below
#ifdef DAPI_JIL_FILESYSTEM_SUPPORT
	, m_jil_fs_mgr(NULL)
#endif // DAPI_JIL_FILESYSTEM_SUPPORT
#ifdef DAPI_JIL_NETWORKRESOURCES_SUPPORT
	, m_jil_network_resources(NULL)
#endif // DAPI_JIL_NETWORKRESOURCES_SUPPORT
#ifdef DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
	, m_system_resource_setter(NULL)
#endif // DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
#ifdef DAPI_ORIENTATION_MANAGER_SUPPORT
	, m_orientation_manager(NULL)
#endif // DAPI_ORIENTATION_MANAGER_SUPPORT
#ifdef DAPI_JIL_ADDRESSBOOK_SUPPORT
	, m_addressbook_mappings(NULL)
#endif // DAPI_JIL_ADDRESSBOOK_SUPPORT
#ifdef DAPI_VCARD_SUPPORT
	, m_v_card_globals(NULL)
#endif // DAPI_VCARD_SUPPORT
{
}

void
DeviceApiModule::InitL(const OperaInitInfo& info)
{
#ifdef DAPI_JIL_FILESYSTEM_SUPPORT
	m_jil_fs_mgr = OP_NEW_L(JILFSMgr, ());
	LEAVE_IF_ERROR(m_jil_fs_mgr->Construct());
#endif // DAPI_JIL_FILESYSTEM_SUPPORT

#ifdef DAPI_JIL_NETWORKRESOURCES_SUPPORT
	m_jil_network_resources = OP_NEW_L(JilNetworkResources, ());
	LEAVE_IF_ERROR(m_jil_network_resources->Construct());
#endif // DAPI_JIL_NETWORKRESOURCES_SUPPORT

#ifdef DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
	m_system_resource_setter = OP_NEW_L(SystemResourceSetter, ());
#endif // DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT

#ifdef DAPI_ORIENTATION_MANAGER_SUPPORT
	LEAVE_IF_ERROR(OrientationManager::Make(m_orientation_manager));
#endif // DAPI_ORIENTATION_MANAGER_SUPPORT
#ifdef DAPI_VCARD_SUPPORT
	 m_v_card_globals = VCardGlobals::MakeL();
#endif // DAPI_VCARD_SUPPORT
}

void
DeviceApiModule::Destroy()
{
#ifdef DAPI_JIL_ADDRESSBOOK_SUPPORT
	OP_DELETE(m_addressbook_mappings);
#endif // DAPI_JIL_ADDRESSBOOK_SUPPORT

#ifdef DAPI_VCARD_SUPPORT
	OP_DELETE(m_v_card_globals);
	m_v_card_globals = NULL;
#endif // DAPI_VCARD_SUPPORT

#ifdef DAPI_ORIENTATION_MANAGER_SUPPORT
	OP_DELETE(m_orientation_manager);
	m_orientation_manager = NULL;
#endif // DAPI_ORIENTATION_MANAGER_SUPPORT

#ifdef DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
	OP_DELETE(m_system_resource_setter);
	m_system_resource_setter = NULL;
#endif // DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT

#ifdef DAPI_JIL_NETWORKRESOURCES_SUPPORT
	OP_DELETE(m_jil_network_resources);
#endif // DAPI_JIL_NETWORKRESOURCES_SUPPORT

#ifdef DAPI_JIL_FILESYSTEM_SUPPORT
	OP_DELETE(m_jil_fs_mgr);
#endif // DAPI_JIL_NETORKRESOURCES_SUPPORT
}


#ifdef DAPI_JIL_ADDRESSBOOK_SUPPORT
OP_STATUS
DeviceApiModule::GetAddressBookMappings(JIL_AddressBookMappings*& out)
{
	if (!m_addressbook_mappings)
		RETURN_IF_ERROR(JIL_AddressBookMappings::Make(m_addressbook_mappings, g_op_address_book));
	out = m_addressbook_mappings;
	return m_addressbook_mappings ? OpStatus::OK : OpStatus::ERR;
}
#endif // DAPI_JIL_ADDRESSBOOK_SUPPORT
