/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/util/opfile/opfile.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

// The external application names are written here as strings, we cannot use
// OpFile for most platforms since they need to be able to include command
// line parameters.
// Platforms that require external applications in other formats should ifdef
// themselves out of this file and add their application to the platform
// specific collection in their preferred format.

#include "modules/prefs/prefsmanager/collections/pc_app_c.inl"

PrefsCollectionApp *PrefsCollectionApp::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcapp)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcapp = OP_NEW_L(PrefsCollectionApp, (reader));
	return g_opera->prefs_module.m_pcapp;
}

PrefsCollectionApp::~PrefsCollectionApp()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCAPP_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCAPP_NUMBEROFINTEGERPREFS);
#endif

#ifdef DYNAMIC_VIEWERS
	OP_DELETE(m_viewersection);
	OP_DELETE(m_viewerextensionsection);
	// NB! m_viewer_current must not be deleted, it is owned by m_viewersection
#endif

#if defined _PLUGIN_SUPPORT_ && defined PREFS_HAS_PREFSFILE && defined PREFS_READ
# ifdef PREFS_IGNOREPLUGIN_CONFIGURE
	OP_DELETE(m_ignoreini);
# else
	OP_DELETE(m_IgnorePlugins);
# endif
#endif // _PLUGIN_SUPPORT_ && PREFS_HAS_PREFSFILE && PREFS_READ

	g_opera->prefs_module.m_pcapp = NULL;
}

void PrefsCollectionApp::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCAPP_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCAPP_NUMBEROFINTEGERPREFS);

#ifdef DYNAMIC_VIEWERS
	// Cache viewer data
	m_viewersection = m_reader->ReadSectionL(UNI_L("File Types"));
	/*DOC
	 *section=File Types
	 *name=m_viewersection
	 *key=mime/type
	 *type=string
	 *value=action,app,plugin,pluginname,ext,|fileopenext
	 *description=How to handle file type
	 */
	m_viewerextensionsection = m_reader->ReadSectionL(UNI_L("File Types Extension"));
	/*DOC
	 *section=File Types Extension
	 *name=m_viewerextensionsection
	 *key=mime/type
	 *type=string
	 *value=destfolder,flags
	 *description=Additional data on how to handle file type
	 */
#endif

#ifdef _PLUGIN_SUPPORT_
	// Fix-up plug-in path
	OpString newpath; ANCHOR(OpString, newpath);
	g_op_system_info->GetPluginPathL(m_stringprefs[PluginPath], newpath);
	m_stringprefs[PluginPath].SetL(newpath);

# if defined PREFS_HAS_PREFSFILE && defined PREFS_READ
	ReadPluginsToBeIgnoredL();
# endif
#endif

#ifdef PREFS_HAVE_DEFAULT_SOURCE_VIEWER
	// Retrieve non-static defaults
	if (m_stringprefs[SourceViewer].IsEmpty())
	{
		m_stringprefs[SourceViewer].SetL(g_op_system_info->GetDefaultTextEditorL());
	}
#endif // PREFS_HAVE_DEFAULT_SOURCE_VIEWER
}

#ifdef PREFS_VALIDATE
void PrefsCollectionApp::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
#if defined DYNAMIC_VIEWERS && defined UPGRADE_SUPPORT
	case ViewerVersion:
		break; // Nothing to do.
#endif

#ifdef _PLUGIN_SUPPORT_
	case PluginScriptAccess:
		break; // Nothing to do.
#endif // _PLUGIN_SUPPORT_

#ifdef EXTERNAL_APPLICATIONS_SUPPORT
# ifdef PREFS_HAVE_RUN_IN_TERMINAL
	case RunSourceViewerInTerminal:
	case RunEmailInTerminal:
	case RunNewsInTerminal:
		break; // Nothing to do.
# endif
# ifdef PREFS_HAVE_PLUGIN_TIMEOUT
	case PluginStartTimeout:
	case PluginResponseTimeout:
		break; // Nothing to do.
# endif

# if defined PREFS_HAVE_MAIL_APP || defined PREFS_HAVE_NEWS_APP
	case ExtAppParamSpaceSubst:
		break; // Nothing to do.
# endif
#endif
#ifdef NS4P_COMPONENT_PLUGINS
	case PluginSyncTimeout:
		if (*value < 0)
			*value = NS4P_SYNC_TIMEOUT;
		break;
