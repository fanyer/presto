/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef CSS_BUFFER_H
#define CSS_BUFFER_H

#include "modules/logdoc/html_col.h"
#include "modules/url/url2.h"
#include "modules/util/opfile/unistream.h"
#include "modules/logdoc/namespace.h"
#include "modules/style/css_media.h"
#include "modules/style/css_webfont.h"

#include "modules/logdoc/logdoc_util.h"  // for HexToInt()

// size of stack used for PushState()/RestoreState()
#define STACK_SIZE 4

// Get the maximum size of an ident using unicode escapes.
// The longest possible representation per character is.
// escape char + 6 digits + CR + LF = 9
#define CSS_ESCAPED_IDENT_LEN(len) (9*len)

const unsigned int orig_tag_buf_len = 64;

class CSS_Buffer
{
public:
	CSS_Buffer() : buf_cnt(0)
		,buf(0)
		,len(0)
		,total_len(0)
		,tag_buf(orig_tag_buf)
		,tag_buf_len(orig_tag_buf_len)
		,in_buffer(0)
		,bytes_left(0)
		,buf_idx(0)
		,pos_base(0)
		,stack_idx(0)
	{
#ifdef _DEBUG
		has_run_AllocBufferArrays = FALSE;
#endif // _DEBUG
	}

	unsigned int Length() { return total_len; }
	uni_char LastChar() { return buf[buf_cnt-1][len[buf_cnt-1]-1]; }
	unsigned int CurrentPos() { return pos_base + len[buf_idx]-bytes_left; }

#ifdef LIMITED_FOOTPRINT_DEVICE

	~CSS_Buffer();
	BOOL AllocBufferArrays(unsigned int count);
	void AddBuffer(const uni_char* buffer, unsigned int length);
	BOOL PushState();
	void PopState();
	BOOL RestoreState();
	unsigned int BytesLeft();
	uni_char LookAhead(unsigned int n=0);
	uni_char GetNextChar();
	void UnGetChar();
	void EatChars(unsigned int cnt);
	void SetCurrentPos(unsigned int new_pos);
	COLORREF GetColor(unsigned int start_pos, unsigned int length);
	void GetNChars(uni_char* dst, unsigned int count);

#else // LIMITED_FOOTPRINT_DEVICE

	~CSS_Buffer()
	{
		if (tag_buf != orig_tag_buf)
			OP_DELETEA(tag_buf);

		OP_DELETEA(len);
		OP_DELETEA(buf);
	}

#ifdef _DEBUG
	BOOL has_run_AllocBufferArrays;
#endif // _DEBUG

	/** Allocates arrays of \c count strings and length fields. Naturally, the strings themselves
		are not allocated. This can be considered a second-phase constructor.

		This method must only be called once on a given object; otherwise it will leak. */
	BOOL AllocBufferArrays(unsigned int count)
	{
#ifdef _DEBUG
		OP_ASSERT(!has_run_AllocBufferArrays);
		has_run_AllocBufferArrays = TRUE;
#endif // _DEBUG

		if (count > 0)
		{
			len = OP_NEWA(unsigned int, count);
			if (!len)
			  return FALSE;
			buf = OP_NEWA(const uni_char*, count);
			if (!buf)
			{
			  OP_DELETEA(len);
			  len = NULL;
			  return FALSE;
			}
		}
		return (len && buf);
	}

	/** Adds the given buffer to the list of buffers kept by this object, but does not take
		ownership.

		If \c length is zero or \c buffer is NULL, this method does nothing. */
	void AddBuffer(const uni_char* buffer, unsigned int length)
	{
		if (length > 0 && buffer!=NULL)
		{
			if (buf_cnt == 0)
			{
				in_buffer = buffer;
				bytes_left = length;
			}
			buf[buf_cnt] = buffer;
			len[buf_cnt++] = length;
			total_len += length;
		}
	}

	BOOL PushState()
	{
		if (stack_idx < STACK_SIZE)
		{
			stored_bleft[stack_idx] = bytes_left;
			stored_buf[stack_idx] = in_buffer;
			stored_pos_base[stack_idx] = pos_base;
			stored_buf_idx[stack_idx++] = buf_idx;
			return TRUE;
		}
		else
			return FALSE;
	}

