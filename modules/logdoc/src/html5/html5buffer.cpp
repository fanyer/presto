/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/logdoc/src/html5/html5buffer.h"
#include "modules/logdoc/src/html5/html5tokenizer.h"

HTML5StringBuffer::~HTML5StringBuffer()
{
	while (m_first_descriptor)
	{
		ContentDescriptor *to_delete = m_first_descriptor;
		m_first_descriptor = m_first_descriptor->m_next;
		OP_DELETE(to_delete);
	}
}

BOOL HTML5StringBuffer::IsConsecutive(const HTML5Buffer* buffer, const uni_char* content)
{
	if (m_last_buffer == buffer &&
		GetLastBufferContentStart() + m_last_content_length == content)
		return TRUE;

	if (content == buffer->GetBufferStart())
		for (HTML5Buffer *iter = buffer->Pred(); iter; iter = iter->Pred())
		{
			if (m_last_buffer == iter)
				return TRUE;
			if (iter->GetBufferLength() != 0)
				break;
		}

	return FALSE;
}

void HTML5StringBuffer::AppendL(const HTML5Buffer* buffer, const uni_char* position_in_buffer, unsigned length/* = 1*/)
{
	OP_ASSERT(length >= 1);
	OP_ASSERT(buffer->HasPosition(position_in_buffer) && buffer->HasPosition(position_in_buffer + length - 1));

	if (!m_first_buffer || IsConsecutive(buffer, position_in_buffer))
	{
		if (!m_first_buffer)
		{
			m_first_buffer = m_earliest_buffer = buffer;
			m_content_start = position_in_buffer;
		}

		m_last_buffer = buffer;
		m_last_content_length = position_in_buffer - GetLastBufferContentStart() + length;
	}
	else
	{
		ContentDescriptor *desc = OP_NEW_L(ContentDescriptor, (buffer, position_in_buffer, length));
		AddDescriptor(desc);
	}

	m_total_length += length;

	OP_ASSERT(m_first_buffer->HasPosition(m_content_start));
	OP_ASSERT(m_last_buffer->HasPosition(GetLastBufferContentStart() + m_last_content_length - 1));
}

void HTML5StringBuffer::ConcatenateL(const HTML5StringBuffer& concatenate)
{
	if (!concatenate.ContainsData())
		return;
	else if (!ContainsData())
		*this = concatenate;
	else
	{
#ifdef DEBUG_ENABLE_OPASSERT
		ContentIterator iter(*this);
		unsigned prev_size = iter.GetTotalRemainingBufferSize();
		OP_ASSERT(prev_size == GetContentLength());
		ContentIterator concatenate_iter(concatenate);
		unsigned concatenate_size = concatenate_iter.GetTotalRemainingBufferSize();
		OP_ASSERT(concatenate_size == concatenate.GetContentLength());
#endif // DEBUG_ENABLE_OPASSERT

		if (!m_first_descriptor)
		{
			OP_ASSERT(IsConsecutive(concatenate.m_first_buffer, concatenate.m_content_start));  // Should only concatenate consecutive data

			CopyDescriptorsFromL(concatenate);

			// Just move this buffer's endpoint to be same as concatenate's
			m_last_buffer = concatenate.m_last_buffer;
			m_last_content_length = concatenate.GetLastBufferContentStart() - GetLastBufferContentStart() + concatenate.m_last_content_length;
		}
		else
		{
			// Move the concatenate's buffers to descriptors.
			ContentIterator iter(concatenate);
			ANCHOR(ContentIterator, iter);
			unsigned length;
			const uni_char *block = iter.GetNext(length);
			while (block)
			{
				ContentDescriptor *desc = OP_NEW_L(ContentDescriptor, (iter.GetBuffer(), block, length));
				AddDescriptor(desc);
				block = iter.GetNext(length);
			}
		}

		m_total_length += concatenate.GetContentLength();
#ifdef DEBUG_ENABLE_OPASSERT
		iter.Reset();
		OP_ASSERT(prev_size + concatenate_size == iter.GetTotalRemainingBufferSize());
		OP_ASSERT(m_total_length == prev_size + concatenate_size);
#endif // DEBUG_ENABLE_OPASSERT
	}
}

