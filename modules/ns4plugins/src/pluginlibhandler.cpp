/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/pluginlibhandler.h"
#include "modules/ns4plugins/src/pluginlib.h"
#include "modules/viewers/plugins.h"
#include "modules/ns4plugins/src/pluginhandler.h"

#ifdef  STATIC_NS4PLUGINS
#include "modules/pi/OpSystemInfo.h"
#include "modules/ns4plugins/src/staticplugin.h"
#endif

/** PluginLib::PluginLibHandler()
	The basic constructor of the plugin library handler
*/
PluginLibHandler::PluginLibHandler()
{
#ifdef NS4P_NDATA_REALPLAYER_WORKAROUND
	op_memset(ndata_dummy, 0, ARRAY_SIZE(ndata_dummy));
#endif // NS4P_NDATA_REALPLAYER_WORKAROUND
#ifdef NS4P_UNLOAD_PLUGIN_TIMER
	m_unload_timer = NULL;
#endif // NS4P_UNLOAD_PLUGIN_TIMER
}

/** PluginLibHandler::~PluginLibHandler()
	The basic destructor of the Plugin library handler
*/
PluginLibHandler::~PluginLibHandler()
{
    PluginLib* p = (PluginLib *)pluginlib_list.First();
    PluginLib* ps;
    while (p)
    {
    	ps = p->Suc();
    	p->Out();
      	OP_DELETE(p);
      	p = ps;
    }
#ifdef NS4P_UNLOAD_PLUGIN_TIMER
	OP_DELETE(m_unload_timer);
	m_unload_timer = NULL;
#endif // NS4P_UNLOAD_PLUGIN_TIMER
}

/** PluginLibHandler::FindPluginLib(const char *lib)
	Searches the plugin library list and finds an instance of the mentioned
	plugin library, then returns a pointer to it.
*/
PluginLib *PluginLibHandler::FindPluginLib(const uni_char *lib)
{
	// Cycle through the linked list of Plugin libraries
	PluginLib *p = (PluginLib*)pluginlib_list.First();

#ifdef NS4P_SUPPORT_PROXY_PLUGIN
	// There is only one plugin and that is the proxy - just return it
	return p;
#else
	while (p)
	{
    	// Use the library's own function to check the identity of the library
    	// against the requested library. Return a pointer to the library if
    	// this is the one.
		if (p->isThisLibrary(lib))
		{
			return p;
		}

		// Advance to the next library in the list
		p = p->Suc();
	}

	// Return a null pointer since the library doesn't exist in the list.
	return NULL;
#endif // NS4P_SUPPORT_PROXY_PLUGIN
}

OP_STATUS PluginLibHandler::CreateLib(OpDLL *dll, const uni_char *lib_name, BOOL owns_dll/*=TRUE*/)
{
	OpAutoPtr<PluginLib> p(OP_NEW(PluginLib, ()));
	RETURN_OOM_IF_NULL(p.get());
	RETURN_IF_ERROR(p->InitLibFromDll(dll, lib_name, owns_dll));
	if (!p->ok())
		// If the library failed to load, the OpAutoPtr deletes the structure.
		return OpStatus::ERR;

	// If the plugin loads ok, then increment the library usage counter
	p->Inc();

	// Add the library to the loaded library list
	p->Into(&pluginlib_list);

	p.release();
	return OpStatus::OK;
}

