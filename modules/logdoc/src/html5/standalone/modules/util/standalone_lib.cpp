/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999-2005
 *
 * Library for standalone builds of the HTML5 parser.
 * NOTE!  Some parts are specific to MS Windows.
 */

#include "core/pch.h"
#include "modules/util/uniprntf.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/logdoc/src/html5/standalone/standalone.h"

#include <stdio.h>
#include <sys/types.h>

#ifndef OP_ASSERT
void OP_ASSERT( int x )
{
	if (!x)
	{
		assert(x);
	}
}
#endif

void *operator new(size_t size, TLeave)
{
	void *p = ::operator new(size);
	if (p == NULL)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return p;
}
#if defined USE_CXX_EXCEPTIONS && defined _DEBUG
void operator delete( void *loc, TLeave ) { /* just to shut up the compiler */ }
#endif

#ifndef MSWIN
#include <new>

void *operator new[](size_t size, TLeave)
{
	void *p = ::operator new[](size, std::nothrow);
	if (p == NULL)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return p;
}
#endif

#ifndef NOT_A_CHARACTER
# define NOT_A_CHARACTER 0xFFFD
#endif // NOT_A_CHARACTER

OP_STATUS UniSetStrN(uni_char* &str, const uni_char* val, int len)
{
	uni_char *new_str = NULL;
	if (val)
	{
		new_str = OP_NEWA(uni_char, len + 1);
		if (!new_str)
			return OpStatus::ERR_NO_MEMORY;

		uni_strncpy(new_str, val, len);
		new_str[len] = 0;
	}
	else
		OP_DELETEA(str);

	str = new_str;
	return OpStatus::OK;
}

void*
OperaModule::operator new(size_t nbytes, OperaModule* m)
{
	return m;
}

void
Opera::InitL()
{
	OperaInitInfo x;
#ifdef STDLIB_MODULE_REQUIRED
	new (&stdlib_module) StdlibModule();
	stdlib_module.InitL(x);
#endif
}

void
Opera::Destroy()
{
#ifdef STDLIB_MODULE_REQUIRED
	stdlib_module.Destroy();
#endif
}

Opera::~Opera()
{
	Destroy();
}


/** Convert Unicode encoded as UTF-8 to UTF-16 */
class Ragnarok_UTF8toUTF16Converter
{
public:
	Ragnarok_UTF8toUTF16Converter();
	int Convert(const void *src, int len, void *dest, int maxlen, int *read);

private:
	int m_charSplit;    ///< Bytes left in UTF8 character, -1 if in ASCII
	int m_sequence;     ///< Type of current UTF8 sequence (total length - 2)
	UINT32 m_ucs;       ///< Current ucs character being decoded
	UINT16 m_surrogate; ///< Surrogate left to insert in next iteration
};



Ragnarok_UTF8toUTF16Converter::Ragnarok_UTF8toUTF16Converter()
: m_charSplit(-1), m_surrogate(0)
{
}


#ifdef DEBUG_ENABLE_SYSTEM_OUTPUT

# if defined WIN32
#  define WRITE_OUT(buf, count) fwrite(buf, sizeof(char), count, stdout)
# elif defined UNIX
#  define WRITE_OUT(buf, count) write(1, buf, count)
# endif // WIN32/UNIX

extern "C"
void dbg_systemoutput(const uni_char* str)
{
	unsigned char buffer[2048]; /* ARRAY OK 2011-09-08 danielsp */

	Ragnarok_UTF16toUTF8Converter conv;
	int read = 0;
	int input_size = uni_strlen(str) * sizeof(uni_char);
	for (; input_size > 0; input_size -= read, str += read / sizeof(uni_char))
	{
		int length = conv.Convert(str, input_size, buffer, 2048, &read);
		WRITE_OUT(buffer, length);
	}
}


extern char* g_logfilename;
#define DEBUG_LOGFILE_BUFFER_SIZE		8192