void HTML5StringBuffer::MoveCharacterFrom(HTML5StringBuffer& move_from)
{
	OP_ASSERT(!m_first_descriptor); // Non-consecutive buffers are not supported by this function.
	OP_ASSERT(move_from.ContainsData());
	if (!m_last_buffer)
	{
		// this buffer is empty to start with, so just set it to be first character
		// in the move_from buffer
		m_last_buffer = m_first_buffer = m_earliest_buffer = move_from.m_first_buffer;
		m_content_start = move_from.m_content_start;
		m_last_content_length = m_total_length = 1;
	}
	else
	{
		// this buffer should end where move_from buffer continues.
		OP_ASSERT(m_last_buffer == move_from.m_first_buffer
				  || (m_last_buffer == move_from.m_first_buffer->Pred() && move_from.StartsAtBeginningOfBuffer()));  // this buffer ends exactly at last byte of buffer
		OP_ASSERT(GetLastBufferContentStart() + m_last_content_length == move_from.m_content_start);

		// Move end position one space further in this buffer
		if (m_last_buffer->HasPosition(GetLastBufferContentStart() + 1))  // next char is in same buffer
			m_last_content_length++;
		else  // in next buffer, switch buffer
		{
			m_last_buffer = m_last_buffer->Suc();
			m_last_content_length = 1;  // include first character from buffer

			OP_ASSERT(m_last_buffer && m_last_buffer == move_from.m_first_buffer);
		}
		m_total_length++;
	}

	// and move start position in the move_from buffer
	if (move_from.m_first_buffer->HasPosition(move_from.m_content_start + 1))
	{
		move_from.m_content_start++;
		if (move_from.m_first_buffer == move_from.m_last_buffer)
			move_from.m_last_content_length--;
	}
	else
	{
		// need to move start to beginning of next buffer
		move_from.m_first_buffer = move_from.m_first_buffer->GetNextBuffer();
		move_from.m_content_start = move_from.m_first_buffer->GetBufferStart();
	}
	move_from.m_total_length--;
}

BOOL HTML5StringBuffer::Matches(const uni_char *str, BOOL skip_tag_start) const
{
	const uni_char *block;
	unsigned block_len, str_offset = 0, str_len = str ? uni_strlen(str) : 0;
	ContentIterator iter(*this);

	block = iter.GetNext(block_len);
	while (block && block_len > 0)
	{
		if (str_offset == 0 && skip_tag_start)
		{
			// skip leading '<'
			if (block_len > 0 && block[0] == '<')
				block++, block_len--;

			// skip leading '/', if this is an endtag
			if (block_len > 0 && block[0] == '/')
				block++, block_len--;
		}

		if (block_len > 0)
		{
			if (str_len - str_offset < block_len)
				return FALSE;
			else if (uni_strnicmp(str + str_offset, block, block_len) != 0)
				return FALSE;
			str_offset += block_len;
		}

		block = iter.GetNext(block_len);
	}

	return str_offset == str_len;
}

