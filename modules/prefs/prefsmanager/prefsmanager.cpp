/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/opprefscollection.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/impl/backend/prefssectioninternal.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefsfile.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_parsing.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#ifdef _PRINT_SUPPORT_
# include "modules/prefs/prefsmanager/collections/pc_print.h"
#endif
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#if defined GEOLOCATION_SUPPORT
# include "modules/prefs/prefsmanager/collections/pc_geoloc.h"
#endif
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
# include "modules/database/prefscollectiondatabase.h"
#endif //DATABASE_MODULE_MANAGER_SUPPORT
#ifdef WEBSERVER_SUPPORT
# include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
# include "modules/prefs/prefsmanager/collections/pc_ui.h"
#endif
#ifdef M2_SUPPORT
# include "modules/prefs/prefsmanager/collections/pc_m2.h"
#endif
#ifdef PREFS_HAVE_MSWIN
# include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#endif
#ifdef PREFS_HAVE_COREGOGI
# include "platforms/core/prefscollectioncoregogi.h"
#endif
#ifdef PREFS_HAVE_UNIX
# include "modules/prefs/prefsmanager/collections/pc_unix.h"
#endif
#ifdef PREFS_HAVE_MAC
# include "modules/prefs/prefsmanager/collections/pc_macos.h"
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
# include "modules/prefs/prefsmanager/collections/pc_tools.h"
#endif
#ifdef SUPPORT_DATA_SYNC
# include "modules/prefs/prefsmanager/collections/pc_sync.h"
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
# include "modules/prefs/prefsmanager/collections/pc_opera_account.h"
#endif
#ifdef DOM_JIL_API_SUPPORT
# include "modules/prefs/prefsmanager/collections/pc_jil.h"
#endif

#ifdef MSWIN
# include "platforms/windows/pi/WindowsOpSystemInfo.h"
#endif

#ifdef PREFS_GETOVERRIDDENHOSTS
# include "modules/util/opstrlst.h"
#endif

PrefsManager::PrefsManager(PrefsFile *reader)
	: m_initialized(FALSE)
#ifdef PREFS_READ
	, m_reader(reader)
#endif
#ifdef PREFS_HOSTOVERRIDE
	, m_overrides(NULL)
# ifdef PREFS_READ
	, m_overridesreader(NULL)
# endif
#endif
{
}

PrefsManager::~PrefsManager()
{
	// Destroy all collections
	g_opera->prefs_module.m_collections->Clear();

#ifdef PREFS_READ
	// Destroy reader
	OP_DELETE(m_reader);
#endif

#ifdef PREFS_HOSTOVERRIDE
	// Destroy cache of overridden hosts
	OP_DELETE(m_overrides);

# ifdef PREFS_READ
	// Destroy overrides reader
	OP_DELETE(m_overridesreader);
# endif
#endif
}

