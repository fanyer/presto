/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"
#include "modules/util/opfile/opfile.h"

#include "unix_opdesktopresources.h"
#include "unixutils.h"

#include "platforms/posix/posix_native_util.h"
#include "platforms/posix/posix_file_util.h"
#include "platforms/posix/posix_system_info.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/unix/base/common/unixutils.h"

#include "adjunct/desktop_util/string/stringutils.h"

#include <stdlib.h>
#include <ctype.h>
#include <wctype.h>

static bool IsValidLanguageFileVersion(const uni_char* path);
static OP_STATUS GetLanguageFromLocale(const OpString& locale_folder, OpString& matched_folder);


UnixOpDesktopResources::Folders UnixOpDesktopResources::m_folder;

OP_STATUS UnixOpDesktopResources::AppendPathSeparator(OpString& path)
{
	int length = path.Length();
	int pos = path.FindLastOf('/');
	if( length == 0 || pos == KNotFound || pos+1 < length )
	{
		RETURN_IF_ERROR(path.Append("/"));
	}
	return OpStatus::OK;
}

OP_STATUS UnixOpDesktopResources::Normalize(OpString& path)
{
	if (path[0] != '/')
	{
		OpString relative;
		relative.TakeOver(path);
		RETURN_IF_ERROR(path.Set(m_folder.base));
		if (relative[0] == '.' && relative[1] == '/')
			RETURN_IF_ERROR(path.Append(relative.CStr() + 2));
		else
			RETURN_IF_ERROR(path.Append(relative));
	}
	RETURN_IF_ERROR(AppendPathSeparator(path));
	return OpStatus::OK;
}

// static 
OP_STATUS OpDesktopResources::Create(OpDesktopResources **newObj)
{
	*newObj = OP_NEW(UnixOpDesktopResources, ());
	if (!*newObj)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS result = ((UnixOpDesktopResources *)*newObj)->Init();
	if (OpStatus::IsError(result))
	{
		OP_DELETE(*newObj);
		*newObj = 0;
	}

	return result;
}


UnixOpDesktopResources::~UnixOpDesktopResources() 
{
}


