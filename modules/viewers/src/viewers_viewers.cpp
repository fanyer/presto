/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/pi/OpSystemInfo.h"
# include "modules/prefs/prefsmanager/collections/pc_app.h"
# include "modules/prefs/prefsmanager/collections/pc_doc.h"
# include "modules/prefs/prefsmanager/collections/pc_network.h"
# include "modules/prefs/prefsmanager/collections/pc_display.h"
# include "modules/prefs/prefsmanager/prefsmanager.h"
# include "modules/prefsfile/prefssection.h"
# include "modules/prefsfile/prefsentry.h"
# include "modules/url/url2.h"
# include "modules/util/gen_str.h"
# include "modules/util/hash.h"

# include "modules/viewers/viewers.h"

#ifdef DYNAMIC_VIEWERS
#include "modules/util/opstrlst.h"
#endif // DYNAMIC_VIEWERS

//#define VIEWER_VERSION 0   // 3 token format
//#define VIEWER_VERSION 1	 // 5 token format
//#define VIEWER_VERSION 2	 // 7 token format, added flags and dst folder
#define VIEWER_VERSION 3	 // Moved from main prefs file

#include "modules/viewers/src/generated_viewers_data.inc"

Viewer::Viewer(void)
	: m_owner(0)
	, m_action(VIEWER_ASK_USER)
	, m_content_type(FROM_RANGED_ENUM(URLContentType, URL_UNKNOWN_CONTENT))
	, m_container(VIEWER_NULL)
#ifdef WEB_HANDLERS_SUPPORT
	, m_web_handler_allowed(TRUE)
#endif // WEB_HANDLERS_SUPPORT
	, m_allow_any_extension(FALSE)
{
}

Viewer::~Viewer()
{
#ifdef _PLUGIN_SUPPORT_
	m_plugins.Clear();
#endif // _PLUGIN_SUPPORT_
}

OP_STATUS Viewer::Empty(void)
{
	if ( m_owner != 0 )
		return OpStatus::ERR_OUT_OF_RANGE;

	m_action = VIEWER_ASK_USER;
	m_flags.Reset();
	m_content_type = FROM_RANGED_ENUM(URLContentType, URL_UNKNOWN_CONTENT);

	m_content_type_string.Empty();
	m_content_type_string8.Empty();
	m_extensions_string.Empty();
	m_extension_list.Clear();
	m_application_to_open_with.Empty();
	m_save_to_folder.Empty();

#ifdef _PLUGIN_SUPPORT_
	m_plugins.Clear();
	m_preferred_plugin_path.Empty();
#endif

	return OpStatus::OK;
}

OP_STATUS Viewer::Construct(ViewAction action, URLContentType content_type, const OpStringC& content_type_string,
                            const OpStringC& extensions_string, ViewActionFlag::ViewActionFlagType flags,
                            const OpStringC& application_to_open_with, const OpStringC& save_to_folder
#ifdef WEB_HANDLERS_SUPPORT
						  , const OpStringC& web_handler_to_open_with
#endif // WEB_HANDLERS_SUPPORT
#ifdef _PLUGIN_SUPPORT_
                          , const OpStringC& preferred_plugin_path
#endif // _PLUGIN_SUPPORT_
                          ,const OpStringC& description
	                    )
{
	OP_STATUS ret;
	m_action = action;
	m_content_type = FROM_RANGED_ENUM(URLContentType, content_type);
	m_flags.SetFlags(flags);
	if ((ret=m_content_type_string.Set(content_type_string))!=OpStatus::OK ||
	    (ret=m_content_type_string8.Set(content_type_string.CStr()))!=OpStatus::OK ||
	    (ret=SetExtensions(extensions_string))!=OpStatus::OK ||
	    (ret=m_application_to_open_with.Set(application_to_open_with))!=OpStatus::OK
#ifdef WEB_HANDLERS_SUPPORT
		|| (ret=m_web_handler_to_open_with.Set(web_handler_to_open_with))!=OpStatus::OK
#endif // WEB_HANDLERS_SUPPORT
	    || (ret=m_save_to_folder.Set(save_to_folder))!=OpStatus::OK
#ifdef _PLUGIN_SUPPORT_
	    || (ret=m_preferred_plugin_path.Set(preferred_plugin_path))!=OpStatus::OK
#endif // _PLUGIN_SUPPORT_
		|| (ret=m_description.Set(description.CStr()))!=OpStatus::OK
	   )
	{
		return ret;
	}
	return OpStatus::OK;
}

#if defined PREFS_HAS_PREFSFILE && defined PREFSFILE_WRITE
void Viewer::WriteL()
{
#if defined DYNAMIC_VIEWERS
	const uni_char* plugin_path = NULL;
	const uni_char* plugin_name = NULL;
	const uni_char* plugin_description = NULL;
# ifdef _PLUGIN_SUPPORT_
	PluginViewer* def_plugin = GetDefaultPluginViewer(FALSE);
	if (def_plugin)
	{
		plugin_path = def_plugin->GetPath();
		plugin_name = def_plugin->GetProductName();
		plugin_description = def_plugin->GetDescription();
	}
# endif // _PLUGIN_SUPPORT_

	const OpStringC8 section = m_content_type_string8;
	PrefsFile& file = g_pcdoc->GetHandlerFile();
	file.WriteStringL(section, "Type", UNI_L("Viewer"));
	OpString value;
	value.Empty();
	value.AppendFormat("%d", static_cast<int>(m_action));
	file.WriteStringL(section, "Action", value.CStr());
	file.WriteStringL(section, "Application", m_application_to_open_with);
	file.WriteStringL(section, "Application Description", m_description);
#ifdef WEB_HANDLERS_SUPPORT
	file.WriteStringL(section, "Web handler", m_web_handler_to_open_with);
#endif // WEB_HANDLERS_SUPPORT
	file.WriteStringL(section, "Plugin Path", plugin_path);
	file.WriteStringL(section, "Plugin Name", plugin_name);
	file.WriteStringL(section, "Plugin Description", plugin_description);
	file.WriteStringL(section, "Save To Folder",  m_save_to_folder);
	file.WriteStringL(section, "Extension", m_extensions_string);
	value.Empty();
	value.AppendFormat("%d", static_cast<short>(m_flags.Get()));
	file.WriteStringL(section, "Flags", value);

#endif // DYNAMIC_VIEWERS
}
#endif // defined PREFS_HAS_PREFSFILE && defined PREFSFILE_WRITE

const uni_char* Viewer::GetExtension(int index) const
{
	if (index >= static_cast<int>(m_extension_list.GetCount())) //Not a valid index results in empty extension
		return NULL;

	return m_extension_list.Get(index)->CStr();
}

