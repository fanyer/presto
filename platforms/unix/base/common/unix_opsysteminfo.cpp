/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne, Espen Sand
 */
#include "core/pch.h"

#include "unix_opsysteminfo.h"
#include "unixutils.h"

#include "modules/pi/OpKeys.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#include <stdlib.h>
#include "modules/util/opfile/opfile.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "modules/viewers/plugins.h"
#include "platforms/x11api/plugins/unix_pluginpath.h"

# include <pthread.h>

#if defined (LIBUUID_SUPPORT)
# include "modules/libuuid/uuid.h"
#endif

#include "platforms/viewix/FileHandlerManager.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"
#include "platforms/unix/base/common/applicationlauncher.h"

#ifdef QUICK
# include "adjunct/quick/Application.h"
# include "adjunct/quick/managers/KioskManager.h"
#endif // QUICK

#include <locale.h>
#include <langinfo.h>
#include "modules/encodings/utility/charsetnames.h"

#if defined(USE_OP_THREAD_TOOLS) && defined(THREAD_SUPPORT) && !defined(POSIX_OK_THREAD)
pthread_t UnixOpSystemInfo::m_main_thread;
#endif // USE_OP_THREAD_TOOLS and THREAD_SUPPORT but not POSIX_OK_THREAD

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/utsname.h>

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/types.h> // CTL_HW and HW_PHYSMEM
#include <sys/sysctl.h>
#endif

#include "modules/util/handy.h"

#include "platforms/posix/posix_native_util.h"

#ifndef op_strcpy // core-1 still needs this ...
#define op_strcpy strcpy
#endif

UnixOpSystemInfo::UnixOpSystemInfo()
	: m_system_memory_in_mb(-1)
{
#if defined(USE_OP_THREAD_TOOLS) && defined(THREAD_SUPPORT) && !defined(POSIX_OK_THREAD)
	m_main_thread = pthread_self();
#endif // USE_OP_THREAD_TOOLS and THREAD_SUPPORT but not POSIX_OK_THREAD
}

OP_STATUS UnixOpSystemInfo::Construct()
{
	OpString8 uname;
	RETURN_IF_ERROR(uname.Set(op_getenv("OPERA_SYSTEM_UNAME")));
	if (uname.IsEmpty()) // DSK-181752
	{
		const size_t maxlen = sizeof(static_cast<utsname*>(0)->sysname); // a convoluted way to say sizeof(utsname::sysname)
		if (!uname.Reserve(maxlen))
			return OpStatus::ERR_NO_MEMORY;
		FILE * pipe = popen("uname -s", "r");
		fgets(uname.DataPtr(), maxlen, pipe);
		uname.Strip();
		if (pclose(pipe) == 0 && uname.HasContent())
			RETURN_IF_ERROR(g_env.Set("OPERA_SYSTEM_UNAME", uname.CStr()));
	}

	return PosixSystemInfo::Construct();
}

UINT32 UnixOpSystemInfo::GetPhysicalMemorySizeMB()
{
#if defined(_UNIX_DESKTOP_)
	if( m_system_memory_in_mb + 1 == 0 )
	{
# if defined(__linux__)
		size_t mem_in_kb = -1;
		bool stop = false;

		FILE *fp = fopen("/proc/meminfo", "r");
		if( fp )
		{
			char buf[100];
			while( !stop && fgets(buf, 100, fp ) != 0 )
			{
				for( char* p=buf; *p; p++ )
					*p = tolower(*p);

				if( strstr( buf, "memtotal") )
				{
					for( char* p=buf; *p; p++ )
					{
						if( isdigit(*p) )
						{
							sscanf( p, "%zd %*s", &mem_in_kb);
							stop = true;
							break;
						}
					}
				}
			}
			fclose(fp);
		}
		m_system_memory_in_mb = mem_in_kb > 0 ? mem_in_kb / 1024 : 0;
# elif defined(__FreeBSD__) || defined(__NetBSD__)
		int mib[2];
		mib[0]=CTL_HW;
		mib[1]=HW_PHYSMEM;
		size_t len;

		size_t mem_in_bytes = 0;

		if( sysctl(mib, 2, NULL, &len, NULL, 0) != -1 )
		{
			if( len == sizeof(mem_in_bytes) )
			{
				if( sysctl(mib, 2, &mem_in_bytes, &len, NULL, 0) == -1 )
				{
					mem_in_bytes = 0;
				}
			}
		}
		m_system_memory_in_mb = mem_in_bytes > 0 ? mem_in_bytes / 1048567 : 0;
# else
		OP_NEW_DBG("UnixOpSystemInfo::GetPhysicalMemorySizeMB()", "unimplemented");
		m_system_memory_in_mb = 128;
# endif
	}
	return max(m_system_memory_in_mb, 128U);

#else // _UNIX_DESKTOP_
	OP_NEW_DBG("UnixOpSystemInfo::GetPhysicalMemorySizeMB()", "unimplemented");
	return 2;
#endif // _UNIX_DESKTOP_
}

