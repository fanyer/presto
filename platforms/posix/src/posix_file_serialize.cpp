/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Edward Welbourne (loosely based on unix/base/common/unixutils.cpp by Morten
 * Stenshorne, Espen Sand).
 */
#include "core/pch.h"
#if defined(POSIX_OK_FILE_UTIL) && defined(POSIX_SERIALIZE_FILENAME)

#include "platforms/posix/posix_file_util.h"
#include "platforms/posix/posix_native_util.h"
// Needs <unistd.h>, <sys/stat.h> and <string.h>; should get these from the system system.

// static
OP_STATUS PosixFileUtil::AppendEnvironment(OpString* dst, const char* env, bool remove_trailing_sep)
{
    OP_ASSERT(dst);

    if (env)
    {
        OpString value;
        RETURN_IF_ERROR(PosixNativeUtil::FromNative(env, &value));
        /* Avoid redundant separators at the border between
         * destination string and environment variable: */
        int value_len = value.Length();
        if (remove_trailing_sep && value_len > 0 && value[value_len-1] == PATHSEPCHAR)
            value_len--;
        if (value_len > 0 && value[0] == PATHSEPCHAR)
        {
            int dst_len = dst->Length();
            if (dst_len > 0 && (*dst)[dst_len-1] == PATHSEPCHAR)
                dst->Delete(dst_len-1);
        }
        RETURN_IF_ERROR(dst->Append(value, value_len));
    }
    return OpStatus::OK;
}

// static
OP_STATUS PosixFileUtil::DecodeEnvironment(const uni_char* src, OpString* dst)
{
	/* Note: len limits the length of the string; dst[] has enough space for
	 * this many characters, one of which needs to be the final '\0'. */
	if (dst == NULL)
		return OpStatus::ERR;

	if (!dst->CStr()) // start with an empty string:
		RETURN_IF_ERROR(dst->Set(""));
	else // clear the string without deallocating the buffer:
		dst->Delete(0);

	bool begin = true;
	if (src)
		while (src[0])
			// IMPORTANT: keep the following in sync with EncodeEnvironment():, below
			switch (src[0])
			{
			case ':': case ',':
				RETURN_IF_ERROR(dst->Append(src++, 1));
				begin = true;
				break;

			case '\\':
				/* Preserve the backslash and whatever immediately follows it.
				 * We don't need to worry about parsing escape sequences, as
				 * long as we suppress the meaning of any immediately following
				 * character (including backslash) which we would otherwise
				 * treat specially here.
				 */
				RETURN_IF_ERROR(dst->Append(src, 2));
				if (*++src) ++src;
				break;

			case '$': {
				src++;
				begin = false;
				size_t siz = 0;
				if (src[0] == '{')
				{
					const uni_char *end = ++src;
					while (end[0] && end[0] != '}')
						end += (end[0] == '\\' && end[1]) ? 2 : 1;

					if (end[0] == '\0')
						return OpStatus::ERR;

					siz = end - src;
				}
				else if (src[0] == '_' || Unicode::IsAlpha(src[0]))
				{
					// Scan for end of word: [alpha|_][alpha|numer|_]*
					for (siz = 1; src[siz] == '_' || Unicode::IsAlphaOrDigit(src[siz]); siz++)
						/* skip */ ;
				}
				else if (src[0] == '$') // $$ is current process id.
					siz = 1;

				if (siz == 1 && src[0] == '$')
				{
					RETURN_IF_ERROR(dst->AppendFormat(UNI_L("%u"), (unsigned int)getpid()));
				}
				else if (siz > 0)
				{
					OP_ASSERT(src[0] != '$');
					PosixNativeUtil::NativeString name(src, siz);
					RETURN_IF_ERROR(name.Ready());

					const uni_char* next_src = src+siz;
					if (src[-1] == '{') ++next_src;
					RETURN_IF_ERROR(AppendEnvironment(dst, op_getenv(name.get()), next_src[0] == PATHSEPCHAR));
				}

				if (src[-1] == '{') // to skip over the '}' as well as the contents
					src++;
				src += siz;
			}	break;

#ifdef POSIX_EXPAND_TILDE
			case '~':
				// Could also support ~name (name's HOME), ~+ (PWD) and ~- (OLDPWD).
				if (begin && (!src[1] || src[1] == '$' || src[1] == PATHSEPCHAR))
					if (const char *const env = op_getenv("HOME"))
					{
                        RETURN_IF_ERROR(AppendEnvironment(dst, env, (++src)[0] == PATHSEPCHAR));
						break;
					}
				// else: fall-through, leaving ~ undecoded.
#define FF_TO_NEXT_OF ":,\\$~"
#else // !POSIX_EXPAND_TILDE
#define FF_TO_NEXT_OF ":,\\$"
#endif // POSIX_EXPAND_TILDE
			default:
			{
				size_t count = uni_strcspn(src+1, UNI_L(FF_TO_NEXT_OF))+1;
				RETURN_IF_ERROR(dst->Append(src, count));
				src += count;
				begin = false;
				break;
			}
#undef FF_TO_NEXT_OF
			}

	return OpStatus::OK;
}