BOOL Viewer::HasExtension(const OpStringC& extension) const
{
	int count = m_extension_list.GetCount();

	if (extension.IsEmpty() || count==0)
		return FALSE;

	OpString* list_item;
	int i;
	for (i=0; i<count; i++)
	{
		list_item = m_extension_list.Get(i);
		if (list_item && list_item->CompareI(extension)==0)
			return TRUE;
	}
	return FALSE;
}

BOOL Viewer::HasExtensionL(const OpStringC8& extension) const
{
	OpString tmp_string;
	LEAVE_IF_ERROR(tmp_string.Set(extension));
	return HasExtension(tmp_string);
}

OP_STATUS Viewer::SetContentType(URLContentType content_type,
								 const OpStringC& content_type_string)
{
	// OP_STATUS rc;

	if ( content_type_string.IsEmpty() || m_owner != 0 )
		return OpStatus::ERR;

	// Viewer* v = 0;

#if 0
	// Should not be needed when SetContentType can't be called on
	// registered types.

	/*
	 * Some special checks and actions are needed if this viewer is
	 * already registered in a viewers database.
	 */
	if ( m_owner != 0 )
	{
		/*
		 * Check if the new content-type already exists in the hashtables, and
		 * it isn't this viewer. If so, return an error.
		 */
		rc = m_owner->FindViewerByMimeType(content_type_string, v, FALSE);
		if ( rc == OpStatus::OK && v != this )
			return OpStatus::ERR_OUT_OF_RANGE;

		/*
		 * Unregister the previous content type before setting the new one.
		 */
		m_owner->m_viewer_list.Remove(GetContentTypeString(), &v);

		m_owner->m_viewer_list8.Remove(GetContentTypeString8(),
									   reinterpret_cast<void**>(&v));
	}
#endif

	m_content_type = FROM_RANGED_ENUM(URLContentType, content_type);
	OpStatus::Ignore(m_content_type_string.Set(content_type_string)); // FIXME: If this fails, we're in trouble anyways
	OpStatus::Ignore(m_content_type_string8.Set(content_type_string.CStr())); //FIXME: If this fails, we're in trouble anyways

#if 0
	if ( m_owner != 0 )
	{
		// FIXME: Error checking here? Probably...
		m_owner->m_viewer_list.Add(GetContentTypeString(), this);
		m_owner->m_viewer_list8.Add(GetContentTypeString8(), this);
	}
#endif

	return OpStatus::OK;
}

OP_STATUS Viewer::SetContentType(URLContentType content_type, const OpStringC8& content_type_string8)
{
	OpString tmp_string;
	RETURN_IF_ERROR(tmp_string.Set(content_type_string8)); //Note to self: Should code be duplicated from the 16-bit version, to avoid this extra string copy?

	return SetContentType(content_type, tmp_string);
}

OP_STATUS Viewer::SetContentType(const OpStringC& content_type_string)
{
	OpString8 tmp_string8;
	RETURN_IF_ERROR(tmp_string8.Set(content_type_string.CStr()));

    return SetContentType(tmp_string8);
}

OP_STATUS Viewer::SetContentType(const OpStringC8& content_type_string)
{
	URLContentType content_type = URL_UNKNOWN_CONTENT;

	if (content_type_string.HasContent())
	{
		UINT i;
		for (i=0; i<defaultOperaViewerTypes_SIZE; i++)
		{
			if (content_type_string.CompareI(g_viewers->defaultOperaViewerTypes[i].type) == 0)
			{
				content_type = GET_RANGED_ENUM(URLContentType, g_viewers->defaultOperaViewerTypes[i].ctype);
				break;
			}
		}
	}
    return SetContentType(content_type, content_type_string);
}

OP_STATUS Viewer::SetExtensions(const OpStringC& extensions)
{
	RETURN_IF_ERROR(m_extensions_string.Set(extensions));

	m_extensions_string.Strip();

	OP_STATUS ret = ParseExtensions(m_extensions_string, UNI_L(",. "), &m_extension_list);
	if (OpStatus::IsError(ret))
		m_extensions_string.Empty();

	return ret;
}

OP_STATUS Viewer::AddExtension(const OpStringC& extension)
{
	if (extension.IsEmpty())
		return OpStatus::ERR;

	if (HasExtension(extension))
		return OpStatus::OK;

	OpString* new_extension = OP_NEW(OpString, ());
	if (!new_extension)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret;
	if (OpStatus::IsError(ret=new_extension->Set(extension)) ||
		(m_extensions_string.HasContent() && OpStatus::IsError(ret=m_extensions_string.Append(","))) ||
		OpStatus::IsError(ret=m_extensions_string.Append(extension)) ||
	    OpStatus::IsError(ret=m_extension_list.Add(new_extension)))
	{
		OP_DELETE(new_extension);
		return ret;
	}
	return OpStatus::OK;
}

OP_STATUS Viewer::ResetToDefaultExtensions()
{
	UINT i;
	for(i=0; i<defaultOperaViewerTypes_SIZE; i++)
	{
		if (m_content_type_string8.CompareI(g_viewers->defaultOperaViewerTypes[i].type) == 0)
		{
			return SetExtensions(g_viewers->defaultOperaViewerTypes[i].ext);
		}
	}
	return OpStatus::OK;
}

OP_STATUS Viewer::SetExtensions(const char* extensions)
{
	OpString tmp_string;
	RETURN_IF_ERROR(tmp_string.Set(extensions));

	return SetExtensions(tmp_string);
}