void UnixOpSystemInfo::GetProxySettingsL(const uni_char *protocol, int &enabled,
										 OpString &proxyserver)
{
#ifndef DYNAMIC_PROXY_UPDATE
	// Look for a proxy setting in the environment
	const char suffix[] = "_proxy";
	const size_t envlen = uni_strlen(protocol) + sizeof(suffix); // sizeof includes the '\0'
	char *environment = OP_NEWA_L(char, envlen);
	ANCHOR_ARRAY(char, environment);

	int i = 0;
	while (protocol[i])
	{
		const char here = protocol[i];
		OP_ASSERT(protocol[i] == (uni_char)here); // All protocol names are pure ASCII.
		environment[i++] = here;
	}
	op_strcpy(environment + i, suffix);

	const char *proxy = op_getenv(environment);
	if (proxy && strncmp(proxy, "http://", 7) == 0)
	{
		proxyserver.SetL(proxy + 7);
		enabled = TRUE;
	}
	else
	{
		proxyserver.Empty();
		enabled = FALSE;
	}
#endif // DYNAMIC_PROXY_UPDATE
}

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
void UnixOpSystemInfo::GetAutoProxyURLL(OpString *url, BOOL *enabled)
{
	// Should leave url as-is if the platform does not support system proxy settings.
	*enabled = FALSE;
}
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION

void UnixOpSystemInfo::GetProxyExceptionsL(OpString *exceptions, BOOL *enabled)
{
	// Should do nothing if the platform does not support system proxy settings
}

void UnixOpSystemInfo::GetPluginPathL(const OpStringC &dfpath, OpString &newpath)
{
	PluginPathList::Self()->ReadL(dfpath, newpath);

#ifdef DEBUG
	OpString pluginwrapper_dir;
	OpStatus::Ignore(g_folder_manager->GetFolderPath(OPFILE_STYLE_FOLDER, pluginwrapper_dir));

	newpath.Append(":");
	newpath.Append(pluginwrapper_dir);
	newpath.Append("/../../../../etc/plugins/");
# ifdef SIXTY_FOUR_BIT
	OpStatus::Ignore(newpath.Append("linux64"));
# else
	OpStatus::Ignore(newpath.Append("linux32"));
# endif
#endif // DEBUG
}

void UnixOpSystemInfo::GetDefaultJavaClassPathL(OpString &target)
{
	OpString resource_folder;
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_RESOURCES_FOLDER, resource_folder));
	target.SetL(resource_folder.CStr());
	target.AppendL(UNI_L("java/opera.jar"));
	target.AppendL(CLASSPATHSEP);
	target.AppendL(resource_folder.CStr());
	target.AppendL(UNI_L("java/lc.jar"));
}

void UnixOpSystemInfo::GetDefaultJavaPolicyFilenameL(OpString &target)
{
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_RESOURCES_FOLDER, target));
	target.AppendL(UNI_L("java/opera.policy"));
}

#if defined(USE_OP_THREAD_TOOLS) && defined(THREAD_SUPPORT) && !defined(POSIX_OK_THREAD)
BOOL UnixOpSystemInfo::IsInMainThread()
{
	return pthread_self() == m_main_thread;
}
#endif // USE_OP_THREAD_TOOLS and THREAD_SUPPORT but not POSIX_OK_THREAD

OP_STATUS UnixOpSystemInfo::GetFileHandler(const OpString* filename, OpString& contentType, OpString& handler)
{
	if( !filename )
		return OpStatus::ERR_NULL_POINTER;

	FileHandlerManager* manager = FileHandlerManager::GetManager();
	if (manager)
	{
		OpString handler_name; // We should expand this function to use this
		return manager->GetFileHandler(*filename,contentType,handler, handler_name);
	}

	return OpStatus::ERR;
}

