/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/ns4plugins_module.h"
#include "modules/ns4plugins/opns4pluginhandler.h"
#include "modules/ns4plugins/src/pluginscript.h"
#include "modules/ns4plugins/src/pluginmemoryhandler.h"
#include "modules/ns4plugins/src/pluginhandler.h"

#ifdef NS4P_STREAM_FILE_WITH_EXTENSION_NAME
#include "modules/util/opfile/opfile.h"
#endif // NS4P_STREAM_FILE_WITH_EXTENSION_NAME

#ifndef HAS_COMPLEX_GLOBALS
#include "modules/ns4plugins/src/plug-inc/npfunctions.h"
#endif

Ns4pluginsModule::Ns4pluginsModule() :
	plugin_handler(NULL),
	plugin_memory_handler(NULL),
	plugin_script_data(NULL)
	,plugin_in_synchronous_loop(0)
	,plugin_internal_object_class(NULL)
#ifdef NS4P_TRY_CATCH_PLUGIN_CALL
	,is_executing_plugin_code(FALSE)
#endif // NS4P_TRY_CATCH_PLUGIN_CALL
	,mouse_is_over_plugin(FALSE)
	,plugin_that_should_receive_key_events(NULL)
#ifndef HAS_COMPLEX_GLOBALS
	,m_operafuncs(NULL)
#endif
{
}

void
Ns4pluginsModule::InitL(const OperaInitInfo& info)
{

#ifndef HAS_COMPLEX_GLOBALS
	m_operafuncs = OP_NEW_L(NPNetscapeFuncs, ());
	extern void init_operafuncs();
	init_operafuncs();
#endif // !HAS_COMPLEX_GLOBALS

	LEAVE_IF_FATAL(OpNS4PluginHandler::Init());
	plugin_script_data = OP_NEW_L(PluginScriptData, ());
	plugin_internal_object_class = OP_NEW_L(NPClass, ());
	plugin_internal_object_class->structVersion = 0;
    plugin_internal_object_class->allocate = NULL;
    plugin_internal_object_class->deallocate = NULL;
    plugin_internal_object_class->invalidate = NULL;
    plugin_internal_object_class->hasMethod = NULL;
    plugin_internal_object_class->invoke = NULL;
    plugin_internal_object_class->invokeDefault = NULL;
    plugin_internal_object_class->hasProperty = NULL;
    plugin_internal_object_class->getProperty = NULL;
    plugin_internal_object_class->setProperty = NULL;
    plugin_internal_object_class->removeProperty = NULL;
	plugin_internal_object_class->enumerate = NULL;
	plugin_internal_object_class->construct = NULL;

#ifdef NS4P_STREAM_FILE_WITH_EXTENSION_NAME
	ClearTempStreamFiles();
#endif //#ifdef NS4P_STREAM_FILE_WITH_EXTENSION_NAME

}

void
Ns4pluginsModule::Destroy()
{
	OpNS4PluginHandler::Destroy();
	OP_DELETE(plugin_script_data);
	plugin_script_data = NULL;
	OP_DELETE(plugin_internal_object_class);
	plugin_internal_object_class = NULL;
#ifndef HAS_COMPLEX_GLOBALS
	OP_DELETE(m_operafuncs);
#endif
	PluginMemoryHandler::Destroy();
}

#ifdef NS4P_STREAM_FILE_WITH_EXTENSION_NAME
void Ns4pluginsModule::ClearTempStreamFiles()
{
	OpString fileName;
	if (OpStatus::IsSuccess(g_folder_manager->GetFolderPath(OPFILE_CACHE_FOLDER, fileName)) &&
		OpStatus::IsSuccess(fileName.Append(UNI_L("plugin"))))
	{
		OpFile file;
		file.Construct(fileName);
		file.Delete();
	}
}
#endif // NS4P_STREAM_FILE_WITH_EXTENSION_NAME

BOOL Ns4pluginsModule::FreeCachedData(BOOL toplevel_context)
{
	if (toplevel_context)
	{
		if (plugin_handler)
			plugin_handler->DestroyAllLoadingStreams();

		return TRUE;
	}
	else
		return FALSE;
}

#endif // _PLUGIN_SUPPORT_
