/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifndef USE_ABOUT_FRAMEWORK
#include "platforms/unix/product/aboutopera.h"
#undef AboutOpera // avoid core-2's bodge messing us up here ...

#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/url/uamanager/ua.h"
#include "modules/url/url2.h"

#include "modules/prefsfile/prefsfile.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/dom/domenvironment.h"

#include "platforms/posix/posix_native_util.h"

#include "platforms/unix/product/config.h"

#ifndef op_strlen
# define op_strlen strlen
#endif

#include <unistd.h> // F_OK for access()

/* <config> Misc configuration defines for this file: */

#if 0 // 1 chose; shall we htmlify registered user and organisation ?
// apparently this causes some problems with cyrillic text - ask Espen.
# include "modules/util/htmlify.h"
# define HTMLIFY_REGISTRANT
#endif // decide whether to htmlify

#define SHOW_USER_AGENT

/* </config> */

namespace { // Local: first the oft-repeated templates (to avoid repetion of UNI_L(...)):
	void outASCII( URL& url, const char *txt )
	{
		/* Use like url.WriteDocumentData(UNI_L(txt)) but without restricting
		 * txt to be a single string token, without concatenation, as UNI_L
		 * does.  For short texts, the UNI_L form is preferable.
		 */
		size_t len = op_strlen(txt);
		OpString temp; // outside loop, so we only Grow() it once.
		while (len > 0)
		{
			const int chunk = MIN(len, 80);
			temp.Set(txt, chunk);
			url.WriteDocumentData(temp.CStr());
			len -= chunk;
			txt += chunk;
		}
	}

	void outRow( URL& url, OpString& name, OpString& value )
	{
		if (name.HasContent())
		{
			url.WriteDocumentData(UNI_L("\n  <dt>"));
			url.WriteDocumentData(name.CStr());
			url.WriteDocumentData(UNI_L("</dt>"));
		}
		url.WriteDocumentData(UNI_L("<dd>"));
		if (value.HasContent())
			url.WriteDocumentData(value.CStr());
		url.WriteDocumentData(UNI_L("</dd>"));
	}

	void outHeading( URL& url, OpString& heading )
	{
		url.WriteDocumentData(UNI_L("\n <h2>"));
		url.WriteDocumentData(heading.CStr());
		url.WriteDocumentData(UNI_L("</h2>\n"));
	}
};

// Export:
void AboutOpera::generateData( URL& url )
{
	writePreamble( url ); // <html><body> surplus
	// <require> each of these should close exactly the HTML tags it opens:
	writeVersion( url );
	writeUA( url );
	writeRegInfo( url );
	writePaths( url );
	writeSound( url );
	writeCredits( url );
	// </require>
	url.WriteDocumentData(UNI_L("\n\n <address>"));
#ifdef GEIR_MEMORIAL
	OP_STATUS rc;
	OpString span;
	TRAP(rc, g_languageManager->GetStringL(Str::S_DEDICATED_TO, span));
	if (OpStatus::IsSuccess(rc))
	{
		url.WriteDocumentData(UNI_L("<span>"));
		// In memory of Geir Ivars&oslash;y.
		url.WriteDocumentData(span);
		url.WriteDocumentData(UNI_L("</span> "));
	}
#endif
	outASCII(url, "Copyright &copy; 1995-");
	outASCII(url, __DATE__ + 7); // "Mmm dd Year" + 7 == "Year"
	outASCII(url, " Opera Software ASA.\n"
			 "All rights reserved.</address>\n"
			 "</body>\n</html>\n");
	url.WriteDocumentDataFinished();
}

// Private: logical components of the generated data:

void AboutOpera::writePreamble( URL& url )
{
	OP_STATUS rc = url.SetAttribute(URL::KMIME_ForceContentType, "text/html;charset=utf-16");
	// FIXME: OOM - rc is discarded
	uni_char bom = 0xfeff; // unicode byte order mark
	url.WriteDocumentData(&bom, 1);

	outASCII(url,
		"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\n"
		"<html>\n<head>\n <title>");

	OpString tmp;

	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_ABOUT, tmp));

	if (!OpStatus::IsSuccess(rc)) tmp.Set("About Opera");
	url.WriteDocumentData(tmp.CStr());
	url.WriteDocumentData(UNI_L("</title>\n"));

	// Don't mess with tmp until we've re-used it as H1's content

	// Style sheet, if any:
	OpString pref;
	OpFile f;
	TRAP(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::StyleAboutFile, f));
	if (OpStatus::IsSuccess(rc)) rc = pref.Set(f.GetFullPath());
	if (OpStatus::IsSuccess(rc))
	{
		url.WriteDocumentData(UNI_L("  <link rel=\"stylesheet\" href=\"file://localhost"));
		if (pref.FindFirstOf('/') != 0) url.WriteDocumentData(UNI_L("/"));
		url.WriteDocumentData(pref.CStr());
		url.WriteDocumentData(UNI_L("\" media=\"screen,projection,tv,handheld,print\">\n"));
	}
	url.WriteDocumentData(UNI_L("</head>\n<body>\n"));
    url.WriteDocumentData(UNI_L("<h1>"));
	url.WriteDocumentData(tmp.CStr());
	url.WriteDocumentData(UNI_L("</h1>\n"));

	// Splash image, "<IMG SRC=\"file://localhost/...\">\n", if any.
	if (OpStatus::IsSuccess(getSplashImageURL( tmp )))
	{
		url.WriteDocumentData(UNI_L(" <img src=\""));
		url.WriteDocumentData(tmp.CStr());
		url.WriteDocumentData(UNI_L("\">\n"));
	}
}

void AboutOpera::writeVersion( URL& url )
{
	OP_STATUS rc;
	OpString tmp;

	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDSTR_ABT_VER_INFO_STR, tmp));
	if (OpStatus::IsSuccess(rc)) outHeading(url, tmp);
	url.WriteDocumentData(UNI_L(" <dl>"));

	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDSTR_ABT_VER_STR, tmp));
	if (OpStatus::IsSuccess(rc))
	{
		OpString val;
		rc = GetVersion(&val);
		if (OpStatus::IsSuccess(rc))
			outRow(url, tmp, val);
	}

	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDSTR_ABT_BUILD_STR, tmp));
	if (OpStatus::IsSuccess(rc))
	{
		OpString val;
		rc = GetBuild(&val);
		if (OpStatus::IsSuccess(rc))
			outRow(url, tmp, val);
	}

	OpString name;

	rc = getPlatformDetails(name, tmp);
	if (OpStatus::IsSuccess(rc))
	{
		OpString label;
		TRAP(rc, g_languageManager->GetStringL(Str::SI_IDSTR_ABT_PLATFORM_STR, label));
		if (OpStatus::IsSuccess(rc)) // platform
			outRow(url, label, name);

		TRAP(rc, g_languageManager->GetStringL(Str::SI_IDSTR_ABT_WIN_VER, label));
		if (OpStatus::IsSuccess(rc)) // platform version
			outRow(url, label, tmp);
	}

	rc = getToolkitDetails(name, tmp);
	if (OpStatus::IsSuccess(rc))
		outRow(url, name, tmp);

	if (OpStatus::IsSuccess(getJavaVersion( name, tmp )))
		outRow(url, name, tmp);

	url.WriteDocumentData(UNI_L("\n </dl>\n"));
}

void AboutOpera::writeUA( URL& url )
{
#ifdef SHOW_USER_AGENT
	OpString tmp;

	TRAPD(rc, g_languageManager->GetStringL(Str::DI_IDLABEL_BROWSERIDENT, tmp));
	if (OpStatus::IsSuccess(rc))
	{
		char useragent[1024];
		if (g_uaManager->GetUserAgentStr(useragent, sizeof(useragent),
										 UA_NotOverridden))
		{
			OpString val;
			rc = val.Set(useragent);
			if (OpStatus::IsSuccess(rc))
			{
				outHeading(url, tmp);
				url.WriteDocumentData(UNI_L(" <p>"));
				url.WriteDocumentData(val.CStr());
				url.WriteDocumentData(UNI_L("</p>\n"));
			}
		}
	}
#endif // SHOW_USER_AGENT
}

void AboutOpera::writePaths( URL& url )
{
	OpString type, name;

	TRAPD(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_OPERASYSINFO, name));
	if (OpStatus::IsSuccess(rc)) outHeading(url, name);
	url.WriteDocumentData(UNI_L(" <dl>"));

	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_SETTINGS, type));
	// This code makes the assumption that we're using PrefsFile and OpFile
# ifdef PREFS_READ
	PrefsFile *prefsfile  = (PrefsFile *) g_prefsManager->GetReader();
	OpFile    *inifile    = (OpFile *)    prefsfile->GetFile();
	rc = name.Set(inifile->GetFullPath());
# else
	rc = OpStatus::ERR_NOT_SUPPORTED;
# endif
	if (OpStatus::IsSuccess(rc))
	{
		cleanPath(name);
		outRow(url, type, name);
	}

