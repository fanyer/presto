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

#include "modules/dom/src/js/mimetype.h"
#include "modules/dom/src/js/js_plugin.h"

#include "modules/util/str.h"
#include "modules/viewers/viewers.h"

// Defined in js_plugins.cpp.
extern BOOL PluginsDisabled(DOM_EnvironmentImpl *environment);

/* static */ OP_STATUS
JS_MimeTypes::Make(JS_MimeTypes *&mimeTypes, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	return DOMSetObjectRuntime(mimeTypes = OP_NEW(JS_MimeTypes, ()), runtime, runtime->GetObjectPrototype(), "MimeTypes");
}

/* virtual */ ES_GetState
JS_MimeTypes::GetName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (uni_str_eq(property_name, "length"))
	{
		DOMSetNumber(value, g_viewers->GetViewerCount());
		return GET_SUCCESS;
	}

	Viewer* viewer = g_viewers->FindViewerByMimeType(property_name);
	if (viewer)
	{
		if (viewer->GetAction() == VIEWER_PLUGIN)
#ifdef _PLUGIN_SUPPORT_
			if (PluginsDisabled(GetEnvironment()))
				return GET_FAILED;
#else // _PLUGIN_SUPPORT_
			return GET_FAILED;
#endif // _PLUGIN_SUPPORT_

		if (value)
		{
			JS_MimeType *mimetype;
			GET_FAILED_IF_ERROR(JS_MimeType::Make(mimetype, GetEnvironment(), viewer));
			DOMSetObject(value, mimetype);
		}

		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_GetState
JS_MimeTypes::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	Viewer* viewer = NULL;
	ChainedHashIterator* viewer_iter;
	GET_FAILED_IF_ERROR(g_viewers->CreateIterator(viewer_iter));
	for (int index = 0; index <= property_index; ++index)
		viewer = g_viewers->GetNextViewer(viewer_iter);
	OP_DELETE(viewer_iter);

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
JS_MimeTypes::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = g_viewers->GetViewerCount();
	return GET_SUCCESS;
}

JS_MimeType::JS_MimeType()
	: description(NULL),
	  suffixes(NULL),
	  type(NULL)
{
}

/* static */ OP_STATUS
JS_MimeType::Make(JS_MimeType *&mimetype, DOM_EnvironmentImpl *environment, Viewer *viewer)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	RETURN_IF_ERROR(DOMSetObjectRuntime(mimetype = OP_NEW(JS_MimeType, ()), runtime, runtime->GetObjectPrototype(), "MimeType"));

	const uni_char *description = NULL;
#ifdef _PLUGIN_SUPPORT_
	PluginViewer* def_plugin = viewer->GetDefaultPluginViewer();
	OpString description_string;
	if (def_plugin)
	{
		RETURN_IF_ERROR(def_plugin->GetTypeDescription(viewer->GetContentTypeString(), description_string));
		description = description_string.CStr();
	}
	else
#endif // _PLUGIN_SUPPORT_
		description = viewer->GetDescription();

	const uni_char *suffixes = viewer->GetExtensions();
	const uni_char *type = viewer->GetContentTypeString();

	if (!(mimetype->description = UniSetNewStr(description ? description : UNI_L(""))) ||
	    !(mimetype->suffixes = UniSetNewStr(suffixes ? suffixes : UNI_L(""))) ||
	    !(mimetype->type = UniSetNewStr(type ? type : UNI_L(""))))
		return OpStatus::ERR_NO_MEMORY;
	else
		return OpStatus::OK;
}

/* virtual */
JS_MimeType::~JS_MimeType()
{
	OP_DELETEA(description);
	OP_DELETEA(suffixes);
	OP_DELETEA(type);
}

/* virtual */ ES_GetState
JS_MimeType::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_description:
		DOMSetString(value, description);
		return GET_SUCCESS;

	case OP_ATOM_suffixes:
		DOMSetString(value, suffixes);
		return GET_SUCCESS;

	case OP_ATOM_type:
		DOMSetString(value, type);
		return GET_SUCCESS;

	case OP_ATOM_enabledPlugin:
		if (value)
		{
			DOMSetNull(value);

#ifdef _PLUGIN_SUPPORT_
			Viewer* viewer = g_viewers->FindViewerByMimeType(type);
			if (viewer)
			{
				PluginViewer* plugin_viewer = viewer->GetDefaultPluginViewer();
				if (plugin_viewer)
				{
					JS_Plugin *plugin;
					GET_FAILED_IF_ERROR(JS_Plugin::Make(plugin, GetEnvironment(), plugin_viewer));
					DOMSetObject(value, plugin);
				}
			}
#endif // _PLUGIN_SUPPORT_
		}
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
JS_MimeType::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (GetName(property_name, NULL, origining_runtime) == GET_SUCCESS)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

