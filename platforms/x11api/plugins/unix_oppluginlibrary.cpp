/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/x11api/plugins/unix_oppluginlibrary.h"

#include "modules/opdata/UniStringUTF8.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/x11api/plugins/elf_identifier.h"
#include "platforms/x11api/utils/unix_process.h"

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <dlfcn.h>

// Include a safe version of basename (see basename(3) in glibc manual
// for more information)
#ifdef __GLIBC__
# include <string.h>
#else
# include <libgen.h>
#endif

#ifndef RTLD_DEEPBIND
# define RTLD_DEEPBIND 0
#endif

NPP_HandleEventProcPtr UnixOpPluginLibrary::s_orig_plugin_funcs_event;

OP_STATUS UnixOpPluginLibrary::Create(OpPluginLibrary** out_library, const UniString& path)
{
	if (path.Length() == 0)
		return OpStatus::ERR;

	size_t stringsize = MB_LEN_MAX * path.Length() + 1;
	OpAutoArray<char> native_path (OP_NEWA(char, stringsize));
	RETURN_OOM_IF_NULL(native_path.get());
	RETURN_IF_ERROR(PosixNativeUtil::ToNative(path, native_path.get(), stringsize));

	OpAutoPtr<UnixOpPluginLibrary> library (OP_NEW(UnixOpPluginLibrary, ()));
	RETURN_OOM_IF_NULL(library.get());

	RETURN_IF_ERROR(library->Init(native_path.get()));
	*out_library = library.release();

	return OpStatus::OK;
}

OP_STATUS UnixOpPluginLibrary::EnumerateLibraries(OtlList<LibraryPath>* out_library_paths, const UniString& suggested_paths)
{
	// Note: the spaces around the inner template is required due to:
	// warning: ‘>>’ operator will be treated as two right angle brackets in C++0x [-Wc++0x-compat]
	OpAutoPtr< OtlCountedList<UniString> > dirpaths (suggested_paths.Split(':'));
	if (!dirpaths.get())
		return OpStatus::ERR_NO_MEMORY;

	for (OtlCountedList<UniString>::ConstRange path = dirpaths->ConstAll(); path; ++path)
	{
		size_t stringsize = MB_LEN_MAX * path->Length() + 1;
		OpAutoArray<char> dirpath(OP_NEWA(char, stringsize));
		RETURN_OOM_IF_NULL(dirpath.get());
		RETURN_IF_ERROR(PosixNativeUtil::ToNative(*path, dirpath.get(), stringsize));

		DIR* dir = opendir(dirpath.get());
		if (!dir)
			continue;

		for (dirent* entry = readdir(dir); entry; entry = readdir(dir))
		{
			if (entry->d_type == DT_DIR)
				continue;

			LibraryPath library;
			library.path = *path;

			if (OpStatus::IsError(GetComponentType(dirpath.get(), entry->d_name, library.type)))
				continue;

			OP_STATUS status = library.path.Append(PATHSEPCHAR);
			if (OpStatus::IsSuccess(status))
			{
				status = PosixNativeUtil::FromNative(entry->d_name, library.path);
				if (OpStatus::IsSuccess(status))
					status = out_library_paths->Append(library);
			}
			if (OpStatus::IsMemoryError(status))
			{
				closedir(dir);
				return OpStatus::ERR_NO_MEMORY;
			}
		}

		closedir(dir);
	}

	return OpStatus::OK;
}

OP_STATUS UnixOpPluginLibrary::GetComponentType(const char* dirpath, const char* filename, OpComponentType& type)
{
	size_t dirpath_len = op_strlen(dirpath);
	size_t filename_len = op_strlen(filename);
	OpAutoArray<char> fullpath (OP_NEWA(char, dirpath_len + filename_len + 2));
	RETURN_OOM_IF_NULL(fullpath.get());

	op_strlcpy(fullpath.get(), dirpath, dirpath_len + 1);
	fullpath[dirpath_len] = PATHSEPCHAR;
	op_strlcpy(fullpath.get() + dirpath_len + 1, filename, filename_len + 1);

	ElfIdentifier identifier;
	RETURN_IF_ERROR(identifier.Init(fullpath.get()));

	// We only load dynamic libraries
	if (identifier.GetFileType() != ElfIdentifier::ET_DYN)
		return OpStatus::ERR;

	// We start by assuming a native plugin
	type = COMPONENT_PLUGIN;

	// On x86-64 Linux and FreeBSD it's possible to run (non-native) LinuxIA32 plugins
#ifdef ARCHITECTURE_IA32
# if (defined(SIXTY_FOUR_BIT) && defined(__linux__)) || defined(__FreeBSD__)
	// Linux ELF files use either ELFOSABI_NONE or ELFOSABI_GNU
	// FreeBSD ELF files always use ELFOSABI_FREEBSD, so should not be caught
	// by this
	if ((identifier.GetOSABI() == ElfIdentifier::ELFOSABI_NONE ||
		identifier.GetOSABI() == ElfIdentifier::ELFOSABI_GNU) &&
		identifier.GetMachine() == ElfIdentifier::EM_386)
		type = COMPONENT_PLUGIN_LINUX_IA32;
# elif defined(THIRTY_TWO_BIT)
	// Only accept binaries intended for this architecture (to remove 64-bit
	// plugins from the list)
	if (identifier.GetMachine() != ElfIdentifier::EM_386)
		return OpStatus::ERR;
# endif // (SIXTY_FOUR_BIT && __linux) || __FreeBSD__
#endif // ARCHITECTURE_IA32

	return OpStatus::OK;
}