OP_STATUS Viewer::ParseExtensions(const OpStringC& extensions, const OpStringC& separator_chars, OpVector<OpString>* result_array)
{
	if (!result_array || separator_chars.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	result_array->DeleteAll();

	if (extensions.IsEmpty())
		return OpStatus::OK; //Nothing to do

	OP_STATUS ret;
	// Parse extensions list, add each extension to the list of extensions
	const uni_char* separator;
	int extension_length;
	const uni_char* tmp_extension = extensions.CStr();
	while (tmp_extension && *tmp_extension)
	{
		separator = uni_strpbrk(tmp_extension, separator_chars.CStr());
		extension_length = separator ? separator-tmp_extension : int(KAll);
		if (extension_length != 0)
		{
			OpString* extension_item = OP_NEW(OpString, ());
			if (!extension_item)
			{
				ret = OpStatus::ERR_NO_MEMORY;
				goto extension_parsing_failed;
			}

			if ((ret=extension_item->Set(tmp_extension, extension_length))!=OpStatus::OK ||
				(ret=result_array->Add(extension_item))!=OpStatus::OK)
			{
				OP_DELETE(extension_item);
				goto extension_parsing_failed;
			}
		}

		if (!separator)
			break; //We're done

		tmp_extension = separator + 1;
	}
	return OpStatus::OK;

extension_parsing_failed:
	result_array->DeleteAll();
	return ret;
}

#ifdef _PLUGIN_SUPPORT_
PluginViewer* Viewer::FindPluginViewerByName(const OpStringC& plugin_name, BOOL enabled_only) const
{
	if (!g_plugin_viewers)
		return NULL;

	OpStatus::Ignore(g_plugin_viewers->MakeSurePluginsAreDetected());

	UINT count = m_plugins.GetCount();
	if (plugin_name.IsEmpty() || count==0)
		return NULL;

	for (UINT i=0; i<count; i++)
	{
		if (PluginViewer* plugin = m_plugins.Get(i))
		{
			if (enabled_only && !plugin->IsEnabled())
				continue;
			if (plugin_name.Compare(plugin->GetProductName())==0)
				return plugin;
		}
	}
	return NULL;
}

PluginViewer* Viewer::FindPluginViewerByPath(const OpStringC& plugin_path) const
{
	if (!g_plugin_viewers)
		return NULL;

	OpStatus::Ignore(g_plugin_viewers->MakeSurePluginsAreDetected());

	UINT count = m_plugins.GetCount();
	if (plugin_path.IsEmpty() || count==0)
		return NULL;

	for (UINT i=0; i<count; i++)
	{
		PluginViewer* plugin = m_plugins.Get(i);
		if (plugin && plugin->GetPath())
		{
			// FIXME: OOM
			BOOL match;
			if (OpStatus::IsSuccess(g_op_system_info->PathsEqual(plugin->GetPath(), plugin_path.CStr(), &match)) && match)
				return plugin;
		}
	}
	return NULL;
}

PluginViewer* Viewer::GetDefaultPluginViewer(BOOL skip_disabled_plugins) const
{
	if (!g_plugin_viewers)
		return NULL;

	OpStatus::Ignore(g_plugin_viewers->MakeSurePluginsAreDetected());

	UINT count = m_plugins.GetCount();
	if (count == 0)
		return NULL;

	for (UINT i=0; i<count; i++)
	{
		if (PluginViewer* plugin = m_plugins.Get(i))
			if (!skip_disabled_plugins || plugin->IsEnabled())
				return plugin;
	}
	return NULL; //All plugins disabled
}

OP_STATUS Viewer::GetDefaultPluginViewerPath(OpString& plugin_path, OpComponentType& component_type, BOOL skip_disabled_plugins) const
{
	PluginViewer* plugin = GetDefaultPluginViewer(skip_disabled_plugins);
	if (plugin)
	{
		component_type = plugin->GetComponentType();
		return plugin_path.Set(plugin->GetPath());
	}
	else
	{
		plugin_path.Empty();
		return OpStatus::OK;
	}
}

const uni_char* Viewer::GetDefaultPluginViewerPath(BOOL skip_disabled_plugins) const
{
	PluginViewer* plugin = GetDefaultPluginViewer(skip_disabled_plugins);
	return plugin ? plugin->GetPath() : NULL;
}

OP_STATUS Viewer::SetDefaultPluginViewer(PluginViewer* plugin)
{
	if (!plugin)
		return OpStatus::ERR_NULL_POINTER;

	m_plugins.RemoveByItem(plugin);
	return m_plugins.Insert(0, plugin);
}

OP_STATUS Viewer::SetDefaultPluginViewer(unsigned int index)
{
	PluginViewer* plugin = GetPluginViewer(index);
	if (!plugin)
		return OpStatus::ERR_OUT_OF_RANGE;

	return SetDefaultPluginViewer(plugin);
}

const uni_char* Viewer::PluginName() const
{
	PluginViewer* def_plugin = GetDefaultPluginViewer();
	return def_plugin ? def_plugin->GetProductName() : NULL;
}

const uni_char* Viewer::PluginFileOpenText() const
{
	PluginViewer* def_plugin = GetDefaultPluginViewer();
	return def_plugin ? def_plugin->GetDescription() : NULL;
}

OP_STATUS Viewer::ConnectToPlugin(PluginViewer* plugin_viewer)
{
	if (!plugin_viewer)
		return OpStatus::ERR_NULL_POINTER;

	OpStringC path = plugin_viewer->GetPath();
	PluginViewer* existing_plugin = FindPluginViewerByPath(path);
	if (existing_plugin)
		return OpStatus::OK;

	BOOL is_preferred = FALSE;
	if (m_preferred_plugin_path.HasContent())
	{
		if ( OpStatus::IsError(g_op_system_info->PathsEqual(m_preferred_plugin_path.CStr(), path.CStr(), &is_preferred)) )
			is_preferred = FALSE;
	}

	if (is_preferred)
	{
		RETURN_IF_ERROR(m_plugins.Insert(0, plugin_viewer));
	}
	else
	{
		RETURN_IF_ERROR(m_plugins.Add(plugin_viewer));
	}

	if (m_action == VIEWER_ASK_USER && !m_flags.IsSet(ViewActionFlag::USERDEFINED_VALUE))
		SetAction(VIEWER_PLUGIN);

	return OpStatus::OK;
}

OP_STATUS Viewer::DisconnectFromPlugin(PluginViewer* plugin_viewer)
{
	if (!plugin_viewer)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_plugins.RemoveByItem(plugin_viewer));

	if (m_action == VIEWER_PLUGIN && m_plugins.GetCount() == 0)
		SetAction(VIEWER_ASK_USER);

	return OpStatus::OK;
}

#endif // _PLUGIN_SUPPORT_

OP_STATUS Viewer::CopyFrom(Viewer* source)
{
	if (source == NULL)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(SetContentType(GET_RANGED_ENUM(URLContentType, source->m_content_type), source->m_content_type_string));

	//If anything goes wrong now, we're in an inconsistent state!
	OP_STATUS ret;
	if ((ret=SetExtensions(source->m_extensions_string)) != OpStatus::OK ||
	    (ret=SetApplicationToOpenWith(source->m_application_to_open_with)) != OpStatus::OK ||
#ifdef WEB_HANDLERS_SUPPORT
		(ret=SetWebHandlerToOpenWith(source->m_web_handler_to_open_with)) != OpStatus::OK ||
#endif //WEB_HANDLERS_SUPPORT
	    (ret=SetSaveToFolder(source->m_save_to_folder)) != OpStatus::OK ||
		(ret=SetDescription(source->m_description.CStr()) != OpStatus::OK))
	{
		return ret;
	}

#ifdef _PLUGIN_SUPPORT_
	UINT count = source->m_plugins.GetCount();
	for (UINT i=0; i<count; i++)
	{
		if (PluginViewer* tmp_plugin = source->m_plugins.Get(i))
			RETURN_IF_ERROR(ConnectToPlugin(tmp_plugin));
	}

	RETURN_IF_ERROR(m_preferred_plugin_path.Set(source->m_preferred_plugin_path));
#endif // _PLUGIN_SUPPORT_

	SetAction(source->m_action);
	SetFlags(source->m_flags.Get());
#ifdef WEB_HANDLERS_SUPPORT
	SetWebHandlerAllowed(source->m_web_handler_allowed);
#endif // WEB_HANDLERS_SUPPORT
	SetAllowAnyExtension(source->m_allow_any_extension);
	SetAllowedContainer(source->m_container);
	return OpStatus::OK;
}

