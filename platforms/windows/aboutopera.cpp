/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef USE_ABOUT_FRAMEWORK
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/prefsfile.h"

#include "adjunct/desktop_pi/AboutDesktop.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/windows_ui/res/version.rc"

class WindowsOperaAbout : public AboutDesktop
{
public:
	WindowsOperaAbout(URL &url)
		: AboutDesktop(url)
	{}

	/** Platform identifier and version */
	virtual OP_STATUS GetPlatform(OpString *platform, OpString *version)
	{
#ifdef _WIN64
		RETURN_IF_ERROR(platform->Set("x64"));
#else
		RETURN_IF_ERROR(platform->Set("Win32"));
#endif //_WIN64
		OpString8 tmpwinVer;
		tmpwinVer.Reserve(64);
		GetWinVersionStr(tmpwinVer.DataPtr(), TRUE);
		return version->Set(tmpwinVer);
	}

	virtual OP_STATUS GetPreferencesFile(OpString *dest)
	{
		const PrefsFile *prefsfile = g_prefsManager->GetReader();
		const OpFileDescriptor *inifile = prefsfile->GetFile();

		if (inifile->Type() == OPFILE)
		{
			return dest->Set(reinterpret_cast<const OpFile *>(inifile)->GetFullPath());
		}
		else
		{
			return OpStatus::ERR;
		}
	}
};

OperaAbout *OperaAbout::Create(URL &url)
{
	return new WindowsOperaAbout(url);
}

#else // not USE_ABOUT_FRAMEWORK

#define SHOW_USER_AGENT

// Export:
void AboutOpera::generateData( URL& url )
{
	OpString szWinVer;
	szWinVer.Set(UNI_L(""));
	OpString8 tmpwinVer;
	tmpwinVer.Reserve(64);
	GetWinVersionStr(tmpwinVer.DataPtr(), TRUE);
	szWinVer.Set(tmpwinVer);
	OpString pluginpath;
	OpString szPlugPath; 
#ifdef _PLUGIN_SUPPORT_
	g_languageManager->GetString(Str::SI_IDABOUT_PLUGINPATH, szPlugPath);
	pluginpath.Set(g_pcapp->GetStringPref(PrefsCollectionApp::PluginPath));
	// If there is trailing ';', remove it
	if (!pluginpath.IsEmpty() && pluginpath[pluginpath.Length() - 1] == ';')
	{
		pluginpath[pluginpath.Length() - 1] = ' ';
		pluginpath.Strip();
	}
#else
	pluginpath.Set(UNI_L("No plugin support")); // mariusab20060107: Is this not an untranslatable hack? What's the point of this solution?
#endif

	uni_char* newPluginPath;
	uni_char* szReplaceString = UNI_L("</dd><dd>");

	newPluginPath = new uni_char[pluginpath.Length()*uni_strlen(szReplaceString)+1];//FIXME:OOM
	if(!newPluginPath)
		return;

	ReplaceCharsWithStr(pluginpath.CStr(), newPluginPath, szReplaceString, ';');

	OP_STATUS ops = pluginpath.Set(newPluginPath);
	delete[] newPluginPath; newPluginPath = NULL;
	RETURN_VOID_IF_ERROR(ops);

	OpString pluginStringAndPaths;
	pluginStringAndPaths.Set(UNI_L(""));
	if (pluginpath.IsEmpty() || pluginpath.Compare(UNI_L("No plugin support")))
	{
		pluginStringAndPaths.AppendFormat(UNI_L("<dt>%s</dt><dd>%s</dd>"), szPlugPath.CStr(), pluginpath.CStr());
	}

	const uni_char *opsysname =
#  if defined( __WIN32OS2__)
		UNI_L("OS/2")
#  elif defined(WIN32)
		UNI_L("Win32")
#  endif
		;

	OpString szAboutVerOpera;
	OpString szAboutBuildOpera;
	OpString szAboutPlatform;
	OpString szVerInf;
	OpString szWinVerstr;
	g_languageManager->GetString(Str::SI_IDSTR_ABT_VER_STR, szAboutVerOpera);
	g_languageManager->GetString(Str::SI_IDSTR_ABT_BUILD_STR, szAboutBuildOpera);
	g_languageManager->GetString(Str::SI_IDSTR_ABT_PLATFORM_STR, szAboutPlatform);
	g_languageManager->GetString(Str::SI_IDSTR_ABT_VER_INFO_STR, szVerInf);
	g_languageManager->GetString(Str::SI_IDSTR_ABT_WIN_VER, szWinVerstr);

#ifdef USER_JAVASCRIPT
	OpString szUserJSFileAndPath;
	OpStringC filenames;
	filenames = g_pcjs->GetStringPref(PrefsCollectionJS::UserJSFiles);
	OpString szUserJS;
	g_languageManager->GetString(Str::S_IDABOUT_USERJSFILE, szUserJS);
	szUserJSFileAndPath.Empty();
	szUserJSFileAndPath.Append("");
	if (!filenames.IsEmpty())
	{
		szUserJSFileAndPath.AppendFormat(UNI_L("<dt>%s</dt>"),szUserJS.CStr());
		szUserJSFileAndPath.AppendFormat(UNI_L("<dd>%s</dd>"), filenames.CStr());
	}
#endif //USER_JAVASCRIPT

#ifdef PREFS_USE_CSS_FOLDER_SCAN
	OpString szUserCSSPathLabel;
	szUserCSSPathLabel.Empty();
	OpString tmp_storage;
	OpStringC userCssPath = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_USERPREFSSTYLE_FOLDER, tmp_storage);
	if (!(userCssPath.IsEmpty()))
	{
		OpString userCssLabel;
		g_languageManager->GetString(Str::S_USER_CSS_LABEL, userCssLabel);
		szUserCSSPathLabel.AppendFormat(UNI_L("<dt>%s</dt>"), userCssLabel.CStr());
		szUserCSSPathLabel.AppendFormat(UNI_L("<dd>%s</dd>"), userCssPath.CStr());
	}