void PrefsManager::ConstructL()
{
	// Create all preference collections, and store them in the linked list
	// owned by the PrefsModule instance. The CreateL methods also make
	// sure the "global" g_pc* objects are set up properly.

	Head *collections = g_opera->prefs_module.m_collections;

#ifndef PREFS_READ
	PrefsFile *m_reader = NULL; // Hack to avoid ifdefs below
#endif

	// Continue with the files collection, which is referenced by several
	// other collections in their initialization.
	PrefsCollectionFiles::CreateL(m_reader)->Into(collections);

	// Now create the other collections, the order should not matter.
	PrefsCollectionNetwork::CreateL(m_reader)->Into(collections);
	PrefsCollectionCore::CreateL(m_reader)->Into(collections);
	PrefsCollectionDisplay::CreateL(m_reader)->Into(collections);
	PrefsCollectionDoc::CreateL(m_reader)->Into(collections);
	PrefsCollectionParsing::CreateL(m_reader)->Into(collections);
	PrefsCollectionFontsAndColors::CreateL(m_reader)->Into(collections);
#if defined GEOLOCATION_SUPPORT
	PrefsCollectionGeolocation::CreateL(m_reader)->Into(collections);
#endif
#ifdef _PRINT_SUPPORT_
	PrefsCollectionPrint::CreateL(m_reader)->Into(collections);
#endif
	PrefsCollectionApp::CreateL(m_reader)->Into(collections);
	PrefsCollectionJS::CreateL(m_reader)->Into(collections);
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	PrefsCollectionDatabase::CreateL(m_reader)->Into(collections);
#endif // DATABASE_MODULE_MANAGER_SUPPORT
#ifdef WEBSERVER_SUPPORT
	PrefsCollectionWebserver::CreateL(m_reader)->Into(collections);
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
	PrefsCollectionTools::CreateL(m_reader)->Into(collections);
#endif
#ifdef SUPPORT_DATA_SYNC
	PrefsCollectionSync::CreateL(m_reader)->Into(collections);
#endif	
#ifdef PREFS_HAVE_DESKTOP_UI
	PrefsCollectionUI::CreateL(m_reader)->Into(collections);
#endif
#ifdef M2_SUPPORT
	PrefsCollectionM2::CreateL(m_reader)->Into(collections);
#endif
#ifdef PREFS_HAVE_MSWIN
	PrefsCollectionMSWIN::CreateL(m_reader)->Into(collections);
#endif
#ifdef PREFS_HAVE_COREGOGI
	PrefsCollectionCoregogi::CreateL(m_reader)->Into(collections);
#endif
#ifdef PREFS_HAVE_UNIX
	PrefsCollectionUnix::CreateL(m_reader)->Into(collections);
#endif
#ifdef PREFS_HAVE_MAC
	PrefsCollectionMacOS::CreateL(m_reader)->Into(collections);
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
	PrefsCollectionOperaAccount::CreateL(m_reader)->Into(collections);
#endif
#ifdef DOM_JIL_API_SUPPORT
	PrefsCollectionJIL::CreateL(m_reader)->Into(collections);
#endif
}

void PrefsManager::ReadAllPrefsL(PrefsModule::PrefsInitInfo *info)
{
#ifdef PREFS_READ
	// Load the INI file into memory; a missing file is not a fatal error,
	// the file will be created whenever the defaults are changed anyway.
	OpStatus::Ignore(m_reader->LoadAllL());
#endif

	// Ask all collections to read their settings
	OpPrefsCollection *first_collection =
		g_opera->prefs_module.GetFirstCollection();

	OpPrefsCollection *p = first_collection;
	while (p)
	{
		p->ReadAllPrefsL(info);
		p = static_cast<OpPrefsCollection *>(p->Suc());
	}

#ifdef PREFS_HOSTOVERRIDE
	PrefsCollectionFiles *pcfiles = g_pcfiles; // cache

	// Create an object for the override.ini file.
	OpStackAutoPtr<PrefsFile> overrideini(OP_NEW_L(PrefsFile, (PREFS_STD)));
	overrideini->ConstructL();
	overrideini->SetFileL(pcfiles->GetFile(PrefsCollectionFiles::OverridesFile));

# ifdef PREFSFILE_CASCADE
#  ifdef PREFSFILE_WRITE_GLOBAL
	// Defaults file is used by prefs download to make sure it doesn't
	// overwrite the settings from the user
	overrideini->SetGlobalFileL(pcfiles->GetFile(PrefsCollectionFiles::DownloadedOverridesFile));
#  endif

	// Global fixed file in the usual location
	OpStackAutoPtr<OpFile> fixedoverridesini(OP_NEW_L(OpFile, ()));
	LEAVE_IF_ERROR(fixedoverridesini->Construct(DEFAULT_OVERRIDES_FILE, OPFILE_FIXEDPREFS_FOLDER));
	if (uni_strcmp(fixedoverridesini->GetSerializedName(),
	               pcfiles->GetFile(PrefsCollectionFiles::OverridesFile)->GetSerializedName()) != 0)
	{
		overrideini->SetGlobalFixedFileL(fixedoverridesini.get());
	}
# endif

	m_overridesreader = overrideini.release();
	m_overridesreader->LoadAllL();

	// Let all the collections know about this reader
	p = first_collection;
	while (p)
	{
		p->SetOverrideReader(m_overridesreader);
		p = static_cast<OpPrefsCollection *>(p->Suc());
	}

	// Read the override section and check for host overrides
	m_overrides = m_overridesreader->ReadSectionInternalL(UNI_L("Overrides"));
	const PrefsEntry *overridehost =
		m_overrides ? m_overrides->Entries() : NULL;
	while (overridehost)
	{
		// Each override entry has the host name in the key. All overrides
		// are stored in a section with the same name as the host, so
		// first we load that section. If the section isn't available,
		// there is no need to try reading the settings.
		OpStackAutoPtr<PrefsSection>
			overrides(m_overridesreader->ReadSectionL(overridehost->Key()));

		if (overrides.get())
		{
			// Check if this override was set by the user.
#ifdef PREFSFILE_WRITE_GLOBAL
			BOOL entered_by_user = m_overridesreader->IsSectionLocal(overridehost->Key());
#else
			const BOOL entered_by_user = TRUE;
#endif

			// Disable flag is stored as the value 0. All other values
			// (including a missing value) means active.
			const uni_char *v = overridehost->Get();
			BOOL active = v == NULL || *v != '0';

			// We pass the host name and the section down to all collections.
			p = first_collection;
			while (p)
			{
				p->ReadOverridesL(overridehost->Key(), overrides.get(), active, entered_by_user);
				p = static_cast<OpPrefsCollection *>(p->Suc());
			}
		}
		overridehost = static_cast<const PrefsEntry *>(overridehost->Suc());
	}

	m_overridesreader->Flush();
#endif // PREFS_HOSTOVERRIDE

	// Specials:
#ifdef PREFS_HAVE_FORCE_CD
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ForceCD))
	{
		char cdletter =
			static_cast<WindowsOpSystemInfo *>(g_op_system_info)->GetCDLetter();

		// Setup home URL
		OpStringC oldhomeurl =
			g_pccore->GetStringPref(PrefsCollectionCore::HomeURL);
		if (!oldhomeurl.IsEmpty() && KNotFound == oldhomeurl.FindFirstOf(':'))
		{
			OpString newurl; ANCHOR(OpString, newurl);
			newurl.ReserveL(oldhomeurl.Length() + 21);
			uni_sprintf(newurl.CStr(), UNI_L("file://localhost/%c:\\%s"), cdletter, oldhomeurl.CStr());
			g_pccore->WriteStringL(PrefsCollectionCore::HomeURL, newurl);
			// oldhomeurl is now invalid
		}
	}	