void Viewer::ResetAction(BOOL user_def /* = FALSE */)
{
	m_action = Viewers::GetDefaultAction(m_content_type_string8.CStr());
	if (user_def)
		m_flags.SetFlags(ViewActionFlag::USERDEFINED_VALUE);
}

Viewers::Viewers()
{
#ifndef HAS_COMPLEX_GLOBALS
	init_defaultOperaViewerTypes();
#endif //HAS_COMPLEX_GLOBALS

	m_viewer_list8.SetHashFunctions(&viewer8_hash_func);
}

Viewers::~Viewers()
{
	m_viewer_list8.RemoveAll(); //Don't delete, it contains the same objects as m_viewer_list (which will autodelete)
}

OP_STATUS Viewers::ConstructL()
{
	DeleteAllViewers();
	TRAPD(err, ReadViewersL());
	if (OpStatus::IsError(err))
		DeleteAllViewers();

	LEAVE_IF_ERROR(err);
	return OpStatus::OK;
}

OP_STATUS Viewers::ReadViewersL()
{
	LEAVE_IF_ERROR(ReadViewers());
	LEAVE_IF_ERROR(ImportGeneratedViewersArray());

	return OpStatus::OK;
}

#if defined PREFS_HAS_PREFSFILE && defined PREFS_WRITE
void Viewers::WriteViewersL()
{
#if defined DYNAMIC_VIEWERS
#if defined UPGRADE_SUPPORT
	unsigned short version = g_pcapp->GetIntegerPref(PrefsCollectionApp::ViewerVersion);
	if (version >= 3)
#endif // UPGRADE_SUPPORT
	{
		OpString_list sections;
		ANCHOR(OpString_list, sections);

		g_pcdoc->GetHandlerFile().ReadAllSectionsL(sections);
		unsigned long sections_cnt = 0;
		while (sections_cnt < sections.Count())
		{
			OpStackAutoPtr<PrefsSection>
			section(g_pcdoc->GetHandlerFile().ReadSectionL(sections.Item(sections_cnt)));

			if (section.get() && section->Get(UNI_L("Type")) && (uni_stri_eq(section->Get(UNI_L("Type")), "viewer")))
			{
				g_pcdoc->GetHandlerFile().DeleteSectionL(sections.Item(sections_cnt));
			}

			++sections_cnt;
		}
	}

    //  Write all viewers defined by user.
	Viewer* tmp_viewer;
	OpHashIterator* iter = m_viewer_list.GetIterator();
	if ( iter == 0 )
		LEAVE(OpStatus::ERR_NO_MEMORY);

	OpStackAutoPtr<OpHashIterator> iter_autoptr(iter);
	OP_STATUS ret = iter->First();
	while(ret == OpStatus::OK)
	{
		tmp_viewer = reinterpret_cast<Viewer*>(iter->GetData());
		if (tmp_viewer)
			tmp_viewer->WriteL();

		ret = iter->Next();
	}

# ifdef UPGRADE_SUPPORT
	if (version < 3) // Ver. 3 moved viewers to separate file
	{
		OpStatus::Ignore(g_pcapp->WriteIntegerL(PrefsCollectionApp::ViewerVersion, VIEWER_VERSION));
		g_pcapp->DeleteViewerSectionsL();
		g_prefsManager->CommitL();
	}
# endif

	g_pcdoc->GetHandlerFile().CommitL(FALSE, FALSE);
#endif // DYNAMIC_VIEWERS
}
#endif // defined PREFS_HAS_PREFSFILE && defined PREFSFILE_WRITE

OP_STATUS Viewers::AddViewer(ViewAction action, URLContentType content_type, const OpStringC& content_type_string,
                             const OpStringC& extensions_string, ViewActionFlag::ViewActionFlagType flags,
                             const OpStringC& application_to_open_with, const OpStringC& save_to_folder
#ifdef _PLUGIN_SUPPORT_
                             , const OpStringC& preferred_plugin_path
#endif // _PLUGIN_SUPPORT_
                             , const OpStringC& description
                             )
{
	if (content_type_string.IsEmpty())
		return OpStatus::ERR;

	Viewer* viewer = OP_NEW(Viewer, ());
	if (!viewer)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret;
	if ((ret=viewer->Construct(action, content_type, content_type_string, extensions_string, flags, application_to_open_with, save_to_folder
#ifdef WEB_HANDLERS_SUPPORT
							   , OpStringC()
#endif // WEB_HANDLERS_SUPPORT
#ifdef _PLUGIN_SUPPORT_
                               , preferred_plugin_path
#endif // _PLUGIN_SUPPORT_
							   , description
                             ))!= OpStatus::OK ||
		(ret=AddViewer(viewer))!=OpStatus::OK)
	{
		OP_DELETE(viewer);
		return ret;
	}

	return OpStatus::OK;
}

OP_STATUS Viewers::AddViewer(Viewer* viewer)
{
	if (!viewer)
		return OpStatus::ERR_NULL_POINTER;

	if (!viewer->GetContentTypeString())
		return OpStatus::ERR;

	Viewer* existing_viewer = NULL;
	if (m_viewer_list.GetData(viewer->GetContentTypeString(), &existing_viewer)==OpStatus::OK && existing_viewer!=NULL)
	{
		return (existing_viewer==viewer) ? OpStatus::OK : OpStatus::ERR;
	}

#ifdef WEB_HANDLERS_SUPPORT
	if (viewer->m_action == VIEWER_REDIRECT)
	{
		OP_ASSERT(viewer->m_application_to_open_with.HasContent());
		viewer->m_action = VIEWER_WEB_APPLICATION;
		viewer->m_application_to_open_with.Append("%s");
	}
#endif // WEB_HANDLERS_SUPPORT

	OP_STATUS ret;
	//Add to 16-bit list (which also will own the object)
	if ((ret=m_viewer_list.Add(viewer->GetContentTypeString(), viewer)) != OpStatus::OK)
		return ret;

	//Add to 8-bit hash list (and clean up 16-bit list, if adding fails)
	if ((ret=m_viewer_list8.Add(viewer->GetContentTypeString8(), viewer)) != OpStatus::OK)
	{
		OpStatus::Ignore(m_viewer_list.Remove(viewer->GetContentTypeString(), &viewer));
		return ret;
	}

	viewer->m_owner = this;

	OnViewerAdded(viewer);

#ifdef _PLUGIN_SUPPORT_
	if (g_plugin_viewers != NULL)
	{
		//Check if any registered plugins matches content-type or extension
		UINT plugin_count = g_plugin_viewers->GetPluginViewerCount();
		for (UINT i=0; i<plugin_count; i++)
		{
			PluginViewer* tmp_plugin = g_plugin_viewers->GetPluginViewer(i);
			if (!tmp_plugin)
				continue;

			BOOL make_connection = FALSE;

			//Does it match content-type?
			if (tmp_plugin->SupportsContentType(viewer->m_content_type_string))
				make_connection = TRUE;

			//Does it match preferred plugin path?
			if (!make_connection && viewer->m_preferred_plugin_path.HasContent() && tmp_plugin->GetPath())
			{
				// FIXME: OOM
				BOOL match;
				if (OpStatus::IsSuccess(g_op_system_info->PathsEqual(viewer->m_preferred_plugin_path.CStr(), tmp_plugin->GetPath(), &match)) && match)
					make_connection = TRUE;
			}

			//Does it match one of the extensions?
			UINT extensions_count = viewer->GetExtensionsCount();
			for (UINT j=0; !make_connection && j<extensions_count; j++)
			{
				const uni_char* tmp_extension = viewer->GetExtension(j);
				if (tmp_plugin->SupportsExtension(tmp_extension))
					make_connection = TRUE;
			}

			if (make_connection)
				OpStatus::Ignore(viewer->ConnectToPlugin(tmp_plugin));
		}
	}
#endif // _PLUGIN_SUPPORT_

	return OpStatus::OK;
}

