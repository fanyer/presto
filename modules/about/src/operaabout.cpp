/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Peter Krefting, Edward Welbourne.
 */
#include "core/pch.h"

#ifndef NO_URL_OPERA
#include "modules/about/operaabout.h"
#include "modules/about/operacredits.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/opfile/opfile.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/url/uamanager/ua.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/sqlite/sqlite3.h" // To get the SQLite version.
#ifdef VEGA_3DDEVICE
# include "modules/libvega/vega3ddevice.h"
#endif // VEGA_3DDEVICE

#ifdef ABOUT_MOBILE_ABOUT
# include "modules/about/operaversion.h"
#endif

// Uncomment this to display *all* attributions (e.g. for legal to review):
// #define SHOW_ALL_ATTRIBUTIONS

// Acknowledgements
#if (defined ABOUT_DESKTOP_ABOUT && defined QUICK && !defined LIBOPERA) || defined SHOW_ALL_ATTRIBUTIONS
# define ABOUT_ELEKTRANS 1 /* The Elektrans. */
#endif

// Desktop implementation ---

#ifdef ABOUT_DESKTOP_ABOUT
OP_STATUS OperaAbout::GenerateData()
{
	// Initialize, open HEAD and write TITLE and stylesheet link
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::SI_IDABOUT_ABOUT, PrefsCollectionFiles::StyleAboutFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::SI_IDABOUT_ABOUT));
#endif

	// Close HEAD, open BODY and write the top H1
	RETURN_IF_ERROR(OpenBody(Str::SI_IDABOUT_ABOUT));

	// Write info block. Each of these should close exactly the HTML tags it opens:
	RETURN_IF_ERROR(WriteVersion());
	RETURN_IF_ERROR(WriteUA());
	RETURN_IF_ERROR(WritePaths());
//	RETURN_IF_ERROR(WriteSound());
	RETURN_IF_ERROR(WriteCredits());

	// Dedication and copyright
	OpString line;
	line.Reserve(256);
	RETURN_IF_ERROR(line.Set("\n\n <address"));
	RETURN_IF_ERROR(line.Append(m_rtl ? " dir=\"ltr\">" : ">"));
