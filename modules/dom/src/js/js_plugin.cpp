/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"


#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/js/js_plugin.h"
#include "modules/dom/src/js/mimetype.h"

#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domrestartobject.h"
#include "modules/util/str.h"
#include "modules/viewers/viewers.h"
#include "modules/viewers/plugins.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"

/* Empty function, defined in domobj.cpp. */
extern DOM_FunctionImpl DOM_dummyMethod;

#ifdef _PLUGIN_SUPPORT_

BOOL PluginsDisabled(DOM_EnvironmentImpl *environment)
{
	FramesDocument *frm_doc = environment->GetFramesDocument();

	if (frm_doc && frm_doc->GetWindow()->IsThumbnailWindow())
		return TRUE;

	return !g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled, DOM_EnvironmentImpl::GetHostName(frm_doc));
}

#ifdef NS4P_COMPONENT_PLUGINS
class DOM_PluginDetectionRestartObject
	: public PluginViewers::OpDetectionDoneListener, public DOM_RestartObject
{
public:
	static ES_GetState BlockUntilDetected(ES_Value *value, ES_Runtime* origining_runtime)
	{
		DOM_PluginDetectionRestartObject* restart_obj;
		GET_FAILED_IF_ERROR(DOMSetObjectRuntime(restart_obj = OP_NEW(DOM_PluginDetectionRestartObject, ()), static_cast<DOM_Runtime*>(origining_runtime)));
		GET_FAILED_IF_ERROR(restart_obj->KeepAlive());
		GET_FAILED_IF_ERROR(g_plugin_viewers->AddDetectionDoneListener(restart_obj));
		return restart_obj->BlockGet(value, origining_runtime);
	}
	virtual void DetectionDone()
	{
		OP_NEW_DBG("DOM_PluginDetectionRestartObject::DetectionDone()", "ns4p");
		OP_DBG(("Detection done signal received, resuming thread."));
		Resume();
	}
	~DOM_PluginDetectionRestartObject()
	{
		OP_NEW_DBG("~DOM_PluginDetectionRestartObject", "ns4p");
		OP_DBG(("DOM_PluginDetectionRestartObject destroyed, removing detection done listener."));
		/* Ignoring; no good way to return error from destructor. */
		OpStatus::Ignore(g_plugin_viewers->RemoveDetectionDoneListener(this));
	}
};
#endif // NS4P_COMPONENT_PLUGINS

#endif // _PLUGIN_SUPPORT_

/* static */ OP_STATUS
JS_PluginArray::Make(JS_PluginArray *&plugins, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	return DOMSetObjectRuntime(plugins = OP_NEW(JS_PluginArray, ()), runtime, runtime->GetPrototype(DOM_Runtime::PLUGINARRAY_PROTOTYPE), "PluginArray");
}

/* virtual */ ES_GetState
JS_PluginArray::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	OP_NEW_DBG("JS_PluginArray::GetName", "ns4p");
#ifdef _PLUGIN_SUPPORT_
	if (PluginsDisabled(GetEnvironment()))
		if (property_code == OP_ATOM_length)
		{
			DOMSetNumber(value, 0);
			return GET_SUCCESS;
		}
		else
			return GET_FAILED;

#ifdef NS4P_COMPONENT_PLUGINS
	if (g_plugin_viewers->IsDetecting())
	{
		OP_DBG(("Suspending thread that uses navigator.plugins because plugin detection is in progress, thread will be resumed when detection is done."));
		return DOM_PluginDetectionRestartObject::BlockUntilDetected(value, origining_runtime);
	}
#endif // NS4P_COMPONENT_PLUGINS

	if (property_code == OP_ATOM_length)
	{
		DOMSetNumber(value, g_plugin_viewers->GetPluginViewerCount(TRUE));
		return GET_SUCCESS;
	}

	for (unsigned i = 0; i < g_plugin_viewers->GetPluginViewerCount(); ++i)
		if (PluginViewer *plugin_viewer = g_plugin_viewers->GetPluginViewer(i))
		{
			if (!plugin_viewer->IsEnabled())
				continue;

			const uni_char *prod_name = plugin_viewer->GetProductName();
			if (prod_name)
				if (uni_str_eq(property_name, prod_name))
				{
					if (value)
					{
						JS_Plugin *plugin;
						GET_FAILED_IF_ERROR(JS_Plugin::Make(plugin, GetEnvironment(), plugin_viewer));
						DOMSetObject(value, plugin);
					}

					return GET_SUCCESS;
				}
		}