#endif // PREFS_USE_CSS_FOLDER_SCAN

	OpString osBrandedPartner;
	RETURN_VOID_IF_ERROR(osBrandedPartner.Set(UNI_L("")));
	OpString szAboutTitle; 
	OpString szRegInfo; 
	OpString szName; 
	OpString szReg; 
	OpString szOrg; 
	OpString szOperaSys; 
	OpString szOSVer; 
	OpString szSetFile; 
	OpString szOpDir; 
	OpString szDefWin; 
	OpString szHotFile; 
	OpString szCacheDir; 
	OpString szHelpDir; 
	OpString szJavaVer;
	OpString szIncludesSoftware; 
	OpString szGrateFul; 
	OpString szInMemoriam;
	OpString szAboutYes; 
	OpString szAboutNo; 
	OpString szAboutNA; 
	OpString szNoJava;
	OpString szMailDir;
	OpString splashPath;

	g_languageManager->GetString(Str::SI_IDABOUT_ABOUT, szAboutTitle);
	g_languageManager->GetString(Str::SI_IDABOUT_REGINFO, szRegInfo);
	g_languageManager->GetString(Str::SI_IDABOUT_NAME, szName);
	g_languageManager->GetString(Str::SI_IDABOUT_REGISTERED, szReg);
	g_languageManager->GetString(Str::SI_IDABOUT_ORGANIZATION, szOrg);
	g_languageManager->GetString(Str::SI_IDABOUT_OPERASYSINFO, szOperaSys);
	g_languageManager->GetString(Str::SI_IDABOUT_WINVER, szOSVer);
	g_languageManager->GetString(Str::SI_IDABOUT_SETTINGS, szSetFile);
	g_languageManager->GetString(Str::SI_IDABOUT_OPERADIRECTORY, szOpDir);
	g_languageManager->GetString(Str::SI_IDABOUT_DEFAULTWINFILE, szDefWin);
	g_languageManager->GetString(Str::SI_IDABOUT_HOTLISTFILE, szHotFile);
	g_languageManager->GetString(Str::S_IDABOUT_HELPDIR, szMailDir);
	g_languageManager->GetString(Str::SI_IDABOUT_CACHEDIR, szCacheDir);
	g_languageManager->GetString(Str::SI_IDABOUT_HELPDIR, szHelpDir);
	g_languageManager->GetString(Str::SI_IDABOUT_JAVAVERSION, szJavaVer);
	g_languageManager->GetString(Str::SI_IDABOUT_INCLUDESFROM, szIncludesSoftware);
	g_languageManager->GetString(Str::SI_IDABOUT_WELOVEYOU, szGrateFul);
	g_languageManager->GetString(Str::SI_IDABOUT_YES, szAboutYes);
	g_languageManager->GetString(Str::SI_IDABOUT_NO, szAboutNo);
	g_languageManager->GetString(Str::SI_IDABOUT_NA, szAboutNA);
	g_languageManager->GetString(Str::SI_IDSTR_NO_JAVA, szNoJava);

	RETURN_VOID_IF_ERROR(url.SetAttribute(URL::KMIME_ForceContentType, "text/html;charset=utf-16"));


	{
		uni_char temp = 0xfeff; // unicode byte order mark
		url.WriteDocumentData(&temp,1);
	}

	// Unicodify registration information (this should be changed to be
	// unicode in PrefsManager... later)

	//	uni_char *username = NULL, *userorganization = NULL;
	OpString userName, userOrganization;
	userName.Set(UNI_L("&nbsp;"));
	userOrganization.Set(UNI_L("&nbsp;"));
	szInMemoriam.Set(UNI_L(""));

#ifdef GEIR_MEMORIAL // Memorial text for Geir Ivarsøy.
	OpString text;
	g_languageManager->GetString(Str::S_DEDICATED_TO, text);
	if (OpStatus::IsSuccess(ops))
	{
		szInMemoriam.AppendFormat(UNI_L("<span>%s</span>"), text.CStr());
	}