#endif
#ifdef PREFS_HAVE_APP_INFO
	case AppState:
		break; // Nothing to do.
#endif // PREFS_HAVE_APP_INFO
#ifdef PREFS_HAVE_HBBTV
	case HbbTVInitialize:
		break; // Nothing to do.
#endif // PREFS_HAVE_HBBTV

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionApp::CheckConditionsL(int which, const OpStringC &invalue,
                                          OpString **outvalue, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
#ifdef EXTERNAL_APPLICATIONS_SUPPORT
	case SourceViewer:
		break; // Nothing to do.
# ifdef PREFS_HAVE_MAIL_APP
	case ExternalMailClient:
		break; // Nothing to do.
# endif
# ifdef PREFS_HAVE_NEWS_APP
	case ExternalNewsClient:
# endif
		break; // Nothing to do.
#endif

#ifdef _PLUGIN_SUPPORT_
	case PluginPath:
		break; // Nothing to do.
	case DisabledPlugins:
		break; // Nothing to do.
#endif
#ifdef PREFS_HAVE_APP_INFO
	case AppId: // Nothing to do.
		break;
#endif // PREFS_HAVE_APP_INFO

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}

	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;
}
#endif // PREFS_VALIDATE

#if defined _PLUGIN_SUPPORT_ && defined PREFS_HAS_PREFSFILE && defined PREFS_READ
void PrefsCollectionApp::ReadPluginsToBeIgnoredL()
{
	OpStackAutoPtr<OpFile> ignorefile(OP_NEW_L(OpFile, ()));
	g_pcfiles->GetFileL(PrefsCollectionFiles::PluginIgnoreFile, *ignorefile);

# ifdef PREFS_IGNOREPLUGIN_CONFIGURE
	OP_ASSERT(m_ignoreini == NULL);

	m_ignoreini = OP_NEW_L(PrefsFile, (PREFS_STD));
	m_ignoreini->ConstructL();
	m_ignoreini->SetFileL(ignorefile.get());
	m_ignoreini->LoadAllL();
# else
	OP_ASSERT(m_IgnorePlugins == NULL);

	OpStackAutoPtr<PrefsFile> ignoreini(OP_NEW_L(PrefsFile, (PREFS_STD)));
	ignoreini->ConstructL();
	ignoreini->SetFileL(ignorefile.get());
	ignoreini->LoadAllL();

	m_IgnorePlugins = ignoreini->ReadSectionL(UNI_L("Plugins to Ignore"));
# endif
}
#endif

#ifdef _PLUGIN_SUPPORT_
BOOL PrefsCollectionApp::IsPluginToBeIgnored(const uni_char *plugfilename)
{
# ifdef PREFS_IGNOREPLUGIN_CONFIGURE
	const uni_char *filename = uni_strrchr(plugfilename, PATHSEPCHAR);

	return m_ignoreini->IsKey(UNI_L("Plugins to Ignore"), filename ? filename + 1 : plugfilename);
# else
#  if defined PREFS_HAS_PREFSFILE && defined PREFS_READ
	if (m_IgnorePlugins)
	{
		const uni_char *filename = uni_strrchr(plugfilename, PATHSEPCHAR);

		return m_IgnorePlugins->FindEntry(filename ? filename + 1 : plugfilename) != NULL;
	}
	else
#  endif
	{
		return FALSE;
	}
# endif
}

# if defined PREFS_IGNOREPLUGIN_CONFIGURE && defined PREFS_WRITE
void PrefsCollectionApp::WritePluginToBeIgnoredL(const uni_char *plugfilename, BOOL ignore)
{
	const uni_char *filename = uni_strrchr(plugfilename, PATHSEPCHAR);
	filename = filename ? filename + 1 : plugfilename;

	BOOL docommit;
	if (ignore)
	{
		docommit = OpStatus::IsSuccess(m_ignoreini->WriteStringL(UNI_L("Plugins to Ignore"), filename, NULL));
	}
	else
	{
		docommit = m_ignoreini->DeleteKeyL(UNI_L("Plugins to Ignore"), filename);
	}

	if (docommit)
	{
		m_ignoreini->CommitL(FALSE, FALSE);
	}
}
# endif
#endif // _PLUGIN_SUPPORT_