// static
OP_STATUS PosixFileUtil::EncodeEnvironment(const uni_char* src, uni_char** answer)
{
	if (answer == 0)
		return OpStatus::ERR;
	*answer = 0;

	char const * const envname[] =
	/* Supported environment variables.
	 *
	 * Any that may be a prefix of others should appear at lower index in the
	 * array, since it's scanned from high index downwards.  Any that expands to
	 * "/" will be ignored.
	 */
		{ "HOME",					// User's home directory on system; may reduce to a "~"
		  "OPERA_DIR",				// where our read-only resources live
		  // TODO: tweak-ify the following
		  "OPERA_HOME",				// see SetGogiBaseFoldersL in posix_native_util.cpp
		  "OPERA_BINARYDIR",		// where desktop stores binaries
		  "OPERA_PERSONALDIR" };	// where desktop stores the user's config
	int env = sizeof(envname) / sizeof(envname[0]);
	const size_t mylen = uni_strlen(src);
	size_t head = 0;
	while (env-- > 0)
	{
		if (const char *value = op_getenv(envname[env]))
		{
			head = op_strlen(value);

			// Strip trailing slashes
			while (head > 0 && value[head-1] == PATHSEPCHAR)
				head--;

			// Skip replacement if the variable points to the root directory
			if (!head)
				continue;

			// If it matches all, or a prefix followed by "/", use it:
			if ((mylen == head ||
				 (mylen > head && src[head] == PATHSEPCHAR)) &&
				uni_strncmp(src, value, head) == 0)
				break;

			// Also check its true path for a match:
#if _MAX_PATH < PATH_MAX
# error "Your system must declare the macro _MAX_PATH to have at least the size of PATH_MAX. Here we use the stdlib function realpath() which expect an output buffer of that length!"
#endif // _MAX_PATH < PATH_MAX
			char full[_MAX_PATH+1]; // ARRAY OK 2011-09-06 eddy
			if (char *ans = realpath(value, full))
			{
				OP_ASSERT(ans == full);
				head = op_strlen(ans);
				OP_ASSERT(head == 0 || ans[head-1] != PATHSEPCHAR);
				if ((mylen == head ||
					 (mylen > head && src[head] == PATHSEPCHAR)) &&
					uni_strncmp(src, ans, head) == 0)
					break;
			}
		}
		// else TODO: try relevant OpFileFolder
	}
	size_t len = 1 + mylen; // 1 for trailing '\0', plus src
	if (env < 0) // No match
		head = 0;
# ifdef POSIX_EXPAND_TILDE
	else if (env == 0)
		len -= head - 1; // replacing src[:head] with 1 for the '~' encoding $HOME
# endif
	else
	{
		size_t replace = 1 + op_strlen(envname[env]); // 1 for '$', plus length of name.
		if (replace > head)
			len += replace - head;
		else
			len -= head - replace;
	}

	// Scan src[head:] for stuff we need to escape:
	bool begin = (head == 0);
	size_t in = head;
	for (; src[in]; in++)
		// Anticipate length of the following transcription:
		switch (src[in])
		{
		case ':': case ',': begin = true; break;
		case '\\': if (src[1+in]) in++; break; // No need to escape the next character.
		case '$': len++; begin = false; break; // Provide space for a '\\'
# ifdef POSIX_EXPAND_TILDE
		case '~':
			if (begin)
			{
				const uni_char &next = src[in+1];
				if (!next || next == '$' || next == PATHSEPCHAR)
				{
					len++; // We need to escape it
					begin = false;
					break;
				}
			}
			begin = false;
			break;
# endif
		case PATHSEPCHAR:
			if (in > 0 && src[in - 1] == PATHSEPCHAR)
				len--; // We'll skip the redundant separators
			// deliberate fall-through
		default: begin = false; break;
		}

	uni_char *result = OP_NEWA(uni_char, len);
	if (result == 0)
		return OpStatus::ERR_NO_MEMORY;

	size_t out = 0;
	if (env < 0)
		/* skip */ ;
# ifdef POSIX_EXPAND_TILDE
	else if (env == 0)
		result[out++] = '~';
# endif
	else
	{
		result[out++] = '$';
		for (in = 0; envname[env][in];)
			result[out++] = envname[env][in++];
	}

	in = head;
	while (src[in])
		// IMPORTANT: keep in sync with DecodeEnvironment (and the above)
		switch (src[in])
		{
		case ':': case ',':
			// FIXME: {c,sh}ould we also do the envname check here ?
			begin = true;
			result[out++] = src[in++];
			break;

		case '\\': // no need to escape the character after it :-)
			result[out++] = src[in++];
			if (src[in])
				result[out++] = src[in++];
			break;

		case '$': // literal '$': escape it !
			result[out++] = '\\';
			result[out++] = src[in++];
			begin = false;
			break;

# ifdef POSIX_EXPAND_TILDE
		case '~':
			if (begin)
			{
				const uni_char &next = src[in+1];
				if (!next || next == '$' || next == PATHSEPCHAR)
				{
					result[out++] = '\\';
					result[out++] = src[in++];
					begin = false;
					break;
				}
			}
			else // Treat ~ mid-name as an ordinary character.
			{
				result[out++] = src[in++];
				break;
			}
			// fall through
# endif // POSIX_EXPAND_TILDE

		case PATHSEPCHAR:
			if (out > 0 && result[out - 1] == PATHSEPCHAR)
				out--; // Avoid redundant separators
			// Deliberate fall-through
		default:
			result[out++] = src[in++];
			begin = false;
			break;
		}
	result[out++] = '\0';
	OP_ASSERT(out == len);

	*answer = result;
	return OpStatus::OK;
}

#endif // POSIX_OK_FILE_UTIL && POSIX_SERIALIZE_FILENAME