#endif

	m_initialized = TRUE;
}

#ifdef PREFS_HOSTOVERRIDE
# ifdef PREFS_WRITE
BOOL PrefsManager::RemoveOverridesL(const uni_char *host, BOOL from_user)
{
#  ifdef PREFSFILE_WRITE_GLOBAL
	if (from_user)
#  endif
	{
		// Delete the settings from the [Overrides] section in the INI file
		if (m_overridesreader->DeleteKeyL(UNI_L("Overrides"), host))
		{
			// There was an entry for this host, and we are allowed to remove it,
			// delete the corresponding entries
			m_overridesreader->DeleteSectionL(host);

			// Remove all data from the collections
			OpPrefsCollection *p = g_opera->prefs_module.GetFirstCollection();
			while (p)
			{
				p->RemoveOverridesL(host);
				p = static_cast<OpPrefsCollection *>(p->Suc());
			}

			// Remove the host key from our cache
			if (m_overrides)
			{
				m_overrides->DeleteEntry(host);
			}

			return TRUE;
		}
	}
#  ifdef PREFSFILE_WRITE_GLOBAL
	else
	{
		// FIXME: When deleting global overrides, the deletion does not take
		// affect immediately if there are also local overrides. Need to
		// rewrite the RemoveOverridesL code to be able to handle this case
		if (m_overridesreader->DeleteKeyGlobalL(UNI_L("Overrides"), host))
		{
			if (!m_overridesreader->IsKey(UNI_L("Overrides"), host))
			{
				// There are no longer any overrides for this host, delete it
				// completely.
				m_overridesreader->DeleteSectionGlobalL(host);

				// Remove all data from the collections
				OpPrefsCollection *p = g_opera->prefs_module.GetFirstCollection();
				while (p)
				{
					p->RemoveOverridesL(host);
					p = static_cast<OpPrefsCollection *>(p->Suc());
				}

				if (m_overrides)
				{
					m_overrides->DeleteEntry(host);
				}
			}

			return TRUE;
		}
	}
#  endif

	return FALSE;
}

