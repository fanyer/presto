/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5BUFFER_H_
#define HTML5BUFFER_H_

#include "modules/util/simset.h"

class HTML5Tokenizer;

/**
 * Linked list of a series of data buffers, used by the HTML 5 tokenizer
 */
class HTML5Buffer : public ListElement<HTML5Buffer>
{
public:
	HTML5Buffer(HTML5Tokenizer* tokenizer, BOOL from_write, BOOL is_place_holder_buffer)
		: m_buffer(NULL), m_buffer_memory(NULL), m_buffer_length(0), m_original_buffer_length(0),
		  m_current_position(0), m_is_last_buffer(FALSE),
		  m_is_from_doc_write(from_write), m_must_signal_EOB(FALSE),
		  m_owns_buffer(TRUE), m_is_place_holder_buffer(is_place_holder_buffer),
		  m_tokenizer(tokenizer), m_move_to_next_buffer(0), m_current_line_number(0),
		  m_current_char_position_in_line(0), m_buffer_end_line_number(1),
		  m_buffer_end_char_position_in_line(1), m_stream_position(0)
	{}
	virtual ~HTML5Buffer();

	BOOL 			IsLastBuffer() const { return m_is_last_buffer; }
	void 			SetIsLastBuffer() { m_is_last_buffer = TRUE; }

	BOOL			IsFromDocWrite() const { return m_is_from_doc_write; }
	BOOL			IsPlaceHolderBuffer() const { return m_is_place_holder_buffer; }

	BOOL			MustSignalEndOfBuffer() { return m_must_signal_EOB; }
	void			SetMustSignalEndOfBuffer(BOOL must_signal) { m_must_signal_EOB = must_signal; }

	BOOL			OwnsBuffer() { return m_owns_buffer; }
	void			SetOwnsBuffer(BOOL owns) { m_owns_buffer = owns; }
	/**
	 * Used to insert a virtual buffer after this buffer if needed.
	 *
	 * Virtual buffers are inserted when skipping some characters in a buffer, for
	 * instance because we have replaced an entity reference, to avoid moving large
	 * amounts of data. The virtual buffer does not have it's own allocated array
	 * of chars but points into another buffers memory. When inserted the original
	 * buffer is truncated to the last character before the skipped ones, and then
	 * the virtual buffer will start at the offset after the last skipped character.
	 *
	 * @param[in] buf_start The last position before the skipped characters.
	 * @param[in] to_buffer The buffer containing buf_to.
	 * @param[in] buf_to The position of the first character after the skipped ones.
	 */
	void			InsertVirtualBufferL(uni_char *buf_start, HTML5Buffer* to_buffer, const uni_char *buf_to);

	BOOL 			HasPosition(const uni_char* pos) const { return pos >= m_buffer && pos < m_buffer + m_buffer_length; }
	/**
	 * Sets the current position of the buffer
	 * @param position                the new position.  MUST point somewhere actually
	 *                                inside the buffer, and should be beyond the current position
	 * @update_line_char_position     update the current line and character counters, which show
	 *                                were in the source buffer the current position is.
	 *                                Should generally be TRUE, unless you know better and
	 *                                have set the position to something more correct
	 *                                with SetLineAndCharacterPosition
	 */
	void 			SetCurrentPosition(const uni_char* position, BOOL update_line_char_position = TRUE)
	{
		if (update_line_char_position)
			GetLineAndCharacterPosition(position, m_current_line_number, m_current_char_position_in_line);
		OP_ASSERT(position >= m_buffer + m_current_position && position - m_buffer <= (signed)m_buffer_length);
		m_current_position = position - m_buffer;
	}

	void			IncreaseCurrentPosition(int distance, BOOL update_line_char_position = TRUE);

	/**
	 * Adds content to this buffer.  Should only be called once, initially per buffer.
	 * If you need to add more data later create a new buffer and let it succeed this
	 * buffer.
	 * @param[in] buffer Pointer to the start of the data to be added.
	 * @param[in] length The length, in uni_chars, of the data.
	 * @param[in] is_last_buffer TRUE if this is the last of the data from the stream.
	 * @param[in] add_newline Adds a newline at the end of the buffer. Used with doc write.
	 * @param[in] restore_skip All data before this offset will be skipped when calculating
	 * line numbers and position. Used when restoring state after a DSE recovery.
	 * @param[in] is_fragment TRUE if the data added is from innerHTML or doc.write().
	 */
	void 			AddDataL(const uni_char* buffer, unsigned int length, BOOL is_last_buffer, BOOL add_newline, unsigned restore_skip, BOOL is_fragment);