#ifdef GEIR_MEMORIAL
	RETURN_IF_ERROR(line.Append(m_rtl ? "<span dir=\"rtl\">" : "<span>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_DEDICATED_TO));
	RETURN_IF_ERROR(line.Append("</span>\n "));
#endif
	RETURN_IF_ERROR(line.Append("Copyright \xA9 1995-"));
	RETURN_IF_ERROR(line.Append(ABOUT_CURRENT_YEAR));
	RETURN_IF_ERROR(line.Append(" Opera Software ASA. All rights reserved.</address>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));

	// Close BODY and HTML elements and finish off.
	return CloseDocument();
}
#endif


// Helpers for creating lists ---

#if defined ABOUT_DESKTOP_ABOUT || defined ABOUT_MOBILE_ABOUT
OP_STATUS OperaAbout::OpenList()
{
	return m_url.WriteDocumentData(URL::KNormal, UNI_L(" <dl>\n"));
}

# ifdef ABOUT_DESKTOP_ABOUT
OP_STATUS OperaAbout::ListEntry(Str::LocaleString header, OpFileFolder data)
{
	OpFile folder;
	OpString data_folder_name;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(data, data_folder_name));
	RETURN_IF_ERROR(folder.Construct(data_folder_name));
	return ListEntry(header, &folder);
}

OP_STATUS OperaAbout::ListEntry(Str::LocaleString header, const OpFile *data)
{
	OpString localized_path;
	RETURN_IF_ERROR(data->GetLocalizedPath(&localized_path));
	return ListEntry(header, localized_path);
}
# endif // ABOUT_DESKTOP_ABOUT

OP_STATUS OperaAbout::ListEntry(Str::LocaleString header, const OpStringC &data)
{
	OpString locale_header;
	RETURN_IF_ERROR(g_languageManager->GetString(header, locale_header));
	return ListEntry(locale_header, data);
}

#if defined _PLUGIN_SUPPORT_ || defined USER_JAVASCRIPT
OP_STATUS OperaAbout::ListEntry(Str::LocaleString header, const OpStringC &data, uni_char itemsep, uni_char tokensep)
{
	if (data.IsEmpty())
		return OpStatus::OK;

	// Split a list of paths at "itemsep" boundaries, and localize them.
	// Optionally split each item at "tokensep" boundaries and only
	// display and localize the first part.

	OpString displaypathlist, originaldata, localizedcomponent;

	// Need to work on a copy
	RETURN_IF_ERROR(originaldata.Set(data));

	localizedcomponent.Reserve(128); // Arbitrary, grows if needed, keep outside loop
	displaypathlist.Reserve(originaldata.Length() + 32); // Arbitrary, to avoid too many allocations

	uni_char *p = originaldata.CStr();
	uni_char *next_p;
	do
	{
		// Find next item separator, and delimit by nul
		next_p = uni_strchr(p, itemsep);
		if (next_p)
			*next_p = 0;

		// Kill extra token, if available
		if (tokensep)
		{
			uni_char *token_p = uni_strchr(p, tokensep);
			if (token_p)
				*token_p = 0;
		}

		// Now get the localized path for this string
		OpFile component;
		RETURN_IF_ERROR(component.Construct(p, OPFILE_SERIALIZED_FOLDER));
		RETURN_IF_ERROR(component.GetLocalizedPath(&localizedcomponent));
		RETURN_IF_ERROR(displaypathlist.AppendFormat(UNI_L("%s\n"), localizedcomponent.CStr()));

		p = next_p + 1;
	} while (next_p != NULL);

	return ListEntry(header, displaypathlist);
}
#endif

OP_STATUS OperaAbout::ListEntry(const OpStringC &header, const OpStringC &data)
{
	OpString line;
	line.Reserve(256);
	RETURN_IF_ERROR(line.Set("  <dt>"));
	RETURN_IF_ERROR(line.Append(header));
	RETURN_IF_ERROR(line.Append("</dt><dd>"));
	const uni_char *p = data.CStr();
	if (p)
	{
# ifdef ABOUT_DESKTOP_ABOUT // Only desktop uses multi-line entries
		const uni_char* q;
		while ((q = uni_strchr(p, '\n')) != NULL)
		{
			RETURN_IF_ERROR(AppendHTMLified(&line, p, q - p));
			p = q + 1;
			if (*p)
			{
				RETURN_IF_ERROR(line.Append("</dd><dd>"));
			}
		}
# endif
		RETURN_IF_ERROR(AppendHTMLified(&line, p, uni_strlen(p)));
	}
	RETURN_IF_ERROR(line.Append("</dd>\n"));
	return m_url.WriteDocumentData(URL::KNormal, line);
}

OP_STATUS OperaAbout::CloseList()
{
	return m_url.WriteDocumentData(URL::KNormal, UNI_L(" </dl>\n\n"));
}
#endif // ABOUT_DESKTOP_ABOUT || ABOUT_MOBILE_ABOUT


// Internals for the desktop about screen ---
#ifdef ABOUT_DESKTOP_ABOUT
OP_STATUS OperaAbout::WriteVersion()
{
	RETURN_IF_ERROR(Heading(Str::SI_IDSTR_ABT_VER_INFO_STR));
	RETURN_IF_ERROR(OpenList());

	// Version number
	OpString versionnr;
	RETURN_IF_ERROR(GetVersion(&versionnr));
	RETURN_IF_ERROR(ListEntry(Str::SI_IDSTR_ABT_VER_STR, versionnr));

	// Build number
	OpString buildnr;
	RETURN_IF_ERROR(GetBuild(&buildnr));
	RETURN_IF_ERROR(ListEntry(Str::SI_IDSTR_ABT_BUILD_STR, buildnr));

	// Platform (i.e "Linux" or "Win32") and version (i.e "Windows XP" or "2.6.15")
	OpString platform, version;
	RETURN_IF_ERROR(GetPlatform(&platform, &version));
	RETURN_IF_ERROR(ListEntry(Str::SI_IDSTR_ABT_PLATFORM_STR, platform));
	RETURN_IF_ERROR(ListEntry(Str::SI_IDSTR_ABT_WIN_VER, version));

	// Toolkit (i.e Qt)
	RETURN_IF_ERROR(WriteToolkit());

	// FIXME: Consider adding method for platform-defined stuff?

	return CloseList();
}

OP_STATUS OperaAbout::WritePaths()
{
	RETURN_IF_ERROR(Heading(Str::SI_IDABOUT_OPERASYSINFO));
	RETURN_IF_ERROR(OpenList());

#ifdef PREFS_READ
	// Preferences file
	OpString inifile;
	RETURN_IF_ERROR(GetPreferencesFile(&inifile));
	RETURN_IF_ERROR(ListEntry(Str::SI_IDABOUT_SETTINGS, inifile));
#endif

#ifdef AUTOSAVE_WINDOWS
	// Saved session file
	{
		const OpFile *f = g_pcfiles->GetFile(PrefsCollectionFiles::AutosaveWindowsStorageFile);
		if (f)
		{
			RETURN_IF_ERROR(ListEntry(Str::SI_IDABOUT_DEFAULTWINFILE, f));
		}
	}
#endif

#if defined PREFS_HAVE_HOTLIST || /*remove me later ->*/ defined QUICK
	// Bookmarks file
	{
		const OpFile *f = g_pcfiles->GetFile(PrefsCollectionFiles::HotListFile);
		if (f)
		{
			RETURN_IF_ERROR(ListEntry(Str::SI_IDABOUT_HOTLISTFILE, f));
		}
	}
#endif

	// Home directory
	RETURN_IF_ERROR(ListEntry(Str::SI_IDABOUT_OPERADIRECTORY, OPFILE_HOME_FOLDER));

#ifndef RAMCACHE_ONLY
	// Cache directory
# if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT && !defined ABOUT_OEM_CACHE_IS_HELP_CACHE
	// List both the regular and the operator cache as cache folders
	OpString cachedirs;
	OpFile cachefolder, oemcachefolder;
	OpString cache_folder_name;
	OpString oemcache_folder_name;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_CACHE_FOLDER, cache_folder_name));
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_OCACHE_FOLDER, oemcache_folder_name));
	RETURN_IF_ERROR(   cachefolder.Construct(cache_folder_name));
	RETURN_IF_ERROR(oemcachefolder.Construct(oemcache_folder_name));
	OpString localizedcachepath, localizedoemcachepath;
	RETURN_IF_ERROR(   cachefolder.GetLocalizedPath(&localizedcachepath));
	RETURN_IF_ERROR(oemcachefolder.GetLocalizedPath(&localizedoemcachepath));
	RETURN_IF_ERROR(cachedirs.SetConcat(localizedcachepath, UNI_L("\n"), localizedoemcachepath));
	RETURN_IF_ERROR(ListEntry(Str::SI_IDABOUT_CACHEDIR, cachedirs));
