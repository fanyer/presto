/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

/**
 * @file opprefscollection.cpp
 * Implementation of OpPrefsCollection.
 *
 * This file contains an implementation of all the non-abstract parts of the
 * OpPrefsCollection interface.
 */

#include "core/pch.h"

#include "modules/prefs/prefsmanager/opprefscollection.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h" // For PREFSMAP_USE_CASE_SENSITIVE
#include "modules/prefs/prefsmanager/prefsinternal.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

// OpPrefsCollection implementation ---------------------------------------

#ifdef PREFS_READ
INITSECTIONS(OPPREFSCOLLECTION_NUMBEROFSECTIONS)
{
	INITSTART
	K("User Prefs"),
	K("Parsing"),
	K("Network"),
	K("Cache"),
	K("Disk Cache"),
	K("Extensions"),
#if defined PREFS_HAVE_MAIL_APP || defined PREFS_HAVE_NEWS_APP || defined M2_SUPPORT
	K("Mail"),
#endif
#ifdef PREFS_HAVE_NEWS_APP
	K("News"),
#endif
#if defined DYNAMIC_VIEWERS && defined UPGRADE_SUPPORT
	K("File Types Section Info"),
#endif
	K("CSS Generic Font Family"),
	K("Link"),
	K("Visited Link"),
	K("System"),
	K("Multimedia"),
	K("User Display Mode"),
	K("Author Display Mode"),
	K("RM1"),
	K("RM2"),
	K("RM3"),
	K("RM4"),
#ifdef TV_RENDERING
	K("RM5"),
#endif
	K("Handheld"),
	K("Special"),
	K("ISP"),
#ifdef PREFS_HAVE_DESKTOP_UI
	K("HotListWindow"),
#endif
	K("Proxy"),
	K("Performance"),
#ifdef _SSL_USE_SMARTCARD_
	K("SmartCards"),
#endif
	K("User Agent"),
	K("Security Prefs"),
#ifdef _PRINT_SUPPORT_
	K("Printer"),
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
	K("Brand"),
#endif
#ifdef PREFS_HAVE_WORKSPACE
	K("Workspace"),
#endif
#ifdef PREFS_HAVE_FILESELECTOR
	K("File Selector"),
#endif
	K("Personal Info"),
#if defined PREFS_HAVE_DESKTOP_UI || defined PREFS_DOWNLOAD
	K("Install"),
#endif
#if defined PREFS_HAVE_DESKTOP_UI || defined PREFS_HAVE_UNIX_PLUGIN
	K("State"),
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
	K("TransferWindow"),
	K("Clear Private Data Dialog"),
	K("Interface Colors"),	
	K("Sounds"),
#endif
#ifdef _FILE_UPLOAD_SUPPORT_
	K("Saved Settings"),
#endif
#ifdef HELP_SUPPORT
	K("Help Window"),
#endif
#ifdef PREFS_HAVE_HOTLIST_EXTRA
	K("MailBox"),
#endif
#ifdef GADGET_SUPPORT
	K("Widgets"),
#endif
#if defined LOCAL_CSS_FILES_SUPPORT && !defined PREFS_USE_CSS_FOLDER_SCAN
	K("Local CSS Files"),
#endif
#ifdef WEBSERVER_SUPPORT
	K("Web Server"),
#endif
#ifdef SVG_SUPPORT
	K("SVG"),
#endif
#ifdef _BITTORRENT_SUPPORT_
	K("BitTorrent"),
#endif
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	K("OEM"),
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
	K("Developer Tools"),
#endif
#if defined GEOLOCATION_SUPPORT
	K("Geolocation"),
#endif
#if defined DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	K("Orientation"),
#endif
#ifdef SUPPORT_DATA_SYNC
	K("Opera Sync"),
	K("Opera Sync Server"),
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
	K("Opera Account"),
#endif
#ifdef AUTO_UPDATE_SUPPORT
	K("Auto Update"),
#endif
	K("Colors"),
	K("Fonts")
#if defined(PREFS_HAVE_APP_INFO)
	,
	K("App Info")
#endif
#if defined CLIENTSIDE_STORAGE_SUPPORT || defined DATABASE_STORAGE_SUPPORT
	,
	K("Persistent Storage")
#endif
#ifdef DOM_JIL_API_SUPPORT
	,
	K("JIL API")
#endif
#ifdef PREFS_HAVE_HBBTV
	,
	K("HbbTV")
#endif
#ifdef UPGRADE_SUPPORT
	,
	K("Trusted Protocols Section Info")
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
	,
	K("Collection View")
#endif
	INITEND
};