UnixOpPluginLibrary::UnixOpPluginLibrary()
	: m_handle(NULL), m_is_flash(false)
{
	op_memset(&m_functions, 0, sizeof(m_functions));
}

OP_STATUS UnixOpPluginLibrary::Init(const char* path)
{
	if (op_getenv("OPERA_PLUGINWRAPPER_DEBUG"))
		fprintf(stderr, "operapluginwrapper [%d]: opening %s\n", getpid(), path);

	OpString8 process_name;
	RETURN_IF_ERROR(process_name.AppendFormat("opera:%s", basename(path)));
	RETURN_IF_ERROR(UnixProcess::SetCallingProcessName(process_name));

	/* This should really call PosixFileUtil::RealPath(), but that
	 * doesn't exist in the pluginwrapper.
	 */
	/* NULL as second argument to realpath() is in POSIX.1-2008, but
	 * not in POSIX.1-2001.  However, there is no other way to ensure
	 * that the second argument is large enough to hold the complete
	 * path name.  It is likely that there exists systems where the
	 * maximum path name length is unbounded, and it is surely
	 * optimistic to assume that a compile-time constant will be valid
	 * for all systems (and not be excessively large).
	 *
	 * NULL as second argument is documented as supported at least in
	 * FreeBSD 8.1 and 9.0, and since glibc 2.3.
	 */
	char *real_path = realpath(path, NULL);

	const char * use_path = path;
	if (real_path)
		use_path = real_path;
	m_handle = dlopen(use_path, RTLD_LAZY|RTLD_DEEPBIND);
	// 'real_path' is allocated by realpath()
	free(real_path);

	if (!m_handle)
		return OpStatus::ERR;

	RETURN_IF_ERROR(LoadFunction(m_functions.NP_GetValue, "NP_GetValue"));
	RETURN_IF_ERROR(LoadFunction(m_functions.NP_GetMIMEDescription, "NP_GetMIMEDescription"));
	RETURN_IF_ERROR(LoadFunction(m_functions.NP_Initialize, "NP_Initialize"));
	RETURN_IF_ERROR(LoadFunction(m_functions.NP_Shutdown, "NP_Shutdown"));

	const char* p = strrchr(path, '/');
	if (p)
		m_is_flash = strstr(p, "flash") != NULL;

	return ParseMimeDescription();
}

template<class T> OP_STATUS UnixOpPluginLibrary::LoadFunction(T*& function, const char* symbol)
{
	function = (T*)dlsym(m_handle, symbol);
	return function ? OpStatus::OK : OpStatus::ERR_NOT_SUPPORTED;
}

UnixOpPluginLibrary::~UnixOpPluginLibrary()
{
	if (m_handle)
		dlclose(m_handle);
}

OP_STATUS UnixOpPluginLibrary::GetContentTypes(OtlList<UniString>* out_content_types)
{
	out_content_types->Clear();

	OpAutoPtr<OpHashIterator> it (m_content_types.GetIterator());
	if (!it.get())
		return OpStatus::ERR_NO_MEMORY;

	for (OP_STATUS stat = it->First(); stat == OpStatus::OK; stat = it->Next())
	{
		RETURN_IF_ERROR(out_content_types->Append(static_cast<ContentType*>(it->GetData())->content_type));
	}

	return OpStatus::OK;
}

OP_STATUS UnixOpPluginLibrary::GetExtensions(OtlList<UniString>* out_extensions, const UniString& content_type)
{
	out_extensions->Clear();

	UniString data(content_type);
	const uni_char* key = data.Data(TRUE);
	if (!key)
		return OpStatus::ERR_NO_MEMORY;

	ContentType* type;
	RETURN_IF_ERROR(m_content_types.GetData(key, &type));

	for (OtlList<UniString>::ConstRange ext = type->extensions.ConstAll(); ext; ++ext)
	{
		RETURN_IF_ERROR(out_extensions->Append(*ext));
	}

	return OpStatus::OK;
}