void Viewers::DeleteViewer(Viewer* viewer)
{
	if (!viewer)
		return;

	OnViewerDeleted(viewer);

	//Remove from 8-bit list
	OpStatus::Ignore(m_viewer_list8.Remove(viewer->GetContentTypeString8(), reinterpret_cast<void**>(&viewer)));
	m_viewer_list.Remove(viewer->GetContentTypeString(), &viewer);
	OP_DELETE(viewer);
}

Viewer* Viewers::FindViewerByMimeType(const OpStringC mimetype)
{
	if (mimetype.IsEmpty())
		return NULL;

	Viewer* ret_viewer = NULL;
	OpStatus::Ignore(m_viewer_list.GetData(mimetype.CStr(), &ret_viewer));
	return ret_viewer;
}

Viewer* Viewers::FindViewerByMimeType(const OpStringC8 mimetype)
{
	if (mimetype.IsEmpty())
		return NULL;

	void* data = NULL;
	OpStatus::Ignore(m_viewer_list8.GetData(mimetype.CStr(), &data));
	return static_cast<Viewer*>(data);
}

OP_STATUS Viewers::FindViewerByExtension(const OpStringC& extension, Viewer*& ret_viewer)
{
	ret_viewer = NULL;

	if (extension.IsEmpty())
		return OpStatus::OK;

	OpHashIterator* iter = m_viewer_list.GetIterator();
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = iter->First();
	while(ret == OpStatus::OK)
	{
		Viewer* viewer = reinterpret_cast<Viewer*>(iter->GetData());
		if (viewer && viewer->HasExtension(extension))
		{
			ret_viewer = viewer;
			break; //We've found a match. Bail out
		}

		ret = iter->Next();
	}
	OP_DELETE(iter);

	return ret;
}

OP_STATUS Viewers::FindViewerByExtension(const OpStringC8& extension, Viewer*& ret_viewer)
{
	ret_viewer = NULL;

	OpString tmp_string;
	RETURN_IF_ERROR(tmp_string.Set(extension));

	return FindViewerByExtension(tmp_string, ret_viewer);
}

OP_STATUS Viewers::FindAllViewersByExtension(const OpStringC& extension, OpVector<Viewer>* result_array)
{
	if (!result_array)
		return OpStatus::ERR_NULL_POINTER;

	result_array->DeleteAll();

	if (extension.IsEmpty())
		return OpStatus::OK;

	OpHashIterator* iter = m_viewer_list.GetIterator();
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = iter->First();
	while(ret == OpStatus::OK)
	{
		Viewer* tmp_viewer = reinterpret_cast<Viewer*>(iter->GetData());
		if (tmp_viewer && tmp_viewer->HasExtension(extension))
			if (OpStatus::IsError(ret = result_array->Add(tmp_viewer)))
				break;

		ret = iter->Next();
	}
	OP_DELETE(iter);
	return OpStatus::OK;
}

OP_STATUS Viewers::FindViewerByFilename(const OpStringC& filename, Viewer*& ret_viewer)
{
	const uni_char *extension = StrFileExt(filename.CStr());
	return extension ? Viewers::FindViewerByExtension(extension, ret_viewer) : OpStatus::ERR;
}

OP_STATUS Viewers::FindViewerByFilename(const OpStringC8& filename, Viewer*& ret_viewer)
{
	const char *extension = StrFileExt(filename.CStr());
	return extension ? Viewers::FindViewerByExtension(extension, ret_viewer) : OpStatus::ERR;
}

OP_STATUS Viewers::FindViewerByURL(URL& url, Viewer*& ret_viewer, BOOL update_url_contenttype)
{
	ret_viewer = NULL;

	OpStringC8 mime_type = url.GetAttribute(URL::KMIME_Type);
	if (mime_type.IsEmpty())
		mime_type = GetContentTypeString(url.ContentType());

	//If we have mime-type, use it (and don't update URL content type, it's correct)
	if (mime_type.HasContent())
	{
		ret_viewer = FindViewerByMimeType(mime_type);
		return OpStatus::OK;
	}

	// If I should NOT trust server types
	// or the type is of zero length
	// or the type is text/plain or octet-stream
	// then determine by extension
	OpString suggested_extension;
	RETURN_IF_LEAVE(url.GetAttributeL(URL::KSuggestedFileNameExtension_L, suggested_extension, TRUE));

	RETURN_IF_ERROR(FindViewerByExtension(suggested_extension, ret_viewer));

	if (update_url_contenttype && ret_viewer!=NULL)
	{
		const OpStringC8 content_type8 = ret_viewer->GetContentTypeString8();
		URLContentType current_contenttype = (URLContentType)url.GetAttribute(URL::KContentType,TRUE);

		if (OpStatus::IsError(url.SetAttribute(URL::KMIME_ForceContentType, content_type8)))
		{
			// If failed, reset to previous Content-Type
			RETURN_IF_LEAVE(url.SetAttributeL(URL::KForceContentType, current_contenttype));
		}
	}

	return OpStatus::OK;
}