BOOL HTML5StringBuffer::GetBufferL(uni_char *&dst, unsigned &len, BOOL force_allocation) const
{
	BOOL must_allocate = force_allocation || !SpansOneBuffer();
	if (must_allocate)
	{
		len = GetContentLength();
		dst = OP_NEWA(uni_char, len + 1);
		LEAVE_IF_NULL(dst);

		unsigned block_len, used = 0;
		ContentIterator iter(*this);
		const uni_char *block = iter.GetNext(block_len);
		while (block)
		{
			uni_strncpy(dst + used, block, block_len);
			used += block_len;
			block = iter.GetNext(block_len);
		}
		dst[used] = 0;
		OP_ASSERT(used == len);  // We either didn't copy all data, or overran the buffer!
	}
	else
    {
		dst = const_cast<uni_char *>(m_content_start);
		len = m_total_length;
    }

	return must_allocate;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

const uni_char* HTML5StringBuffer::ContentIterator::GetNext(unsigned& length)
{
	const uni_char* res = NULL;
	length = 0;

	while (m_next && length == 0)
	{
		OP_ASSERT(m_next->InList());  // The buffer should be in the tokenizer's list of buffers, else it has been deleted!

		if (m_next == m_string_buffer->m_first_buffer)
			res = m_string_buffer->m_content_start;
		else
			res = m_next->GetBufferStart();

		if (m_next == m_string_buffer->m_last_buffer)
			length = m_string_buffer->m_last_content_length;
		else
		{
			length = m_next->GetBufferLength();

			if (m_next == m_string_buffer->m_first_buffer)
				length -= res - m_next->GetBufferStart();
		}

		m_current = m_next;
		if (m_next == m_string_buffer->m_last_buffer)
			m_next = NULL;
		else
			m_next = m_next->Suc();
	}

	while (length == 0 && m_next_descriptor)
	{
		m_current = m_next_descriptor->m_buffer;
		res = m_next_descriptor->m_content_start;
		length = m_next_descriptor->m_length;

		m_next_descriptor = m_next_descriptor->m_next;
	}

	return res;
}

unsigned HTML5StringBuffer::ContentIterator::GetTotalRemainingBufferSize()
{
	unsigned size = 0;
	for (const HTML5Buffer* buf = m_next; buf; buf = buf->Suc())
	{
		int n = 0;
		if (buf == m_string_buffer->m_last_buffer)
			n += m_string_buffer->m_last_content_length;
		else
		{
			n += buf->GetBufferLength();

			if (buf == m_string_buffer->m_first_buffer)
				n -= m_string_buffer->m_content_start - buf->GetBufferStart();
		}

		size += n;

		if (buf == m_string_buffer->m_last_buffer)
			break;
	}

	for (const ContentDescriptor *desc = m_next_descriptor; desc; desc = desc->m_next)
		size += desc->m_length;

	return size;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

HTML5Buffer::~HTML5Buffer()
{
	Out();

	if (m_owns_buffer)
		OP_DELETEA(m_buffer_memory);
}

HTML5Buffer* HTML5Buffer::GetPreviousNonDocWriteBuffer() const
{
	for (HTML5Buffer* buf = GetPreviousBuffer(); buf; buf = buf->GetNextBuffer())
		if (!buf->IsFromDocWrite())
			return buf;

	return NULL;
}

void HTML5Buffer::AddDataL(const uni_char* buffer, unsigned int length, BOOL is_last_buffer, BOOL add_newline, unsigned restore_skip, BOOL is_fragment)
{
	OP_ASSERT(!m_is_last_buffer);  // We were told we already had last buffer, why are we getting more?
	OP_ASSERT(!m_buffer);

	if (add_newline)
		m_original_buffer_length = length + 1;
	else
		m_original_buffer_length = length;

	m_buffer_memory = m_buffer = OP_NEWA_L(uni_char, m_original_buffer_length + 1);  // may need room for one extra byte which was moved from previous buffer
	m_is_last_buffer = is_last_buffer;

	if (is_fragment)
		CopyAndPreprocessFragment(buffer, length, add_newline);
	else
		CopyAndPreprocessInput(buffer, length, restore_skip);

}

void HTML5Buffer::RelayFromPrevious(const uni_char *&buffer, unsigned int &length)
{
	if (HTML5Buffer *prev_buf = GetPreviousBuffer())
	{
		// Line numbers and stream positions only make sense when coming from the same
		// source file (i.e. not doc.write buffers).
		// We're here assuming that only doc.write buffers are not from the original source,
		// which probably isn't correct...
		if (HTML5Buffer* prev_source_buf = GetPreviousNonDocWriteBuffer())
		{
			m_buffer_end_line_number = m_current_line_number = prev_source_buf->m_buffer_end_line_number;
			m_buffer_end_char_position_in_line = m_current_char_position_in_line = prev_source_buf->m_buffer_end_char_position_in_line;
			m_stream_position = prev_source_buf->m_stream_position + prev_source_buf->m_original_buffer_length;
		}

		if (uni_char previous_chr = prev_buf->m_move_to_next_buffer)  // last char of previous buffer which we prefer to have in this one, so add it first
		{
			uni_char *write = m_buffer;
			if (previous_chr == '\n')  // this is really a signal to ignore \n if it's the first byte of the buffer
			{
				if (*buffer == '\n')
				{
					buffer++;
					length--;
				}
			}
			else if (Unicode::IsLowSurrogate(*buffer))
			{
				unsigned long replacement = m_tokenizer->CheckAndReplaceInvalidUnicode(Unicode::DecodeSurrogate(previous_chr, *buffer), TRUE, m_buffer_end_line_number, m_buffer_end_char_position_in_line);

				if (replacement >= 0x10000)  // need to write a surrogate pair
				{
					OP_ASSERT(Unicode::IsHighSurrogate(previous_chr));  // should only need this if the original character was a surrogate character too
					uni_char high, low;
					Unicode::MakeSurrogate(replacement, high, low);
					// since the original buffer contained the surrogate pair, there should be enough room for it in this buffer too
					*write++ = high;
					*write++ = low;
					m_buffer_length += 2;

					buffer++;
					length--;
				}
			}
			else
				*write++ = UNICODE_REPLACEMENT_CHARACTER;
		}
	}
}

void HTML5Buffer::CopyAndPreprocessFragment(const uni_char* buffer, unsigned int length, BOOL add_newline)
{
	/* There shouldn't be any split surrogates for fragments. */
	OP_ASSERT(!GetPreviousBuffer() || !GetPreviousBuffer()->m_move_to_next_buffer);

	const uni_char* read = buffer;
	uni_char* write = m_buffer;

	for (unsigned remaining_length = length; remaining_length; remaining_length--, read++)
	{
		uni_char next_char = *read;

        if (next_char != '\r' && next_char != '\n')
        {
            *write++ = next_char;
            m_buffer_end_char_position_in_line++;
        }
        else
        {
            *write++ = '\n';
            m_buffer_end_line_number++;
            m_buffer_end_char_position_in_line = 1;

            if (next_char == '\r')
                /* If next character is a \n, then skip it. */
                if (remaining_length > 1 && *(read + 1) == '\n')
				{
					remaining_length--; read++;
				}
		}
	}

	if (add_newline)
		*write++ = '\n';
	m_buffer_length = write - m_buffer;
	OP_ASSERT(m_buffer_length <= length + 1);
}

void HTML5Buffer::CopyAndPreprocessInput(const uni_char* buffer, unsigned int length, unsigned restore_skip)
{
	RelayFromPrevious(buffer, length);

	const uni_char* read = buffer;
	uni_char* write = &m_buffer[m_buffer_length];

	for (unsigned remaining_length = length; remaining_length; remaining_length--, read++)
	{
		uni_char next_char = *read;
		if (next_char <= 0x7E && next_char >= 0x20)
			*write++ = next_char;
		else
		{
			unsigned long codepoint = next_char;

			if (next_char == '\r' || next_char == '\n')
			{
				*write++ = '\n';
				if ((write - m_buffer) - static_cast<int>(restore_skip) >= 0)  // already accounted for these characters if they're in the skip range
				{
					m_buffer_end_line_number++;
					m_buffer_end_char_position_in_line = 1;
				}

				if (next_char == '\r')
				{
					// if next character is a \n, then skip it
					if (remaining_length > 1)
					{
						if (*(read + 1) == '\n')
						{
							remaining_length--; read++;
						}
					}
					else
						m_move_to_next_buffer = '\n';  // this is a signal to ignore \n in next buffer if it starts with one.  We've already added a \n here
				}

				continue;
			}
			else if (Unicode::IsHighSurrogate(next_char))
			{
				if (remaining_length > 1)
				{
					uni_char low = *(read + 1);
					if (Unicode::IsLowSurrogate(low))
					{
						codepoint = Unicode::DecodeSurrogate(next_char, low);
						remaining_length--; read++;  // skip the low byte
					}
				}
				else if (!m_is_last_buffer)
				{
					// don't split buffer in surrogate pair, so move this one to next buffer
					m_move_to_next_buffer = next_char;
					continue;
				}
			}

			unsigned long replacement = m_tokenizer->CheckAndReplaceInvalidUnicode(codepoint, TRUE, m_buffer_end_line_number, m_buffer_end_char_position_in_line);

			if (replacement >= 0x10000)  // need to write a surrogate pair
			{
				OP_ASSERT(Unicode::IsHighSurrogate(next_char) || (GetPreviousBuffer() && GetPreviousBuffer()->m_move_to_next_buffer));  // should only need this if the original character was a surrogate character too
				uni_char high, low;
				Unicode::MakeSurrogate(replacement, high, low);
				// since the original buffer contained the surrogate pair, there should be enough room for it in this buffer too
				*write++ = high;
				*write++ = low;
				continue;
			}

			*write++ = static_cast<uni_char>(replacement);
		}

		if ((write - m_buffer) - static_cast<int>(restore_skip) >= 0)  // already accounted for these characters if they're in the skip range
			m_buffer_end_char_position_in_line++;
	}

	m_buffer_length = write - m_buffer;
	OP_ASSERT(m_buffer_length <= length + 2);
}

void HTML5Buffer::InsertVirtualBufferL(uni_char *buf_start, HTML5Buffer* to_buffer, const uni_char *buf_to)
{
	if (this != to_buffer)  // already different buffers, don't need a virtual one inserted
	{
		// adjust the starting point of the to_buffer
		to_buffer->m_buffer_length = to_buffer->m_buffer_length - (buf_to - to_buffer->m_buffer);
		to_buffer->m_buffer = const_cast<uni_char*>(buf_to);
		to_buffer->m_current_position = 0;
	}
	else if (static_cast<int>(m_buffer_length) == (buf_to - m_buffer))  // nothing more in this buffer after buf_to
		m_buffer_length = buf_start - m_buffer;
	else  // need to split buffer and add a virtual buffer
	{
		HTML5Buffer *new_buffer = OP_NEW_L(HTML5Buffer, (m_tokenizer, m_is_from_doc_write, FALSE));

		new_buffer->m_buffer = const_cast<uni_char*>(buf_to);
		new_buffer->m_buffer_memory = m_buffer_memory;

		SetOwnsBuffer(FALSE);
		new_buffer->SetMustSignalEndOfBuffer(m_must_signal_EOB);
		SetMustSignalEndOfBuffer(FALSE);

		new_buffer->m_buffer_length = to_buffer->m_buffer_length - (buf_to - to_buffer->m_buffer);
		m_buffer_length = buf_start - m_buffer;
		new_buffer->m_original_buffer_length = m_original_buffer_length;

		new_buffer->m_current_position = 0;
		if (m_is_last_buffer)
		{
			new_buffer->m_is_last_buffer = TRUE;
			m_is_last_buffer = FALSE;
		}

		new_buffer->m_move_to_next_buffer = m_move_to_next_buffer;
		m_move_to_next_buffer = 0;

		new_buffer->m_current_line_number = m_current_line_number;
		new_buffer->m_current_char_position_in_line = m_current_char_position_in_line;

		new_buffer->m_buffer_end_line_number = m_buffer_end_line_number;
		m_buffer_end_line_number = m_current_line_number;

		new_buffer->m_buffer_end_char_position_in_line = m_buffer_end_char_position_in_line;
		m_buffer_end_char_position_in_line = m_current_char_position_in_line;

		new_buffer->m_stream_position = m_stream_position;

		new_buffer->Follow(this);
	}
}

void HTML5Buffer::IncreaseCurrentPosition(int distance, BOOL update_line_char_position)
{
	OP_PROBE7(OP_PROBE_HTML5BUFFER_INCCURRENTPOS);

	OP_ASSERT(distance >= 0 && distance <= (signed)m_buffer_length);

	// update m_current_line_number and m_current_char_position_in_line each time we advance position
	if (update_line_char_position)
		GetLineAndCharacterPosition(m_buffer + m_current_position + distance, m_current_line_number, m_current_char_position_in_line);

	m_current_position += distance;
}

void HTML5Buffer::GetLineAndCharacterPosition(const uni_char* position, unsigned& line, unsigned& character)
{
	OP_PROBE7(OP_PROBE_HTML5BUFFER_GETLINEANDPOS);

	if (m_current_line_number == 0)  // hasn't been initialized yet
		if (HTML5Buffer* prev_buf = GetPreviousNonDocWriteBuffer())  // we're not first buffer, continue where last one left off
		{
			m_current_line_number = prev_buf->m_current_line_number;
			m_current_char_position_in_line = prev_buf->m_current_char_position_in_line;
		}
		else
			m_current_line_number = m_current_char_position_in_line = 1;

	line = m_current_line_number;
	character = m_current_char_position_in_line;

	if (!position)
		return;

	// m_current_line_number and m_current_char_position_in_line are relative to
	// m_current_position, but position may point someplace else.  So calculate
	// how much further along position is
	int distance = position - m_buffer - m_current_position;
	OP_ASSERT(distance >= 0 && distance <= (signed)m_buffer_length);

	uni_char* chr = m_buffer + m_current_position;
	for (; distance; distance--, chr++)
	{
		if (*chr == '\n')
		{
			line++;
			character = 0;
		}
		else if (!Unicode::IsHighSurrogate(*chr))  // we count characters, so only count surrogate pairs as one character
			character++;
	}
}

void HTML5Buffer::EmptyInterveningBuffers(HTML5Buffer* upto)
{
	HTML5Buffer* end_buffer = Suc();
	for (;
		 end_buffer && end_buffer != upto;
		 end_buffer = end_buffer->Suc())
	{
		end_buffer->m_buffer_length = 0;  // we've replaced all content in this buffer
	}
}

unsigned HTML5Buffer::Replace(const uni_char* from_position, HTML5Buffer* to_buffer, const uni_char* to_position, const uni_char *replacements, unsigned replacements_length)
{
	OP_ASSERT(HasPosition(from_position));

	// To make sure our position counting is correct, adjust for how many characters we replace
	int removed_chars = 0;
	unsigned num_replaced_chars = 0;
	const uni_char* c = from_position;
	const HTML5Buffer* buf = this;
	while (c != to_position)
	{
		if (!Unicode::IsHighSurrogate(*c))
			removed_chars++;

		OP_ASSERT(*c != '\n');  // We only replace entities, they don't contain newlines.  Messes up our line counting if we do replace new lines

		c++;
		if (!buf->HasPosition(c) && c != to_position)  // we've exhausted this buffer
		{
			buf = buf->GetNextBuffer();  // switch to next
			if (!buf)
				break;

			unsigned dummy;
			c = buf->GetDataAtCurrentPosition(dummy);
		}
	}

	// we're replacing all of this with just one or two characters
	m_current_char_position_in_line += removed_chars - ((replacements_length > 2 && replacements[3]) ? 2 : 1);

	HTML5Buffer* write_buf = const_cast<HTML5Buffer*>(this);
	uni_char* write_pos = const_cast<uni_char*>(from_position);

	if (to_buffer == this)
	{
		OP_ASSERT(HasPosition(to_position-1));

		for (unsigned i = 0; i < replacements_length; i++)
		{
			if (replacements[i])
				*write_pos++ = replacements[i];
		}

		num_replaced_chars = write_pos - from_position;
	}
	else
	{
#define SPACE_NEEDED_FOR_CURRENT_REPLACEMENT(idx) \
		(replacements[idx] ? (replacements[idx - 1] ? 2 : 1) : 0)

		// Ensure enough room in the existing buffers (not splitting surrogate pairs)...
		HTML5Buffer *iter_buf = write_buf;
		unsigned unused_space = iter_buf->m_buffer_length - (write_pos - iter_buf->m_buffer);
		unsigned rep_idx = 1; // start on the odd numbered low part
		unsigned space_needed;
		space_needed = SPACE_NEEDED_FOR_CURRENT_REPLACEMENT(rep_idx);

		while (iter_buf != to_buffer)
		{
			while (space_needed > 0 && space_needed <= unused_space)
			{
				write_buf = iter_buf;

				if (replacements[rep_idx - 1]) // check for high part
					*write_pos++ = replacements[rep_idx - 1];
				*write_pos++ = replacements[rep_idx];

				num_replaced_chars += space_needed;
				unused_space -= space_needed;

				rep_idx += 2;
				if (rep_idx < replacements_length)
					space_needed = SPACE_NEEDED_FOR_CURRENT_REPLACEMENT(rep_idx);
				else
					space_needed = 0;
			}

			iter_buf->m_buffer_length = write_pos - iter_buf->m_buffer;

			iter_buf = iter_buf->Suc();
			write_pos = iter_buf->m_buffer;
			unused_space = iter_buf->m_buffer_length;
		}

		if (space_needed > 0)
		{
			space_needed = 0;
			unsigned rest_idx = rep_idx;
			while (rest_idx < replacements_length)
			{
				space_needed += SPACE_NEEDED_FOR_CURRENT_REPLACEMENT(rest_idx);
				rest_idx += 2;
			}

			// Check if there is room in the to_buffer
			unused_space = to_position - to_buffer->m_buffer;
			if (space_needed <= unused_space)
			{
				write_buf = to_buffer;
				rep_idx--; // include the even numbered high part
				for (; rep_idx < replacements_length; rep_idx++)
				{
					if (replacements[rep_idx])
					{
						*write_pos++ = replacements[rep_idx];
						num_replaced_chars++;
					}
				}
			}
			else
			{
				// Need to insert buffer
				HTML5Buffer *new_buffer = OP_NEW_L(HTML5Buffer, (m_tokenizer, write_buf->m_is_from_doc_write, FALSE));

				write_pos = new_buffer->m_buffer = new_buffer->m_buffer_memory = OP_NEWA_L(uni_char, space_needed);

				new_buffer->m_buffer_length = new_buffer->m_original_buffer_length = space_needed;
				new_buffer->m_current_position = 0;

				rep_idx--; // include the even numbered high part
				for (; rep_idx < replacements_length; rep_idx++)
				{
					if (replacements[rep_idx])
					{
						*write_pos++ = replacements[rep_idx];
						num_replaced_chars++;
					}
				}

				new_buffer->Precede(to_buffer);
				write_buf = new_buffer;
			}
		}
	}

	write_buf->InsertVirtualBufferL(write_pos, to_buffer, to_position);

	return num_replaced_chars;
}

void HTML5Buffer::ReplaceNullAt(const uni_char *position)
{
	OP_ASSERT(HasPosition(position));
	m_buffer[position - m_buffer] = UNICODE_REPLACEMENT_CHARACTER;
}
