/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Edward Welbourne (based on Espen Sand's earlier unix/base/common/unixfile.cpp)
 */
#include "core/pch.h"
#ifdef POSIX_OK_MISC
# ifndef POSIX_INTERNAL
#define POSIX_INTERNAL(X) X
# endif

#ifdef POSIX_OK_NATIVE
#include "modules/locale/oplanguagemanager.h" // Str, for Perror
# ifdef POSIX_SET_GOGI_BASE_FOLDER
#include "platforms/gogi/src/gogi_opera.h" // declares SetGogiBaseFoldersL
# endif
#endif

#if defined(POSIX_OK_LOG) || defined(POSIX_OK_NATIVE)
// For logging we need PosixNativeUtil::AffirmativeEnvVar() only
#include "platforms/posix/posix_native_util.h"
#elif defined(POSIX_OK_LOCALE)
// need setlocale for PosixModule::InitLocale()
#include <locale.h>
#endif

#ifdef POSIX_OK_LOCALE
// static
void PosixModule::InitLocale()
{
	op_setlocale(LC_ALL, "");
	if (!op_setlocale(LC_NUMERIC, "POSIX")) // way too much assumes this !
		op_setlocale(LC_NUMERIC, "C"); // fall-back, just in case
}
#endif // POSIX_OK_LOCALE

#if defined(POSIX_OK_LOG) || defined(POSIX_OK_NATIVE)
// static
bool PosixNativeUtil::AffirmativeEnvVar(const char *name)
{
	const char *env = op_getenv(name);
	if (env == 0)
		return false;

	// Ignore any leading space:
	while (op_isspace(env[0]))
		env++;

	// Empty (or pure space) values are false:
	if (env[0] == '\0')
		return false;

	// Locale-independent "affirmative" values:

	/* Recognise any number as true if non-zero, else false.  Ignore any
	 * trailing cruft after the longest thing we can sensibly recognise as a
	 * number.  For our purposes, any exponent is irrelevant; 0.000e42 is still
	 * zero, hence false.  A sign is likewise irrelevant.
	 */
	const char *num = (env[0] == '-' || env[0] == '+') ? env + 1 : env;
	if (op_isdigit(num[0]))
	{
		bool hex = num[0] == '0' && op_tolower((unsigned char) num[1]) == 'x';
		if (hex)
			num += 2; // skip leading 0x

		while (num[0] == '0')
			num++;

		if (!hex && num[0] == '.')
			while (*++num == '0')
				/* skip */ ;

		// We know num[0] is not a '0'
		return hex ? op_isxdigit(num[0]) : op_isdigit(num[0]);
	}

	if (op_strnicmp(env, "yes", 3) == 0 ||
		op_strnicmp(env, "true", 4) == 0)
		return true;

	// We should clearly recognise some translated strings, too !
	// PosixNativeUtil::TransientLocale ignoreme(LC_MESSAGES); // not used

	return false;
}
#endif // POSIX_OK_LOG or POSIX_OK_NATIVE

#ifdef POSIX_OK_PROCESS
PosixNativeUtil::CommandSplit::CommandSplit(const char* str, const char* delim, const char* quote, const char* escape)
	: m_split(NULL)
{
	char* copy;

	if (str && (copy = op_strdup(str)))
	{
		/*
		 * Un-escape escaped sequences, replace occurences of delim with
		 * '\0', and collect the pointers to the resulting sub-strings.
		 */
		OpVector<char> tokens;
		char* src = copy;
		char in_string = 0;

		while (*src)
		{
			char* token = src;

			if (tokens.Add(token) != OpStatus::OK)
			{
				op_free(copy);
				return;
			}

			while (*src && (in_string || !op_strchr(delim, *src)))
			{
				BOOL escaped = FALSE;

				if (op_strchr(escape, *src) && (!in_string || in_string == *(src + 1)))
					src++, escaped = TRUE;

				if (!escaped && op_strchr(quote, *src) && (!in_string || in_string == *src))
					in_string ^= *src++;
				else if ((*token = *src) != 0)
					token++, src++;
			}

			if (*src)
				src++;

			*token = '\0';
		}

		/*
		 * Copy vector contents to a NULL-terminated array.
		 */
		if (!(m_split = OP_NEWA(char*, tokens.GetCount() + 1)))
		{
			op_free(copy);
			return;
		}

		for (size_t i = 0; i < tokens.GetCount(); i++)
			m_split[i] = tokens.Get(i);
		m_split[tokens.GetCount()] = NULL;
	}
}

PosixNativeUtil::CommandSplit::~CommandSplit()
{
	if (m_split)
	{
		op_free(m_split[0]);
		OP_DELETEA(m_split);
	}
}
#endif // POSIX_OK_PROCESS

#ifdef POSIX_OK_NATIVE
// static
bool PosixNativeUtil::AffirmativeEnvVar(const uni_char *name)
{
	NativeString nom(name);
	return AffirmativeEnvVar(nom.get());
}

// static
uni_char *PosixNativeUtil::UniGetEnvL(const char *name)
{
	const char * env = op_getenv(name);
	if (env == 0) // Variable wasn't set.
		return 0;

	OpString val;
	LEAVE_IF_ERROR(FromNative(env, &val));
	size_t space = 1 + val.Length();
	uni_char *const ans = OP_NEWA(uni_char, space);
	if (ans == 0)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	uni_strlcpy(ans, val.CStr(), space);
	return ans;
}

// static
uni_char *PosixNativeUtil::UniGetEnvL(const char *name, size_t namelen)
{
	const char *env;
	{
		OpString8 outman;
		char *cut = outman.Reserve(namelen + 1); // +1 for the '\0'
		if (cut == 0)
			LEAVE(OpStatus::ERR_NO_MEMORY);

		op_memcpy(cut, name, namelen);
		cut[namelen] = '\0';
		env = op_getenv(cut);
	} // let outman fall out of scope
	if (env == 0)
		return 0;

	OpString val;
	LEAVE_IF_ERROR(FromNative(env, &val));
	size_t space = 1 + val.Length();
	uni_char *const ans = OP_NEWA(uni_char, space);
	if (ans == 0)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	uni_strlcpy(ans, val.CStr(), space);
	return ans;
}

// static
uni_char *PosixNativeUtil::UniGetEnvL(const uni_char *name)
{
	NativeString nom(name);
	ANCHOR(NativeString, nom);
	LEAVE_IF_ERROR(nom.Ready());
	return UniGetEnvL(nom.get());
}

// static
uni_char *PosixNativeUtil::UniGetEnvL(const uni_char *name, size_t namelen)
{
	NativeString nom(name, namelen);
	ANCHOR(NativeString, nom);
	LEAVE_IF_ERROR(nom.Ready());
	return UniGetEnvL(nom.get());
}

// static
OP_STATUS PosixNativeUtil::UniGetEnv(OpString &value, const char *name)
{
	const char *const env = op_getenv(name);
	if (env == 0)
	{
		value.Empty();
		return OpStatus::OK;
	}
	return FromNative(env, &value);
}

// static
OP_STATUS PosixNativeUtil::UniGetEnv(OpString &value, const char *name, size_t namelen)
{
	const char *env;
	{
		OpString8 outman;
		char *cut = outman.Reserve(namelen + 1); // +1 for the '\0'
		RETURN_OOM_IF_NULL(cut);

		op_memcpy(cut, name, namelen);
		cut[namelen] = '\0';
		env = op_getenv(cut);
	} // let outman fall out of scope

	value.Empty();
	if (env == 0)
		return OpStatus::OK;

	return FromNative(env, &value);
}

// static
OP_STATUS PosixNativeUtil::UniGetEnv(OpString &value, const uni_char *name)
{
	NativeString nom(name);
	RETURN_IF_ERROR(nom.Ready());
	return UniGetEnv(value, nom.get());
}

// static
OP_STATUS PosixNativeUtil::UniGetEnv(OpString &value, const uni_char *name, size_t namelen)
{
	NativeString nom(name, namelen);
	RETURN_IF_ERROR(nom.Ready());
	return UniGetEnv(value, nom.get());
}

// static
void PosixNativeUtil::Perror(Str::LocaleString message, const char *sysfunc, int err)
{
	bool some = false;
	OpString text;
	if (OpStatus::IsSuccess(g_languageManager->GetString(message, text)) &&
		text.HasContent())
	{
		NativeString out(text.CStr());
		if (OpStatus::IsSuccess(out.Ready()))
		{
			fputs(out.get(), stderr);
			some = true;
		}
	}
	if (sysfunc)
	{
		if (some)
			fputc(' ', stderr);
		fputc('[', stderr);
		fputs(sysfunc, stderr);
		fputc(']', stderr);
		some = true;
	}

	if (some)
		fputs(": ", stderr);

	fputs(strerror(err), stderr);
	fputc('\n', stderr);
}

# ifdef POSIX_SET_GOGI_BASE_FOLDER
/** Default setting of base folders for GOGI, based on environment.
 *
 * Sets readdir to the value of the OPERA_DIR environment variable, if set.
 * Sets writedir to that of OPERA_HOME, if set; else, if HOME is set, uses
 * $HOME/.gogi_opera/.  Relies on caller to set homedir suitably for itself.
 *
 * If environment variables are unset, relevant parameters are left unchanged.
 * Trailing path separators are ensured on parameters when set.  Caller is
 * assumed to have initialized the three parameters to the default values
 * specified by GOGI_HOME_FOLDER, GOGI_READ_FOLDER and GOGI_WRITE_FOLDER.
 *
 * @param homedir Full path of base folder for "home" purposes.
 * @param readdir Full path of read-only resource folder (system installation).
 * @param writedir Full path of modifiable resource folder (user configuration).
 */
extern void SetGogiBaseFoldersL(OpString* homedir, OpString* readdir, OpString* writedir)
{
    OP_ASSERT(homedir && readdir && writedir);
	OP_ASSERT(homedir->IsEmpty() || (*homedir)[homedir->Length() -1] == PATHSEPCHAR);
	OP_ASSERT(readdir->IsEmpty() || (*readdir)[readdir->Length() -1] == PATHSEPCHAR);
	OP_ASSERT(writedir->IsEmpty() || (*writedir)[writedir->Length() -1] == PATHSEPCHAR);
	OpString temp;

	LEAVE_IF_ERROR(PosixNativeUtil::UniGetEnv(temp, "OPERA_HOME"));
	if (temp.IsEmpty())
	{
		LEAVE_IF_ERROR(PosixNativeUtil::UniGetEnv(temp, "HOME"));
		if (temp.HasContent())
		{
			if (temp[temp.Length() - 1] != PATHSEPCHAR)
				temp.AppendL(PATHSEP);

			temp.AppendL(".gogi_opera");
		}
	}
	if (temp.HasContent())
	{
		if (temp[temp.Length() - 1] != PATHSEPCHAR)
			temp.AppendL(PATHSEP);
		writedir->SetL(temp);
		// homedir->SetL(temp);
	}

	LEAVE_IF_ERROR(PosixNativeUtil::UniGetEnv(temp, "OPERA_DIR"));
	if (temp.HasContent())
	{
		if (temp[temp.Length() - 1] != PATHSEPCHAR)
			temp.AppendL(PATHSEP);
		readdir->SetL(temp);
	}
}
# endif // POSIX_SET_GOGI_BASE_FOLDER
#endif // POSIX_OK_NATIVE
#endif // POSIX_OK_MISC