#endif // GEIR_MEMORIAL

	OpString prefsfilename;
#ifdef PREFS_READ
	const PrefsFile *prefsfile = (const PrefsFile *) g_prefsManager->GetReader();
	const OpFile *inifile = (const OpFile *) prefsfile->GetFile();
	prefsfilename.Set(inifile->GetFullPath());
#else
	prefsfilename.Set(szAboutNA);
#endif

	/* Note that OpStringC does not copy the returned value, so we
	 * have to use different storage objects for each of the calls to
	 * GetFolderPath below.
	 */
	OpString tmp_storage1;
	OpStringC operadir = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_USERPREFS_FOLDER, tmp_storage1);
	OpString tmp_storage2;
	OpStringC cachedir = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_CACHE_FOLDER, tmp_storage2);

#ifdef M2_SUPPORT
	OpString tmp_storage3;
	OpStringC mailDir = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MAIL_FOLDER, tmp_storage3);
#endif // M2_SUPPORT
	OpStringC helpdir = g_pcnet->GetStringPref(PrefsCollectionNetwork::HelpUrl);

#define RETRIEVE_PATH(string,object,setting) \
	OpFile object; \
	g_pcfiles->GetFileL(PrefsCollectionFiles::setting, object); \
	OpStringC string(object.GetFullPath());

	RETRIEVE_PATH(winstoragefile, winstoragefileobj, WindowsStorageFile);
	RETRIEVE_PATH(hotlistfile,    hotlistfileobj,    HotListFile);
	OpString stylefilename;
	g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleAboutFile, &stylefilename);

	OpString versionnr;     RETURN_VOID_IF_ERROR(versionnr.Set(VER_NUM_STR));
#ifdef VER_BETA
	versionnr.Append(UNI_L(" Beta"));
#endif // VER_BETA
	OpString buildstr;      RETURN_VOID_IF_ERROR(buildstr.Set(VER_BUILD_NUMBER_STR));

	char *szAboutRC =
		(char*) MallocResourceData(hInst, UNI_L("RCD_ABOUT_HTML"), RT_RCDATA, NULL);//FIXME:OOM


	if( !szAboutRC)
	{
# ifdef _DEBUG
		MessageBoxA( g_main_hwnd, "Merge error ?\nResource\"RCD_ABOUT_HTML\" is missing.", "OPERA DEBUG", MB_OK|MB_ICONSTOP);
# endif // !_DEBUG
		return;
	}

	unsigned szAbout_len = strlen(szAboutRC) + 12*1024;

	uni_char *szAbout = new uni_char[szAbout_len];//FIXME:OOM
	if(!szAbout)
	{
		op_free( szAboutRC);
		return;
	}

	// Unicodify template
	uni_char *utemplate = uni_up_strdup(szAboutRC);
	if(!utemplate)
	{
		op_free( szAboutRC);
		delete [] szAbout;
		return;
	}
	
	int len = uni_sprintf(szAbout, utemplate,

		szAboutTitle.CStr(),
		stylefilename.CStr(),
		szAboutTitle.CStr(),
		// Opera
		szVerInf.CStr(),

		szAboutVerOpera.CStr(),
		versionnr.CStr(),

		szAboutBuildOpera.CStr(),
		buildstr.CStr(),
		szAboutPlatform.CStr(),
		opsysname,
		szWinVerstr.CStr(),
		szWinVer.CStr(),

		szJavaVer.CStr(),
		szNoJava.CStr(),

		UNI_L(""),
		UNI_L(""),

		// Opera system information
		szOperaSys.CStr(),

		szSetFile.CStr(),
		prefsfilename.CStr(),
		szDefWin.CStr(),
		winstoragefile.CStr(),
		szHotFile.CStr(),
		hotlistfile.CStr(),

		szOpDir.CStr(),
		operadir.CStr(),
		szCacheDir.CStr(),
		cachedir.CStr(),
		szMailDir.CStr(),
#ifdef M2_SUPPORT
		mailDir.CStr(),
#else
		szAboutNA.CStr(),
#endif
		szHelpDir.CStr(),
		helpdir.CStr(),

		pluginStringAndPaths.CStr(),

		szUserCSSPathLabel.CStr(), 

#if defined(USER_JAVASCRIPT) && defined(MSWIN)
		szUserJSFileAndPath.CStr(),
#endif // USER_JAVASCRIPT

		// This product includes software developed by
		szIncludesSoftware.CStr(),
		szGrateFul.CStr(),
		szInMemoriam.CStr()

		);
	url.WriteDocumentData(szAbout, len);

	// Free up temporary strings
	op_free(utemplate);

	url.WriteDocumentDataFinished();

	delete [] szAbout;
	op_free( szAboutRC);
}

#endif // not USE_ABOUT_FRAMEWORK