OP_STATUS PluginLibHandler::CreateLib(const uni_char *lib, OpComponentType component_type, NPPluginFuncs** plug_funcs)
{
	// Attempt to locate the plugin library mentioned in the Plugin Library Handler list
	PluginLib *p = FindPluginLib(lib);
	if (p)
	{
		// If the library exits, increment the library usage counter in order to make
		// sure the library doesn't get deleted until no one else is using it.
		p->Inc();

		// Return a pointer to the plugin library.
		*plug_funcs = p->PluginFuncs();
		return OpStatus::OK;
	}
#if defined(NS4P_COMPONENT_PLUGINS) || defined(DYNAMIC_NS4PLUGINS) || (defined(STATIC_NS4PLUGINS) && defined(OPSYSTEMINFO_STATIC_PLUGIN) && !defined(NS4P_SUPPORT_PROXY_PLUGIN))
	else
	{
		// Create a new Plugin Library instance since it's not in the currently loaded list
		OpAutoPtr<PluginLib> p(OP_NEW(PluginLib, ()));
		RETURN_OOM_IF_NULL(p.get());
#if defined(NS4P_COMPONENT_PLUGINS)
		RETURN_IF_ERROR(p->LoadLib(lib, component_type));
#elif defined(DYNAMIC_NS4PLUGINS)
		RETURN_IF_ERROR(p->LoadLib(lib));
#else
		struct StaticPlugin plugin_spec;
		RETURN_IF_ERROR(plugin_spec.plugin_name.Set(lib));
		RETURN_IF_ERROR(g_op_system_info->GetStaticPlugin(plugin_spec));
		RETURN_IF_ERROR(p->LoadStaticLib(lib, plugin_spec.init_function, plugin_spec.shutdown_function));
#endif // NS4P_COMPONENT_PLUGINS || DYNAMIC_NS4PLUGINS

		if (p->ok())
		{
			// If the plugin loads ok, then increment the library usage counter
			p->Inc();

			// Add the library to the loaded library list
			p->Into(&pluginlib_list);

			// Return a pointer to the library asked for.
			*plug_funcs = p->PluginFuncs();

			p.release();
			return OpStatus::OK;
		}
		// If the library failed to load ok, the OpAutoPtr deletes the structure.
	}
#endif // NS4P_COMPONENT_PLUGINS || DYNAMIC_NS4PLUGINS || (STATIC_NS4PLUGINS && OPSYSTEMINFO_STATIC_PLUGIN && !NS4P_SUPPORT_PROXY_PLUGIN)

	// No library could be loaded, return error.
	return OpStatus::ERR;
}

#ifdef  STATIC_NS4PLUGINS
OP_STATUS PluginLibHandler::AddStaticLib(const uni_char *lib, NP_InitializeP init_function, NP_ShutdownP shutdown_function)
{
	OpAutoPtr<PluginLib> p(OP_NEW(PluginLib, ()));
	RETURN_OOM_IF_NULL(p.get());
	RETURN_IF_ERROR(p->LoadStaticLib(lib, init_function, shutdown_function));
	if (!p->ok())
		// If the library failed to load, the OpAutoPtr deletes the structure.
		return OpStatus::ERR;	// No library could be loaded, return error.

	// If the plugin loads ok, then increment the library usage counter
	p->Inc();

	// Add the library to the loaded library list
	p->Into(&pluginlib_list);
	p.release();
	return OpStatus::OK;
}
#endif //STATIC_NS4PLUGINS

void PluginLibHandler::DeleteLib(const uni_char *lib)
{
	PluginLib *p = FindPluginLib(lib);
	if (p)
	{
		p->Dec();
#ifdef NS4P_UNLOAD_PLUGIN_TIMER
		if (!p->Value())
		{
			if (!m_unload_timer)
				m_unload_timer = OP_NEW(OpTimer, ());
			if (m_unload_timer)
			{
				m_unload_timer->SetTimerListener(this);
				m_unload_timer->Start(NS4P_DEFAULT_TIMER * 1000); // ms
			}
		}
#endif // NS4P_UNLOAD_PLUGIN_TIMER
	}
}

#ifdef NS4P_UNLOAD_PLUGIN_TIMER
void PluginLibHandler::OnTimeOut(OpTimer* timer)
{
	if ( timer == m_unload_timer )
	{
		PluginLib* pl = (PluginLib *)pluginlib_list.First();
		PluginLib* pls;
		while (pl)
		{
			if (!pl->Value())
			{
				pls = pl->Suc();
    			pl->Out();
      			OP_DELETE(pl);
      			pl = pls;
			}
			else
				pl = pl->Suc();
		}
	}
}
#endif // NS4P_UNLOAD_PLUGIN_TIMER

void PluginLibHandler::DestroyLib(const uni_char *lib, bool fatal_error, const OpMessageAddress& address)
{
	if (fatal_error)
		g_pluginhandler->OnLibraryError(lib, address);

	if (PluginLib *p = FindPluginLib(lib))
	{
		p->Out();
		OP_DELETE(p);
	}
}

NPSavedData *PluginLibHandler::GetSavedDataPointer(const uni_char *lib)
{
	PluginLib *p = FindPluginLib(lib);
	if (p)
		return p->SavedData();
	else
		return NULL;
}

#ifdef NS4P_SUPPORT_PROXY_PLUGIN
# define RETURN_OR_CONTINUE return OpStatus::ERR
#else
# define RETURN_OR_CONTINUE continue
#endif

