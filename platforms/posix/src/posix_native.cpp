/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Edward Welbourne (based on Espen Sand's earlier unix/base/common/unixfile.cpp)
 */
#include "core/pch.h"
#ifdef POSIX_OK_NATIVE
# ifdef POSIX_SYS_WCHAR_ENCODING
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/encodings/encoders/outputconverter.h"
# endif // POSIX_SYS_WCHAR_ENCODING
# ifdef POSIX_SYS_WCHAR_ENCODING_UTF32
#include "modules/unicode/utf32.h"
# endif // POSIX_SYS_WCHAR_ENCODING_UTF32

#include "platforms/posix/posix_native_util.h"
#if defined POSIX_SYS_WCHAR_ENCODING && !defined POSIX_SYS_WCHAR_ENCODING_UTF32
# include "modules/encodings/utility/opstring-encodings.h"
#endif // POSIX_SYS_WCHAR_ENCODING && !POSIX_SYS_WCHAR_ENCODING_UTF32

#if defined(POSIX_SYS_WCHAR_ENCODING) || defined(POSIX_SYS_WCHAR_ENCODING_UTF32)
#define POSIX_NEEDS_TOLK 1
#else
#define POSIX_NEEDS_TOLK 0
#endif

/** @file posix_native.cpp Implementation of FromNative and ToNative.
 *
 * These convert between the char* strings returned from assorted system
 * functions (e.g. to query the file system) and our internal uni_char strings.
 * It is assumed that the char* system strings are encoded as multi-byte strings
 * in whatever locale-dependent manner the system has selected, based on the
 * user's configuration.  For simple enough locales, these strings may actually
 * be single-byte encodings, but that is none of our concern.
 *
 * These functions require significant amounts of <code>\#if</code>-ery to cope
 * with the various relationships between \c uni_char and \c wchar_t. We can
 * use system library functions to convert between the system strings
 * (multi-byte) and wide character strings, which are assumed to be
 * locale-independent.
 *
 * If you're very lucky, your system library functions deal with a 16-bit
 * wchar_t encoded as UTF-16 - exactly the same as uni_char - and we can simply
 * use those functions to convert between system strings and uni_char.
 *
 * However, the system libraries on some systems (all Unixes we know of, in
 * fact) use 32-bit wchar_t interpreted (we believe) as UTF-32.  This obliges us
 * to perform a separate conversion between wchar_t and uni_char, using the
 * encodings module.  We would have to do the same if the system wchar_t were
 * 16-bit but encoded as something other than UTF-16.
 *
 * The case for Unix is further complicated by the fact that gcc supports a flag
 * to let us compile with a two-byte wchar_t (e.g. used by L"..." strings);
 * using this as uni_char simplifies our handling of UNI_L - but creates
 * problems when using the system library functions for handling wchar_t, since
 * these are compiled with 32-bit wchar_t, but our compilation run thinks the
 * type they're taking is our 16-bit uni_char.  Fortunately, aside from some
 * casts to wchar_t*, this last issue can be handled painlessly enough by some
 * judicious use of typedef and some aliasing of functions - see the system_*
 * type and functions immediately following.
 *
 * Furthermore, this code has to work before modules are fully initialized:
 * PosixLowLevelFile depends on it and several modules' initialization depends
 * on using OpFile.  When we need character converters, this code depends on the
 * encodings module being initialized.  We are thus obliged to bodge round this
 * by hoping that all files accessed before we're initialized properly have
 * plain enough (i.e. ASCII) names that can be faithfully handled by simple
 * transcription, without conversion.  This shall break if the user's home
 * directory's name uses non-ASCII characters, e.g. because the user-name is
 * localized.  The only fix for that is to sort out how we do initialization ...
 *
 * See TWEAK_POSIX_SYS_WCHAR_SIZE and TWEAK_POSIX_SYS_WCHAR_ENCODING for how to
 * control your selection of which variant of the following mess you actually
 * get, so that it matches what you need !
 */

#if defined(POSIX_SYS_WCHAR_ENCODING_UTF32)
class SystemToUni
{
public:
	bool IsLive() { return true; }
	int Convert(const void *src, int len, void *dest, int maxlen, int *read)
		{ UTF32Decoder decoder; return decoder.Convert(src, len, dest, maxlen, read); }
	OP_STATUS SetStringFromSystem(OpString *dest, const void *src, int bytes, int *bad)
	{
		dest->Empty();
		if (bad)
			*bad = 0;

		if (!src)
			return OpStatus::OK;

		/* Convert the string in chunks, since we cannot determine the size of the
		 * output string in advance */
		const size_t chunksize = 1024;
		uni_char buffer[UNICODE_DOWNSIZE(chunksize)]; /* ARRAY OK 2012-01-10 arjanl */
		const char *input = reinterpret_cast<const char *>(src);
		int processed_bytes;
		int written;
		UTF32Decoder decoder;

		int current_length = dest->Length();

		while (bytes > 0)
		{
			int new_length = current_length;
			written = decoder.Convert(input, bytes,
										buffer, chunksize,
										&processed_bytes);
			if (written == -1)
				return OpStatus::ERR_NO_MEMORY;

			if (written == 0 && processed_bytes == 0)
				break;

			new_length += UNICODE_DOWNSIZE(written);
			if (new_length + 1 > dest->Capacity())
			{
				RETURN_OOM_IF_NULL(dest->Reserve(2*new_length));
			}

			op_memcpy(dest->CStr() + current_length, buffer, written);
			dest->CStr()[new_length] = 0;

			current_length = new_length;
			input += processed_bytes;
			bytes -= processed_bytes;
		}

		return OpStatus::OK;
	}
};

class UniToSystem
{
public:
	UniToSystem() : m_encoder(FALSE) {}
	bool IsLive() { return true; }
	int Convert(const void *src, int len, void *dest, int maxlen, int *read)
		{ return m_encoder.Convert(src, len, dest, maxlen, read); }
private:
	UTF32Encoder m_encoder;
};
#elif defined(POSIX_SYS_WCHAR_ENCODING)
class LocalConverter
{
	CharConverter *const m_proxy;
protected:
	CharConverter *GetProxy() { return m_proxy; }
	LocalConverter(CharConverter *proxy) : m_proxy(proxy) {}
public:
	virtual ~LocalConverter() { OP_DELETE(m_proxy); }
	bool IsLive() { return m_proxy != 0; }
	int Convert(const void *src, int len, void *dest, int maxlen, int *read)
		{ return m_proxy ? m_proxy->Convert(src, len, dest, maxlen, read) : -1; }
};

class SystemToUni : public LocalConverter
{
	static OP_STATUS Construct(InputConverter* &proxy)
	{
		OP_ASSERT(op_strcmp(POSIX_SYS_WCHAR_ENCODING, "utf-32") != 0 ||
				  !"Tweak POSIX_SYS_WCHAR_ENCODING_UTF32 if your system uses UTF-32");
		return InputConverter::CreateCharConverter(POSIX_SYS_WCHAR_ENCODING, &proxy);
	}
	static InputConverter *CreateProxy()
	{
		InputConverter *proxy;
		return OpStatus::IsSuccess(Construct(proxy)) ? proxy : 0;
	}
public:
	SystemToUni() : LocalConverter(CreateProxy()) {}
	OP_STATUS SetStringFromSystem(OpString *dest, const void *src, int bytes, int *bad)
	{
		InputConverter* const proxy = static_cast<InputConverter*>(GetProxy());
		return SetFromEncoding(dest, proxy, src, bytes, bad);
	}
};

class UniToSystem : public LocalConverter
{
	static OP_STATUS Construct(OutputConverter* &proxy)
	{
		return OutputConverter::CreateCharConverter(POSIX_SYS_WCHAR_ENCODING, &proxy, FALSE, TRUE);
	}
	static OutputConverter *CreateProxy()
	{
		OutputConverter *proxy;
		return OpStatus::IsSuccess(Construct(proxy)) ? proxy : 0;
	}
public:
	UniToSystem() : LocalConverter(CreateProxy()) {}
};
#endif // POSIX_SYS_WCHAR_ENCODING

/* TODO: implement direct uni_char -> locale-specific encoding for mbs.  Depends
 * on being reliable about discovering the correct local-specific encoding.
 */

/** system_wchar_t
 *
 * An integral type of the same size as the wchar_t with which the system
 * libraries were compiled.
 */
#if WCHAR_SYSTEMLIB_SIZE == 2
typedef uni_char system_wchar_t;
#elif WCHAR_SYSTEMLIB_SIZE == 4
typedef UINT32 system_wchar_t;
#else
#error "Unsupported size for system wchar_t"
#endif

#if defined(OWN_POSIX_WCMB_LEN) || defined(POSIX_NATIVE_OOM_FALLBACK)
/* TODO: call system mbtowc/mbstowcs with NULL input at suitable points to
 * ensure shift state is sane at start of processing a given string.
 */

# if defined(__GNUC_PATCHLEVEL__) && __GNUC__+0 == 3 && \
	(__GNUC_MINOR__+0 < 3 || (__GNUC_MINOR__+0 == 3 && __GNUC_PATCHLEVEL__+0 < 6))
#define FuncConst /* gcc 3.3.[14] and possibly others can't cope: bug 313792, ITVSDK-1550 */
# else
#define FuncConst const
# endif

/* When system_wchar_t is bigger than wchar_t, passing a system_wchar_t to
 * wctomb would truncate it to a compile-time wchar_t, in accordance with
 * wctomb's declaration; the actual implementation being called would thus not
 * get the correct-sized datum we tried to pass it.  We thus need to call the
 * implementation via a pointer that, at our compile-time, thinks it is of the
 * type we actually need to match the system libraries. */
typedef int wctomb_t(char *, system_wchar_t);
static FuncConst wctomb_t &system_wctomb = *reinterpret_cast<wctomb_t*>(&wctomb);
// Doing similar for mbtowc avoids gcc's "type-punned pointer" warnings.
typedef int mbtowc_t(system_wchar_t *, const char *, size_t);
static FuncConst mbtowc_t &system_mbtowc = *reinterpret_cast<mbtowc_t*>(&mbtowc);
// and eliminates casts; mbstowcs and wcstombs might benifit similarly.
#endif // OWN_POSIX_WCMB_LEN || POSIX_NATIVE_OOM_FALLBACK

// <bodge> Fix up platforms with ANSI but not fully POSIX mbstowcs and wcstombs:
#ifdef OWN_POSIX_WCMB_LEN // TWEAK_POSIX_OWN_WCMB_LEN
/* The names of these functions are so chosen as to give at least half a
 * chance that, if the relevant functions ever do get defined, their names
 * will clash with these and get us a warning that'll let us retire this
 * bodge on affected platforms ...
 *
 * For documentation, see #else clause, below.
 */

// Wide Character String, as Multi-Byte, LENgth
static size_t wcsmblen(const system_wchar_t *str)
{
	// Fall back on using wctomb() directly.
	char buf[MB_CUR_MAX]; // ARRAY OK 2010-08-12 markuso
	system_wctomb(0, 0); // reset shift state
	size_t ans = 0;
	while (str[0])
	{
		int need = system_wctomb(buf, str++[0]);
		if (need < 0)
			return (size_t) -1;
		else if (need == 0)
			break;

		ans += need;
	}
	return ans;
}

// Multi-Byte STRing LENgth
static size_t mbstrlen(const char *mbstr)
{
	size_t ans = 0;
	system_mbtowc(0, 0, 0); // initialise state
	while (mbstr[0])
	{
		system_wchar_t token;
		int took = system_mbtowc(&token, mbstr, MB_CUR_MAX);
		if (took < 0)
			return (size_t) -1;
		else if (took == 0 || token == 0)
			break;

		ans++;
		mbstr += took;
	}

	return ans;
}
#else
# ifndef SIZE_MAX
#define SIZE_MAX (size_t)-1
#  ifdef __GNUC__
#warning "Using substitute SIZE_MAX; did you need to set TWEAK_POSIX_OWN_WCMB_LEN ?"
// It happens that the system (FreeBSD 4) on which we need the latter lacks the former ...
#  endif
# endif
/** wcsmblen - determine multi-byte len of a wide character string.
 *
 * @param str The wide character string.
 * @return -1 on failure, else the number of bytes needed for a multi-byte
 * character string capable of representing str[:limit]. */
static inline size_t wcsmblen(const system_wchar_t *str)
	{ return wcstombs(0, reinterpret_cast<const wchar_t *>(str), SIZE_MAX); }
/** mbstrlen - determine number of characters in a multi-byte string.
 *
 * @param mbstr The multi-byte character string.
 * @return -1 if invalid, else the number of characters represented by the
 * multi-byte character tokens making up the string.
 */
static inline size_t mbstrlen(const char *mbstr)
	{ return mbstowcs(0, mbstr, SIZE_MAX); }
#endif // TWEAK_POSIX_OWN_WCMB_LEN
// </bodge>

static OP_STATUS UniToNative(const uni_char *str, size_t len,
							 OpString8 *target,
							 char *buffer, size_t space)
{
	PosixNativeUtil::TransientLocale ignoreme(LC_CTYPE);
	OP_ASSERT(!target == !!buffer); // exactly one of them is NULL
	int err = errno = 0;
#if WCHAR_SYSTEMLIB_SIZE == 2 && !POSIX_NEEDS_TOLK
	if (buffer && len + 1 == 0) // Deal with the easy case:
	{
		errno = 0;
		size_t end = wcstombs(buffer, reinterpret_cast<const wchar_t *>(str), space);
		err = errno;
		if (end + 1 == 0)
			return err == EILSEQ ? OpStatus::ERR_PARSING_FAILED : OpStatus::ERR;

		buffer[end >= space ? space - 1 : end] = '\0'; // just to be on the safe side ...
		return end < space ? OpStatus::OK : OpStatus::ERR_OUT_OF_RANGE;
	}
#endif

#if POSIX_NEEDS_TOLK
	UniToSystem tolk;
#endif
	size_t eat = uni_strlen(str);
	if (len + 1 && len < eat)
		eat = len;

	// +1 for an initial BOM, +1 for a final zero; utf-32-len <= utf-16-len + 1:
	system_wchar_t *tmp = OP_NEWA(system_wchar_t, eat+2); // ARRAY OK - Eddy/2008/July/28
	if (tmp)
	{
		size_t out = eat;
#if !POSIX_NEEDS_TOLK
		const int off = 0;
#else
		int off = 0, read = 2 * eat;
		if (tolk.IsLive())
		{
			read = 0;
			int got = tolk.Convert(str, 2 * eat, tmp,
								   WCHAR_SYSTEMLIB_SIZE * (eat + 1),
								   &read);
			// May include a BOM as first "output" character, in tmp.
			off = got >= WCHAR_SYSTEMLIB_SIZE && (tmp[0] == 0xfeff || tmp[0] == 0xffef);
			OP_ASSERT(read % 2 == 0 && got >= 0 && got % WCHAR_SYSTEMLIB_SIZE == 0);
			// got is measured in bytes:
			OP_ASSERT(got >= 0 && size_t(got) <= WCHAR_SYSTEMLIB_SIZE * (eat + 1));
			tmp[got / WCHAR_SYSTEMLIB_SIZE] = 0;
		}
		else // bodge: should only happen before InitL() or after Destroy()
#endif // POSIX_SYS_WCHAR_ENCODING
		{
#if WCHAR_SYSTEMLIB_SIZE == 2
			uni_strlcpy(tmp, str, eat + 2);
			if (len == eat) tmp[len] = 0;
#else
			/* Fall-back handling by size coercion, assuming data to be plain
			 * ASCII or equally simple: */
			tmp[eat] = 0;
			while (out-- > 0)
			{
				// Is this utf-16 to utf-32 conversion safe ?
				// TODO: how easy is it to do this more faithfully ?
				OP_ASSERT(!(str[out] & (1 << 15))); // TODO: right test ?
				tmp[out] = str[out]; // size coercion
			}
#endif
		}

#if POSIX_NEEDS_TOLK
		// read is measured in bytes, out in uni_char:
		if (read >= 0 && (size_t)read >= 2 * eat)
#endif
		{
			errno = 0;
			out = wcsmblen(tmp + off);
			err = errno;
			if (++out) // -1 was error value, otherwise we want +1 for the '\0'
			{
				OP_STATUS ok = OpStatus::OK;
				if (buffer && space + 1 && out > space)
				{
					OpStatus::Ignore(ok);
					ok = OpStatus::ERR_OUT_OF_RANGE;
					out = space;
				}

				char *result = 0;
				if (buffer)
					result = buffer;
				else if (target)
				{
					int prior = target->Length();
					result = target->Reserve(out + prior) + prior;
				}

				if (result)
				{
					errno = 0;
					out = wcstombs(result, reinterpret_cast<wchar_t *>(tmp + off), out);
					err = errno;
				}
				else
					out = ~0;

				OP_DELETEA(tmp);
				if (out + 1)
					return ok;
			}
			else
				OP_DELETEA(tmp);

			return err == EILSEQ ? OpStatus::ERR_PARSING_FAILED : OpStatus::ERR;
		} // else: failed to digest all of the input :-( maybe bad string )
		OP_DELETEA(tmp);
	} // else: OOM

#ifdef POSIX_NATIVE_OOM_FALLBACK
	/* Fall back on using wctomb directly (and being paranoid about OOM) so as
	 * to avoid needing the transient buffer tmp. */
# if POSIX_NEEDS_TOLK
	if (!tolk.IsLive())
		return OpStatus::ERR;
# endif

	int need = 0;
	size_t out = 1; // for the dangling '\0'
	{
		OpString8 bufman; // Ensures proper deletion of buf when we're done with it.
		char *buf = bufman.Reserve(MB_CUR_MAX); // ARRAY OK - Eddy/2007/May/11
		if (buf == 0)
			return OpStatus::ERR_NO_MEMORY;

		system_wctomb(0, 0); // reset shift state
# if WCHAR_SYSTEMLIB_SIZE == 2 && !POSIX_NEEDS_TOLK
		for (const uni_char *p = str; p[0] && (need = system_wctomb(buf, p[0])) > 0; p++)
			out += need;
# else
		system_wchar_t one;
		int read, done = 0;
		while (done < eat &&
			   1 == tolk.Convert(str + done, 2 * (eat - done), &one,
								 WCHAR_SYSTEMLIB_SIZE, &read) &&
			   (errno = 0, need = system_wctomb(buf, one)) > 0)
		{
			out += need;
			OP_ASSERT(read % 2 == 0);
			done += read / 2;
		}
		err = errno;
		if (done < eat) // failed to ingest all of input
		{
			OP_ASSERT(!(need + 1));
			need = -1;
		}
# endif
	} // release buf via bufman's destructor
	if (need < 0) // invalid character
		return err == EILSEQ ? OpStatus::ERR_PARSING_FAILED : OpStatus::ERR;

	OP_STATUS ok = OpStatus::OK;
	if (buffer && space + 1 && out > space)
	{
		OpStatus::Ignore(ok);
		ok = OpStatus::ERR_OUT_OF_RANGE;
		out = space;
	}

	char *result = 0;
	if (buffer)
		result = buffer;
	else if (target)
	{
		int prior = target->Length();
		result = target->Reserve(out + prior) + prior;
	}

	if (result)
	{
		size_t i = 0;
		system_wctomb(0, 0); // reset shift state
# if WCHAR_SYSTEMLIB_SIZE == 2 && !POSIX_NEEDS_TOLK
		while (str[0] &&
			   (need = system_wctomb(result + i, str++[0])) > 0 &&
			   i + need < out)
			i += need;
# else
		system_wchar_t one;
		int done = 0, read;
		while (done < eat &&
			   1 == tolk.Convert(str + done, 2 * (eat - done),
								 reinterpret_cast<void*>(&one),
								 WCHAR_SYSTEMLIB_SIZE, &read) &&
			   (need = system_wctomb(result + i, one)) > 0)
		{
			i += need;
			done += read / 2;
		}
		OP_ASSERT(done >= eat); // else why didn't the previous loop fail ?
# endif
		OP_ASSERT(need >= 0); // else why didn't the previous loop fail ?
		OP_ASSERT(i < out);
		result[i] = '\0';

		// this whole fall-back may be redundant !
		// OP_ASSERT(!"Do we really ever come via here ?");
		return ok;
	}
#endif // POSIX_NATIVE_OOM_FALLBACK
	return OpStatus::ERR_NO_MEMORY;
}

// static
OP_STATUS PosixNativeUtil::ToNative(const uni_char *str, OpString8 *target, size_t len)
{
	if (target == 0)
		return OpStatus::ERR_NULL_POINTER;
	if (str == 0) // no conversion to do
		return OpStatus::OK;

	return UniToNative(str, len, target, NULL, 0);
}

// static
OP_STATUS PosixNativeUtil::ToNative(const UniString& str, char *target, size_t space, size_t len)
{
	const uni_char* stringptr;
	RETURN_IF_ERROR(str.CreatePtr(&stringptr, 1));
	return ToNative(stringptr, target, space, len);
}

// static
OP_STATUS PosixNativeUtil::ToNative(const uni_char *str,
									char *target, size_t space,
									size_t len)
{
	if (len == 0 || str == 0 || !(len + 1 || str[0])) // no conversion to do
		return OpStatus::OK;
	if (target == 0)
		return OpStatus::ERR_NULL_POINTER;
	if (1 + space < 2)
		return OpStatus::ERR_OUT_OF_RANGE;

	return UniToNative(str, len, NULL, target, space);
}

static OP_STATUS NativeToUni(const char *str, OpString *target,
							 uni_char * buffer, size_t space)
{
	PosixNativeUtil::TransientLocale ignoreme(LC_CTYPE);
	OP_ASSERT(!buffer == !!target); // exactly one of them is NULL
#if WCHAR_SYSTEMLIB_SIZE == 2 && !POSIX_NEEDS_TOLK
	// Deal with the easy case:
	if (buffer)
	{
		size_t out = mbstowcs(reinterpret_cast<wchar_t *>(buffer), str, space);
		if (out + 1 == 0)
			return OpStatus::ERR_PARSING_FAILED;

		else if (out < space)
		{
			buffer[out] = 0;
			return OpStatus::OK;
		}
		else
		{
			buffer[space - 1] = 0;
			return OpStatus::ERR_OUT_OF_RANGE;
		}
	}
#endif

#if POSIX_NEEDS_TOLK
	SystemToUni tolk;
#endif
	size_t out = mbstrlen(str);
	if (++out == 0) // -1 was the error value; otherwise, we want +1 for the terminator
		return OpStatus::ERR_PARSING_FAILED;

	OP_STATUS ok = OpStatus::OK;
	if (buffer && space + 1 && out > space)
	{
		OpStatus::Ignore(ok);
		ok = OpStatus::ERR_OUT_OF_RANGE;
		out = space;
	}

	system_wchar_t *tmp = OP_NEWA(system_wchar_t, out);
	if (tmp)
	{
		if (0 == (1 + mbstowcs(reinterpret_cast<wchar_t *>(tmp), str, out)))
		{
			OP_ASSERT(!"So how come mbstrlen didn't return -1 ?");
			OP_DELETEA(tmp);
			return OpStatus::ERR_PARSING_FAILED;
		}

#if POSIX_NEEDS_TOLK
		if (tolk.IsLive())
		{
			if (buffer)
			{
				int read = 0;
#ifdef DEBUG_ENABLE_OPASSERT
				int got =
#endif
					tolk.Convert(tmp, out * WCHAR_SYSTEMLIB_SIZE,
								 buffer, space * 2, &read);
				OP_DELETEA(tmp);
				OP_ASSERT(read % WCHAR_SYSTEMLIB_SIZE == 0);
				OP_ASSERT(got >= 0 && got % 2 == 0 && size_t(got / 2) <= space);
				if (read < 0 || (size_t) read < out * WCHAR_SYSTEMLIB_SIZE)
					return OpStatus::ERR_PARSING_FAILED;

				buffer[out - 1] = 0;
				return ok;
			}
			else
			{
				int bad = 0;
				OP_STATUS err = tolk.SetStringFromSystem(target, tmp,
														 out * WCHAR_SYSTEMLIB_SIZE, &bad);

				OP_DELETEA(tmp);
				return OpStatus::IsError(err) ? err :
					bad ? OpStatus::ERR_PARSING_FAILED : OpStatus::OK;
			}
		}
		else // Bodge: see below
#endif
		{
			uni_char *result;
			if (buffer)
				result = buffer;
			else if (target)
			{
				int prior = target->Length();
				result = target->Reserve(out + prior) + prior;
			}
			else
				result = 0;

#if WCHAR_SYSTEMLIB_SIZE == 2
			if (result)
				uni_strlcpy(result, tmp, out);

			OP_DELETEA(tmp);
			return result ? ok : OpStatus::ERR_NO_MEMORY;
#else
			/* Bodge: cope with being called before InitL() or after Destroy().
			 * Hope files created outside g_opera's useful lifetime don't have fancy names.
			 */
			if (result)
			{
				result[--out] = 0;
				while (out-- > 0)
				{
					result[out] = tmp[out]; // size coercion
					// Did fancy char get truncated ?
					OP_ASSERT(system_wchar_t(result[out]) == tmp[out]);
				}
			}

			OP_DELETEA(tmp);
			return ok;
#endif
		}
	} // else: OOM

#ifdef POSIX_NATIVE_OOM_FALLBACK
# if POSIX_NEEDS_TOLK
	if (!tolk.IsLive())
		return OpStatus::ERR;
# endif

	system_mbtowc(0, 0, 0); // initialise state
	const size_t eat = op_strlen(str);
	system_wchar_t token;
	int took = 0;
	out = 1; // for the trailing zero uni_char
# if WCHAR_SYSTEMLIB_SIZE == 2 && !POSIX_NEEDS_TOLK
	for (const char *p = str;
		 p[0] && (took = system_mbtowc(&token, p, MB_CUR_MAX)) > 0 && token;
		 p += took)
		out++;
# else
	{
		// TODO: how many utf-16 tokens can one utf-32 token expand to ?
		uni_char buf[1 + WCHAR_SYSTEMLIB_SIZE / 2];
		int done = 0, got, read;
		while (done < eat &&
			   (took = system_mbtowc(&token, str + done, MB_CUR_MAX)) > 0 && token &&
			   (got = tolk.Convert(&token, sizeof(token),
								   buf, sizeof(buf), &read)) > 0)
		{
			OP_ASSERT(read == 1);
			OP_ASSERT(got % sizeof(uni_char) == 0);
			done += took;
			out += got / sizeof(uni_char);
		}
	}
# endif

	if (took < 0) // invalid content
		return OpStatus::ERR_PARSING_FAILED;

	if (buffer && space + 1 && out > space)
		out = space;

	uni_char *result;
	if (buffer)
		result = buffer;
	else if (target)
	{
		int prior = target->Length();
		result = target->Reserve(out + prior) + prior;
	}

	if (result)
 	{
		size_t i = 0;
		system_mbtowc(0, 0, 0); // initialise state
# if WCHAR_SYSTEMLIB_SIZE == 2 && !POSIX_NEEDS_TOLK
		while (i + 1 < out && str[0] &&
			   (took = system_mbtowc(&token, str, MB_CUR_MAX)) &&
			   token)
 		{
			result[i++] = token;
			str += took;
 		}
# else
		int done = 0, got, read;
		while (done < eat &&
			   (took = system_mbtowc(&token, str + done, MB_CUR_MAX)) > 0 && token &&
			   (got = tolk.Convert(&token, sizeof(token),
								   result + i, out - i, &read)) > 0)
		{
			OP_ASSERT(read == 1);
			OP_ASSERT(got % sizeof(uni_char) == 0);
			done += took;
			i += got / sizeof(uni_char);
		}
# endif
		OP_ASSERT(took >= 0); // else how come the last loop didn't fail ?
		OP_ASSERT(i < out);
		result[i] = 0;

		// This whole fall-back may be redundant !
		// OP_ASSERT(!"Do we really ever come via here ?");
		return ok;
 	}
#endif // POSIX_NATIVE_OOM_FALLBACK
	return OpStatus::ERR_NO_MEMORY;
}

// static
OP_STATUS PosixNativeUtil::FromNative(const char* str, OpString *target)
{
	if (str == 0) // no conversion to do
		return OpStatus::OK;
	if (target == 0)
		return OpStatus::ERR_NULL_POINTER;

	return NativeToUni(str, target, NULL, 0);
}

//static
OP_STATUS PosixNativeUtil::FromNative(const char* str, UniString& target)
{
	size_t stringsize = op_strlen(str) + 1;
	uni_char* targetptr = target.GetAppendPtr(stringsize);
	RETURN_OOM_IF_NULL(targetptr);

	RETURN_IF_ERROR(FromNative(str, targetptr, stringsize));
	target.Trunc(target.Length() - (stringsize - uni_strlen(targetptr)));

	return OpStatus::OK;
}

// static
OP_STATUS PosixNativeUtil::FromNative(const char* str,
									  uni_char *target, size_t space)
{
	if (str == 0 || !str[0]) // no conversion to do
		return OpStatus::OK;
	if (target == 0)
		return OpStatus::ERR_NULL_POINTER;
	if (1 + space < 2)
		return OpStatus::ERR_OUT_OF_RANGE;

	return NativeToUni(str, NULL, target, space);
}
#endif // POSIX_OK_NATIVE