# ifndef HAS_COMPLEX_GLOBALS
/*static*/ const char *OpPrefsCollection::SectionNumberToString(IniSection section)
{
	// Cannot have static data, so need to find an instance of us that
	// have initialized the data.
	return static_cast<OpPrefsCollection *>(g_opera->prefs_module.m_collections->First())
				->SectionNumberToStringInternal(section);
}
# endif
#endif // PREFS_READ

OpPrefsCollection::OpPrefsCollection(enum Collections identity, PrefsFile *reader)
	: Link(),
	  m_identity(identity), m_listeners(),
#ifdef PREFS_READ
	  m_reader(reader),
#endif
	  m_stringprefs(NULL), m_intprefs(NULL)
#ifdef PREFS_COVERAGE
	, m_stringprefused(NULL), m_intprefused(NULL)
#endif
{
#ifndef HAS_COMPLEX_GLOBALS
# ifdef PREFS_READ
	InitSections();
# endif
#endif
}

OpPrefsCollection::~OpPrefsCollection()
{
	// Are there any listeners still registered?
	m_listeners.Clear();

	OP_DELETEA(m_stringprefs);
	OP_DELETEA(m_intprefs);

#ifdef PREFS_COVERAGE
	OP_DELETEA(m_stringprefused);
	OP_DELETEA(m_intprefused);
#endif
}

OP_STATUS OpPrefsCollection::RegisterListener(OpPrefsListener *listener)
{
	// Don't do anything if we already have it registered
	if (NULL != GetContainerFor(listener))
		return OpStatus::OK;

	// Encapsulate the OpPrefsListener in a ListenerContainer and put it into
	// our list.
	ListenerContainer *new_container = OP_NEW(ListenerContainer, (listener));
	if (!new_container)
		return OpStatus::ERR_NO_MEMORY;

	new_container->IntoStart(&m_listeners);
	return OpStatus::OK;
}

void OpPrefsCollection::RegisterListenerL(OpPrefsListener *listener)
{
	LEAVE_IF_ERROR(RegisterListener(listener));
}

void OpPrefsCollection::UnregisterListener(OpPrefsListener *listener)
{
	// Find the Listener object corresponding to this OpPrefsListener
	ListenerContainer *container = GetContainerFor(listener);

	if (NULL == container)
	{
		// Don't do anything if we weren't registered
		return;
	}

	container->Out();
	OP_DELETE(container);
}

OpPrefsCollection::ListenerContainer *
	OpPrefsCollection::GetContainerFor(const OpPrefsListener *which)
{
	Link *p = m_listeners.First();
	while (p && p->LinkId() != reinterpret_cast<const char *>(which))
	{
		p = p->Suc();
	}

	// Either the found object, or NULL
	return static_cast<ListenerContainer *>(p);
}