OP_STATUS Viewers::GetAppAndAction(URL& url, ViewAction& action, const uni_char*& app, BOOL update_url_contenttype)
{
	app = NULL;

	Viewer* viewer = NULL;
	RETURN_IF_ERROR(FindViewerByURL(url, viewer, update_url_contenttype));

	if (!viewer)
	{
		if (url.GetContentLoaded()==0 && url.Status(TRUE)==URL_LOADING)
		{
			action = VIEWER_WAIT_FOR_DATA;
		}
		else
		{
			viewer = FindViewerByMimeType(UNI_L("..."));
		}
	}

	if (viewer)
	{
		action = viewer->GetAction();
		switch (action)
		{
#ifdef _PLUGIN_SUPPORT_
			case VIEWER_PLUGIN:
				if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled, url) || (app=viewer->GetDefaultPluginViewerPath()) == NULL)
					action = VIEWER_ASK_USER;
			break;
#endif // _PLUGIN_SUPPORT_
#ifdef WEB_HANDLERS_SUPPORT
			case VIEWER_WEB_APPLICATION:
				app = viewer->GetWebHandlerToOpenWith();
			break;
#endif // WEB_HANDLERS_SUPPORT
			default:
				app = viewer->GetApplicationToOpenWith();
			break;
		}
	}
	return OpStatus::OK;
}

BOOL Viewers::IsNativeViewerType(const OpStringC8& content_type)
{
	if (content_type.IsEmpty())
        return FALSE;

	UINT i;
	for (i=0; i<defaultOperaViewerTypes_SIZE; i++)
    {
        if (content_type.CompareI(defaultOperaViewerTypes[i].type) == 0)
            return TRUE;
    }
    return FALSE;
}

OP_STATUS Viewers::CreateIterator(ChainedHashIterator*& iterator)
{
	if ((iterator=static_cast<ChainedHashIterator*>(m_viewer_list.GetIterator())) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = iterator->First();
	if (OpStatus::IsError(ret))
	{
		OP_DELETE(iterator);
		iterator = NULL;
	}
	return ret;
}

Viewer* Viewers::GetNextViewer(ChainedHashIterator* iterator)
{
	if (!iterator || iterator->GetHashLinkPos() < 0)
		return NULL;

	Viewer* viewer = reinterpret_cast<Viewer*>(iterator->GetData());
	iterator->Next();
	return viewer;
}

const char* Viewers::GetContentTypeString(URLContentType content_type)
{
	if(content_type==URL_UNKNOWN_CONTENT || content_type==URL_UNDETERMINED_CONTENT)
		return NULL;

	UINT32 i;
	for(i=0; i<defaultOperaViewerTypes_SIZE; i++)
	{
		if (GET_RANGED_ENUM(URLContentType, g_viewers->defaultOperaViewerTypes[i].ctype) == content_type)
			return g_viewers->defaultOperaViewerTypes[i].type;
	}
	return NULL;
}

const char* Viewers::GetDefaultContentTypeStringFromExt(const OpStringC& ext)
{
	OpString8 ext8;
	if (OpStatus::IsError(ext8.SetUTF8FromUTF16(ext.CStr())))
		return NULL;

	return GetDefaultContentTypeStringFromExt(ext8);
}

const char* Viewers::GetDefaultContentTypeStringFromExt(const OpStringC8& ext)
{
	if (ext.IsEmpty())
		return NULL;

	int len = ext.Length();

	for(unsigned i = 0; i<defaultOperaViewerTypes_SIZE; i++)
	{
		if (g_viewers->defaultOperaViewerTypes[i].type == NULL)
		{
			break;
		}

		BOOL found = FALSE;
		const char* tmp_extension = g_viewers->defaultOperaViewerTypes[i].ext;

		while (!found && tmp_extension)
		{
			const char* separator = op_strpbrk(tmp_extension, ",. ");
			if (!separator) // special case for end of string
			{
				found = ext.CompareI(tmp_extension)==0;
				break;
			}
			else if (len==(separator-tmp_extension) && ext.CompareI(tmp_extension, len)==0)
				found = TRUE;
			else
				tmp_extension = separator + 1;
		}

		if (found)
			return g_viewers->defaultOperaViewerTypes[i].type;
	}

	return NULL;
}

OP_STATUS Viewers::GetViewAction(URL& url, const OpStringC mime_type, ViewActionReply& reply, BOOL want_plugin, BOOL check_octetstream)
{
	reply.component_type = COMPONENT_SINGLETON;
	reply.mime_type = mime_type;
	reply.app = NULL;
	reply.action = VIEWER_NOT_DEFINED;

	BOOL has_file_extension = FALSE;
	OpString fileext;
	TRAPD(status, url.GetAttributeL(URL::KSuggestedFileNameExtension_L, fileext));
	if (OpStatus::IsSuccess(status) && fileext.HasContent())
		has_file_extension = TRUE;

	BOOL use_mime = TRUE;
	if (mime_type.IsEmpty())	// if mime is not set, must use extension
	{
		use_mime=FALSE;
	}
	else if (check_octetstream ||	// if we are not trusting server types, text/plain and octet-stream should be tested on extension
	         (has_file_extension && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TrustServerTypes) == FALSE) // If we dont have file extension then still use mime-type (for example: "data:text/plain, blah")
	        )
	{
		if (uni_stri_eq(mime_type.CStr(),"APPLICATION/OCTET-STREAM")!=0 ||
			uni_stri_eq(mime_type.CStr(),"TEXT/PLAIN")!=0)
		{
			use_mime=FALSE;
		}
	}

	if (use_mime)
	{
		if (Viewer* viewer = g_viewers->FindViewerByMimeType(mime_type))
		{
			reply.action = viewer->GetAction();

#ifdef _PLUGIN_SUPPORT_
			if (reply.action==VIEWER_PLUGIN || want_plugin)
			{
				if (PluginViewer* plugin = viewer->GetDefaultPluginViewer())
				{
					reply.component_type = plugin->GetComponentType();
					reply.app = plugin->GetPath();
				}

				if (reply.action == VIEWER_ASK_USER && reply.app.HasContent())
					reply.action = VIEWER_PLUGIN;
			}
#endif // _PLUGIN_SUPPORT_
		}
	}

	if (reply.action==VIEWER_NOT_DEFINED)
	{
		if(has_file_extension)
		{
			if (fileext.Length() > 3)
				fileext.CStr()[3] = 0;

			Viewer* viewer = NULL;
			if (g_viewers->FindViewerByExtension(fileext, viewer)==OpStatus::OK && viewer!=NULL)
			{
				reply.action = viewer->GetAction();
				reply.mime_type = viewer->GetContentTypeString();

#ifdef _PLUGIN_SUPPORT_
				if (reply.action == VIEWER_PLUGIN || want_plugin)
					if (PluginViewer* plugin = viewer->GetDefaultPluginViewer())
					{
						reply.component_type = plugin->GetComponentType();
						reply.app = plugin->GetPath();
					}
#endif // _PLUGIN_SUPPORT_
			}
		}
	}
	return OpStatus::OK;
}