#ifdef DYNAMIC_VIEWERS
// Read viewer types defined in the fixed global, user and global
// preferences files. A type can only be returned once, and that is
// from the highest ranking file it is found in.
BOOL PrefsCollectionApp::ReadViewerTypesL(OpString &key, OpString &val,
                                          OP_STATUS &errval, BOOL restart)
{
	errval = OpStatus::OK;

	if (!restart && !m_viewersection)
	{
		OP_ASSERT(!"Must start with restart = TRUE");
		LEAVE(OpStatus::ERR_NULL_POINTER);
	}

	if (restart)
	{
		if (!m_viewersection)
		{
			// Make sure we have the preference file loaded, since this code
			// reads directly from it
			if (OpStatus::IsError(m_reader->LoadAllL()))
				return FALSE;

			OP_DELETE(m_viewersection);
			m_viewersection = NULL;
			m_viewer_current = NULL;

			m_viewersection = m_reader->ReadSectionL(UNI_L("File Types"));
			if (NULL == m_viewersection)
			{
				m_reader->Flush();
				return FALSE;
			}
			m_viewerextensionsection = m_reader->ReadSectionL(UNI_L("File Types Extension"));

			// Flush the read preferences file
			m_reader->Flush();
		}

		m_viewer_current = m_viewersection->Entries();
	}

	if (m_viewer_current)
	{
		key.SetL(m_viewer_current->Key());
		val.SetL(m_viewer_current->Value());

# ifdef UPGRADE_SUPPORT
		// Be backwards compatible with earlier versions of the prefs
		// code which replaced "=" in the type name with a "~" since
		// the storage code used to be unable to handle "=" inside keys.
		if (!key.IsEmpty())
		{
			for (uni_char *runner = key.CStr(); *runner; ++runner)
				if (*runner == '~')
					*runner = '=';
		}
# endif

		m_viewer_current = m_viewer_current->Suc();
		return TRUE;
	}

	// Delete the cached sections
	OP_DELETE(m_viewersection);
	m_viewersection = NULL;
	m_viewer_current = NULL;

	OP_DELETE(m_viewerextensionsection);
	m_viewerextensionsection = NULL;

	// The read is completed
	return FALSE;
}

BOOL PrefsCollectionApp::ReadViewerExtensionL(const OpStringC &key, OpString &buf)
{
	if (m_viewerextensionsection)
	{
		buf.SetL(m_viewerextensionsection->Get(key.CStr()));
		return !buf.IsEmpty();
	}

	return FALSE;
}
#endif // DYNAMIC_VIEWERS

const uni_char *PrefsCollectionApp::GetDefaultStringPref(stringpref which) const
{
#ifdef PREFS_HAVE_DEFAULT_SOURCE_VIEWER
	switch (which)
	{
	case SourceViewer:
		{
			const uni_char * OP_MEMORY_VAR deftexteditor = NULL;
			TRAPD(rc, deftexteditor = g_op_system_info->GetDefaultTextEditorL());
			if (OpStatus::IsSuccess(rc))
				return deftexteditor;
		}
		// else fall through

	default:
#else // PREFS_HAVE_DEFAULT_SOURCE_VIEWER
	{
#endif // PREFS_HAVE_DEFAULT_SOURCE_VIEWER
		return m_stringprefdefault[which].defval;
	}
}

#if defined PREFS_HAVE_STRING_API || defined PREFS_WRITE
const uni_char *PrefsCollectionApp::GetDefaultStringInternal(int which, const struct stringprefdefault *)
{
	return GetDefaultStringPref(stringpref(which));
}
#endif // PREFS_HAVE_STRING_API

#if defined DYNAMIC_VIEWERS && defined UPGRADE_SUPPORT
void PrefsCollectionApp::DeleteViewerSectionsL()
{
	m_reader->DeleteSectionL(UNI_L("File Types"));
	m_reader->DeleteSectionL(UNI_L("File Types Extension"));
	OP_DELETE(m_viewersection);
	m_viewersection = NULL;
	OP_DELETE(m_viewerextensionsection);
	m_viewerextensionsection = NULL;
}
#endif // defined DYNAMIC_VIEWERS && defined UPGRADE_SUPPORT