extern "C"
void dbg_logfileoutput(const char* str, int len)
{
	/*
	static int fd = -1;
	static int buffer_size = 0;
	static char buffer[DEBUG_LOGFILE_BUFFER_SIZE + 100]; // ARRAY OK 2008-05-13 mortenro

	// Open logfile for writing if needed (and possible)
	if ( fd < 0 && g_logfilename != 0 )
		fd = open(g_logfilename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

	// Append to the buffer during startup and for small strings
	if ( len < 128 || fd < 0 )
	{
		if ( buffer_size + len < DEBUG_LOGFILE_BUFFER_SIZE )
		{
			op_memcpy(buffer + buffer_size, str, len);
			buffer_size += len;
			len = 0;
		}
		else if ( fd < 0 && buffer_size < DEBUG_LOGFILE_BUFFER_SIZE )
		{
			// Oops, buffer overflowed during startup - give warning
			const char* w = "\nWARNING: Parts of initial log was dropped\n";
			int ws = op_strlen(w);
			op_memcpy(buffer + buffer_size, w, ws);
			buffer_size += ws;
		}
	}

	// Flush buffer for each new line, or when needed for large data
	if ( fd >= 0 )
	{
		if ( buffer_size > 0 && (buffer[buffer_size - 1] == '\n' || len > 0) )
		{
			write(fd, buffer, buffer_size);
			buffer_size = 0;
		}
		if ( len > 0 )
			write(fd, str, len);
	}
	*/
}

#endif // DEBUG_ENABLE_SYSTEM_OUTPUT

#define IS_SURROGATE(x) ((x & 0xF800) == 0xD800)

#ifndef NOT_A_CHARACTER
# define NOT_A_CHARACTER 0xFFFD
#endif // NOT_A_CHARACTER

static inline void
MakeSurrogate(UINT32 ucs, uni_char &high, uni_char &low)
{
	OP_ASSERT(ucs >= 0x10000UL && ucs <= 0x10FFFFUL);

	// Surrogates spread out the bits of the UCS value shifted down by 0x10000
	ucs -= 0x10000;
	high = 0xD800 | uni_char(ucs >> 10);	// 0xD800 -- 0xDBFF
	low  = 0xDC00 | uni_char(ucs & 0x03FF);	// 0xDC00 -- 0xDFFF
}