/* static */
ViewAction Viewers::GetDefaultAction(URLContentType content_type)
{
	const ViewerTypes* global_viewer_types = g_viewers->defaultOperaViewerTypes;
	for (UINT32 i = 0; i < defaultOperaViewerTypes_SIZE && global_viewer_types[i].type; i++)
	{
		if (GET_RANGED_ENUM(URLContentType, global_viewer_types[i].ctype) == content_type)
			return global_viewer_types[i].action;
	}
	return VIEWER_ASK_USER;
}

/* static */
ViewAction Viewers::GetDefaultAction(const char* type)
{
	const ViewerTypes* global_viewer_types = g_viewers->defaultOperaViewerTypes;
	for (UINT32 i = 0; i < defaultOperaViewerTypes_SIZE && global_viewer_types[i].type; i++)
	{
		if (op_strcmp(type, global_viewer_types[i].type) == 0)
			return global_viewer_types[i].action;
	}
	return VIEWER_ASK_USER;
}

void Viewers::DeleteAllViewers()
{
	if (m_viewer_list8.GetCount() > 0)
		m_viewer_list8.RemoveAll(); //Don't delete, it contains the same objects as m_viewer_list (which will be deleted below)

	if (m_viewer_list.GetCount() > 0)
		m_viewer_list.DeleteAll();
}

OP_STATUS Viewers::ReadViewers()
{
#ifdef DYNAMIC_VIEWERS
	OpString	key_buf, value_buf;

# ifdef UPGRADE_SUPPORT
	const int	TOKENS_ON_A_LINE = 5;           //  # of tokens on a line in the ini-file
    uni_char* token_array[TOKENS_ON_A_LINE];
    short version = g_pcapp->GetIntegerPref(PrefsCollectionApp::ViewerVersion);
    OP_MEMORY_VAR short num_tokens = version ? 5 : 3;
# endif
	OP_MEMORY_VAR OP_STATUS ret;
# ifdef UPGRADE_SUPPORT
	if (version < 3)
	{
		// Read all entries in the "File Types" section sequentially. This will
		// automatically combine the entries from the global fixed, user and
		// global settings file (in that order). No key value occurs twice.

		OP_MEMORY_VAR BOOL got_something;
		OP_MEMORY_VAR OP_STATUS ret;
		OP_STATUS stat;
		TRAP(ret, got_something = g_pcapp->ReadViewerTypesL(key_buf, value_buf, stat, TRUE));
		OpStatus::Ignore(stat);
		if (ret != OpStatus::OK)
			return ret;

		while (got_something)
		{
			//  Parse substrings
			if (value_buf.HasContent() && GetStrTokens(value_buf.CStr(), UNI_L(","), UNI_L(" \t"), token_array, num_tokens)==num_tokens)
			{
				if (Viewer* tmp_viewer = FindViewerByMimeType(key_buf))
					DeleteViewer(tmp_viewer);

				Viewer* viewer = NULL;
				ret = OpStatus::OK;

				// Parse settings
				if (version)
				{
					uni_char* token = (token_array[4] ? uni_strrchr(token_array[4], '|') : NULL);
					if (token && token!=token_array[4])
					{
						*token = 0;
						*(token-1) = 0;
					}

					OpString ext_buf;
					const uni_char* dst_folder = NULL;
					short flags = 0;
					if (version == 2)
					{
						// version is 2, get extension
						OP_MEMORY_VAR BOOL got;
						TRAP(ret, got = g_pcapp->ReadViewerExtensionL(key_buf, ext_buf));
						if (ret != OpStatus::OK)
							return ret;

						if (got)
						{
							uni_char* ext_token_array[2];
							GetStrTokens(ext_buf.CStr(), UNI_L(","), UNI_L(" \t"), ext_token_array, 2);
							dst_folder = ext_token_array[0];

							// when dealing with last token, check if a "," can be found
							// and put 0 there, so that this routine can read future
							// extensions, where more tokens appear. NOTE that order
							// can never change.
							if (ext_token_array[1])
							{
								uni_char *end = uni_strrchr(ext_token_array[1], ',');
								if (end)
									*end = 0;

								flags = uni_atoi(ext_token_array[1]);
							}
						}
					}

					viewer = OP_NEW(Viewer, ());
					if (!viewer)
						return OpStatus::ERR_NO_MEMORY;

					ret = viewer->Construct((ViewAction)uni_atoi(token_array[0]), URL_UNKNOWN_CONTENT, key_buf,
											token_array[4], (ViewActionFlag::ViewActionFlagType)flags,
											token_array[1], dst_folder
#ifdef WEB_HANDLERS_SUPPORT
											, OpStringC()
#endif // WEB_HANDLERS_SUPPORT
#ifdef _PLUGIN_SUPPORT_
											, token_array[2]
#endif // _PLUGIN_SUPPORT_
										   );
				}
				else  // version == 0
				{
					viewer = OP_NEW(Viewer, ());
					if (!viewer)
						return OpStatus::ERR_NO_MEMORY;

					ret = viewer->Construct((ViewAction)uni_atoi(token_array[0]), URL_UNKNOWN_CONTENT, key_buf,
											token_array[2], ViewActionFlag::NO_FLAGS, token_array[1]);
				}

				if (ret!=OpStatus::OK ||
					(viewer && (ret=AddViewer(viewer))!=OpStatus::OK))
				{
					OP_DELETE(viewer);
					return ret;
				}
			}

			got_something = g_pcapp->ReadViewerTypesL(key_buf, value_buf, stat);
			OpStatus::Ignore(stat);
		} // while

			g_prefsManager->CommitL();
	}
	else // version >= 3
#endif // UPGRADE_SUPPORT
	{
#ifdef HAVE_HANDLERS_FILE
		OpString_list sections;

		RETURN_IF_LEAVE(g_pcdoc->GetHandlerFile().ReadAllSectionsL(sections));
		unsigned long sections_cnt = 0;
		while (sections_cnt < sections.Count())
		{
			PrefsSection *section = NULL;
			RETURN_IF_LEAVE(section = g_pcdoc->GetHandlerFile().ReadSectionL(sections.Item(sections_cnt)));
			OpAutoPtr<PrefsSection> ap_section(section);
			if (!section || !section->Get(UNI_L("Type")) || (!uni_stri_eq(section->Get(UNI_L("Type")), "viewer")))
			{
				++sections_cnt;
				continue;
			}

			if (Viewer* tmp_viewer = FindViewerByMimeType(section->Name()))
				DeleteViewer(tmp_viewer);

			Viewer* viewer = OP_NEW(Viewer, ());
			RETURN_OOM_IF_NULL(viewer);
			OpAutoPtr<Viewer> ap_viewer(viewer);


			ret = viewer->Construct((ViewAction)uni_atoi(section->Get(UNI_L("Action"))), URL_UNKNOWN_CONTENT, sections.Item(sections_cnt),
											section->Get(UNI_L("Extension")), (ViewActionFlag::ViewActionFlagType)uni_atoi(section->Get(UNI_L("Flags"))),
									        section->Get(UNI_L("Application")), section->Get(UNI_L("Save To Folder"))
#ifdef WEB_HANDLERS_SUPPORT
											, section->Get(UNI_L("Web handler"))
#endif // WEB_HANDLERS_SUPPORT
#ifdef _PLUGIN_SUPPORT_
											, section->Get(UNI_L("Plugin Path"))
#endif // _PLUGIN_SUPPORT_

										   , section->Get(UNI_L("Application Description")));
			RETURN_IF_ERROR(ret);
			RETURN_IF_ERROR(AddViewer(viewer));
			ap_viewer.release();
			++sections_cnt;
		}
#endif // HAVE_HANDLERS_FILE
	}
#endif // DYNAMIC_VIEWERS

	return OpStatus::OK;
}

