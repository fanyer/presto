/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DEVICE_API_DEVICE_API_MODULE_H
#define DEVICE_API_DEVICE_API_MODULE_H

#include "modules/hardcore/opera/module.h"

#ifdef DAPI_JIL_FILESYSTEM_SUPPORT
class JILFSMgr;
#endif // DAPI_JIL_FILESYSTEM_SUPPORT
#ifdef DAPI_JIL_NETWORKRESOURCES_SUPPORT
class JilNetworkResources;
#endif // DAPI_JIL_NETWORKRESOURCES_SUPPORT
#ifdef DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
class SystemResourceSetter;
#endif // DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
#ifdef DAPI_JIL_ADDRESSBOOK_SUPPORT
class JIL_AddressBookMappings;
#endif // DAPI_JIL_ADDRESSBOOK_SUPPORT

class OrientationManager;

class DeviceApiModule : public OperaModule
{
public:
	DeviceApiModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

#ifdef DAPI_JIL_FILESYSTEM_SUPPORT
	JILFSMgr *m_jil_fs_mgr;
#endif // DAPI_JIL_FILESYSTEM_SUPPORT
#ifdef DAPI_JIL_NETWORKRESOURCES_SUPPORT
	JilNetworkResources *m_jil_network_resources;
#endif // DAPI_JIL_NETWORKRESOURCES_SUPPORT
#ifdef DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
	SystemResourceSetter* m_system_resource_setter;
#endif // DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
#ifdef DAPI_ORIENTATION_MANAGER_SUPPORT
	OrientationManager* m_orientation_manager;
#endif // DAPI_ORIENTATION_MANAGER_SUPPORT
#ifdef DAPI_JIL_ADDRESSBOOK_SUPPORT
	/** See JIL_AddressBookMappings.
	 */
	OP_STATUS GetAddressBookMappings(JIL_AddressBookMappings*& out);
#endif // DAPI_JIL_ADDRESSBOOK_SUPPORT

#ifdef DAPI_VCARD_SUPPORT
	const class VCardGlobals* GetVCardGlobals(){ return m_v_card_globals; }
#endif // DAPI_VCARD_SUPPORT

private:
#ifdef DAPI_JIL_ADDRESSBOOK_SUPPORT
	JIL_AddressBookMappings* m_addressbook_mappings;
#endif // DAPI_JIL_ADDRESSBOOK_SUPPORT

#ifdef DAPI_VCARD_SUPPORT
	const class VCardGlobals* m_v_card_globals;
#endif // DAPI_VCARD_SUPPORT
};

#define g_device_api	(g_opera->device_api_module)

#ifdef DAPI_JIL_NETWORKRESOURCES_SUPPORT
# define g_DAPI_network_resources                 (g_opera->device_api_module.m_jil_network_resources)
#endif// DAPI_JIL_NETWORKRESOURCES_SUPPORT

#ifdef DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT
# define g_device_api_system_resource_setter      (g_opera->device_api_module.m_system_resource_setter)
#endif // DAPI_SYSTEM_RESOURCE_SETTER_SUPPORT

#ifdef DAPI_JIL_FILESYSTEM_SUPPORT
# define g_DAPI_jil_fs_mgr                        (g_opera->device_api_module.m_jil_fs_mgr)
#endif // DAPI_JIL_FILESYSTEM_SUPPORT

#ifdef DAPI_ORIENTATION_MANAGER_SUPPORT
# define g_DAPI_orientation_manager               (g_opera->device_api_module.m_orientation_manager)
#endif // DAPI_ORIENTATION_MANAGER_SUPPORT

#define DEVICE_API_MODULE_REQUIRED

#endif // DEVICE_API_DEVICE_API_MODULE_H