#else // _PLUGIN_SUPPORT_
	if (property_code == OP_ATOM_length)
	{
		DOMSetNumber(value, 0);
		return GET_SUCCESS;
	}
#endif // _PLUGIN_SUPPORT_

	return GET_FAILED;
}

#ifdef NS4P_COMPONENT_PLUGINS
/* virtual */ ES_GetState
JS_PluginArray::GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	return GetName(property_name, 0, value, origining_runtime);
}
#endif // NS4P_COMPONENT_PLUGINS

/* virtual */ ES_GetState
JS_PluginArray::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	OP_NEW_DBG("JS_PluginArray::GetIndex", "ns4p");
#ifdef _PLUGIN_SUPPORT_
#ifdef NS4P_COMPONENT_PLUGINS
	if (g_plugin_viewers->IsDetecting())
	{
		OP_DBG(("Suspending thread that uses navigator.plugins[i] because plugin detection is in progress, thread will be resumed when detection is done."));
		return DOM_PluginDetectionRestartObject::BlockUntilDetected(value, origining_runtime);
	}
#endif // NS4P_COMPONENT_PLUGINS

	int enabled_count = 0;
	for (UINT i = 0; i < g_plugin_viewers->GetPluginViewerCount(); i++)
	{
		PluginViewer *plugin_viewer = g_plugin_viewers->GetPluginViewer(i);
		// Count only enabled plugins to find plugin matching requested index
		if (plugin_viewer && plugin_viewer->IsEnabled() && property_index == enabled_count++)
		{
			if (value)
			{
				JS_Plugin *plugin;
				GET_FAILED_IF_ERROR(JS_Plugin::Make(plugin, GetEnvironment(), plugin_viewer));
				DOMSetObject(value, plugin);
			}

			return GET_SUCCESS;
		}
	}
#endif // _PLUGIN_SUPPORT_

	return GET_FAILED;
}

#ifdef NS4P_COMPONENT_PLUGINS
/* virtual */
ES_GetState JS_PluginArray::GetIndexRestart(int property_index, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object)
{
	return GetIndex(property_index, value, origining_runtime);
}
#endif // NS4P_COMPONENT_PLUGINS

/* virtual */ ES_GetState
JS_PluginArray::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
#ifdef _PLUGIN_SUPPORT_
	count = g_plugin_viewers->GetPluginViewerCount(TRUE);
#else // _PLUGIN_SUPPORT_
	count = 0;
#endif // _PLUGIN_SUPPORT_
	return GET_SUCCESS;
}

/* static */ int
JS_PluginArray::refresh(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_PLUGINARRAY);

#ifdef _PLUGIN_SUPPORT_
	if (PluginsDisabled(origining_runtime->GetEnvironment()))
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(g_plugin_viewers->RefreshPluginViewers(FALSE));
	if (argc > 0 && argv[0].value.boolean)
		if (FramesDocument* frames_doc = origining_runtime->GetFramesDocument())
			frames_doc->GetDocManager()->Reload(NotEnteredByUser);
#endif // _PLUGIN_SUPPORT_

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(JS_PluginArray)
	DOM_FUNCTIONS_FUNCTION(JS_PluginArray, JS_PluginArray::refresh, "-refresh", "b-")
DOM_FUNCTIONS_END(JS_PluginArray)

#ifdef _PLUGIN_SUPPORT_

JS_Plugin::JS_Plugin()
	: description(NULL),
	  filename(NULL),
	  name(NULL)
{
}