// TODO: Move the call to Init out of Create function and cache. Preferably set up in each GetFolderFunction
OP_STATUS UnixOpDesktopResources::Init()
{
	struct stat statbuf;

	// base folder (CWD)
	OpString8 base;
	RETURN_IF_LEAVE(base.ReserveL(MAX_PATH));
	getcwd(base.DataPtr(), MAX_PATH);
	m_folder.base.Empty();
	OP_STATUS rc = UnixUtils::FromNativeOrUTF8(base.CStr(), &m_folder.base);
	if (OpStatus::IsError(rc))
	{
		printf("opera: failed to prepare base directory\n");
		return rc;
	}
	RETURN_IF_ERROR(AppendPathSeparator(m_folder.base));

	// read folder (resources)
	m_folder.read.Empty();
	rc = UnixUtils::FromNativeOrUTF8(g_startup_settings.share_dir, &m_folder.read);
	if (OpStatus::IsError(rc) || m_folder.read.IsEmpty())
	{
		printf("opera: please specify shared directory with -sharedir or in $OPERA_DIR\n");
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(Normalize(m_folder.read));
	PosixNativeUtil::NativeString folder(m_folder.read.CStr());
	if (stat(folder.get(), &statbuf) != 0
		|| !S_ISDIR(statbuf.st_mode)
		|| access(folder.get(), R_OK) != 0)
	{
		printf("opera: shared directory %s does not exist or is not readable\n", g_startup_settings.share_dir.CStr());
		return OpStatus::ERR;
	}
	g_startup_settings.share_dir.Empty();
	RETURN_IF_ERROR(PosixNativeUtil::ToNative(m_folder.read.CStr(), &g_startup_settings.share_dir));

	// home folder
	RETURN_IF_ERROR(PosixNativeUtil::UniGetEnv(m_folder.home, "HOME"));
	if (m_folder.home.IsEmpty())
	{
		RETURN_IF_ERROR(PosixNativeUtil::UniGetEnv(m_folder.home, "TMPDIR"));
		if (m_folder.home.IsEmpty())
		{
			RETURN_IF_ERROR(m_folder.home.Set(UNI_L("/tmp")));
		}
	}
	RETURN_IF_ERROR(AppendPathSeparator(m_folder.home));

	// write folder (profile)
	m_folder.write.Empty();
	UnixUtils::FromNativeOrUTF8(g_startup_settings.personal_dir, &m_folder.write);
	if (OpStatus::IsError(rc) || m_folder.write.IsEmpty())
	{
		RETURN_IF_ERROR(m_folder.write.Set(m_folder.home));
		if (g_startup_settings.watir_test)
			RETURN_IF_ERROR(m_folder.write.Append(UNI_L(".opera-autotest/")));
		else
			RETURN_IF_ERROR(m_folder.write.Append(UNI_L(".opera/")));
	}
	RETURN_IF_ERROR(Normalize(m_folder.write));
	g_startup_settings.personal_dir.Empty();
	RETURN_IF_ERROR(PosixNativeUtil::ToNative(m_folder.write, &g_startup_settings.personal_dir));

	// tmp folder
	m_folder.tmp.Empty();
	RETURN_IF_ERROR(m_folder.tmp.Set(m_folder.write));
	RETURN_IF_ERROR(m_folder.tmp.Append((UNI_L("tmp/"))));
	RETURN_IF_ERROR(Normalize(m_folder.tmp));
	RETURN_IF_ERROR(AppendPathSeparator(m_folder.tmp));
	g_startup_settings.temp_dir.Empty();
	RETURN_IF_ERROR(PosixNativeUtil::ToNative(m_folder.tmp, &g_startup_settings.temp_dir));

	// binary folder
	m_folder.binary.Empty();
	UnixUtils::FromNativeOrUTF8(g_startup_settings.binary_dir, &m_folder.binary);
	if (m_folder.binary.IsEmpty())
	{
		// The default is where our binary is
		m_folder.binary.Empty();
		rc = UnixUtils::FromNativeOrUTF8(g_startup_settings.our_binary, &m_folder.binary);
		if( OpStatus::IsError(rc))
		{
			printf("opera: failed to prepare binary directory\n");
			return rc;
		}

		uni_char * slash = uni_strrchr(m_folder.binary.DataPtr(), '/');
		if (slash)
			*slash = 0;
		else
			m_folder.binary.Empty(); // can continue without, but some features like plugins won't work
	}
	if (m_folder.binary.HasContent())
	{
		RETURN_IF_ERROR(Normalize(m_folder.binary));
		g_startup_settings.binary_dir.Empty();
		RETURN_IF_ERROR(PosixNativeUtil::ToNative(m_folder.binary, &g_startup_settings.binary_dir));
	}

	// fixed folder
#if defined(__FreeBSD__)
	RETURN_IF_ERROR(m_folder.fixed.Set(UNI_L("/usr/local/etc/")));
#else
	RETURN_IF_ERROR(m_folder.fixed.Set(UNI_L("/etc/")));
#endif
	RETURN_IF_ERROR(AppendPathSeparator(m_folder.fixed));

	// locale folder
	RETURN_IF_ERROR(m_folder.locale.Set(m_folder.read));
	RETURN_IF_ERROR(m_folder.locale.Append(UNI_L("locale/")));

#ifdef WIDGET_RUNTIME_SUPPORT
	// gadgets folder
	RETURN_IF_ERROR(PosixNativeUtil::UniGetEnv(m_folder.gadgets, "OPERA_WIDGETS_DIR"));
	if (m_folder.gadgets.IsEmpty())
	{
		RETURN_IF_ERROR(m_folder.gadgets.Set(m_folder.home));
		RETURN_IF_ERROR(m_folder.gadgets.Append(UNI_L(".opera-widgets/")));
	}
	RETURN_IF_ERROR(Normalize(m_folder.gadgets));
#endif // WIDGET_RUNTIME_SUPPORT

	// update checker path
	RETURN_IF_ERROR(m_folder.update_checker.Set(m_folder.binary));
	RETURN_IF_ERROR(m_folder.update_checker.Append(UNI_L("opera_autoupdatechecker")));

	return SetupPredefinedFolders(m_folder);
}


// static
OP_STATUS UnixOpDesktopResources::PrepareFolder(const OpStringC& folder, OpString& target)
{
	if (folder.IsEmpty())
		return OpStatus::ERR;

	OpString expanded;
	if (OpStatus::IsSuccess(UnixUtils::UnserializeFileName(folder.CStr(), &expanded)))
	{
		RETURN_IF_ERROR(target.Set(expanded));
		StringUtils::Unquote(target);
		if (!StringUtils::StartsWith(target, UNI_L("/"), FALSE))
			target.Insert(0, UNI_L("/"));
	}
	return OpStatus::OK;
}

// static 
OP_STATUS UnixOpDesktopResources::SetupPredefinedFolders(Folders& folders)
{
	OpString config_dir;
	RETURN_IF_ERROR(PosixNativeUtil::UniGetEnv(config_dir, "XDG_CONFIG_HOME")); // default user/.config
	if (config_dir.IsEmpty())
	{
		RETURN_IF_ERROR(config_dir.Set(folders.home));
		RETURN_IF_ERROR(config_dir.Append(UNI_L(".config")));
	}
	if (config_dir.HasContent())
	{
		RETURN_IF_ERROR(config_dir.Append(UNI_L("/user-dirs.dirs")));

		OpFile file;
		if (OpStatus::IsSuccess(file.Construct(config_dir.CStr())) && OpStatus::IsSuccess(file.Open(OPFILE_READ)))
		{
			OpString8 buf;
			while (OpStatus::IsSuccess(file.ReadLine(buf)))
			{
				if (buf.IsEmpty())
					break;
				buf.Strip(TRUE, FALSE);
				if (buf.Find("#") == 0)
				{
					continue;
				}
				else // Strip off is anything after a #?
				{
					OpAutoVector<OpString> strings;
					OpString line;

					if (OpStatus::IsError(UnixUtils::FromNativeOrUTF8(buf.CStr(), &line)))
						continue;

					if (OpStatus::IsSuccess(StringUtils::SplitString( strings, line, '=')))
					{
						if (!strings.Get(0))
							continue;

						if (strings.Get(0)->Compare(UNI_L("XDG_DESKTOP_DIR")) == 0) 
						{
							if (strings.Get(1))
							{
								OpStatus::Ignore(PrepareFolder(strings.Get(1)->CStr(), folders.desktop));
							}
						}
						else if (strings.Get(0)->Compare(UNI_L("XDG_DOWNLOAD_DIR")) == 0)
						{
							if (strings.Get(1))
							{
								OpStatus::Ignore(PrepareFolder(strings.Get(1)->CStr(), folders.download));
							}
						}
						else if (strings.Get(0)->Compare(UNI_L("XDG_DOCUMENTS_DIR")) == 0)
						{
							if (strings.Get(1))
							{
								OpStatus::Ignore(PrepareFolder(strings.Get(1)->CStr(), folders.documents));
							}
						}
						else if (strings.Get(0)->Compare(UNI_L("XDG_MUSIC_DIR")) == 0)
						{
							if (strings.Get(1))
							{
								OpStatus::Ignore(PrepareFolder(strings.Get(1)->CStr(), folders.music));
							}
						}
						else if (strings.Get(0)->Compare(UNI_L("XDG_PICTURES_DIR")) == 0)
						{
							if (strings.Get(1))
							{
								OpStatus::Ignore(PrepareFolder(strings.Get(1)->CStr(), folders.pictures));
							}
						}
						else if (strings.Get(0)->Compare(UNI_L("XDG_VIDEOS_DIR")) == 0)
						{
							if (strings.Get(1))
							{
								OpStatus::Ignore(PrepareFolder(strings.Get(1)->CStr(), folders.videos));
							}
						}
						else if (strings.Get(0)->Compare(UNI_L("XDG_PUBLICSHARE_DIR"))==0)
						{
							if (strings.Get(1))
							{
								OpStatus::Ignore(PrepareFolder(strings.Get(1)->CStr(), folders.public_folder));
							}
						}
					}
				}
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS UnixOpDesktopResources::GetResourceFolder(OpString &folder)
{
	return folder.Set(m_folder.read);
}

OP_STATUS UnixOpDesktopResources::GetBinaryResourceFolder(OpString &folder)
{
	return folder.Set(m_folder.binary);
}

OP_STATUS UnixOpDesktopResources::GetFixedPrefFolder(OpString &folder)
{
	return folder.Set(m_folder.fixed);
}

OP_STATUS UnixOpDesktopResources::GetGlobalPrefFolder(OpString &folder)
{
	return GetFixedPrefFolder(folder);
}

OP_STATUS UnixOpDesktopResources::GetLargePrefFolder(OpString &folder, const uni_char *profile_name)
{
	if( profile_name && *profile_name)
	{
		// Remove whitespaces in front and at end, convert remaining to 
		// underscores and lowercase everything.
		
		OpString tmp;
		RETURN_IF_ERROR(tmp.Set(profile_name));
		tmp.Strip();

		uni_char* p = tmp.CStr();
		if( !p )
		{
			return OpStatus::ERR_NULL_POINTER;
		}

		while(*p )
		{
			if( isspace(*p) )
				*p = '_';
			else
			{
				*p = towlower(*p);
			}
			p++;
		}

		RETURN_IF_ERROR(folder.Set(m_folder.home));
		RETURN_IF_ERROR(folder.Append(UNI_L(".")));
		RETURN_IF_ERROR(folder.Append(tmp));
		RETURN_IF_ERROR(AppendPathSeparator(folder));

		return OpStatus::OK;
	}
	else
	{
		return folder.Set(m_folder.write);
	}
}

OP_STATUS UnixOpDesktopResources::GetDefaultShareFolder(OpString &folder)
{
	OP_ASSERT(m_folder.home.HasContent());
	return folder.Set(m_folder.home);
}

OP_STATUS UnixOpDesktopResources::GetSmallPrefFolder(OpString &folder, const uni_char *profile_name)
{
	return GetLargePrefFolder(folder, profile_name);
}

OP_STATUS UnixOpDesktopResources::GetTempPrefFolder(OpString &folder, const uni_char* profile_name)
{
	return GetLargePrefFolder(folder, profile_name);
}

OP_STATUS UnixOpDesktopResources::GetTempFolder(OpString &folder)
{
	return folder.Set(m_folder.tmp);
}

OP_STATUS UnixOpDesktopResources::GetDesktopFolder(OpString &folder)
{
	if (m_folder.desktop.HasContent())
		return folder.Set(m_folder.desktop);
	else
		return folder.Set(m_folder.home);
}

OP_STATUS UnixOpDesktopResources::GetDocumentsFolder(OpString &folder)
{
	if (m_folder.documents.HasContent())
		return folder.Set(m_folder.documents);
	else
		return folder.Set(m_folder.home);
}

OP_STATUS UnixOpDesktopResources::GetDownloadsFolder(OpString &folder)
{
	if (m_folder.download.HasContent())
		return folder.Set(m_folder.download);
	else
		return folder.Set(m_folder.home);
}


OP_STATUS UnixOpDesktopResources::GetPicturesFolder(OpString &folder)
{
	if (m_folder.pictures.HasContent())
		return folder.Set(m_folder.pictures);
	else
		return folder.Set(m_folder.home);
}

OP_STATUS UnixOpDesktopResources::GetPublicFolder(OpString &folder)
{
	if (m_folder.public_folder.HasContent())
		return folder.Set(m_folder.public_folder);
	else
		return GetDocumentsFolder(folder);
}


OP_STATUS UnixOpDesktopResources::GetVideosFolder(OpString &folder)
{
	if (m_folder.videos.HasContent())
		return folder.Set(m_folder.videos);
	else
		return folder.Set(m_folder.home);
}


OP_STATUS UnixOpDesktopResources::GetMusicFolder(OpString &folder)
{
	if (m_folder.music.HasContent())
		return folder.Set(m_folder.music);
	else
		return folder.Set(m_folder.home);
}


OP_STATUS UnixOpDesktopResources::GetLocaleFolder(OpString &folder)
{
	return folder.Set(m_folder.locale);
}

OpFolderLister* UnixOpDesktopResources::GetLocaleFolders(const uni_char* locale_root)
{
	OpFolderLister* lister;

	if (OpStatus::IsError(OpFolderLister::Create(&lister)) ||
		OpStatus::IsError(lister->Construct(locale_root, UNI_L("*"))))
	{
		OP_DELETE(lister);
		return 0;
	}

	return lister;
}

OP_STATUS UnixOpDesktopResources::GetLanguageFolderName(OpString &folder_name, LanguageFolderType type, OpStringC lang_code)
{
	if (lang_code.HasContent())
	{
		RETURN_IF_ERROR(folder_name.Set(lang_code));
	}
	else
	{
		if (m_language_code.IsEmpty())
		{
			if (OpStatus::IsError(GetLanguageFromLocale(m_folder.locale, m_language_code)) ||
				m_language_code.IsEmpty())
			{
				// We assume there is a folder named "en" in the locale directory
				RETURN_IF_ERROR(m_language_code.Set("en"));
			}
		}
		RETURN_IF_ERROR(folder_name.Set(m_language_code));
	}
	if (type == REGION)
	{
		// strip territory/script/variant code - only language code is used
		// inside region folder (DSK-346940)
		int pos = folder_name.FindFirstOf(UNI_L('-'));
		if (pos == KNotFound)
			pos = folder_name.FindFirstOf(UNI_L('_'));
		if (pos > 0)
			folder_name.Delete(pos);
	}
	return OpStatus::OK;
}

OP_STATUS UnixOpDesktopResources::GetLanguageFileName(OpString& file_name, OpStringC lang_code)
{
	// file name = folder name + extension
	RETURN_IF_ERROR(GetLanguageFolderName(file_name, LOCALE, lang_code));
	return file_name.Append(".lng");
}

OP_STATUS UnixOpDesktopResources::GetCountryCode(OpString& country_code)
{
	country_code.Empty();

	OpString languages;
	RETURN_IF_ERROR(PosixSystemInfo::LookupUserLanguages(&languages));

	// use first territory code as country code
	int pos = languages.FindFirstOf('_');
	if( pos != KNotFound )
	{
		int len = languages.Length();
		if( len > pos+2 )
		{
			RETURN_IF_ERROR(country_code.Set(languages.CStr() + pos + 1, 2));
		}
	}
	return OpStatus::OK;
}

// static
OP_STATUS UnixOpDesktopResources::GetHomeFolder(OpString &folder)
{
	return folder.Set(m_folder.home);
}

OP_STATUS UnixOpDesktopResources::EnsureCacheFolderIsAvailable(OpString &folder)
{
	// UNIX does not use a separate cache folder. At least not yet.
	return OpStatus::OK;
}



OP_STATUS UnixOpDesktopResources::ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len)
{
	OpString outbuffer;
	RETURN_IF_ERROR(UnixUtils::UnserializeFileName(in, &outbuffer));
	if (outbuffer.HasContent())
		uni_strlcpy(out, outbuffer.CStr(), out_max_len);
	else
		*out = 0;

	return OpStatus::OK;
}



OP_STATUS UnixOpDesktopResources::SerializeFileName(const uni_char* in, uni_char* out, INT32 out_max_len)
{
	uni_char* res = UnixUtils::SerializeFileName(in);
	*out = 0;
	if (!res)
        return OpStatus::ERR_NO_MEMORY;
	if (uni_strlen(res) < size_t(out_max_len))
	{
		uni_strcpy(out, res);
		OP_DELETEA(res);
		return OpStatus::OK;
	}
	else
	{
		OP_DELETEA(res);
		return OpStatus::ERR_OUT_OF_RANGE;
	}
}


BOOL UnixOpDesktopResources::IsSameVolume(const uni_char* path1, const uni_char* path2)
{
	return TRUE; // TODO: maybe we should return something else?
}


OP_STATUS UnixOpDesktopResources::GetOldProfileLocation(OpString& old_profile_path)
{
	old_profile_path.Empty();
	return OpStatus::OK;
}

#ifdef WIDGET_RUNTIME_SUPPORT
OP_STATUS UnixOpDesktopResources::GetGadgetsFolder(OpString &folder)
{
	return folder.Set(m_folder.gadgets);
}
#endif // WIDGET_RUNTIME_SUPPORT


OP_STATUS UnixOpDesktopResources::GetUpdateCheckerPath(OpString &checker_path)
{
	return checker_path.Set(m_folder.update_checker);
}

static bool IsValidLanguageFileVersion(const uni_char* path)
{
	INT32 version = -1;

	OpFile file;
	if (OpStatus::IsSuccess(file.Construct(path)) && OpStatus::IsSuccess(file.Open(OPFILE_READ)))
	{
		for( INT32 i=0; i<100; i++ )
		{
			OpString8 buf;
			if( OpStatus::IsError(file.ReadLine(buf)) )
				break;
			buf.Strip(TRUE, FALSE);
			if (buf.CStr() && strncasecmp(buf.CStr(), "DB.version", 10) == 0)
			{
				for (const char *p = &buf.CStr()[10]; *p; p++)
				{
					if (isdigit(*p))
					{
						sscanf(p,"%d",&version);
						break;
					}
				}
				break;
			}
		}
	}

	return version >= 811;
}


static OP_STATUS GetLanguageFromLocale(const OpString& locale_folder, OpString& matched_folder)
{
	OpString languages;
	PosixSystemInfo::LookupUserLanguages(&languages);

	OpAutoVector<OpString> list;
	StringUtils::SplitString(list, languages, ',');

	// Normalize strings. Opera stores language files in contry specific
	// directories named like "zh-cn" while OS locale information is typically
	// saved as zh_cn" We also have a character case issue. That is dealt with 
	// later.
	
	for( UINT32 i=0; i<list.GetCount(); i++ )
	{
		OpString* text = list.Get(i);
		int pos = text->FindFirstOf('_');
		if( pos == KNotFound )
		{
			pos = text->FindFirstOf('-');
		}
		if( pos != KNotFound )
		{
			text->CStr()[pos] = '-';

			OpString* s = OP_NEW(OpString, ());
			s->Set(*text);

			for( pos++; pos < text->Length(); pos++ )
			{
				text->CStr()[pos] = tolower(text->CStr()[pos]);
				s->CStr()[pos] = toupper(s->CStr()[pos]);
			}

			list.Insert(i+1,s);
			i++;
		}
	}

	for( UINT32 i=0; i<list.GetCount() && matched_folder.IsEmpty(); i++ )
	{
		OpString candidate;
		RETURN_IF_ERROR(candidate.Set(locale_folder));
		RETURN_IF_ERROR(candidate.AppendFormat(UNI_L("%s"), list.Get(i)->CStr()));
 
		if (g_startup_settings.debug_locale)
		{
			OpString8 tmp;
			tmp.Set(candidate.CStr());
			printf("opera: Locale. Testing directory: %s\n", tmp.CStr() );
		}

		PosixNativeUtil::NativeString candidate_filename (candidate.CStr());
		if( access(candidate_filename.get(), F_OK) == 0)
		{			
			OpFolderLister* lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*.lng"), candidate.CStr());
			if(lister)
			{
				for( BOOL more = lister->Next(); more; more=lister->Next())
				{
					const uni_char* c = lister->GetFullPath();
					if( c )
					{
						if( !IsValidLanguageFileVersion(c) )
						{
							if (g_startup_settings.debug_locale)
							{
								OpString8 tmp;
								tmp.Set(c);
								printf("opera: Locale. Too old language file: %s\n", tmp.CStr() );
							}
							continue;
						}
						else
						{
							if (g_startup_settings.debug_locale)
							{
								OpString8 tmp;
								tmp.Set(candidate.CStr());
								printf("opera: Locale. Using as default directory: %s\n", tmp.CStr() );
							}
							matched_folder.Set(list.Get(i)->CStr());
							break;
						}
					}
				}
			}
			OP_DELETE(lister);
		}
	}

	// Earlier we tested no match here and had a fallback for "english.lng" on the locale directory
	// That has bee removed. Time will test if that is smart [espen 2009-02-18]

	return OpStatus::OK;
}