# else
	RETURN_IF_ERROR(ListEntry(Str::SI_IDABOUT_CACHEDIR, OPFILE_CACHE_FOLDER));
#  ifdef ABOUT_OEM_CACHE_IS_HELP_CACHE
	// The operator cache directory holds help documents
	RETURN_IF_ERROR(ListEntry(Str::SI_IDABOUT_HELPDIR, OPFILE_OCACHE_FOLDER));
#  endif
# endif
#endif

#ifdef M2_SUPPORT
	// E-mail directory
	// Yes, S_IDABOUT_HELPDIR *is* "Mail directory" :-(
	RETURN_IF_ERROR(ListEntry(Str::S_IDABOUT_HELPDIR, OPFILE_MAIL_FOLDER));
#endif

#ifdef _PLUGIN_SUPPORT_
	// Plug-in path
	RETURN_IF_ERROR(ListEntry(Str::SI_IDABOUT_PLUGINPATH, g_pcapp->GetStringPref(PrefsCollectionApp::PluginPath), CLASSPATHSEPCHAR, 0)); // FIXME: Correct separator?
#endif

#ifdef USER_JAVASCRIPT
	// User JavaScript paths
	/* This preference can be a comma-joined sequence of tokens, each of
	 * which is a directory or file and may have arbitrarily many ;-suffixes
	 * which may be either greasemonkey or opera (other things may be added
	 * to this list in future).
	 */
	RETURN_IF_ERROR(ListEntry(Str::S_IDABOUT_USERJSFILE, g_pcjs->GetStringPref(PrefsCollectionJS::UserJSFiles), ',', ';'));