	void PopState()
	{
		if (stack_idx > 0)
			--stack_idx;
	}

	BOOL RestoreState()
	{
		if (stack_idx > 0)
		{
			bytes_left = stored_bleft[--stack_idx];
			in_buffer = stored_buf[stack_idx];
			pos_base = stored_pos_base[stack_idx];
			buf_idx = stored_buf_idx[stack_idx];
			return TRUE;
		}
		else
			return FALSE;
	}

	unsigned int BytesLeft()
	{
		return buf_cnt > 0 ? total_len - (pos_base + len[buf_idx]-bytes_left) : 0;
	}

	uni_char LookAhead(unsigned int n=0)
	{
		if (bytes_left > n)
			return in_buffer[n];

		return LookAheadSlow(n);
	}

	uni_char GetNextChar()
	{
		if (bytes_left > 0)
		{
			--bytes_left;
			return *in_buffer++;
		}
		else if (buf_idx < buf_cnt-1)
		{
			pos_base += len[buf_idx];
			in_buffer = buf[++buf_idx];
			bytes_left = len[buf_idx]-1;
			return *in_buffer++;
		}
		else
			return 0;
	}

	void UnGetChar()
	{
		if (bytes_left < len[buf_idx])
		{
			bytes_left++;
			--in_buffer;
		}
		else if (buf_idx > 0)
		{
			bytes_left = 1;
			buf_idx--;
			in_buffer = buf[buf_idx]+(len[buf_idx]-1);
			pos_base -= len[buf_idx];
		}
	}

	void EatChars(unsigned int cnt)
	{
		while (cnt > bytes_left && buf_idx < buf_cnt-1)
		{
			pos_base += len[buf_idx];
			cnt -= bytes_left;
			bytes_left = len[++buf_idx];
			in_buffer = buf[buf_idx];
		}

		if (cnt > bytes_left)
			bytes_left = 0;
		else
		{
			bytes_left -= cnt;
			in_buffer += cnt;
		}
	}

	void SetCurrentPos(unsigned int new_pos)
	{
		buf_idx = 0;
		pos_base = 0;
		while (new_pos > pos_base+len[buf_idx] && buf_idx < buf_cnt-1)
			pos_base += len[buf_idx++];

		if (buf_idx < buf_cnt)
		{
			in_buffer = buf[buf_idx]+new_pos-pos_base;
			bytes_left = len[buf_idx]-(new_pos-pos_base);
		}
		else
		{
			in_buffer = buf[buf_idx]+len[buf_idx];
			bytes_left = 0;
		}
	}

	COLORREF GetColor(unsigned int start_pos, unsigned int length)
	{
		if (length >= 3 && length <= CSS_ESCAPED_IDENT_LEN(6))
		{
			uni_char color[CSS_ESCAPED_IDENT_LEN(6)+1]; /* ARRAY OK 2009-02-11 rune */

			GetString(color, start_pos, length);
			size_t color_len = uni_strlen(color);
			if (color_len == 3 || color_len == 6)
			{
				for (size_t i=0; i<color_len; i++)
					if (!uni_isxdigit(color[i]))
						return USE_DEFAULT_COLOR;

				int r, g, b;
				int n=color_len/3;
				r = HexToInt(color, n);
				g = HexToInt(color+n, n);
				b = HexToInt(color+2*n, n);
				if (color_len == 3)
				{
					r*=0x11;
					g*=0x11;
					b*=0x11;
				}
				return OP_RGB(r, g, b);
			}
		}
		return USE_DEFAULT_COLOR;
	}

	void GetNChars(uni_char* dst, unsigned int count)
	{
		if (dst)
			for (unsigned int i=0; i<count; i++)
				*dst++ = GetNextChar();
	}

#endif // LIMITED_FOOTPRINT_DEVICE

	/** Get a CSS value from the buffer.
		@param start_pos The start position in the buffer.
		@param length The number of uni_chars the string representation occupies.
		@return The corresponding CSSValue or CSS_VALUE_UNKNOWN (a negative integer)
				if no corresponding CSSValue was found. */
	CSSValue GetValueSymbol(unsigned int start_pos, unsigned int length);
	short GetPropertySymbol(unsigned int start_pos, unsigned int length);
	unsigned short GetPseudoSymbol(unsigned int start_pos, unsigned int length);
	short GetPseudoPage(unsigned int start_pos, unsigned int length);
	CSS_MediaType GetMediaType(unsigned int start_pos, unsigned int length);
	COLORREF GetNamedColorIndex(unsigned int start_pos, unsigned int length);
	CSS_WebFont::Format GetFontFormat(unsigned int start_pos, unsigned int length);