OP_STATUS PluginLibHandler::ClearAllSiteData()
{
	OP_STATUS status = OpStatus::OK;

	// Traverse and execute ClearSiteData on all plugins.
	PluginViewers *plugin_viewers = g_plugin_viewers;

#ifdef NS4P_SUPPORT_PROXY_PLUGIN
	const UINT i = 0;
#else
	UINT num_plugins = plugin_viewers->GetPluginViewerCount();
	for (UINT i = 0; i < num_plugins; ++i)
#endif
	{
		PluginViewer *cand = plugin_viewers->GetPluginViewer(i);
		if (!cand || !cand->IsEnabled())
			RETURN_OR_CONTINUE;

		const uni_char *path = cand->GetPath();
		if (!path)
			RETURN_OR_CONTINUE;

		const uni_char *name = cand->GetProductName();
		// Only clear site data for these whitelisted plugins.
		if (uni_strnicmp(name, UNI_L("shockwave flash"), 15) != 0
			&& uni_strnicmp(name, UNI_L("silverlight"), 11) != 0
			&& uni_strnicmp(name, UNI_L("opera test plugin"), 17) != 0)
			RETURN_OR_CONTINUE;

		PluginLib *lib = FindPluginLib(path);
#ifdef NS4P_COMPONENT_PLUGINS
		bool unload = false;
#else // !NS4P_COMPONENT_PLUGINS
		OpDLL *dll = NULL;
#endif // !NS4P_COMPONENT_PLUGINS
		if (!lib)  // If plugin is not loaded
		{
#ifdef NS4P_COMPONENT_PLUGINS
			NPPluginFuncs* plug_funcs = 0;
			if (OpStatus::IsError(CreateLib(path, COMPONENT_PLUGIN, &plug_funcs)))
				continue;
			lib = static_cast<PluginLib *>(pluginlib_list.Last());
			unload = true;
#else // !NS4P_COMPONENT_PLUGINS
			if (OpStatus::IsError(OpDLL::Create(&dll))
				|| OpStatus::IsError(dll->Load(path))
				|| OpStatus::IsError(CreateLib(dll, path, FALSE)))
			{
				OP_DELETE(dll);
				RETURN_OR_CONTINUE;
			}
			lib = static_cast<PluginLib *>(pluginlib_list.Last());
#endif  // !NS4P_COMPONENT_PLUGINS

			lib->Init();
			if (!lib->Initialized())
			{
				lib->Out();
				OP_DELETE(lib);
#ifdef NS4P_COMPONENT_PLUGINS
				continue;
#else // !NS4P_COMPONENT_PLUGINS
				OP_DELETE(dll);
				RETURN_OR_CONTINUE;
#endif // !NS4P_COMPONENT_PLUGINS
			}
		}

		/**
		 * NOTE: Some plugins MAY return NPERR_GENERIC_ERROR when it has no
		 *       data to delete. One such plugin is Adobe Flash 10.3
		 *       (surprise!). The error can be safely ignored, of course.
		 */
		NPError ret = lib->ClearSiteData(NULL, NP_CLEAR_ALL, NP_CLEAR_MAX_AGE_ALL);
		if (ret != NPERR_NO_ERROR)
		{
			if (ret != NPERR_GENERIC_ERROR)
			{
				if (ret == NPERR_TIME_RANGE_NOT_SUPPORTED)
					OP_ASSERT(!"PluginLibHandler::ClearAllSiteData: This code assumes NPERR_TIME_RANGE_NOT_SUPPORTED is never returned for max_age == (UINT64)(-1)");
				else if (ret == NPERR_MALFORMED_SITE)
					OP_ASSERT(!"PluginLibHandler::ClearAllSiteData: Plugin returned NPERR_MALFORMED_SITE, which shouldn't happen if Opera used the strings returned from NPP_GetSitesWithData");
				else
					/**
					 * In the case that a plugin returns something not defined in
					 * the specification, we would want to know. Causes for this to
					 * happen include faulty plugin and an update to the
					 * specification. In either case, this should be dealt with.
					 */
					OP_ASSERT(!"PluginLibHandler::ClearAllSiteData: Error code is not defined in specification, https://wiki.mozilla.org/NPAPI: ClearSiteData, or it has been updated and we haven't");
			}
			status = OpStatus::ERR;
		}

#ifdef NS4P_COMPONENT_PLUGINS
		if (unload)  // If plugin should be unloaded
#else
		if (dll)  // If plugin was loaded inside this function
#endif
		{
			lib->Out();
			OP_DELETE(lib);
#ifndef NS4P_COMPONENT_PLUGINS
			OP_DELETE(dll);
#endif
		}
	}

	return status;
}

#undef RETURN_OR_CONTINUE

#endif  // _PLUGIN_SUPPORT_
