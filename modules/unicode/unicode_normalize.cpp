/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SUPPORT_UNICODE_NORMALIZE

#include "modules/unicode/unicode.h"
#include "modules/util/handy.h"

// Defines for Hangul from the unicode book.
# define SBase 0xAC00
# define LBase 0x1100
# define VBase 0x1161
# define TBase 0x11A7
# define LCount 19
# define VCount 21
# define TCount 28
# define NCount (VCount * TCount)
# define SCount (LCount * NCount)

// First character with character class.
#define FIRST_CLASSED 768

// Size of temporary buffer for decomposing one character.
// Maximum with current unicodedata.txt is 6. 32 should do for quite a while
#define DECOMPOSE_TMPBUFSIZE 32

struct Composition				// 6 bytes
{
	uni_char c1;
	uni_char c2;
	uni_char c;
};

struct CompositionNonBMP		// 12 bytes
{
	UINT32 c1;
	UINT32 c2;
	UINT32 c;
};

struct Decomposition			// 6 bytes
{
	uni_char c;
	uni_char data[2]; /* ARRAY OK 2009-07-17 marcusc */
};

struct DecompositionNonBMP		// 8 bytes
{
	UINT32 c;
	uni_char data[2]; /* ARRAY OK 2009-07-17 marcusc */
};

struct Canonical				// 4 bytes
{
	uni_char c;
	unsigned char cl;
	unsigned char num;
};

struct CanonicalNonBMP			// 6 bytes
{
	UINT32 c;
	unsigned char cl;
	unsigned char num;
};

// Include after the structures have been defined
#include "modules/unicode/tables/decompositions.inl"
#include "modules/unicode/tables/canonicals.inl"

// FIXME: Try to rewrite to use TempBuffer instead, currently cannot
// do that because it does not allow me to take over the pointer when
// I'm done, which causes unnecessary buffer copying.
/** Simple buffer class used in the normalization code. */
struct UniBuffer
{
	BOOL nospecials;
	int size, allocated_size;
	uni_char *data;

	// Smaller as inline
	~UniBuffer()
	{
		OP_DELETEA(data);
	}

	/** Allocate buffer of length l */
	UniBuffer(int l)
	{
		size = 0;
		allocated_size = l;
		data = OP_NEWA(uni_char, l);
		nospecials = FALSE;
		if (!data)
			allocated_size = 0;
	}

	// Only used once. OK to inline.
	/** Allocate buffer of length l + 12, copying the first l characters
	  * from pointer p.
	  */
	UniBuffer(const uni_char * p, int l)
	{
		allocated_size = l + 12;
		data = OP_NEWA(uni_char, l + 12);
		if (!data)
			size = allocated_size = 0;
		else
		{
			size = l;
			nospecials = TRUE;
			for (int i = 0; i < l; i++)
				if ((data[i] = p[i]) >= 160)
				{
					nospecials = FALSE;
					for (; i < l; i++)
						data[i] = p[i];
				}
		}
	}

	BOOL EnsureSpace();

	/** Insert character x at position pos */
	BOOL Insert(int pos, uni_char x)
	{
		if (pos != size)
			for (int i = size; i > pos; i--)	// Move old data.
				data[i] = data[i - 1];
		size++;
		data[pos] = x;
		return EnsureSpace();
	}
};

/** Ensure there is enough space to expand the string.
  * Makes sure Insert() can always be called.
  */
BOOL UniBuffer::EnsureSpace()
{
	if (size >= allocated_size)
	{
		int new_size;
		new_size = size * 2 + 128;
		uni_char *nd = OP_NEWA(uni_char, new_size);
		if (!nd)
			return FALSE;		// Cannot grow buffer.
		allocated_size = new_size;
		if (data)
		{
			op_memcpy(nd, data, size * sizeof (uni_char));
			OP_DELETEA(data);
		}
		data = nd;
	}
	return TRUE;
}

unsigned char Unicode::GetCombiningClass(uni_char c)
{
	if (c < FIRST_CLASSED)
		return 0;
	else
	{
		// Binary search the canonicals table
		size_t high = ARRAY_SIZE(canonicals);
		size_t low = 0;
		size_t middle;
		while (1)
		{
			middle = ((high - low) >> 1) + low;
			if (canonicals[middle].c <= c
				&& canonicals[middle].c + canonicals[middle].num > c)
				return canonicals[middle].cl;
			else if (canonicals[middle].c < c)
			{
				if (low == middle)
					break;
				low = middle;
			}
			else
			{
				if (high == middle)
					break;
				high = middle;
			}
		}
	}
	return 0;
}