BOOL PrefsManager::RemoveOverridesAllHostsL(BOOL from_user)
{
	OpString_list* host_list = NULL;
	host_list = GetOverriddenHostsL();

	if (host_list)
	{
		OP_STATUS err;
		OP_MEMORY_VAR OP_STATUS last_err = OpStatus::OK;
		OP_MEMORY_VAR BOOL removed_any = FALSE;
		for (OP_MEMORY_VAR unsigned int i = 0; i < host_list->Count(); i++)
		{
			err = OpStatus::OK;
			OP_MEMORY_VAR BOOL removed_this = FALSE;
			TRAP(err, removed_this = RemoveOverridesL(host_list->Item(i).CStr(), from_user));
			if (OpStatus::IsSuccess(err))
			{
				if (removed_this)
					removed_any = TRUE;
			}
			else
			{
				last_err = err;
			}
		}
		OP_DELETE(host_list);
		CommitL();
		LEAVE_IF_ERROR(last_err);

		return removed_any;
	}

	return FALSE;
}

# endif // PREFS_WRITE

HostOverrideStatus PrefsManager::IsHostOverridden(const uni_char *host, BOOL exact)
{
	// The collections only keep the override objects for the domains
	// where there are overrides for themselves, so we need to iterate
	// and check them all. Also, objects may have different ideas on
	// whether the override was set by the user if it has been modified
	// at run-time, so we need to continue checking if we are told it
	// is a global override.
	OpPrefsCollection *p = g_opera->prefs_module.GetFirstCollection();

	HostOverrideStatus rc = HostNotOverridden;
	while (p)
	{
		HostOverrideStatus status = p->IsHostOverridden(host, exact);
		if (status != HostNotOverridden)
		{
			// Return immediately if we are told the setting is set by the
			// user.
			if (HostOverrideActive == status || HostOverrideDisabled == status)
				return status;

			// Otherwise we cache the value.
			rc = status;
		}
		p = static_cast<OpPrefsCollection *>(p->Suc());
	}

	return rc;
}

size_t PrefsManager::HostOverrideCount(const uni_char *host)
{
	size_t override_count = 0;

	OpPrefsCollection *collection = g_opera->prefs_module.GetFirstCollection();
	for (; collection; collection = static_cast<OpPrefsCollection *>(collection->Suc()))
		override_count += collection->HostOverrideCount(host);

	return override_count;
}

BOOL PrefsManager::SetHostOverrideActiveL(const uni_char *host, BOOL active)
{
	// The collections only keep the override objects for the domains
	// where there are overrides for themselves, so we need to iterate
	// and check them all.
	BOOL found = FALSE;
	OpPrefsCollection *p = g_opera->prefs_module.GetFirstCollection();
	while (p)
	{
		if (p->SetHostOverrideActive(host, active))
		{
			found = TRUE;
		}
		p = static_cast<OpPrefsCollection *>(p->Suc());
	}

#ifdef PREFS_WRITE
	if (found)
	{
		// There is an override recorded for this host, change its status in
		// the preferences file.
		m_overridesreader->WriteIntL(UNI_L("Overrides"), host, active);
	}
#endif

	return found;
}

# ifdef PREFS_GETOVERRIDDENHOSTS
OpString_list *PrefsManager::GetOverriddenHostsL()
{
	return m_overrides->GetKeyListL();
}
# endif

# ifdef PREFS_WRITE
void PrefsManager::OverrideHostAddedL(const uni_char *host)
{
	// Add the new host to the list
	if (!m_overrides->FindEntry(host))
	{
		m_overrides->NewEntryL(host, NULL);
	}
}
# endif
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_HAVE_STRING_API
BOOL PrefsManager::GetPreferenceL(
	const char *section, const char *key, OpString &target,
	BOOL defval, const uni_char *host)
{
	// Check validity of parameters
	if (!section || !key)
	{
		LEAVE(OpStatus::ERR_NULL_POINTER);
	}
	OP_ASSERT(!defval || !host); // Can't get a default value for a host

	OpPrefsCollection::IniSection section_id =
		OpPrefsCollection::SectionStringToNumber(section);

	// Try reading the preference from each of the collections in turn
	OpPrefsCollection *p = g_opera->prefs_module.GetFirstCollection();
	while (p)
	{
		if (p->GetPreferenceL(section_id, key, target, defval, host))
			return TRUE;

		p = static_cast<OpPrefsCollection *>(p->Suc());
	}
	return FALSE;
}
#endif // PREFS_HAVE_STRING_API