	static uni_char* CopyAndRemoveEscapes(uni_char* dst, const uni_char* src, unsigned int nchars);

	/** Copy a string from the buffer into a heap allocated string.
		@param start_pos The start position in the buffer.
		@param length The number of uni_chars to copy from the buffer. The number of
					  uni_chars copied to the resulting string might be fewer due to
					  un-escaping.
		@param unescape CSS escapes are converted to their UTF-16 representations.
		@return A heap allocated null-terminated string that the caller is responsible
				for deleting with OP_DELETEA. The length of the returned string might differ
				from 'length' is unescape is TRUE. NULL on OOM. */
	uni_char* GetString(unsigned int start_pos, unsigned int length, BOOL unescape = TRUE);

	/** Copy a string from the buffer into a pre-allocated string.
		@param ret_str The buffer to copy the string into. The size of this buffer
					   must be at least (length + 1). Must be non-NULL.
		@param start_pos The start position in the buffer.
		@param length The number of uni_chars to copy from the buffer. The number of
					  uni_chars copied to the resulting string might be fewer due to
					  un-escaping.
		@param unescape CSS escapes are converted to their UTF-16 representations.
		@return Pointer to the null-termination character of the ret_str. The pointer
				to the null-termination character is returned to immediately know where
				to continue writing data into the buffer. Also, since the original string
				pointer is provided by the caller, it is not useful to return that. */
	uni_char* GetString(uni_char* ret_str, unsigned int start_pos, unsigned int length, BOOL unescape = TRUE);

#ifdef CSS_ANIMATIONS
	/** Get keyframe position from identifier. Known identifiers
	    are 'from' = 0.0 and 'to' = 1.0. */
	double GetKeyframePosition(unsigned int start_pos, unsigned int str_len);
#endif // CSS_ANIMATIONS

	uni_char* GetNameAndVal(unsigned int name_start, unsigned int name_len, unsigned int val_start, unsigned int val_len);
	uni_char* GetSkinString(unsigned int start_pos, unsigned int length);
	int GetTag(unsigned int start_pos, unsigned int length, NS_Type ns_type, BOOL case_sensitive, BOOL any_namespace);
	int GetHtmlAttr(unsigned int start_pos, unsigned int length);
	URL GetURLL(const URL& base_url, unsigned int start_pos, unsigned int length);
	CSS_MediaFeature GetMediaFeature(unsigned int start_pos, unsigned int length);

#ifdef CSS_ERROR_SUPPORT
	uni_char* GetLineString(unsigned int start_pos, unsigned int max_len);
#endif // CSS_ERROR_SUPPORT

#ifdef SAVE_SUPPORT
	/** Write characters from start_pos and length characters to an output stream.
		If length is -1 write the rest of the buffer from start_pos. */
	void WriteToStreamL(UnicodeFileOutputStream* stream, unsigned int start_pos, int length);
#endif // SAVE_SUPPORT

private:
	uni_char LookAheadSlow(unsigned int n);

private:

	// buffers
	unsigned int buf_cnt;
	const uni_char** buf;
	unsigned int* len;
	unsigned int total_len;

	uni_char orig_tag_buf[orig_tag_buf_len]; /* ARRAY OK 2009-02-12 rune */
	uni_char* tag_buf;
	unsigned int tag_buf_len;

	// ** current state **
	// current in_buffer
	const uni_char* in_buffer;
	// bytes left in current in_buffer
	unsigned int bytes_left;
	// buf index for current in_buffer
	unsigned int buf_idx;
	// position index base (total length of buffers before in_buffer
	unsigned int pos_base;

	// stored states
	unsigned int stack_idx;
	const uni_char* stored_buf[STACK_SIZE];
	unsigned int stored_bleft[STACK_SIZE];
	unsigned int stored_pos_base[STACK_SIZE];
	unsigned int stored_buf_idx[STACK_SIZE];
};

#endif