#ifdef AUTOSAVE_WINDOWS
	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_DEFAULTWINFILE, type));
	if (OpStatus::IsSuccess(rc))
	{
		OpFile f;
		TRAP(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::WindowsStorageFile, f));
		if (OpStatus::IsSuccess(rc)) rc = name.Set(f.GetFullPath());
		if (OpStatus::IsSuccess(rc))
		{
			cleanPath(name);
			outRow(url, type, name);
		}
	}
#endif // AUTOSAVE_WINDOWS

	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_HOTLISTFILE, type));
	if (OpStatus::IsSuccess(rc))
	{
		OpFile f;
		TRAP(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::HotListFile, f));
		if (OpStatus::IsSuccess(rc)) rc = name.Set(f.GetFullPath());
		if (OpStatus::IsSuccess(rc))
		{
			cleanPath(name);
			outRow(url, type, name);
		}
	}

	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_OPERADIRECTORY, type));
	if (OpStatus::IsSuccess(rc))
	{
		rc = g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, name);
		if (OpStatus::IsSuccess(rc))
		{
			cleanPath(name, TRUE);
			outRow(url, type, name);
		}
	}

	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_CACHEDIR, type));
	if (OpStatus::IsSuccess(rc))
	{
		rc = g_folder_manager->GetFolderPath(OPFILE_CACHE_FOLDER, name);
		if (OpStatus::IsSuccess(rc))
		{
			cleanPath(name, TRUE);
			outRow(url, type, name);
		}
	}

#ifdef M2_SUPPORT
	TRAP(rc, g_languageManager->GetStringL(Str::S_IDABOUT_HELPDIR, type)); // HELPDIR -> MAILDIR maybe
	if (OpStatus::IsSuccess(rc))
	{
		rc = g_folder_manager->GetFolderPath(OPFILE_MAIL_FOLDER, name);
		if (OpStatus::IsSuccess(rc))
		{
			cleanPath(name, TRUE);
			outRow(url, type, name);
		}
	}
#endif // M2_SUPPORT

	if (OpStatus::IsSuccess(getHelpDir(name)))
	{
		TRAP(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_HELPDIR, type));
		if (OpStatus::IsSuccess(rc))
		{
			cleanPath(name, TRUE);
			outRow(url, type, name);
		}
	}

	// Plugin path:
	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_PLUGINPATH, type));
	if (OpStatus::IsSuccess(rc))
	{
		rc = name.Set(g_pcapp->GetStringPref(PrefsCollectionApp::PluginPath));
		if (!OpStatus::IsSuccess(rc)) name.Empty();

		while (name.Length())
		{
			int col = name.FindFirstOf(':');
			if (col == KNotFound)
			{
				cleanPath(name, TRUE);
				outRow(url, type, name);
				name.Empty();
			}
			else
			{
				OpString nom;
				if (OpStatus::IsSuccess(nom.Set(name.CStr(), col)))
				{
					cleanPath(nom, TRUE);
					outRow(url, type, nom);
				}
				// else: silent OOM :-(
				name.Delete(0, col+1);
			}
			type.Empty(); // only say "Plugin path:" on first row.
		}
#if 0 // 1 do we want to mention plugin path even if empty ?  Moose prefers not in bug #206083.
		if (type.Length())
		{
			OP_ASSERT(name.IsEmpty());
			outRow(url, type, name); // name is empty, which is what we want here ...
		}
#endif // mention plugin path even if empty
	}

#ifdef USER_JAVASCRIPT
	TRAP(rc, g_languageManager->GetStringL(Str::S_IDABOUT_USERJSFILE, type));
	if (OpStatus::IsSuccess(rc))
	{
		name.SetL(g_pcjs->GetStringPref(PrefsCollectionJS::UserJSFiles));
		/* This preference can be a comma-joined sequence of tokens, each of
		 * which is a directory or file and may have arbitrarily many ;-suffixes
		 * which may be either greasemonkey or opera (other things may be added
		 * to this list in future).  For now (2005/May, O8) this is unofficial -
		 * users are told it's just a single file or directory name - so we can
		 * just display it "as is"; but we'll eventually need to parse it as
		 * above and display it nicely.
		 */
		PosixNativeUtil::NativeString native_path (name.CStr());
		if (native_path.get() && access(native_path.get(), F_OK) == 0)
		{
			cleanPath(name, FALSE);
			outRow(url, type, name);
		}
	}
#endif // USER_JAVASCRIPT

#ifdef PREFS_USE_CSS_FOLDER_SCAN
	TRAP(rc, g_languageManager->GetStringL(Str::S_USER_CSS_LABEL, type));
	if (OpStatus::IsSuccess(rc))
	{
		OpString tmp_storage;
		name.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_USERPREFSSTYLE_FOLDER, tmp_storage));
		cleanPath(name, FALSE);
		outRow(url, type, name);
	}