#endif

#ifdef PREFS_USE_CSS_FOLDER_SCAN
	// User stylesheet diretory
	RETURN_IF_ERROR(ListEntry(Str::S_USER_CSS_LABEL, OPFILE_USERPREFSSTYLE_FOLDER));
#endif

	return CloseList();
}


#endif // ABOUT_DESKTOP_ABOUT



// Mobile implementation ---

#ifdef ABOUT_MOBILE_ABOUT
OP_STATUS OperaAbout::GenerateData()
{
	// Initialize, open HEAD and write TITLE and stylesheet link
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::SI_IDABOUT_ABOUT, PrefsCollectionFiles::StyleAboutFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::SI_IDABOUT_ABOUT));
#endif


	// Close HEAD, open BODY
#ifdef NEED_URL_WRITE_DOC_IMAGE
	RETURN_IF_ERROR(OpenBody(Str::NOT_A_STRING, UNI_L("smartphone")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<h1><img src=\"opera:logo\" alt=\"Opera\"></h1>")));
#else
	RETURN_IF_ERROR(OpenBody(Str::SI_IDABOUT_ABOUT, UNI_L("smartphone")));
#endif

	// Information
	RETURN_IF_ERROR(OpenList());
	// Version
	RETURN_IF_ERROR(ListEntry(Str::SI_IDSTR_ABT_VER_STR, VER_NUM_STR_UNI));
	// Build
	RETURN_IF_ERROR(ListEntry(Str::SI_IDSTR_ABT_BUILD_STR, VER_BUILD_IDENTIFIER_UNI));
	// Platform
	RETURN_IF_ERROR(ListEntry(Str::SI_IDSTR_ABT_PLATFORM_STR, g_op_system_info->GetPlatformStr()));
	RETURN_IF_ERROR(CloseList());

	// Copyright
	OpString tmp_str;
	tmp_str.Reserve(128);
	RETURN_IF_ERROR(tmp_str.Set("<p>Copyright \xA9 1995-"));
	RETURN_IF_ERROR(tmp_str.Append(ABOUT_CURRENT_YEAR));
	RETURN_IF_ERROR(tmp_str.Append(" Opera Software ASA. All rights reserved. <a href=\"http://www.opera.com/\">www.opera.com</a></p>\n"));
	m_url.WriteDocumentData(URL::KNormal, tmp_str);

	// UA string
	RETURN_IF_ERROR(WriteUA());

	// Third-party information
	RETURN_IF_ERROR(WriteCredits());

	// Close BODY and HTML elements and finish off.
	return CloseDocument();
}
#endif

// Common for all opera:about screens ---

#if defined ABOUT_DESKTOP_ABOUT || defined ABOUT_MOBILE_ABOUT
OP_STATUS OperaAbout::WriteCredits()
{
	RETURN_IF_ERROR(Heading(Str::SI_IDABOUT_INCLUDESFROM));

	OpString credits;
	RETURN_IF_ERROR(OperaCredits::GetString(credits, m_rtl));
	return m_url.WriteDocumentData(URL::KNormal, credits.CStr());
}

OP_STATUS OperaAbout::WriteUA()
{
	char useragent[256]; /* ARRAY OK 2008-06-11 eddy */
	if (g_uaManager->GetUserAgentStr(useragent, ARRAY_SIZE(useragent), NULL))
	{
		RETURN_IF_ERROR(Heading(Str::DI_IDLABEL_BROWSERIDENT));
		OpString line;
		line.Reserve(128);
		// UA string is always left-to-right. The parantheses in it
		// make it look weird in an RTL context.
		RETURN_IF_ERROR(line.Set(m_rtl ? " <p dir=\"ltr\">" : " <p>"));
		OpString tmp;
		RETURN_IF_ERROR(tmp.Set(useragent));
		RETURN_IF_ERROR(AppendHTMLified(&line, tmp, tmp.Length()));
		RETURN_IF_ERROR(line.Append("</p>\n\n"));
		return m_url.WriteDocumentData(URL::KNormal, line.CStr());
	}

	return OpStatus::OK;
}
#endif

#endif // NO_URL_OPERA