/* static */ OP_STATUS
JS_Plugin::Make(JS_Plugin *&plugin, DOM_EnvironmentImpl *environment, PluginViewer *plugin_viewer)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(plugin = OP_NEW(JS_Plugin, ()), runtime, runtime->GetObjectPrototype(), "Plugin"));

	const uni_char *description = plugin_viewer->GetDescription();
	const uni_char *filename = plugin_viewer->GetPath();
	const uni_char *name = plugin_viewer->GetProductName();

	// We don't want scripts to have access to the path as that gives
	// the script information about the local computer's directory
	// structure and might contain user names and other sensitive information.
	// Strip everything before the last path seperator.
	if (filename)
	{
		const uni_char* last_sep = uni_strrchr(filename, PATHSEPCHAR);
		if (last_sep)
			filename = last_sep + 1;
	}


	if (!(plugin->description = UniSetNewStr(description ? description : UNI_L(""))) ||
	    !(plugin->filename = UniSetNewStr(filename ? filename : UNI_L(""))) ||
	    !(plugin->name = UniSetNewStr(name ? name : UNI_L(""))))
		return OpStatus::ERR_NO_MEMORY;
	else
		return OpStatus::OK;
}

JS_Plugin::~JS_Plugin()
{
	OP_DELETEA(description);
	OP_DELETEA(filename);
	OP_DELETEA(name);
}

/* virtual */ ES_GetState
JS_Plugin::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (PluginsDisabled(GetEnvironment()))
		return GET_FAILED;

	switch (property_name)
	{
	case OP_ATOM_description:
		DOMSetString(value, description);
		return GET_SUCCESS;

	case OP_ATOM_filename:
		DOMSetString(value, filename);
		return GET_SUCCESS;

	case OP_ATOM_name:
		DOMSetString(value, name);
		return GET_SUCCESS;

	case OP_ATOM_length:
		if (value)
		{
			int count = 0;

			ChainedHashIterator* viewer_iter;
			GET_FAILED_IF_ERROR(g_viewers->CreateIterator(viewer_iter));
			while (Viewer *viewer = g_viewers->GetNextViewer(viewer_iter))
			{
				if (viewer->FindPluginViewerByName(name, TRUE))
					++count;
			}
			OP_DELETE(viewer_iter);

			DOMSetNumber(value, count);
		}
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_GetState
JS_Plugin::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState result = DOM_Object::GetName(property_name, property_code, value, origining_runtime);
	if (result != GET_FAILED)
		return result;

	Viewer *viewer = NULL;

	viewer = g_viewers->FindViewerByMimeType(property_name);
	if (viewer && !viewer->FindPluginViewerByName(name, TRUE))
		viewer = NULL;

	if (viewer)
	{
		if (value)
		{
			JS_MimeType *mimetype;
			GET_FAILED_IF_ERROR(JS_MimeType::Make(mimetype, GetEnvironment(), viewer));
			DOMSetObject(value, mimetype);
		}

		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_GetState
JS_Plugin::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	if (PluginsDisabled(GetEnvironment()))
		return GET_FAILED;

	ChainedHashIterator *viewer_iter;
	GET_FAILED_IF_ERROR(g_viewers->CreateIterator(viewer_iter));
	while (Viewer *viewer = g_viewers->GetNextViewer(viewer_iter))
		if (viewer->FindPluginViewerByName(name, TRUE))
			if (property_index > 0)
				--property_index;
			else
			{
				if (value)
				{
					JS_MimeType *mimetype;
					OP_STATUS rc = JS_MimeType::Make(mimetype, GetEnvironment(), viewer);
					if (OpStatus::IsError(rc))
					{
						OP_DELETE(viewer_iter);
						return rc == OpStatus::ERR_NO_MEMORY ? GET_NO_MEMORY : GET_FAILED;
					}
					DOMSetObject(value, mimetype);
				}

				OP_DELETE(viewer_iter);
				return GET_SUCCESS;
			}

	OP_DELETE(viewer_iter);

	return GET_FAILED;
}

/* virtual */ ES_PutState
JS_Plugin::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (GetName(property_name, NULL, origining_runtime) == GET_SUCCESS)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

#endif // _PLUGIN_SUPPORT_