NPError UnixOpPluginLibrary::Initialize(PluginFunctions plugin_funcs, const BrowserFunctions browser_funcs)
{
	NPError error = m_functions.NP_Initialize(browser_funcs, plugin_funcs);
	if (m_is_flash && error == NPERR_NO_ERROR)
	{
		s_orig_plugin_funcs_event = plugin_funcs->event;
		plugin_funcs->event = UnixOpPluginLibrary::WrapperForEventsToFlash;
	}
	return error;
}

/* static */ short UnixOpPluginLibrary::WrapperForEventsToFlash(NPP instance, void* event)
{
	XEvent* xevent = static_cast<XEvent*>(event);
	if (xevent->type == ButtonPress || xevent->type == ButtonRelease)
	{
		// Filter middle clicks out. Flash gets into a deadlock when requesting content of PRIMARY selection.
		// We block in the plugin call and during that block flash hangs. It hangs regardless of what process
		// holds the PRIMARY (opera or other). Seems Firefox does just the same workaround. See DSK-375488
		if (xevent->xbutton.button == 3)
			return NPERR_NO_ERROR;
	}

	return s_orig_plugin_funcs_event(instance, event);
}

void UnixOpPluginLibrary::Shutdown()
{
	return m_functions.NP_Shutdown();
}

int UnixOpPluginLibrary::GetValue(void* instance, int variable, void* value)
{
	return m_functions.NP_GetValue(instance, (NPPVariable)variable, value);
}

OP_STATUS UnixOpPluginLibrary::GetStringValue(NPPVariable variable, UniString& value)
{
	char *utf8_value = 0;

	NPError err = m_functions.NP_GetValue(NULL, variable, (void *) &utf8_value);
	if (err != NPERR_NO_ERROR)
		return OpStatus::ERR;

	// Check for empty string
	if (!utf8_value || !*utf8_value)
		return OpStatus::OK;

	return UniString_UTF8::FromUTF8(value, utf8_value);
}

OP_STATUS UnixOpPluginLibrary::ParseMimeDescription()
{
	const char* mimedesc = m_functions.NP_GetMIMEDescription();
	if (!mimedesc || !*mimedesc)
		return OpStatus::ERR;

	for (const char* end = 0; mimedesc; mimedesc = end ? end + 1 : 0)
	{
		OpAutoPtr<ContentType> contenttype (OP_NEW(ContentType, ()));
		if (!contenttype.get())
			return OpStatus::ERR_NO_MEMORY;

		end = op_strchr(mimedesc, ':');
		if (!end)
			break;

		RETURN_IF_ERROR(UniString_UTF8::FromUTF8(contenttype->content_type, mimedesc, end - mimedesc));

		mimedesc = end + 1;
		end = op_strchr(mimedesc, ':');
		if (!end)
			break;

		RETURN_IF_ERROR(ParseExtensions(contenttype->extensions, mimedesc, end));

		mimedesc = end + 1;
		end = op_strchr(mimedesc, ';');
		RETURN_IF_ERROR(UniString_UTF8::FromUTF8(contenttype->description, mimedesc, end ? end - mimedesc : -1));

		const uni_char* key = contenttype->content_type.Data(TRUE);
		if (!key)
			return OpStatus::ERR_NO_MEMORY;

		// Don't take the same type more than once
		if (m_content_types.Contains(key))
			continue;

		RETURN_IF_ERROR(m_content_types.Add(key, contenttype.get()));
		contenttype.release();
	}

	return OpStatus::OK;
}

OP_STATUS UnixOpPluginLibrary::ParseExtensions(OtlList<UniString>& extensions, const char* string, const char* string_end)
{
	for (const char* end; string != string_end; string = end)
	{
		for (; string < string_end && (*string == ',' || *string == ' '); string++) {}
		for (end = string; end < string_end && *end != ','; end++) {}

		UniString extension;
		RETURN_IF_ERROR(UniString_UTF8::FromUTF8(extension, string, end - string));
		RETURN_IF_ERROR(extensions.Append(extension));
	}

	return OpStatus::OK;
}

OP_STATUS OpPluginLibrary::Create(OpPluginLibrary** out_library, const UniString& path)
{
	return UnixOpPluginLibrary::Create(out_library, path);
}

OP_STATUS OpPluginLibrary::EnumerateLibraries(OtlList<LibraryPath>* out_library_paths, const UniString& suggested_paths)
{
	return UnixOpPluginLibrary::EnumerateLibraries(out_library_paths, suggested_paths);
}

#endif // X11API && NS4P_COMPONENT_PLUGINS