#endif // PREFS_USE_CSS_FOLDER_SCAN

	url.WriteDocumentData(UNI_L("\n </dl>\n"));
}

void AboutOpera::writeSound( URL& url )
{
	OpString hdr, txt;
	OP_STATUS st = getSoundDoc( hdr, txt );
	if (OpStatus::IsSuccess(st))
	{
		outHeading(url, hdr);
		url.WriteDocumentData(txt.CStr());
	}
}

void AboutOpera::writeCredits( URL& url )
{
// #if the UL below is going to get any content
	OpString tmp;

	TRAPD(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_INCLUDESFROM, tmp));
	if (!OpStatus::IsSuccess(rc)) tmp.Set("Third Parties");
	outHeading(url, tmp);

	/* The following probably all need to be reviewed periodically for
	 * up-to-date and relevant; in particular, more of them should be #if'd on
	 * suitable conditions matching whether or not the relevant credit is
	 * warranted in the present build (c.f. the number<->string one).  See the
	 * developer Wiki's Third_party_code page for authoritative details: *
	 * https://ssl.opera.com:8008/developerwiki/index.php/Third_party_code
	 *
	 * Note, however, that UNI_L() may be a macro and the C preprocessor has
	 * undefined behaviour if any preprocessor directives appear within a macro
	 * argument: so you *should not* try to do #if'ing within a single UNI_L()'s
	 * argument string.  Furthermore, since we want the following to be portable
	 * between platforms with UNI_L either as macro or mediated by unil_convert,
	 * we can't do string concatenation with UNI_L()s; macro form can't cope
	 * with several strings inside one UNI_L - TODO: as used below - while the
	 * unil_convert version breaks if we try to use several adjacent UNI_L()s.
	 */
	outASCII(
		url,
		" <ul>\n"

#ifdef MDE_AGFA_SUPPORT
		"  <li>This product includes software developed by Monotype Imaging Inc.\n"
		"Copyright &copy; 2005 Monotype Imaging Inc.<a\n"
		"href=\"http://www.monotypeimaging.com\">Monotype Imageing</a></li>\n"
#endif // MDE_AGFA_SUPPORT

#if defined(_NATIVE_SSL_SUPPORT_)
		"  <li>This product includes software developed by the OpenSSL Project for\n"
		"      use in the OpenSSL Toolkit. Copyright &copy; 1998-2001\n"
		"      <a href=\"http://www.openssl.org/\">The OpenSSL Project</a>.\n"
		"      All rights reserved.</li>\n"

		"  <li>This product includes cryptographic software written by Eric Young.\n"
		"      Copyright &copy; 1995-1998 <a\n"
		"      href=\"mailto:eay@cryptsoft.com\">Eric Young</a></li>\n"
#endif // _NATIVE_SSL_SUPPORT_

#ifdef OPERA_MINI // third-party regexp code, not currently used in any browser product (?)
		"  <li>This product includes software developed by the University of California,\n"
		"      Berkeley and its contributors</li>\n"
#endif // check with LTH ...

#ifndef USE_JAYPEG
		"  <li>The Independent JPEG Group</li>\n"
#endif // USE_JAYPEG

#ifdef USE_ZLIB
		"  <li>Zlib compression library, developed by Jean-loup Gailly\n"
		"      and Mark Adler</li>\n" // zlib
#endif

#ifdef EXPAT_XML_PARSER
		"  <li>James Clark</li>\n" // xml - expat
#endif // EXPAT_XML_PARSER

	"  <li>Eberhard Mattes</li>\n" // uni_sprintf, uni_sscanf

#  ifndef ECMASCRIPT_NO_SPECIAL_DTOA_STRTOD
		"  <li>Number-to-string and string-to-number conversions are\n"
		"      covered by the following notice:\n"
		"   <blockquote>\n"
		"    <p>The author of this software is David M. Gay.</p>\n"
        "    <p>Copyright (c) 1991, 2000, 2001 by Lucent Technologies.</p>\n"
        "    <p>Permission to use, copy, modify, and distribute this software for any\n"
        "       purpose without fee is hereby granted, provided that this entire notice\n"
        "       is included in all copies of any software which is or includes a copy\n"
        "       or modification of this software and in all copies of the supporting\n"
        "       documentation for such software.</p>\n"
        "    <p>THIS SOFTWARE IS BEING PROVIDED <q>AS IS</q>,\n"
		"       WITHOUT ANY EXPRESS OR IMPLIED\n"
        "       WARRANTY.  IN PARTICULAR, NEITHER THE AUTHOR NOR LUCENT MAKES ANY\n"
        "       REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY\n"
        "       OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.\n"
		"    </p>\n"
		"   </blockquote>\n"
		"  </li>\n"
#  endif // ecmascript number <-> string conversions

# ifdef EXPAT_XML_PARSER
		"  <li>expat is covered by the following license: \n"
		"   <blockquote>\n"
		"    <p>Copyright (c) 1998, 1999, 2000 Thai Open Source Software Center Ltd</p>\n"
		"    <p>Permission is hereby granted, free of charge, to any person obtaining\n"
		"    a copy of this software and associated documentation files (the\n"
		"    <q>Software</q>), to deal in the Software without restriction, including\n"
		"    without limitation the rights to use, copy, modify, merge, publish,\n"
		"    distribute, sublicense, and/or sell copies of the Software, and to\n"
		"    permit persons to whom the Software is furnished to do so, subject to\n"
		"    the following conditions:</p>\n"
		"    <p>THE SOFTWARE IS PROVIDED <q>AS IS</q>, WITHOUT WARRANTY OF ANY KIND,\n"
		"    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n"
		"    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.\n"
		"    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY\n"
		"    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,\n"
		"    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE\n"
		"    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.</p>\n"
		"   </blockquote>\n"
		"  </li>\n"
# endif

# ifdef SVG_SUPPORT
		"  <li>Bitstream Vera Fonts are covered by the following license:\n"
		"   <blockquote>\n"
		"    <p>Copyright (c) 2003 by Bitstream, Inc. All Rights Reserved.\n"
		"    Bitstream Vera is a trademark of Bitstream, Inc.</p>\n"
		"    <p>Permission is hereby granted, free of charge, to any person obtaining a\n"
		"    copy of the fonts accompanying this license (<q>Fonts</q>) and associated\n"
		"    documentation files (the <q>Font Software</q>), to reproduce and distribute\n"
		"    the Font Software, including without limitation the rights to use, copy,\n"
		"    merge, publish, distribute, and/or sell copies of the Font Software, and\n"
		"    to permit persons to whom the Font Software is furnished to do so, subject\n"
		"    to the following conditions:</p>\n"
		"    <p>The above copyright and trademark notices and this permission notice shall\n"
		"    be included in all copies of one or more of the Font Software typefaces.</p>\n"
		"    <p>The Font Software may be modified, altered, or added to, and in particular\n"
		"    the designs of glyphs or characters in the Fonts may be modified and additional\n"
		"    glyphs or characters may be added to the Fonts, only if the fonts are renamed to\n"
		"    names not containing either the words <q>Bitstream</q> or the word <q>Vera</q>.</p>\n"
		"    <p>This License becomes null and void to the extent applicable to Fonts or\n"
		"    Font Software that has been modified and is distributed under the\n"
		"    <q>Bitstream Vera</q> names.</p>\n"
		"    <p>The Font Software may be sold as part of a larger software package but no copy\n"
		"    of one or more of the Font Software typefaces may be sold by itself.</p>\n"
		"    <p>THE FONT SOFTWARE IS PROVIDED <q>AS IS</q>,\n"
		"    WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,\n"
		"    INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A\n"
		"    PARTICULAR PURPOSE AND NONINFRINGEMENT OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER\n"
		"    RIGHT. IN NO EVENT SHALL BITSTREAM OR THE GNOME FOUNDATION BE LIABLE FOR ANY CLAIM,\n"
		"    DAMAGES OR OTHER LIABILITY, INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL,\n"
		"    OR CONSEQUENTIAL DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,\n"
		"    ARISING FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM OTHER\n"
		"    DEALINGS IN THE FONT SOFTWARE.</p>\n"
		"   </blockquote>\n"
		"  </li>\n"
# endif // SVG_SUPPORT
# ifdef SKIN_SUPPORT
		"  <li>Nice Graphics &trade; by P&aring;l Syvertsen, <a\n"
		"      href=\"http://www.flottaltsaa.no/\">Flott Alts&aring;</a></li>\n"
# endif
		"  <li>The Elektrans</li>\n" // what about panic ?
		" </ul>\n <p>");

	TRAP(rc, g_languageManager->GetStringL(Str::SI_IDABOUT_WELOVEYOU, tmp));
	if (OpStatus::IsSuccess(rc)) url.WriteDocumentData(tmp.CStr());
	url.WriteDocumentData(UNI_L("</p>\n"));
// #endif // the UL got some content.
}
#endif // USE_ABOUT_FRAMEWORK