unsigned char Unicode::GetCombiningClass(uni_char c1, uni_char c2)
{
	OP_ASSERT(IsHighSurrogate(c1) || !"Invalid call to Unicode::GetCombiningClass()");
	OP_ASSERT(IsLowSurrogate(c2)  || !"Invalid call to Unicode::GetCombiningClass()");

	// Binary search the non-BMP canonicals table
	UINT32 match = (c1 << 16) | c2;
	size_t high = ARRAY_SIZE(canonicalsnonbmp);
	size_t low = 0;
	size_t middle;
	while (1)
	{
		middle = ((high - low) >> 1) + low;
		if (canonicalsnonbmp[middle].c <= match
			&& canonicalsnonbmp[middle].c + canonicalsnonbmp[middle].num > match)
			return canonicalsnonbmp[middle].cl;
		else if (canonicalsnonbmp[middle].c < match)
		{
			if (low == middle)
				break;
			low = middle;
		}
		else
		{
			if (high == middle)
				break;
			high = middle;
		}
	}
	return 0;
}

unsigned char Unicode::GetCombiningClassFromCodePoint(UnicodePoint c)
{
	if (GetUnicodePlane(c) > 0)
	{
		// Code points outside of the basic multilingual plane are represented
		// using a surrogate pair in UTF-16.

		uni_char high, low;
		MakeSurrogate(c, high, low);
		return GetCombiningClass(high, low);
	}

	return GetCombiningClass(c);
}

BOOL Unicode::IsWideOrNarrowDecomposition(int decomposition_index)
{
	size_t high = ARRAY_SIZE(narrow_wide_decomposition_indexes);
	size_t low = 0;
	size_t middle;
	while (1)
	{
		middle = ((high - low) >> 1) + low;
		if (narrow_wide_decomposition_indexes[middle] == decomposition_index)
			return TRUE;
		else if (narrow_wide_decomposition_indexes[middle] < decomposition_index)
		{
			if (low == middle)
				break;
			low = middle;
		}
		else
		{
			if (high == middle)
				break;
			high = middle;
		}
	}
	return FALSE;
}

/**
 * Decompose a single character recursively.
 * @param c Character to decompose.
 * @param p Output buffer.
 * @param canonical Decompose to canonical form.
 * @return Number of characters this character decomposed into.
 */
int Unicode::Decompose(uni_char c, uni_char * p, BOOL canonical)
{
	if (c < 160)
		;
	else if ((c >= SBase) && c < (SBase + SCount))
	{
		int l, v, t;
		c -= SBase;
		l = LBase + c / NCount;
		v = VBase + (c % NCount) / TCount;
		t = TBase + (c % TCount);
		p[0] = l;
		p[1] = v;
		if (t != TBase)
		{
			p[2] = t;
			return 3;
		}
		return 2;
	}
	else
	{
		// Decompose a single character. Binary search for the
		// decomposition in our data table.
		size_t high = ARRAY_SIZE(decompositions);
		size_t low = 0;
		size_t middle;
		while (1)
		{
			middle = ((high - low) >> 1) + low;
			if (decompositions[middle].c == c)
			{
				// Check if we have a canonical decomposition, if
				// so we end here
				int len;
				if (canonical
					&& (compat_decomposition[middle / 8] & (1 << (middle & 7))))
				{
					p[0] = c;
					return 1;
				}

				// Decompose the first character recursively
				len = Decompose(decompositions[middle].data[0], p, canonical);

				// Decompose the second character recursively
				if (decompositions[middle].data[1])
					len +=
						Decompose(decompositions[middle].data[1], p + len,
								  canonical);

				// Since we decompose on UTF-16 code units, we need to
				// reconsider the first two characters if it consists
				// of a surrogate pair. This is done recursively by
				// virtue of us calling Decompose() again.
				if (IsHighSurrogate(p[0]) && IsLowSurrogate(p[1]))
				{
					uni_char tmp[DECOMPOSE_TMPBUFSIZE]; /* ARRAY OK 2009-07-17 marcusc */
					int newlen = Decompose(p[0], p[1], tmp, canonical);
					if (2 == newlen)
					{
						// Most probably the same as we already had, but copy
						// over to the output buffer just to make sure.
						p[0] = tmp[0];
						p[1] = tmp[1];
					}
					else
					{
						// Make room and move into place.
						op_memmove(p + newlen, p + 2, (len - 2) * sizeof (uni_char));
						op_memcpy(p, tmp, newlen * sizeof (uni_char));
						len = len - 2 + newlen;
					}
				}

				return len;
			}
			else if (decompositions[middle].c < c)
			{
				if (low == middle)
					break;
				low = middle;
			}
			else
			{
				if (high == middle)
					break;
				high = middle;
			}
		}
	}

	p[0] = c;
	return 1;
}

