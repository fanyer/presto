/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SEARCH_ENGINE

#include "modules/search_engine/UniCompressor.h"

#define DICT_SIZE 0x4000
#define DICT_MASK 0x3FFF
// Modified Bernstein hash (experimentally as good as FNV-1a hash)
#define hash(cp) (((((((cp[0] * 0x21) ^ cp[1]) * 0x21) ^ cp[2]) * 0x21) ^ cp[3]) & DICT_MASK)

OP_STATUS UniCompressor::InitCompDict(void)
{
	OP_ASSERT(m_dict == NULL);  // it is not necessary to init the dictionary several times
	FreeCompDict();

	RETURN_OOM_IF_NULL(m_dict = OP_NEWA(UINT32, DICT_SIZE));

#ifdef VALGRIND
	// Mark 'm_dict' as defined, even though it is not, since the
	// algorithm is not sensitive to the initial values for correct
	// operation.
	op_valgrind_set_defined(m_dict, DICT_SIZE*sizeof(UINT32));
#endif

	return OpStatus::OK;
}

unsigned UniCompressor::Compress(unsigned char *dst, const uni_char *src)
{
	const uni_char *sp, *cp, *end, *match_end;
	unsigned char *op;
	int i, di;
	const uni_char *dptr;
	unsigned len = (unsigned)uni_strlen(src);

	if (len < 8)
	{
		dst[0] = len;
		dst[1] = 0;
		dst[2] = 0;
		dst[3] = 0;

		if (len == 0)
			return 4;

		return (unsigned)(OutputLiteral(dst + 4, src, src + len) - dst);
	}

	sp = src;
	cp = src;

	end = cp + len;
	match_end = end - 4;

	op = dst + 4;

	while (cp < match_end)
	{
		di = hash(cp);
		dptr = src+m_dict[di];
		m_dict[di] = (UINT32)(cp-src);
		if (dptr >= src && dptr < cp && cp - dptr <= 0xFFFF &&
			dptr[0] == cp[0] && dptr[1] == cp[1] && dptr[2] == cp[2] && dptr[3] == cp[3])
		{
			// match found
			if (cp > sp)
				op = OutputLiteral(op, sp, cp);

			sp = cp;
			cp += 4;
			i = 4;

			while (*cp == dptr[i])
			{
				++cp;
				++i;
			}

			op = OutputMatch(op, (unsigned)(cp - sp), (unsigned short)(sp - dptr));

			sp++;
			while (sp < cp && sp < match_end)
			{
				m_dict[hash(sp)] = (UINT32)(sp-src);
				++sp;
			}
			sp = cp;
		}
		else {
			++cp;
		}
	}

	cp = end;

	if (cp > sp)
		op = OutputLiteral(op, sp, cp);

	i = (int)(cp - src);

	dst[0] = i & 0xFF;
	dst[1] = (i >> 8) & 0xFF;
	dst[2] = (i >> 16) & 0xFF;
	dst[3] = (i >> 24) & 0xFF;

	return (unsigned)(op - dst);
}