OP_STATUS UnixOpSystemInfo::GetFileHandlers(const OpString& filename,
					    const OpString &content_type,
					    OpVector<OpString>& handlers,
					    OpVector<OpString>& handler_names,
					    OpVector<OpBitmap>& handler_icons,
					    URLType type,
					    UINT32 icon_size)
{
    FileHandlerManager* manager = FileHandlerManager::GetManager();
    if( manager )
		return manager->GetFileHandlers(filename,
										content_type,
										handlers,
										handler_names,
										handler_icons,
										type,
										icon_size);

	return OpStatus::ERR;
}

OP_STATUS UnixOpSystemInfo::OpenFileFolder(const OpStringC & file_path, BOOL treat_folders_as_files)
{
	FileHandlerManager* manager = FileHandlerManager::GetManager();

    if( manager )
		return manager->OpenFileFolder(file_path);

	return OpStatus::ERR;
}

OP_STATUS UnixOpSystemInfo::GetFileTypeInfo(const OpStringC& filename,
											const OpStringC& content_type,
											OpString & content_type_name,
											OpBitmap *& content_type_bitmap,
											UINT32 content_type_bitmap_size)
{
	FileHandlerManager* manager = FileHandlerManager::GetManager();
	if( manager )
		return manager->GetFileTypeInfo(filename,
										content_type,
										content_type_name,
										content_type_bitmap,
										content_type_bitmap_size);

	return OpStatus::ERR;
}