#if defined PREFS_HAVE_STRING_API && defined PREFS_WRITE
BOOL PrefsManager::WritePreferenceL(const char *section, const char *key, const OpStringC value)
{
	// Check validity of parameters (value may be NULL)
	if (!section || !key)
	{
		LEAVE(OpStatus::ERR_NULL_POINTER);
	}

	OpPrefsCollection::IniSection section_id =
		OpPrefsCollection::SectionStringToNumber(section);

	// Try writing the preference to each of the collections in turn
	OpPrefsCollection *p = g_opera->prefs_module.GetFirstCollection();
	while (p)
	{
		if (p->WritePreferenceL(section_id, key, value))
			return TRUE;

		p = static_cast<OpPrefsCollection *>(p->Suc());
	}
	return FALSE;
}

# ifdef PREFS_HOSTOVERRIDE
BOOL PrefsManager::OverridePreferenceL(
	const uni_char *host, const char *section, const char *key,
	const OpStringC value, BOOL from_user)
{
	// Check validity of parameters (value may be NULL)
	if (!host || !section || !key)
	{
		LEAVE(OpStatus::ERR_NULL_POINTER);
	}

	OpPrefsCollection::IniSection section_id =
		OpPrefsCollection::SectionStringToNumber(section);

	// Try writing the preference to each of the collections in turn
	OpPrefsCollection *p = g_opera->prefs_module.GetFirstCollection();
	while (p)
	{
		if (p->OverridePreferenceL(host, section_id, key, value, from_user))
			return TRUE;

		p = static_cast<OpPrefsCollection *>(p->Suc());
	}
	return FALSE;
}

BOOL PrefsManager::RemoveOverrideL(const uni_char *host, const char *section,
								   const char *key, BOOL from_user)
{
	if (!section || !key)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	OpPrefsCollection::IniSection section_id =
		OpPrefsCollection::SectionStringToNumber(section);

	BOOL override_removed = FALSE;
	OpPrefsCollection *p = g_opera->prefs_module.GetFirstCollection();
	while (p)
	{
		override_removed |= p->RemoveOverrideL(host, section_id, key, from_user);
		p = static_cast<OpPrefsCollection *>(p->Suc());
	}

	return override_removed;
}
# endif
#endif // PREFS_HAVE_STRING_API && PREFS_WRITE

#ifdef PREFS_ENUMERATE
prefssetting *PrefsManager::GetPreferencesL(BOOL sort, unsigned int &length) const
{
	const OpPrefsCollection *first = g_opera->prefs_module.GetFirstCollection();

	// Count
	unsigned int numprefs = 0;
	for (const OpPrefsCollection *p = first; p;
	     p = static_cast<OpPrefsCollection *>(p->Suc()))
	{
		numprefs += p->GetNumberOfPreferences();
	}

	// Fetch
	prefssetting *settings;
	settings = OP_NEWA_L(prefssetting, numprefs);
	ANCHOR_ARRAY(prefssetting, settings);
	prefssetting *cur = settings;
	for (const OpPrefsCollection *q = first; q;
	     q = static_cast<OpPrefsCollection *>(q->Suc()))
	{
		cur += q->GetPreferencesL(cur);
	}
	OP_ASSERT(cur == settings + numprefs); // Sanity check

	// Sort
	if (sort)
	{
		op_qsort(settings, numprefs, sizeof (prefssetting), GetPreferencesSort);
	}

#ifdef PREFSFILE_CASCADE
	// Flag which settings user is allowed to change; do this after
	// sorting to optimize PrefsMap lookups.
	for (prefssetting *r = settings; r < cur; ++ r)
	{
		r->enabled = m_reader->AllowedToChangeL(r->section, r->key);
	}
#endif

	length = numprefs;
	ANCHOR_ARRAY_RELEASE(settings);
	return settings;
}

/** Helper for GetPreferencesL */
/*static*/ int PrefsManager::GetPreferencesSort(const void *p1, const void *p2)
{
	int diff =
		op_strcmp(reinterpret_cast<const prefssetting *>(p1)->section,
		          reinterpret_cast<const prefssetting *>(p2)->section);
	if (!diff)
	{
		diff =
			op_strcmp(reinterpret_cast<const prefssetting *>(p1)->key,
			          reinterpret_cast<const prefssetting *>(p2)->key);
	}
	return diff;
}
#endif