unsigned UniCompressor::Decompress(uni_char *dst, const unsigned char *src, unsigned len)
{
	const unsigned char *cp;
	uni_char *op;
	register uni_char *shiftp;
	int c;
	int lit_len, max_len;
	int shift;
	
	if (!dst || !src || len < 4)
		return 0;

	max_len = (int)Length(src);

	cp = src + 4;
	op = dst;

	src += len;

	while (cp < src && op - dst < max_len)
	{
		if ((*cp & 0x40) == 0)  // literal
		{
			// length
			lit_len = *cp & 0x3F;
			shift = 6;
			while ((*cp++ & 0x80) != 0)
			{
				if (shift > 30 || cp >= src)
					return 0;

				lit_len |= (*cp & 0x7F) << shift;
				shift += 7;
			}

			if (lit_len == 0 ||
				op - dst + lit_len > max_len ||
				cp + 2 > src)
				return 0;

			// first char
			*op = *cp++;
			*op++ |= ((uni_char)(*cp++)) << 8;

			// differences
			while (--lit_len > 0)
			{
				if (cp >= src)
					return 0;

				c = *cp & 0x7F;
				if ((*cp++ & 0x80) != 0)
				{
					if (cp >= src)
						return 0;

					c |= ((uni_char)*cp & 0x7F) << 7;
					if ((*cp++ & 0x80) != 0)
					{
						if (cp >= src)
							return 0;

						c |= *cp++ << 14;
						if ((c & 0x10000) != 0)
							c |= 0xFFFF0000;
					}
					else if ((c & 0x2000) != 0)
						c |= 0xFFFFC000;
				}
				else if (c & 0x40)
					c |= 0xFFFFFF80;

				*op = op[-1] - c;
				++op;
			}
		}
		else {  // match
			// length
			lit_len = *cp & 0x1F;
			c = (*cp & 0x20) == 0;
			shift = 5;
			while ((*cp++ & 0x80) != 0)
			{
				if (shift > 30 || cp >= src)
					return 0;

				lit_len |= (*cp & 0x7F) << shift;
				shift += 7;
			}
			lit_len += 2;

			if (cp >= src || (c && cp+1 >= src))
				return 0;
			shift = *cp++;
			if (c)
				shift |= ((int)*cp++) << 8;

			shiftp = op - shift;

			if (shiftp < dst || shiftp >= op || op - dst + lit_len > max_len)
				return 0;

			while (lit_len-- > 0)
				*op++ = *shiftp++;
		}
	}

	*op = 0;
	return (unsigned)(op - dst);
}

unsigned UniCompressor::Length(const unsigned char *src)
{
	return (unsigned)(src[0] | ((int)src[1]) << 8 | ((int)src[2]) << 16 | ((int)src[3]) << 24);
}

unsigned char *UniCompressor::OutputLiteral(unsigned char *op, const uni_char *sp, const uni_char *cp)
{
	register INT32 prev, diff;
	int length;

	OP_ASSERT(cp - sp > 0);

	length = (int)(cp - sp);
	*(op++) = (unsigned char)(length & 0x3F) | ((length > 0x3F) << 7);

	length >>= 6;

	while (length > 0)
	{
		*(op++) = (unsigned char)(length & 0x7F) | ((length > 0x7F) << 7);
		length >>= 7;
	}

	prev = *sp++;
	*op++ = prev & 0xFF;
	*op++ = prev >> 8;

	while (sp < cp)
	{
		diff = prev - *sp;
		if ((diff & 0xFFFFFFC0) == 0 || (diff | 0x3F) == -1)
			*op++ = (unsigned char)(diff & 0x7F);
		else {
			*op++ = (unsigned char)((diff & 0x7F) | 0x80);
			diff >>= 7;

			if ((diff & 0xFFFFFFC0) == 0 || (diff | 0x3F) == -1)
				*op++ = (unsigned char)(diff & 0x7F);
			else {
				*op++ = (unsigned char)((diff & 0x7F) | 0x80);
				*op++ = diff >> 7;
			}
		}

		prev = *sp++;
	}

	return op;
}

unsigned char *UniCompressor::OutputMatch(unsigned char *op, unsigned length, unsigned short offset)
{
	length -= 2; // Must still be 2 (not 4) for backwards compatibility (compression impact: 0.0016%)

	*(op++) = (unsigned char)(length & 0x1F) | ((length > 0x1F) << 7) | 0x40 | ((offset <= 0xFF) << 5);

	length >>= 5;

	while (length > 0)
	{
		*(op++) = (unsigned char)(length & 0x7F) | ((length > 0x7F) << 7);
		length >>= 7;
	}

	if (offset <= 0xFF)
		*op++ = (unsigned char)offset;
	else
	{
		*op++ = (unsigned char)(offset & 0xFF);
		*op++ = (unsigned char)(offset >> 8);
	}

	return op;
}

#undef DICT_SIZE
#undef DICT_MASK
#undef hash

#endif  // SEARCH_ENGINE