int
Ragnarok_UTF8toUTF16Converter::Convert(const void *src, int len, void *dest, int maxlen, int *read_ext)
{
	register const unsigned char *input = reinterpret_cast<const unsigned char *>(src);
	const unsigned char *input_start = input;
	const unsigned char *input_end = input_start + len;

	maxlen &= ~1; // Make sure destination size is always even
	if (!maxlen)
	{
		if (read_ext) *read_ext = 0;
		return 0;
	}

	register uni_char *output = reinterpret_cast<uni_char *>(dest);
	uni_char *output_start = output;
	uni_char *output_end =
		reinterpret_cast<uni_char *>(reinterpret_cast<char *>(dest) + maxlen);

	/** Length of UTF-8 sequences for initial byte */
	static const UINT8 utf8_len[256] =
	{
		/* 00-0F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 10-1F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 20-2F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 30-3F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 40-4F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 50-5F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 60-6F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 70-7F */	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		/* 80-8F */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
		/* 90-9F */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
		/* A0-AF */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
		/* B0-BF */	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // tail
		/* C0-CF */	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		/* D0-DF */	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		/* E0-EF */	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
		/* F0-FF */	4,4,4,4,4,4,4,4,5,5,5,5,6,6,0,0,
	};

	/** Bit masks for first UTF-8 byte */
	static const UINT8 utf8_mask[5] =
	{
		0x1F, 0x0F, 0x07, 0x03, 0x01
	};

	/** Low boundary of UTF-8 sequences */
	static const UINT32 low_boundary[5] =
	{
		0x80, 0x800, 0x10000, 0x200000, 0x4000000
	};


	int written = 0;

	if (m_surrogate)
	{
		if (output)
		{
			*(output ++) = m_surrogate;
		}
		else
		{
			written = sizeof (uni_char);
		}
		m_surrogate = 0;
	}

	// Work on the input buffer one byte at a time, and output when
	// we have a completed Unicode character
	if (output)
	{
		// Decode the UTF-8 string; duplicates code below, decoupling
		// them is a speed optimization.
		while (input < input_end && output < output_end)
		{
			register unsigned char current = *input;

			// Check if we are in the middle of an escape (this works
			// across buffers)
			if (m_charSplit >= 0)
			{
				if (m_charSplit > 0 && (current & 0xC0) == 0x80)
				{
					// If current is 10xxxxxx, we continue to shift bits.
					m_ucs <<= 6;
					m_ucs |= current & 0x3F;
					++ input;
					-- m_charSplit;
				}
				else if (m_charSplit > 0)
				{
					// If current is not 10xxxxxx and we expected more input,
					// this is an illegal character, so we trash it and continue.
					*(output ++) = NOT_A_CHARACTER;
					m_charSplit = -1;
				}

				if (0 == m_charSplit)
				{
					// We are finished. We do not consume any more characters
					// until next iteration.
					if (m_ucs < low_boundary[m_sequence] || IS_SURROGATE(m_ucs))
					{
						// Overlong UTF-8 sequences and surrogates are ill-formed,
						// and must not be interpreted as valid characters
						*output = NOT_A_CHARACTER;
					}
					else if (m_ucs >= 0x10000 && m_ucs <= 0x10FFFF)
					{
						// UTF-16 supports this non-BMP range using surrogates
						uni_char high, low;
						MakeSurrogate(m_ucs, high, low);

						*(output ++) = high;

						if (output == output_end)
						{
							m_surrogate = low;
							m_charSplit = -1;
							written = (output - output_start) * sizeof (uni_char);
							if (read_ext) *read_ext = input - input_start;
							return written;
						}

						*output = low;
					}
					else if (m_ucs >> (sizeof(uni_char) * 8))
					{
						// Non-representable character
						*output = NOT_A_CHARACTER;
					}
					else
					{
						*output = static_cast<uni_char>(m_ucs);
					}

					++ output;
					-- m_charSplit; // = -1
				}
			}
			else
			{
				switch (utf8_len[current])
				{
				case 1:
					// This is a US-ASCII character
					*(output ++) = static_cast<uni_char>(*input);
					written += sizeof (uni_char);
					break;

					// UTF-8 escapes all have high-bit set
				case 2: // 110xxxxx 10xxxxxx
				case 3: // 1110xxxx 10xxxxxx 10xxxxxx
				case 4: // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (outside BMP)
				case 5: // 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (outside Unicode)
				case 6: // 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (outside Unicode)
					m_charSplit = utf8_len[current] - 1;
					m_sequence = m_charSplit - 1;
					m_ucs = current & utf8_mask[m_sequence];
					break;

				case 0: // Illegal UTF-8
					*(output ++) = NOT_A_CHARACTER;
					break;
				}
				++ input;
			}
		}

		// Calculate number of bytes written
		written = (output - output_start) * sizeof (uni_char);
	}
	else
	{
		// Just count the number of bytes needed; duplicates code above,
		// decoupling them is a speed optimization.
		while (input < input_end && written < maxlen)
		{
			register unsigned char current = *input;

			// Check if we are in the middle of an escape (this works
			// across buffers)
			if (m_charSplit >= 0)
			{
				if (m_charSplit > 0 && (current & 0xC0) == 0x80)
				{
					// If current is 10xxxxxx, we continue to shift bits.
					++ input;
					-- m_charSplit;
				}
				else if (m_charSplit > 0)
				{
					// If current is not 10xxxxxx and we expected more input,
					// this is an illegal character, so we trash it and continue.
					m_charSplit = -1;
				}

				if (0 == m_charSplit)
				{
					// We are finished. We do not consume any more characters
					// until next iteration.
					if (low_boundary[m_sequence] > 0xFFFF)
					{
						// Surrogate
						// Overestimates ill-formed characters
						written += 2 * sizeof (uni_char);
					}
					else
					{
						written += sizeof (uni_char);
					}
					-- m_charSplit; // = -1
				}
			}
			else
			{
				switch (utf8_len[current])
				{
				case 1:
					// This is a US-ASCII character
					written += sizeof (uni_char);
					break;

					// UTF-8 escapes all have high-bit set
				case 2: // 110xxxxx 10xxxxxx
				case 3: // 1110xxxx 10xxxxxx 10xxxxxx
				case 4: // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (outside BMP)
				case 5: // 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (outside Unicode)
				case 6: // 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (outside Unicode)
					m_charSplit = utf8_len[current] - 1;
					m_sequence = m_charSplit - 1;
					break;

				case 0: // Illegal UTF-8
					written += sizeof (uni_char);
					break;
				}
				++ input;
			}
		}
	}
	if (read_ext) *read_ext = input - input_start;
	return written;
}