/**
 * Decompose a single non-BMP character recursively.
 * @param c1 High surrogate for character to decompose.
 * @param c2 Low surrogate for character to decompose.
 * @param p Output buffer.
 * @param canonical Decompose to canonical form.
 * @return Number of characters this character decomposed into.
 */
int Unicode::Decompose(uni_char c1, uni_char c2, uni_char * p, BOOL canonical)
{
	// Decompose a single character. Binary search for the
	// decomposition in our data table. The table stores
	// non-BMP characters as a 32-bit value with the high
	// surrogate in the high word and the low surrogate
	// in the low word.
	OP_ASSERT(IsHighSurrogate(c1) || !"Invalid call to Unicode::Decompose()");
	OP_ASSERT(IsLowSurrogate(c2)  || !"Invalid call to Unicode::Decompose()");
	UINT32 match = (c1 << 16) | c2;
	size_t high = ARRAY_SIZE(decompositionsnonbmp);
	size_t low = 0;
	size_t middle;
	while (1)
	{
		middle = ((high - low) >> 1) + low;
		if (decompositionsnonbmp[middle].c == match)
		{
			// Check if we have a canonical decomposition, if
			// so we end here
			int len;
			if (canonical
			     && (compat_decomposition_nonbmp[middle / 8] & (1 << (middle & 7))))
			{
				p[0] = c1;
				p[1] = c2;
				return 2;
			}

			// Decompose the first character recursively
			len = Decompose(decompositionsnonbmp[middle].data[0], p, canonical);

			// Decompose the second character recursively
			if (decompositionsnonbmp[middle].data[1])
				len += Decompose(decompositionsnonbmp[middle].data[1], p + len, canonical);

			// Since we decompose on UTF-16 code units, we need to
			// reconsider the first two characters if it consists
			// of a surrogate pair. This is done recursively by
			// virtue of us calling Decompose() again.
			// When we get here, we have recursed, so p is initialized.
			// coverity[read_parm: FALSE]
			if (IsHighSurrogate(p[0]) && IsLowSurrogate(p[1]))
			{
				uni_char tmp[DECOMPOSE_TMPBUFSIZE]; /* ARRAY OK 2009-07-17 marcusc */
				int newlen = Decompose(p[0], p[1], tmp, canonical);
				if (2 == newlen)
				{
					// Most probably the same as we already had, but copy
					// over to the output buffer just to make sure.
					p[0] = tmp[0];
					p[1] = tmp[1];
				}
				else
				{
					// Make room and move into place.
					op_memmove(p + newlen, p + 2, (len - 2) * sizeof (uni_char));
					op_memcpy(p, tmp, newlen * sizeof (uni_char));
					len = len - 2 + newlen;
				}
			}
			
			return len;
		}
		else if (decompositionsnonbmp[middle].c < match)
		{
			if (low == middle)
				break;
			low = middle;
		}
		else
		{
			if (high == middle)
				break;
			high = middle;
		}
	}
	
	p[0] = c1;
	p[1] = c2;
	return 2;
}

/**
 * Decompose a buffer.
 * @param source Buffer to decompose.
 * @param canonical Decompose to canonical form.
 * @return Buffer with decomposed form.
 */