	/**
	 * The buffer has a current position, which is not necessarily the same as the
	 * current position in the tokenizer.  This current position is only updated
	 * when the tokenizer is in a restartable state, while the state in tokenizer
	 * is volatile and will be discarded if it LEAVES.
	 * @param[out] length Will hold the length of data after the returned position.
	 * @returns The current committed position in the buffer.
	 */
	const uni_char* GetDataAtCurrentPosition(unsigned& length) const
	{
		length = m_buffer_length - m_current_position;
		return m_buffer + m_current_position;
	}

	/** Get line number and character position of position in the original buffer.
	 *
	 * @param position position in buffer to get line/char position of.  Can be NULL
	 *                 to signify buffers current position, or a pointer to someplace
	 *                 in the buffer AFTER current position.
	 * @param line     Line number of position (1-indexed)
	 * @param character Character position in line of position (1-indexed)
	 */
	void			GetLineAndCharacterPosition(const uni_char* position, unsigned& line, unsigned& character);

	/**
	 * Sets the current line and character in the source file the current position can be found.
	 * Generally is updated automatically from SetCurrentPosition, but this can be called
	 * if the caller knows better.
	 */
	void			SetLineAndCharacterPosition(unsigned line, unsigned character) { m_current_line_number = line; m_current_char_position_in_line = character; }

	/**
	 * As for SetLineAndCharacterPosition, but tell the position of the last character
	 * in the buffer instead of the current.
	 */
	void			SetBufferEndLineAndCharacterPosition(unsigned line, unsigned character) { m_buffer_end_line_number = line; m_buffer_end_char_position_in_line = character; }

	/**
	 * The uni_char offset from the original stream where the memory buffer starts
	 * If this is the first buffer (or one which uses the same memory as the first buffer),
	 * then this will be 0, otherwise it will be the sum of the original_buffer_lengths
	 * of all preceding buffers, but not counting buffers inserted by scripts.
	 */
	unsigned		GetStreamPosition() const { return m_stream_position; }
	void			SetStreamPosition(unsigned pos) { m_stream_position = pos; }
	unsigned		GetPositionOffset() const { return m_current_position + (m_buffer - m_buffer_memory); }
	unsigned		GetCurrentPosition() const { return m_current_position; }

	HTML5Buffer*	GetNextBuffer() const { return Suc(); }
	HTML5Buffer*	GetPreviousBuffer()	const { return Pred(); }
	/// Gets first of previous buffers not inserted by doc.write
	HTML5Buffer*	GetPreviousNonDocWriteBuffer() const;

	const uni_char*	GetBufferStart() const { return m_buffer; }
	unsigned		GetBufferLength() const { return m_buffer_length; }
	unsigned		GetLengthRemainingFromPosition(const uni_char* pos) { OP_ASSERT(HasPosition(pos)); return m_buffer_length - (pos - m_buffer); }

	/// Replaces a section of the buffer with a unicode character (represented by the high and low uni_char of a surrogate pair)
	unsigned		Replace(const uni_char* from_position, HTML5Buffer* to_buffer, const uni_char* to_position, const uni_char *replacements, unsigned replacements_length);

	void			ReplaceNullAt(const uni_char *position);

	/// Sets the length of any buffers between this and upto to 0.  No including neither this nor upto.
	void			EmptyInterveningBuffers(HTML5Buffer* upto);
private:
	/**
	 * Updates accounting and transfers left over characters from previous buffer.
	 */
    void			RelayFromPrevious(const uni_char *&buffer, unsigned int &length);

	/**
	 * Copies content of input to internal buffer, and pre-process the input
	 * according to HTML 5 specification.
	 *
	 * @param restore_skip Means that we've already tokenized this number of
	 * characters from the *processed* input - causes that part of data not to
	 * be considered for some accounting.
	 */
	void			CopyAndPreprocessInput(const uni_char* input, unsigned int length, unsigned restore_skip);

	/**
	 * Copies content of fragment (typically from innerHTML) to internal buffer,
	 * and normalize newlines.
	 *
	 * @param add_newline Adds an extra \n character to the end of the input.
	 */
	void			CopyAndPreprocessFragment(const uni_char* input, unsigned int length, BOOL add_newline);