#ifdef PREFS_WRITE
void OpPrefsCollection::BroadcastChange(int pref, int newvalue)
{
	ListenerContainer *p =
		static_cast<ListenerContainer *>(m_listeners.First());
	while (p)
	{
		OP_ASSERT(p->LinkId() != NULL);
		p->GetListener()->PrefChanged(m_identity, pref, newvalue);

		p = static_cast<ListenerContainer *>(p->Suc());
	}
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
void OpPrefsCollection::BroadcastChange(int pref, const OpStringC &newvalue)
{
	ListenerContainer *p =
		static_cast<ListenerContainer *>(m_listeners.First());
	while (p)
	{
		OP_ASSERT(p->LinkId() != NULL);
		p->GetListener()->PrefChanged(m_identity, pref, newvalue);

		p = static_cast<ListenerContainer *>(p->Suc());
	}
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
OP_STATUS OpPrefsCollection::WriteIntegerL(
	const struct integerprefdefault *pref, int which, int value)
{
# ifdef PREFS_COVERAGE
	++ m_intprefused[which];
# endif

	if (m_intprefs[which] == value)
		return OpStatus::OK;

# ifdef PREFS_VALIDATE
	CheckConditionsL(which, &value, NULL);
# endif
# ifdef PREFS_READ
	OP_STATUS rc = m_reader->WriteIntL(m_sections[pref->section], pref->key, value);
# else
	const OP_STATUS rc = OpStatus::OK;
# endif

	if (OpStatus::IsSuccess(rc))
	{
		m_intprefs[which] = value;
		BroadcastChange(which, value);
	}

	return rc;
}
#endif // PREFS_WRITE

#ifdef PREFS_WRITE
OP_STATUS OpPrefsCollection::WriteStringL(
	const struct stringprefdefault *pref, int which, const OpStringC &value)
{
# ifdef PREFS_COVERAGE
	++ m_stringprefused[which];
# endif

	if (0 == m_stringprefs[which].Compare(value))
		return OpStatus::OK;

	OP_STATUS rc;
# ifdef PREFS_VALIDATE
	OpString *newvalue;
	if (CheckConditionsL(which, value, &newvalue, NULL))
	{
		OpStackAutoPtr<OpString> anchor(newvalue);
#  ifdef PREFS_READ
		rc = m_reader->WriteStringL(m_sections[pref->section], pref->key, *newvalue);
#  else
		rc = OpStatus::OK;
#  endif

		if (OpStatus::IsSuccess(rc))
		{
			m_stringprefs[which].SetL(*newvalue);
			BroadcastChange(which, *newvalue);
		}

		anchor.release();
		OP_DELETE(newvalue);
	}
	else
# endif // PREFS_VALIDATE
	{
# ifdef PREFS_READ
		rc = m_reader->WriteStringL(m_sections[pref->section], pref->key, value);
# else
		rc = OpStatus::OK;
# endif

		if (OpStatus::IsSuccess(rc))
		{
			m_stringprefs[which].SetL(value);
			BroadcastChange(which, value);
		}
	}

	return rc;
}

BOOL OpPrefsCollection::ResetStringL(const struct stringprefdefault *pref, int which)
{
#ifdef PREFS_COVERAGE
	++ m_stringprefused[which];
#endif

#ifdef PREFS_READ
	BOOL is_deleted = m_reader->DeleteKeyL(m_sections[pref->section], pref->key);
#else
	const BOOL is_deleted = TRUE;
#endif

	if (is_deleted && m_stringprefs[which].Compare(GetDefaultStringInternal(which, pref)) != 0)
	{
		m_stringprefs[which].SetL(GetDefaultStringInternal(which, pref));
		BroadcastChange(which, m_stringprefs[which]);
	}
	return is_deleted;
}

BOOL OpPrefsCollection::ResetIntegerL(const struct integerprefdefault *pref, int which)
{
#ifdef PREFS_COVERAGE
	++ m_intprefused[which];
#endif

#ifdef PREFS_READ
	BOOL is_deleted = m_reader->DeleteKeyL(m_sections[pref->section], pref->key);
#else
	const BOOL is_deleted = TRUE;
#endif

	if (is_deleted && m_intprefs[which] != GetDefaultIntegerInternal(which, pref))
	{
		m_intprefs[which] = GetDefaultIntegerInternal(which, pref);
		BroadcastChange(which, m_intprefs[which]);
	}
	return is_deleted;
}
#endif // PREFS_WRITE

void OpPrefsCollection::ReadAllPrefsL(
	const struct stringprefdefault *strings, int numstrings,
	const struct integerprefdefault *integers, int numintegers)
{
	if (strings)
	{
#if defined _DEBUG
		// Sanity check to see that the number of elements match, i.e
		// check that the last element in the array is the sentinel.
		// If this assert catches you, you have too many elements in
		// the prefdefault structure.
# ifdef PREFS_READ
		OP_ASSERT(strings[numstrings].section == SNone &&
		          strings[numstrings].key     == NULL);
# endif
		OP_ASSERT(strings[numstrings].defval  == NULL);

		// Sanity check to see that we are never called twice
		OP_ASSERT(!m_stringprefs);
#endif

		if (numstrings)
			m_stringprefs = OP_NEWA_L(OpString, numstrings);
#ifdef PREFS_COVERAGE
		if (numstrings)
			m_stringprefused = OP_NEWA_L(int, numstrings);
		op_memset(m_stringprefused, 0, sizeof (int) * numstrings);
#endif

		for (int i = 0; i < numstrings; i ++)
		{
#ifdef PREFS_READ
# ifdef _DEBUG
			// Sanity check to see that the number of elements match
			// if this assert catches you, you have too few elements in
			// m_stringprefdefaults
			OP_ASSERT(strings[i].section != SNone && strings[i].key);
# endif

			m_reader->ReadStringL(m_sections[strings[i].section],
			                      strings[i].key, m_stringprefs[i],
			                      strings[i].defval);

# ifdef PREFS_VALIDATE
			OpString *newvalue;
			if (CheckConditionsL(i, m_stringprefs[i], &newvalue, NULL))
			{
				// Move new value over to the current string object
				m_stringprefs[i].TakeOver(*newvalue);
				OP_DELETE(newvalue);
			}
# endif // PREFS_VALIDATE
#else
			// Only use default
			m_stringprefs[i].SetL(strings[i].defval);
#endif
		}
	}

	if (integers)
	{
#if defined _DEBUG && defined PREFS_READ
		// Sanity check to see that the number of elements match, i.e
		// check that the last element in the array is the sentinel.
		// If this assert catches you, you have too many elements in
		// the prefdefault structure.
		OP_ASSERT(integers[numintegers].section == SNone &&
		          integers[numintegers].key     == NULL);

		// Sanity check to see that we are never called twice
		OP_ASSERT(!m_intprefs);
#endif

		if (numintegers)
			m_intprefs = OP_NEWA_L(int, numintegers);
#ifdef PREFS_COVERAGE
		if (numintegers)
			m_intprefused = OP_NEWA_L(int, numintegers);
		op_memset(m_intprefused, 0, sizeof (int) * numintegers);
#endif

		for (int i = 0; i < numintegers; i ++)
		{
#ifdef PREFS_READ
# ifdef _DEBUG
			// Sanity check to see that the number of elements match
			// if this assert catches you, you have too few elements in
			// m_integerprefdefaults
			OP_ASSERT(integers[i].section != SNone && integers[i].key);
# endif

			m_intprefs[i] =
				m_reader->ReadIntL(m_sections[integers[i].section],
				                   integers[i].key,
				                   integers[i].defval);

# ifdef PREFS_VALIDATE
			CheckConditionsL(i, &m_intprefs[i], NULL);
# endif
#else
			// Only use default
			m_intprefs[i] = integers[i].defval;
#endif
		}
	}
}

#ifdef PREFS_READ
enum OpPrefsCollection::IniSection OpPrefsCollection::SectionStringToNumber(const char *section)
{
	if (section)
	{
		for (int i = 0; i < OPPREFSCOLLECTION_NUMBEROFSECTIONS; ++ i)
		{
#ifdef PREFSMAP_USE_CASE_SENSITIVE
			if (0 == STRINGAPI_STRCMP(SectionNumberToString(static_cast<IniSection>(i)), section))
#else
			if (0 == op_stricmp(SectionNumberToString(static_cast<IniSection>(i)), section))
#endif
			{
				return OpPrefsCollection::IniSection(i);
			}
		}
	}
	return OpPrefsCollection::SNone;
}
#endif


#ifdef PREFS_HAVE_STRING_API
/**
 * Retrieve the index of an integer preference referenced by its name.
 * @return -1 on error.
 */
int OpPrefsCollection::GetIntegerPrefIndex(
	IniSection section, const char *key,
	const struct integerprefdefault *integers, int numintegers)
{
	for (int i = 0; i < numintegers; ++ i)
	{
		if (integers[i].section == section && 0 == STRINGAPI_STRCMP(integers[i].key, key))
		{
			return i;
		}
	}

	return -1;
}

/**
 * Retrieve the index of an string preference referenced by its name.
 * @return -1 on error.
 */
int OpPrefsCollection::GetStringPrefIndex(
	IniSection section, const char *key,
	const struct stringprefdefault *strings, int numstrings)
{
	for (int i = 0; i < numstrings; ++ i)
	{
		if (strings[i].section == section && 0 == STRINGAPI_STRCMP(strings[i].key, key))
		{
			return i;
		}
	}

	return -1;
}
#endif

#if defined PREFS_HAVE_STRING_API || defined PREFS_WRITE
const uni_char *OpPrefsCollection::GetDefaultStringInternal(int which, const struct stringprefdefault *pref)
{
	return pref->defval;
}

int OpPrefsCollection::GetDefaultIntegerInternal(int which, const struct integerprefdefault *pref)
{
	return pref->defval;
}
#endif // PREFS_HAVE_STRING_API

#if defined PREFS_HAVE_STRING_API && defined PREFS_WRITE
BOOL OpPrefsCollection::WritePreferenceInternalL(IniSection section, const char *key, const OpStringC &value,
	const struct stringprefdefault *strings, int numstrings,
	const struct integerprefdefault *integers, int numintegers)
{
	int i = GetStringPrefIndex(section, key, strings, numstrings);
	if (i >= 0)
	{
		return OpStatus::IsSuccess(WriteStringL(&strings[i], i, value));
	}

	int j = GetIntegerPrefIndex(section, key, integers, numintegers);
	if (j >= 0)
	{
		// Need to convert to an integer. API doc says we do a LEAVE if
		// "value" does not contain a proper integer.
		uni_char *endptr;
		if (value.IsEmpty())
		{
			LEAVE(OpStatus::ERR_OUT_OF_RANGE);
		}
		long intval = uni_strtol(value.CStr(), &endptr, 0);
		if (*endptr)
		{
			LEAVE(OpStatus::ERR_OUT_OF_RANGE);
		}

		return OpStatus::IsSuccess(WriteIntegerL(&integers[j], j,
		                                        static_cast<int>(intval)));
	}

	return FALSE;
}
#endif // PREFS_HAVE_STRING_API && PREFS_WRITE

#ifdef PREFS_ENUMERATE
unsigned int OpPrefsCollection::GetPreferencesInternalL(struct prefssetting *settings,
	const struct stringprefdefault *strings, int numstrings,
	const struct integerprefdefault *integers, int numintegers) const
{
	prefssetting *dest = settings;

	// Copy string values
	if (strings)
	{
		OpString *cur = m_stringprefs;

		while (numstrings)
		{
			dest->section = m_sections[strings->section];
			dest->key     = strings->key;
			dest->type    = prefssetting::string;
			dest->value.SetL(*cur);

			++ dest;
			++ strings;
			++ cur;
			-- numstrings;
		}
	}

	// Copy integer values
	if (integers)
	{
		// We need log(256)/log(10) * sizeof(value) characters to store
		// the number, adding one for the rounding, one for the sign
		// and one for the terminator. 53/22 is a good approximation
		// of this.
			
		static const int buflen = 3 + sizeof (int) * 53 / 22;
		char buf[buflen]; /* ARRAY OK 2009-04-23 adame */

		int *cur = m_intprefs;

		while (numintegers)
		{
			dest->section = m_sections[integers->section];
			dest->key     = integers->key;
			dest->type    = integers->type;

			op_itoa(*cur, buf, 10);
			dest->value.SetL(buf);

			++ dest;
			++ integers;
			++ cur;
			-- numintegers;
		}
	}

	return dest - settings;
}
#endif

#ifdef PREFS_COVERAGE
#include "modules/util/opfile/opfile.h"
void OpPrefsCollection::CoverageReportPrint(const char *string)
{
#ifdef DEBUG
	OpFile f;
	f.Construct(UNI_L("prefscoverage.txt"), OPFILE_HOME_FOLDER);
	f.Open(OPFILE_APPEND);
	f.Write(string, op_strlen(string));
	f.Close();
#endif
}

void OpPrefsCollection::CoverageReport(
	const struct stringprefdefault *strings, int numstrings,
	const struct integerprefdefault *integers, int numintegers)
{
	// Report all unreferenced preferences
	CoverageReportPrint("\nPreference coverage report for collection ");
	switch (m_identity)
	{
	case Core:				CoverageReportPrint("Core\n"); break;
	case Network:			CoverageReportPrint("Network\n"); break;
	case Display:			CoverageReportPrint("Display\n"); break;
	case Doc:				CoverageReportPrint("Doc\n"); break;
	case FontsAndColors:	CoverageReportPrint("FontsAndColors\n"); break;
	case Print:				CoverageReportPrint("Print\n"); break;
	case App:				CoverageReportPrint("App\n"); break;
	case Files:				CoverageReportPrint("Files\n"); break;
	case JS:				CoverageReportPrint("JS\n"); break;
#ifdef WEBSERVER_SUPPORT
	case Webserver:			CoverageReportPrint("Webserver\n"); break;
#endif
#ifdef PREFS_HAVE_DESKTOP_UI
	case UI:				CoverageReportPrint("UI\n"); break;
#endif
#ifdef M2_SUPPORT
	case M2:				CoverageReportPrint("M2\n"); break;
#endif
#ifdef PREFS_HAVE_MSWIN
	case MSWin:				CoverageReportPrint("MSWin\n"); break;
#endif
#ifdef PREFS_HAVE_COREGOGI
	case Coregogi:			CoverageReportPrint("Coregogi\n"); break;
#endif
#ifdef PREFS_HAVE_UNIX
	case Unix:				CoverageReportPrint("Unix\n"); break;
#endif
#ifdef PREFS_HAVE_MAC
	case MacOS:				CoverageReportPrint("MacOS\n"); break;
#endif
#if defined SCOPE_SUPPORT || defined SUPPORT_DEBUGGING_SHELL
	case Tools:				CoverageReportPrint("Tools\n"); break;
#endif
#if defined GEOLOCATION_SUPPORT
	case Geolocation:		CoverageReportPrint("Geolocation\n"); break;
#endif
#ifdef SUPPORT_DATA_SYNC
	case Link:				CoverageReportPrint("Link\n"); break;
#endif
#ifdef PREFS_HAVE_OPERA_ACCOUNT
	case OperaAccount:		CoverageReportPrint("OperaAccount\n"); break;
#endif
	}

	if (m_stringprefused && numstrings)
	{
		BOOL all = TRUE;
		CoverageReportPrint("* String preferences:\n");
		for (int i = 0; i < numstrings; ++ i)
		{
			if (!m_stringprefused[i])
			{
				CoverageReportPrint("  \"");
				CoverageReportPrint(m_sections[strings[i].section]);
				CoverageReportPrint("\".\"");
				CoverageReportPrint(strings[i].key);
				CoverageReportPrint("\" is unreferenced\n");
				all = FALSE;
			}
		}
		if (all)
		{
			CoverageReportPrint("  All string preferences referenced\n");
		}
	}
	else
	{
		CoverageReportPrint("* There are no string preferences\n");
	}

	if (m_intprefused && numintegers)
	{
		BOOL all = TRUE;
		CoverageReportPrint("* Integer preferences:\n");
		for (int i = 0; i < numintegers; ++ i)
		{
			if (!m_intprefused[i])
			{
				CoverageReportPrint("  \"");
				CoverageReportPrint(m_sections[integers[i].section]);
				CoverageReportPrint("\".\"");
				CoverageReportPrint(integers[i].key);
				CoverageReportPrint("\" is unreferenced\n");
				all = FALSE;
			}
		}
		if (all)
		{
			CoverageReportPrint("  All integer preferences referenced\n");
		}
	}
	else
	{
		CoverageReportPrint("* There are no integer preferences\n");
	}
}
#endif