OP_STATUS UnixOpSystemInfo::GetFileTypeName(const uni_char* filename, uni_char *out, size_t out_max_len)
{
	if (filename == NULL || out == NULL)
		return OpStatus::ERR_NULL_POINTER;

	FileHandlerManager* manager = FileHandlerManager::GetManager();
	if (manager)
	{
		OpString content_type; // We should expand this function to use this
		OpString file_name;
		file_name.Set(filename);
		const uni_char * desc = manager->GetFileTypeName(file_name, content_type);

		if (desc)
			uni_strlcpy(out, desc, out_max_len);
		else if (out_max_len > 0)
			out[0] = 0;

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

OP_STATUS UnixOpSystemInfo::PlatformExecuteApplication(const uni_char* application,
													   const uni_char* args,
													   BOOL silent_errors)
{
	if (!application && !args) // No hope
		return OpStatus::ERR_NULL_POINTER;
	if (application && !application[0] && !args) // Empty command and no args 
		return OpStatus::ERR;

	OpString program;
	OpString parameters;
	RETURN_IF_ERROR(program.Set(application));
	RETURN_IF_ERROR(parameters.Set(args));
	program.Strip(TRUE, TRUE);

	// The way of specifying app name in the preferences dialog (for Downloads) is poorly specified -
	// That is - if we accept args in the 'Open with other application' input field - we cannot accept 
	// app names with spaces unless we change the preferences dialog.
	// So get the name of the executable this way:
	OpString program_executabe;
	RETURN_IF_ERROR(program_executabe.Set(program));
	int len = program.Length();
	for(int i=0; i<len; i++)
	{
		if( program[i] == ' ')
		{
			RETURN_IF_ERROR(program_executabe.Set(program.SubString(0), i));
			break;
		}
	}

	if(!program.IsEmpty() && g_op_system_info->GetIsExecutable(&program_executabe))
	{
		RETURN_IF_ERROR(PosixSystemInfo::PosixExecuteApplication(application, args, silent_errors));
	}
	else
	{
		OpString filename;
		if (parameters.IsEmpty() || parameters.Length()==0)
		{
			// Here we should find an appropriate application and simply try to view the content in 'application' which is not executable
			RETURN_IF_ERROR(filename.Set(program));
		}
		else
		{
			// Here parameters is regarded as file
			RETURN_IF_ERROR(filename.Set(parameters));
		}

		OpString tmp_filename; // Unquoted filename
		OpString tmp_content_type;
		OpString candidate_application;
		OpString tmp_application_name;

		// We need an unquoted version of the 'filename' for some of the function calls below
		RETURN_IF_ERROR(tmp_filename.Set(filename));
		RETURN_IF_ERROR(tmp_filename.ReplaceAll(UNI_L("\""), UNI_L(""))); // Unquote
		//RETURN_IF_ERROR(UnQuote(tmp_filename));

		FileHandlerManager* manager = FileHandlerManager::GetManager();
		if (manager)
		{
			manager->GetFileHandler(tmp_filename, tmp_content_type, candidate_application, tmp_application_name);
		}
		else
		{
			return OpStatus::ERR;
		}

		if(candidate_application.IsEmpty())
		{
			// If no handler was found, we can still open a file manager for directories
			if (FileHandlerManagerUtilities::IsDirectory(filename))
				return manager->OpenFileFolder(filename, FALSE);
			// No handler, give up
			return OpStatus::ERR;
		}

		// If the call should be validated then validate. Probably safest to always validate in this situation. Check this!
		if( !manager->ValidateHandler(tmp_filename, candidate_application))
		{
			return OpStatus::ERR;
		}


		// Quote the filename
		if (!StringUtils::StartsWith(filename, UNI_L("\""), FALSE) || !StringUtils::EndsWith(filename, UNI_L("\""), FALSE))
		{
			RETURN_IF_ERROR(filename.Insert(0, UNI_L("\"")));
			RETURN_IF_ERROR(filename.Append(UNI_L("\"")));
		}

		RETURN_IF_ERROR(PosixSystemInfo::PosixExecuteApplication(candidate_application.CStr(), filename.CStr(), silent_errors));
	}

	return OpStatus::OK;
}

OP_STATUS UnixOpSystemInfo::OpenURLInExternalApp(const URL& url)
{
    BOOL result = ApplicationLauncher::GenericTrustedAction(NULL/*Window*/, url);
    return result ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS UnixOpSystemInfo::GetProtocolHandler(const OpString& uri_string, OpString& protocol, OpString& handler)
{

	FileHandlerManager* manager = FileHandlerManager::GetManager();

	if (manager)
		return manager->GetProtocolHandler(uri_string, protocol, handler);

	return OpStatus::ERR;
}

void UnixOpSystemInfo::ComposeExternalMail(const uni_char* to, const uni_char* cc, const uni_char* bcc, const uni_char* subject, const uni_char* message, const uni_char* raw_address, MAILHANDLER mailhandler)
{
	if( mailhandler == MAILHANDLER_OPERA )
	{
		// Do nothing
	}
	else
	{
#ifdef QUICK
		if( !KioskManager::GetInstance()->GetNoMailLinks() )
#endif // QUICK
		{
			ApplicationLauncher::GenericMailAction( to, cc, bcc, subject, message, raw_address );
		}
	}
}

BOOL UnixOpSystemInfo::IsFullKeyboardAccessActive()
{
	/* Adam said: The idea of IsFullKeyboardAccess is that if it returns FALSE
	 * only edit box style field will be navigated when using the keyboard. When
	 * it returns TRUE all fields will be navigated. It's a setting in the MacOS
	 * we needed to support. I assume for all other OS's you can simply write an
	 * over riding function that returns TRUE.
	 *
	 * Hopefully, the documentation of the base class will be updated to reflect
	 * this position.  If any product really wants to return FALSE here, feel
	 * free to add a #if specific enough to leave the rest of Unix alone.
	 */
	return TRUE;
}

#ifdef SYNCHRONOUS_HOST_RESOLVING
# ifndef POSIX_OK_DNS
BOOL UnixOpSystemInfo::HasSyncLookup()
{
	return TRUE;

}
# endif // POSIX_OK_DNS
#endif // SYNCHRONOUS_HOST_RESOLVING

#ifdef PI_HOST_RESOLVER
OP_STATUS
UnixOpSystemInfo::GetDnsAddressAndSuffixes()
{
	if (m_dns_address.IsEmpty())
	{
		OpFile f;
		RETURN_IF_ERROR(f.Construct(UNI_L("/etc/resolv.conf")));
		RETURN_IF_ERROR(f.Open(OPFILE_READ));

		while (!f.Eof())
		{
			OpString8 line;
			RETURN_IF_ERROR(f.ReadLine(line));

			if (line.HasContent())
			{
				if (line.Compare("nameserver ", 11) == 0)
				{
					int a = line.FindLastOf('\n');
					if (a != KNotFound)
						line.Delete(a);

					RETURN_IF_ERROR(m_dns_address.Set(line.CStr() + 11));
				}
				else if (line.Compare("search ", 7) == 0)
				{
					int a = line.FindLastOf('\n');
					if (a != KNotFound)
						line.Delete(a);

					RETURN_IF_ERROR(m_dns_suffixes.Set(line.CStr() + 7));
				}
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS
UnixOpSystemInfo::GetDnsAddress(OpSocketAddress** dns_address)
{
	RETURN_IF_ERROR(GetDnsAddressAndSuffixes());

	if (m_dns_address.IsEmpty())
	{
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(OpSocketAddress::Create(dns_address));
	(*dns_address)->FromString(m_dns_address.CStr());
	return OpStatus::OK;
}

OP_STATUS
UnixOpSystemInfo::GetDnsSuffixes(OpString* result)
{
	RETURN_IF_ERROR(GetDnsAddressAndSuffixes());

	return result->Set(m_dns_suffixes);
}

#endif // PI_HOST_RESOLVER

#if !(defined(POSIX_CAP_USER_LANGUAGE) && defined(POSIX_OK_SYS_LOCALE))
OP_STATUS UnixOpSystemInfo::GetUserLanguages(OpString *result)
{
	result->Empty();

	/* Unix systems use several different environment variables for user
	 * locale settings:
	 *
	 *  LANGUAGE (GNU extension)
	 *        a colon-separated list of locales
	 *  LC_ALL
	 *        overrides all other LC_* defines
	 *  LC_MESSAGES
	 *        setting for the message locale, which is what we are interested in
	 *  LANG
	 *        fallback for missing LC_* defines
	 *
	 * Since the syntax is so similar (LANGUAGE is the only one with a list),
	 * just use the first one that is available.
	 */
	const char *env = op_getenv("LANGUAGE");
	if (!env)
		env = op_getenv("LC_ALL");
	if (!env)
		env = op_getenv("LC_MESSAGES");
	if (!env)
		env = op_getenv("LANG");
	if (!env)
	{
		// No data available, so just ignore it.
		// result->Empty(); // done above
		return OpStatus::OK;
	}

	/* The format is (usually) more or less what we want. At least on GNU
	 * systems. That is, "language[_territory][.codeset][@modifier]".  We clip
	 * everything after the full stop or at sign, keep the rest as is, and make
	 * the colon-separated list into a comma-separated one */
	OpString8 token_string;
	const char *next;
	do
	{
		const char *clip = strchr(env, '.');
		if (!clip)
			clip = strchr(env, '@');

		next = strchr(env, ':');

		// Find length of what we want to keep of this token
		int length;
		if (clip && (!next || clip < next))
		{
			length = clip - env;
		}
		else if (next)
		{
			length = next - env;
		}
		else
		{
			length = strlen(env);
		}

		OpString8 newtoken;
		RETURN_IF_ERROR(newtoken.AppendFormat(",%.*s", length, env));
		if (token_string.Find(newtoken) == KNotFound &&
			newtoken.CompareI(",C") != 0 &&
			newtoken.CompareI(",POSIX") != 0)
		{
			token_string.Append(newtoken);

			// Add language without territory
			int underscore = newtoken.FindFirstOf('_');
			if (underscore != KNotFound)
			{
				token_string.Append(newtoken, underscore);
			}
		}

		env = next + 1;
	} while (next);

	if (token_string.HasContent())
		return result->Set(token_string.CStr() + 1);

	// No valid data available, so just ignore it.
	return OpStatus::OK;
}

void
UnixOpSystemInfo::GetUserCountry(uni_char result[3])
{
	OpString s;
	GetUserLanguages(&s);

	// Pick out territory (see GetUserLanguage)
	int pos = s.FindFirstOf('_');
	if( pos != KNotFound )
	{
		int len = s.Length();
		if( len > pos+2 )
		{
			uni_strlcpy(result, &s.CStr()[pos+1], 3);
			return;
		}
	}

	result[0] = 0;
}
#endif // !(POSIX_CAP_USER_LANGUAGE && POSIX_OK_SYS_LOCALE)

#if defined(GUID_GENERATE_SUPPORT)
OP_STATUS
UnixOpSystemInfo::GenerateGuid(OpGuid &guid)
{
# if defined (LIBUUID_SUPPORT)
	uuid_t *	uuid_pt;
	size_t		uuid_size = sizeof(guid);
	void *		uuid_export_pt = &guid;
	if (uuid_create(&uuid_pt) != UUID_RC_OK)
	{
		return OpStatus::ERR;
	}
	if (uuid_make(uuid_pt, UUID_MAKE_V1 | UUID_MAKE_MC) != UUID_RC_OK)
	{
		return OpStatus::ERR;
	}
	if (uuid_export(uuid_pt, UUID_FMT_BIN, &uuid_export_pt, &uuid_size) != UUID_RC_OK)
	{
		return OpStatus::ERR;
	}
	uuid_destroy(uuid_pt);

	return OpStatus::OK;
# else // ! LIBUUID_SUPPORT
	return OpStatus::ERR_NOT_SUPPORTED;
# endif // ! LIBUUID_SUPPORT
}
#endif // GUID_GENERATE_SUPPORT

OpFolderLister* UnixOpSystemInfo::GetLocaleFolders()
{
	OpFolderLister* lister;

	OpString locale_folder;
	OP_STATUS err = g_folder_manager->GetFolderPath(OPFILE_LOCALE_FOLDER, locale_folder);
	if (OpStatus::IsError(err) ||
		OpStatus::IsError(OpFolderLister::Create(&lister)) ||
		OpStatus::IsError(lister->Construct(locale_folder, UNI_L("*"))))
	{
		OP_DELETE(lister);
		return 0;
	}

	return lister;
}

////////////////////////////////////////////////////////////////////////////////////

OpString UnixOpSystemInfo::GetLanguageFolder(const OpStringC &lang_code)
{
	OpString ret; 
	
	ret.Set(lang_code.CStr()); 
	
	return ret;
}

OP_STATUS UnixOpSystemInfo::GetBinaryPath(OpString *path)
{
	// Code copied from PlatformGadgetUtils::GetOperaExecPath(). At some stage
	// that function should be removed and use this. If the code used there
	// gets improved we should update what is below [espen 2010-04-13]
	if (!path)
		return OpStatus::ERR_NULL_POINTER;

	char buf[PATH_MAX];
	int  endpos = 0;

#if defined(__FreeBSD__) || defined(__NetBSD__)
	// *bsd
	char procfs_link[PATH_MAX];
	snprintf(procfs_link, sizeof(procfs_link), "/proc/%i/file", getpid());
    endpos = readlink(procfs_link, buf, sizeof(buf));
#else
	// linux
    endpos = readlink("/proc/self/exe", buf, sizeof(buf));
#endif

	if (endpos < 0)
		return OpStatus::ERR;
	return path->Set(buf, endpos);
}

#ifdef PI_POWER_STATUS
BOOL UnixOpSystemInfo::IsPowerConnected()
{
	OP_ASSERT(!"not implemented");
	return TRUE;
}

BYTE UnixOpSystemInfo::GetBatteryCharge()
{
	OP_ASSERT(!"not implemented");
	return 255;
}

BOOL UnixOpSystemInfo::IsLowPowerState()
{
	OP_ASSERT(!"not implemented");
	return FALSE;
}

#endif // PI_POWER_STATUS

OP_STATUS UnixOpSystemInfo::ExpandSystemVariablesInString(const uni_char* in, OpString* out)
{
    return UnixUtils::UnserializeFileName(in, out);
}

uni_char* UnixOpSystemInfo::SerializeFileName(const uni_char *path)
{
    return UnixUtils::SerializeFileName(path);
}

#ifdef OPSYSTEMINFO_CPU_FEATURES
unsigned int UnixOpSystemInfo::GetCPUFeatures()
{
	int ecx = 0, edx = 0;
	unsigned result = 0;
	const int SSE2_MASK = 0x4000000; // sse2 capability is written on the 27th bit of edx
	const int SSE3_MASK = 1; // // sse3 capability is written on the first bit of ecx
	const int SSSE3_MASK = 0x200; // // ssse3 capability is written on the 10th bit of ecx


	asm ("mov $1, %%eax\n\t"
#ifndef SIXTY_FOUR_BIT
		"pushl %%ebx\n\t"
#endif
		"cpuid\n\t"
#ifndef SIXTY_FOUR_BIT
		"popl %%ebx\n\t"
#endif
		: "=c"(ecx), "=d"(edx)
		:
		 :"%eax"
#ifdef SIXTY_FOUR_BIT
		  , "%ebx"
#endif
);

	if (ecx & SSE3_MASK)
	{
		result |= CPU_FEATURES_IA32_SSE3;
	}
	if (ecx & SSSE3_MASK)
	{
		result |= CPU_FEATURES_IA32_SSSE3;
	}
	if (edx & SSE2_MASK)
	{
		result |= CPU_FEATURES_IA32_SSE2;
	}

	return result;
}
#endif // OPSYSTEMINFO_CPU_FEATURES

OP_STATUS UnixOpSystemInfo::GetDefaultWindowTitle(OpString& title)
{
    switch (g_desktop_product->GetProductType()) {
    case PRODUCT_TYPE_OPERA_NEXT:
        return title.Set(UNI_L("Opera Next"));
    case PRODUCT_TYPE_OPERA_LABS:
        return title.Set(UNI_L("Opera Labs"));
    default:
        return title.Set(UNI_L("Opera"));
    }
}