	// Dummy implementation with just one simple buffer
	uni_char*		m_buffer;
	uni_char*		m_buffer_memory;
	unsigned		m_buffer_length;
	unsigned		m_original_buffer_length;
	unsigned		m_current_position;
	BOOL			m_is_last_buffer;
	BOOL			m_is_from_doc_write;
	BOOL			m_must_signal_EOB;
	BOOL			m_owns_buffer; // at least one succeeding buffer uses this buffer memory
	BOOL			m_is_place_holder_buffer;  // marks empty buffer inserted on DSE recovery until we get the real data
	HTML5Tokenizer*	m_tokenizer;

	/* We don't split surrogate pairs between buffers, so this is used to transfer
	 * first part of a pair to next buffer if it otherwise would have been split.
	 * Also serves secondary function of replacing \r\n with \n correctly when
	 * buffers ends with a \r. */
	uni_char		m_move_to_next_buffer;

	unsigned		m_current_line_number;
	unsigned		m_current_char_position_in_line;
	unsigned		m_buffer_end_line_number;  // line number of last character in buffer
	unsigned		m_buffer_end_char_position_in_line;  // position in line of last character in buffer
	unsigned		m_stream_position;  // the number of uni_chars the start of this buffer is from the start of the input stream
};

/**
 * Buffer which can be used to point to sub strings inside HTML5Buffers, including
 * strings which span several HTML5Buffers.
 *
 * The strings does not need to be consecutive.
 */
class HTML5StringBuffer
{
private:
	/**
	 * Handles non-consecutive strings. For performance reasons the first
	 * buffer, and consecutive ones that follows, are always stored in the
	 * HTML5StringBuffer itself. It is not until a non-consecutive buffer is
	 * added that a descriptor is used.
	 */
	class ContentDescriptor
	{
	public:
		ContentDescriptor(const HTML5Buffer *buffer, const uni_char *content_start, unsigned length)
			: m_next(NULL),
			  m_buffer(buffer),
			  m_content_start(content_start),
			  m_length(length)
		{
		}

		ContentDescriptor *m_next;
		const HTML5Buffer *m_buffer;
		const uni_char *m_content_start;
		unsigned m_length;
	};

public:
	class ContentIterator;
	friend class ContentIterator;

	class ContentIterator
	{
	public:
		ContentIterator(const HTML5StringBuffer& buffer) : m_string_buffer(&buffer) { Reset(); }
		void Reset()
		{
			m_current = NULL;
			m_next = m_string_buffer->m_first_buffer;
			m_next_descriptor = m_string_buffer->m_first_descriptor;
		}

		const uni_char* GetNext(unsigned& length);
		unsigned GetTotalRemainingBufferSize();
		const HTML5Buffer* GetBuffer() { return m_current; }

	private:
		const HTML5Buffer*	m_next;
		const HTML5Buffer*	m_current;
		const ContentDescriptor* m_next_descriptor;
		const HTML5StringBuffer* m_string_buffer;
	};

	HTML5StringBuffer()
		: m_first_buffer(NULL),
		  m_content_start(NULL),
		  m_last_buffer(NULL),
		  m_earliest_buffer(NULL),
		  m_first_descriptor(NULL),
		  m_last_descriptor(NULL),
		  m_last_content_length(0),
		  m_total_length(0)
	{
	}

	~HTML5StringBuffer();

	HTML5StringBuffer &operator= (const HTML5StringBuffer &rhs)
	{
		if (this != &rhs)
		{
			m_first_buffer = rhs.m_first_buffer;
			m_content_start = rhs.m_content_start;
			m_last_buffer = rhs.m_last_buffer;
			m_earliest_buffer = rhs.m_earliest_buffer;
			m_last_content_length = rhs.m_last_content_length;
			m_total_length = rhs.m_total_length;
			m_first_descriptor = NULL;
			m_last_descriptor = NULL;

			CopyDescriptorsFromL(rhs);
		}
		return *this;
	}

	BOOL	ContainsData() const { return m_first_buffer != NULL; }
	unsigned	GetContentLength() const { return m_total_length; }
	BOOL	SpansOneBuffer() const { return m_first_buffer == m_last_buffer && !m_first_descriptor; }

	void	Clear()
	{
		m_first_buffer = m_last_buffer = m_earliest_buffer = NULL;
		m_content_start = NULL;
		m_last_content_length = 0;
		m_total_length = 0;
		while (m_first_descriptor)
		{
			ContentDescriptor *to_delete = m_first_descriptor;
			m_first_descriptor = m_first_descriptor->m_next;
			OP_DELETE(to_delete);
		}
		m_last_descriptor = NULL;
	}