OP_STATUS Viewers::ImportGeneratedViewersArray()
{
    for (UINT i=0; i<defaultOperaViewerTypes_SIZE; i++)
	{
		if (defaultOperaViewerTypes[i].type == NULL)
		{
			// we are done
			break;
		}

		if (Viewer* existing_viewer = FindViewerByMimeType(defaultOperaViewerTypes[i].type))
		{
			//Update content type enum value if it isn't set already
			if (existing_viewer->m_content_type == FROM_RANGED_ENUM(URLContentType, URL_UNKNOWN_CONTENT))
				existing_viewer->m_content_type = defaultOperaViewerTypes[i].ctype;

#ifdef WEB_HANDLERS_SUPPORT
			existing_viewer->SetWebHandlerAllowed(defaultOperaViewerTypes[i].web_handler_allowed);
			/* Reset the viewer to the default when the web handler is not allowed
			for this viewer and yet its action is VIEWER_WEB_APPLICATION. */
			if (!existing_viewer->GetWebHandlerAllowed() && existing_viewer->GetAction() == VIEWER_WEB_APPLICATION)
			{
				/* Checks that there's no misconfiguration: if a web handler
				is not allowed the default action must be other than
				VIEWER_WEB_APPLICATION. */
				OP_ASSERT(defaultOperaViewerTypes[i].action != VIEWER_WEB_APPLICATION);
				existing_viewer->SetAction(defaultOperaViewerTypes[i].action);
			}
#endif // WEB_HANDLERS_SUPPORT
			existing_viewer->SetAllowedContainer(defaultOperaViewerTypes[i].container);
			existing_viewer->SetAllowAnyExtension(defaultOperaViewerTypes[i].allow_any_extension);

			continue;
		}

		Viewer* viewer = OP_NEW(Viewer, ());
		if (!viewer)
			return OpStatus::ERR_NO_MEMORY;

		//Add manually (not using AddViewer), as we know we are at startup (no plugins present yet), and we have direct access to some 8-bit strings (avoid unneeded string convert/copy)
		viewer->SetAction(defaultOperaViewerTypes[i].action);
#ifdef WEB_HANDLERS_SUPPORT
		viewer->SetWebHandlerAllowed(defaultOperaViewerTypes[i].web_handler_allowed);
#endif // WEB_HANDLERS_SUPPORT
		viewer->SetAllowedContainer(defaultOperaViewerTypes[i].container);
		viewer->SetAllowAnyExtension(defaultOperaViewerTypes[i].allow_any_extension);

		OP_STATUS ret;
		if ((ret=viewer->SetContentType(GET_RANGED_ENUM(URLContentType, defaultOperaViewerTypes[i].ctype), defaultOperaViewerTypes[i].type))!=OpStatus::OK ||
		    (ret=viewer->SetExtensions(defaultOperaViewerTypes[i].ext))!=OpStatus::OK)
		{
			OP_DELETE(viewer);
			return ret;
		}

		if ((ret=m_viewer_list.Add(viewer->GetContentTypeString(), viewer)) != OpStatus::OK)
			return ret;

		//Add to 8-bit hash list (and clean up, if adding fails)
		if ((ret=m_viewer_list8.Add(viewer->GetContentTypeString8(), viewer)) != OpStatus::OK)
		{
			OpStatus::Ignore(m_viewer_list.Remove(viewer->GetContentTypeString(), &viewer));
			OP_DELETE(viewer);
			return ret;
		}

		viewer->m_owner = this;
	}
	return OpStatus::OK;
}

#ifdef _PLUGIN_SUPPORT_
void Viewers::RemovePluginViewerReferences() //Called by Plugins::RefreshPlugins
{
	OpHashIterator* iter = m_viewer_list.GetIterator();
	if (!iter)
		return;

	OP_STATUS ret = iter->First();
	while (ret == OpStatus::OK)
	{
		if (Viewer* viewer = reinterpret_cast<Viewer*>(iter->GetData()))
			viewer->m_plugins.Clear();

		ret = iter->Next();
	}
	OP_DELETE(iter);
}
#endif // _PLUGIN_SUPPORT_

UINT32 Viewers::Viewer8HashFunctions::Hash(const void* k)
{
	return static_cast<UINT32>(djb2hash_nocase(reinterpret_cast<const char*>(k)));
}

BOOL Viewers::Viewer8HashFunctions::KeysAreEqual(const void* key1, const void* key2)
{
	return (op_stricmp(static_cast<const char*>(key1), static_cast<const char*>(key2)) == 0);
}

#ifdef WEB_HANDLERS_SUPPORT
OP_STATUS Viewers::FindViewerByWebApplication(const OpStringC& url, Viewer*& ret_viewer)
{
	OpHashIterator* iter = m_viewer_list.GetIterator();
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = iter->First();
	while(OpStatus::IsSuccess(status))
	{
		ret_viewer = reinterpret_cast<Viewer*>(iter->GetData());
		if (ret_viewer && ret_viewer->GetAction() == VIEWER_WEB_APPLICATION)
		{
			const uni_char* handler_url = ret_viewer->GetWebHandlerToOpenWith();
			if (handler_url && *handler_url)
			{
				const uni_char* parameter_pos = uni_strstr(handler_url, UNI_L("%s"));
				if (parameter_pos)
				{
					size_t num_of_chars = (parameter_pos - handler_url);
					if (!url.CompareI(handler_url, num_of_chars))
						break; //We've found a match. Bail out
				}
			}
		}

		status = iter->Next();
	}
	OP_DELETE(iter);

	if (OpStatus::IsError(status))
		ret_viewer = NULL;

	return OpStatus::OK;
}
#endif // WEB_HANDLERS_SUPPORT