UniBuffer *Unicode::Decompose(UniBuffer * source, BOOL canonical)
{
	int i = 0;
	UniBuffer *res = OP_NEW(UniBuffer, (source->size));	// Output
	uni_char tmp[DECOMPOSE_TMPBUFSIZE]; /* ARRAY OK 2009-07-17 marcusc */

	if (!res || !res->data)
		goto oom_err;
	for (; i < source->size; i++)
	{
		int size;

		// If the first character in the buffer is a single UTF-16
		// code unit that is not a surrogate half or a private
		// use character, we use the standard decompose routine.
		// Private use characters are used internally in the algorithm
		// to handle decompositions that go from one to more than
		// two characters recursively. Doing it like this is slower,
		// but saves a lot of space in the tables and gives correct
		// results.
		if (source->data[i] < 0xd800 || source->data[i] > 0xf7ff)
		{
			size = Decompose(source->data[i], tmp, canonical);
		}
		// Decompose surrogate pairs
		else if (i < source->size && IsHighSurrogate(source->data[i]) && IsLowSurrogate(source->data[i + 1]))
		{
			size = Decompose(source->data[i], source->data[i + 1], tmp, canonical);
			++ i;
		}
		else
		{
			tmp[0] = (source->data[i] >= 0xE000) ? source->data[i] : NOT_A_CHARACTER;
			size = 1;
		}

		// Normalization isn't stable over string concatenation,
		// so we might need to add characters before the end of
		// the current result (res). We have to do it like this.
		// The reason is that we are supposed to sort each
		// character separately by canonical class.
		int width;
		for (int j = 0; j < size; j += width)
		{
			// Find current character to insert, and its class and width
			UINT32 c = tmp[j];
			unsigned char cl;
			if (IsHighSurrogate(c))
			{
				cl = GetCombiningClass(c, tmp[j + 1]);
				c = (c << 16) | tmp[j + 1];
				width = 2;
			}
			else
			{
				cl = GetCombiningClass(c);
				width = 1;
			}

			// If this character has a combining class, iterate
			// backwards to find where to insert it, otherwise
			// insert it after the last character.
			int k = res->size;
			if (cl)
			{
				for (; k > 0; -- k)
				{
					uni_char last = res->data[k - 1];
					if (IsLowSurrogate(last))
					{
						if (GetCombiningClass(res->data[k - 2], last) <= cl)
							break;
						-- k; /* Do not stop inside surrogate pair */
					}
					else
					{
						if (GetCombiningClass(res->data[k - 1]) <= cl)
							break;
					}
				}
			}
			if (c > 0xFFFF)
			{
				if (!res->Insert(k, c >> 16) || !res->Insert(k + 1, c & 0xFFFF))
					goto oom_err;
			}
			else
			{
				if (!res->Insert(k, c))
					goto oom_err;
			}
		}
	}
	goto cleanup;
  oom_err:
	OP_DELETE(res);
	res = 0;
  cleanup:
	OP_DELETE(source);
	return res;
}

/**
 * Compose two characters into one.
 * @param c1 Character to compose into.
 * @param c2 Character to compose with.
 * @return The composed character.
 */
UINT32 Unicode::Compose(UINT32 c1, UINT32 c2)
{
	// Check for valid characters
	if (c2 < FIRST_CLASSED)
		return 0;

	// There are only a few compositions which have the second
	// character outside the BMP.
	if (c2 > 0xFFFF)
	{
		if (c1 > 0xFFFF)
		{
			// All of them have their first character also outside
			// the BMP.

			// Consider changing this to binary search if the
			// compositionsnonbmp[] grows.
			for (size_t i = 0; i < ARRAY_SIZE(compositionsnonbmp); ++ i)
			{
				if (compositionsnonbmp[i].c1 == c1 &&
				    compositionsnonbmp[i].c2 == c2)
				{
					return compositionsnonbmp[i].c;
				}
			}
		}

		// Nothing to see here, move along.
		return 0;
	}

	/* This code comes directly from the unicode book.
	 * It handles composition of hangul characters in a way more compact
	 * way than adding them all to the composition table.
	 */
	if (c1 >= LBase)
	{
		int LIndex = c1 - LBase, SIndex;
		if (LIndex < LCount)
		{
			int VIndex = c2 - VBase;
			if (0 <= VIndex && VIndex < VCount)
				return SBase + (LIndex * VCount + VIndex) * TCount;
		}

		if (c1 >= SBase)
		{
			SIndex = c1 - SBase;
			if (SIndex < SCount && (SIndex % TCount) == 0)
			{
				int TIndex = c2 - TBase;
				if (0 <= TIndex && TIndex <= TCount)
					return c1 + TIndex;
			}
		}
	}
	// Nope, not hangul. Try the table.
	size_t high = ARRAY_SIZE(compositions);
	size_t low = 0;
	size_t middle;
	while (1)
	{
		middle = ((high - low) >> 1) + low;
		if (compositions[middle].c1 == c1)
		{
			int j = middle;
			while (compositions[--j].c1 == c1)
				if (compositions[j].c2 == c2)
					return compositions[j].c;

			while (compositions[middle].c1 == c1)
			{
				if (compositions[middle].c2 == c2)
					return compositions[middle].c;
				middle++;
			}
			return 0;
		}
		else if (compositions[middle].c1 < c1)
		{
			if (low == middle)
				break;
			low = middle;
		}
		else
		{
			if (high == middle)
				break;
			high = middle;
		}
	}
	return 0;
}