	/// Clears previous content of StringBuffer, and adds string as new content.
	void	ResetL(const HTML5Buffer* buffer, const uni_char* string, unsigned length = 1) { Clear(); AppendL(buffer, string, length); }
	void	CopyL(const HTML5StringBuffer& other) { *this = other; }
	/// Duplicates values and takes over ownership of memory owned by other.
	void	TakeOver(HTML5StringBuffer& other) { *this = other; }

	BOOL	IsConsecutive(const HTML5Buffer* buffer, const uni_char* content);

	void	AppendL(const HTML5Buffer* buffer, const uni_char* position_in_buffer, unsigned length = 1);

	/// Concatenates one StringBuffer to the end of the other.  Only valid if this
	/// StringBuffer ends where the next StringBuffer starts.
	void	ConcatenateL(const HTML5StringBuffer& to_concatenate);

	/// Moves the first character from move_from and makes it the last character of this StringBuffer
	/// Only valid if this String Ends where the move_from StringBuffer starts.
	void	MoveCharacterFrom(HTML5StringBuffer& move_from);

	BOOL	HasData() const { return ContainsData(); }
	BOOL	Matches(const uni_char *str, BOOL skip_tag_start) const;

	/**
	 * Sets a pointer to a buffer that contains the entire value,
	 * allocates a buffer if needed.
	 * @param[out] dst Pointer to the buffer containing the value.
	 * @param[out] len Length of the value in uni_chars.
	 * @param[in] force_allocation If TRUE, a buffer for the value will always be allocated.
	 * @returns TRUE if the value buffer was allocated, in which case the caller mus free it.
	 */
	BOOL	GetBufferL(uni_char *&dst, unsigned &len, BOOL force_allocation) const;

	/**
	 * Returns the sub buffer referenced by this string buffer that
	 * occurs earliest in the tokenizers main list of buffers.
	 */
	const HTML5Buffer*	GetEarliestBuffer() { return m_earliest_buffer; }

private:
	HTML5StringBuffer (const HTML5StringBuffer &); // Copy constructor not implemented.

	void AddDescriptor(ContentDescriptor *desc)
	{
		if (!m_first_descriptor)
		{
			m_first_descriptor = desc;

			// The first descriptor might precede the current earliest buffer in the
			// list so we must check that. Subsequent descriptors will always be
			// succeeding the first descriptor.
			if (desc->m_buffer->Precedes(m_earliest_buffer))
				m_earliest_buffer = desc->m_buffer;
		}
		else
		{
			m_last_descriptor->m_next = desc;
			OP_ASSERT(!desc->m_buffer->Precedes(m_earliest_buffer));
		}
		m_last_descriptor = desc;
	}

	void CopyDescriptorsFromL(const HTML5StringBuffer &buffer)
	{
		for (ContentDescriptor *desc = buffer.m_first_descriptor; desc; desc = desc->m_next)
		{
			ContentDescriptor *new_desc = OP_NEW_L(ContentDescriptor, (*desc));
			AddDescriptor(new_desc);
		}
	}

	BOOL StartsAtBeginningOfBuffer() const { return m_first_buffer->GetBufferStart() == m_content_start; }

	/** Returns the content start of the last consecutive buffer. */
	const uni_char*	GetLastBufferContentStart() const { return (m_last_buffer && m_last_buffer != m_first_buffer) ? m_last_buffer->GetBufferStart() : m_content_start; }

	const HTML5Buffer*	m_first_buffer;
	const uni_char*		m_content_start; ///< Obviously points into first buffer.
	const HTML5Buffer*	m_last_buffer; ///< Last consecutive buffer, there might be more buffers in descriptors.
	/**
	 * Points to the sub buffer referenced in this HTML5StringBuffer that is
	 * earliest in the main list of all buffers in the HTML5Tokenizer if traversed
	 * from the beginning using Suc() which is done when cleaning up unused
	 * buffers. Used to avoid deleting buffers that are still referenced.
	 */
	const HTML5Buffer*	m_earliest_buffer;
	ContentDescriptor*  m_first_descriptor; ///< List of descriptors holding non-consecutive budfers.
	ContentDescriptor*  m_last_descriptor;
	unsigned			m_last_content_length; ///< How much of last consecutive buffer is part of the string.
	unsigned			m_total_length;
};

#endif /* HTML5BUFFER_H_ */