int Ragnarok_UTF16toUTF8Converter::Convert(const void *src, unsigned len, void *dest,
                                  int maxlen, int *read_ext)
{
	int read,written;
  
	const uni_char *source = reinterpret_cast<const uni_char *>(src);
	char *output = reinterpret_cast<char *>(dest);
	len &= ~1; // Make sure source size is always even

	for (read = 0, written = 0; read * sizeof(uni_char) < len && written < maxlen; read ++)
	{
		if (m_surrogate || Unicode::IsLowSurrogate(*source))
		{
			if (m_surrogate && Unicode::IsLowSurrogate(*source))
			{
				if (written + 4 > maxlen)
					break;

				// Second half of a surrogate
				// Calculate UCS-4 character
				UINT32 ucs4char = Unicode::DecodeSurrogate(m_surrogate, *source);

				// U+10000 - U+10FFFF
				written += 4;
				(*output++) = static_cast<char>(0xF0 | ((ucs4char & 0x1C0000) >> 18));
				(*output++) = static_cast<char>(0x80 | ((ucs4char &  0x3F000) >> 12));
				(*output++) = static_cast<char>(0x80 | ((ucs4char &    0xFC0) >>  6));
				(*output++) = static_cast<char>(0x80 |  (ucs4char &     0x3F)       );

			}
			else
			{
				// Either we have a high surrogate without a low surrogate,
				// or a low surrogate on its own. Anyway this is an illegal
				// UTF-16 sequence.
				if (written + 3 > maxlen)
					break;

				written += 3;
				(*output++) = static_cast<char>(static_cast<unsigned char>(0xEF)); // U+FFFD - replacement character
				(*output++) = static_cast<char>(static_cast<unsigned char>(0xBF));
				(*output++) = static_cast<char>(static_cast<unsigned char>(0xBD));
			}

			m_surrogate = 0;
		}
		else if (*source < 128)
		{
			(*output++) = static_cast<char>(*source);
			written ++;
		}
		else if (*source < 2048)
		{
			if (written + 2 > maxlen)
				break;

			written += 2;
			(*output++) = static_cast<char>(0xC0 | ((*source & 0x07C0) >> 6));
			(*output++) = static_cast<char>(0x80 | (*source & 0x003F));
		}
		else if (Unicode::IsHighSurrogate(*source))
		{
			// High surrogate area, should be followed by a low surrogate
			m_surrogate = *source;
		}
		else
		{ // up to U+FFFF
			if (written + 3 > maxlen)
				break;

			written += 3;
			(*output++) = static_cast<char>(0xE0 | ((*source & 0xF000) >> 12));
			(*output++) = static_cast<char>(0x80 | ((*source & 0x0FC0) >> 6));
			(*output++) = static_cast<char>(0x80 | (*source & 0x003F));
		}
		source++;
	}

	*read_ext = read * sizeof(uni_char);
	m_num_converted += read;
	return written;
}

OP_STATUS
read_stdin(ByteBuffer &bbuf)
{
	char buf[2048]; /* ARRAY OK 2011-09-08 danielsp */
	uni_char buf16[2048]; /* ARRAY OK 2011-09-08 danielsp */

	bbuf.Clear();

	Ragnarok_UTF8toUTF16Converter conv;
	int l = 0, r;

	for (;;)
	{
		int k = fread(buf + l, 1, sizeof(buf) - l, stdin);
		if (k <= 0) break;
		
		l += k;
		int w = conv.Convert(buf, l, buf16, sizeof buf16, &r);
		if (r != l)
			op_memmove(buf, buf + r, l - r);
		l -= r;
#if 0		
		// Replace NULs with 0xDFFF which are handled the same later, as tempbuffer doesn't handle NULs
		for (uni_char* c = buf16; c < buf16 + w / sizeof(uni_char); c++)
			if (*c == 0)
				*c = 0xDFFF;
#endif // 0

		if (OpStatus::IsError(bbuf.AppendBytes(reinterpret_cast<const char*>(buf16), w)))
			return OpStatus::ERR_NO_MEMORY;
	}

	bbuf.Append2(0); // terminate string

	return OpStatus::OK;
}