/**
 * Compose characters in a buffer.
 * @param source Buffer with characters to compose.
 * @return Buffer with composed characters.
 */
UniBuffer *Unicode::Compose(UniBuffer * source)
{
	// Find start character
	UINT32 startch = source->data[0];
	unsigned int lastclass;
	unsigned int startpos = 0, comppos = 1;
	if (IsHighSurrogate(startch) && IsLowSurrogate(source->data[1]))
	{
		startch = startch << 16 | source->data[1];
		lastclass = GetCombiningClass(source->data[0], source->data[1]) ? 256 : 0;
		comppos = 2;
	}
	else
	{
		lastclass = GetCombiningClass(startch) ? 256 : 0;
	}

	// We have to handle character classes when composing too. We must
	// keep iterating from the base character over all combining marks
	// until we find the lowest classed mark that combines with the
	// base, and compose those two.
	int width;
	for (int pos = comppos; pos < source->size; pos += width)
	{
		// Find combining character
		UINT32 ch = source->data[pos];
		unsigned char cl;
		if (IsHighSurrogate(ch) && IsLowSurrogate(source->data[pos + 1]))
		{
			cl = GetCombiningClass(ch, source->data[pos + 1]);
			ch = ch << 16 | source->data[pos + 1];
			width = 2;
		}
		else
		{
			cl = GetCombiningClass(ch);
			width = 1;
		}

		// Check if we can compose
		UINT32 co = Compose(startch, ch);
		if (co && ((lastclass < cl) || (lastclass == 0)))
		{
			// Replace start character with the combined character
			startch = co;
			if (co > 0xFFFF)
			{
				source->data[startpos] = co >> 16; // High surrogate
				source->data[startpos + 1] = co & 0xFFFF; // Low surrogate
			}
			else
				source->data[startpos] = co & 0xFFFF;
		}
		else
		{
			if (cl == 0)
			{
				// New base character
				startpos = comppos;
				startch = ch;
			}

			// Remember last combining class
			lastclass = cl;

			// Emit character
			if (ch > 0xFFFF)
				source->data[comppos++] = ch >> 16; // High surrogate
			source->data[comppos++] = ch & 0xFFFF;
		}
	}
	source->size = comppos;		// Possibly truncate.
	return source;
}

uni_char *Unicode::Normalize(const uni_char * x, int len, BOOL compose,
							 BOOL compat)
{
	if (!x)
		return NULL;

	if (len == -1)
		len = uni_strlen(x);

	UniBuffer *tmp = OP_NEW(UniBuffer, (x, len));

	if (!tmp || !tmp->data)
	{
		OP_DELETE(tmp);
		return NULL;			// OOM
	}

	if (!tmp->nospecials)
	{
		tmp = Decompose(tmp, !compat);
		if (compose && tmp)
			tmp = Compose(tmp);
		if (!tmp)
			return NULL;
	}

	uni_char *rptr = tmp->data;
	rptr[tmp->size] = 0;		// At least one more char always available.
	tmp->data = NULL;
	OP_DELETE(tmp);
	return rptr;
}

void Unicode::DecomposeNarrowWide(const uni_char *src, int len, uni_char *dst)
{
	int j=0;
	for (int i = 0; i < len; ++i, ++j)
	{
		if (i+1 < len && IsHighSurrogate(src[i]) && IsLowSurrogate(src[i + 1]))
		{	
			// There are no <narrow> nor <wide> decompositions for characters beyond the BMP
			dst[j] = src[i];
			dst[j+1] = src[i+1];
			++i, ++j;
		}
		else
		{
			// Decompose a single character. Binary search for the decomposition in data table.
			size_t high = ARRAY_SIZE(decompositions);
			size_t low = 0;
			size_t middle;
			BOOL found = FALSE;
			while (1)
			{
				middle = ((high - low) >> 1) + low;
				if (decompositions[middle].c == src[i])
				{
					found = TRUE;

					// Check if we have narrow/wide decomposition
					if (IsWideOrNarrowDecomposition(middle))
						dst[j] = decompositions[middle].data[0];
					else
						dst[j] = src[i];
					break;
				}
				else if (decompositions[middle].c < src[i])
				{
					if (low == middle)
						break;
					low = middle;
				}
				else
				{
					if (high == middle)
						break;
					high = middle;
				}
			}
			if (!found)
				dst[j] = src[i];
		}
	}
	dst[j] = 0;
}

#endif // SUPPORT_UNICODE_NORMALIZE
