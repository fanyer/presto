/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/logdoc/src/html5/html5tokenizer.h"
#include "modules/logdoc/src/html5/html5token.h"

#include "modules/util/opstring.h"
#include "modules/encodings/detector/charsetdetector.h"

#ifdef HTML5_STANDALONE
# include "modules/logdoc/src/html5/standalone/standalone.h"
# include "modules/logdoc/logdoc_constants.h" // for SplitTextLen
#endif // HTML5_STANDALONE

#define is_ascii_upper(c) ((c) >= 'A' && (c) <= 'Z')
#define is_ascii_lower(c) ((c) >= 'a' && (c) <= 'z')
#define is_ascii_alpha(c) (is_ascii_lower(c) || is_ascii_upper(c))
#define is_ascii_alnum(c) (is_ascii_alpha(c) || ((c) >= '0') && (c) <= '9')
#define ascii_tolower(c) (is_ascii_upper(c) ? (c) + 0x20 : (c))
#define ascii_force_lower(c) (c | 0x20)

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof(arr[0]))
#endif

#define ENSURE_NOT_EOF_L() do { if (EmitErrorIfEOFL(token)) return TRUE; } while (0)
#define REPORT_ERROR_AND_CONTINUE_IN_DATA_STATE_IF_EOF(emit) do { \
	if (IsAtEOF()) \
	{ \
		ReportTokenizerError(HTML5Parser::UNEXPECTED_EOF); \
		m_state = DATA; \
		return emit; \
	} \
} while (0)

#define CONSUME_AND_CONTINUE_IN_STATE(state) do { ConsumeL(); m_state = (state); return FALSE; } while (0)
#define CONSUME_EMIT_AND_CONTINUE_IN_STATE(state) do { ConsumeL(); m_state = (state); return TRUE; } while (0)

#define RESET_TOKEN_TO_EOF_L { \
	if (m_current_buffer->MustSignalEndOfBuffer()) { \
		m_current_buffer->SetMustSignalEndOfBuffer(FALSE); \
		LEAVE(HTML5ParserStatus::FINISHED_DOC_WRITE); \
	} \
	token.Reset(HTML5Token::ENDOFFILE); \
}
#define RESET_TO_DATA_STATE { m_state = DATA; }

#define OUTPUT_TOKENS 0

#define CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L do { \
	if (token.GetType() == HTML5Token::INVALID) \
		token.Reset(HTML5Token::CHARACTER); \
	token.ConcatenateToDataL(m_html5_temp_buffer); \
	m_html5_temp_buffer.Clear(); \
	data_start = m_position; \
} while (0)

static unsigned long replacement_characters[0x9F - 0x7F + 1] = {
	0x007F, // 0x7F -> just itself
	0x20AC, // 0x80 -> euro sign
	0x0081, // 0x81 -> <control>
	0x201A, // 0x82 -> single low-9 quotation mark
	0x0192,	// 0x83 -> F with hook
	0x201E,	// 0x84 -> double low-9 quotation mark
	0x2026, // 0x85 -> horizontal ellipsis
	0x2020, // 0x86 -> dagger
	0x2021, // 0x87 -> double dagger
	0x02C6, // 0x88 -> modifier letter circumflex accent
	0x2030, // 0x89 -> per mille sign
	0x0160, // 0x8A -> latin capital letter s with caron
	0x2039, // 0x8B -> single left-pointing angle quotation mark
	0x0152, // 0x8C -> latin capital ligature oe
	0x008D, // 0x8D -> <control>
	0x017D, // 0x8E -> latin capital letter z with caron
	0x008F, // 0x8F -> <control>
	0x0090, // 0x90 -> <control>
	0x2018, // 0x91 -> left single quotation mark
	0x2019, // 0x92 -> right single quotation mark
	0x201C, // 0x93 -> left double quotation mark
	0x201D, // 0x94 -> right double quotation mark
	0x2022, // 0x95 -> bullet
	0x2013, // 0x96 -> en dash
	0x2014, // 0x97 -> em dash
	0x02DC, // 0x98 -> small tilde
	0x2122, // 0x99 -> trade mark sign
	0x0161, // 0x9A -> latin small letter s with caron
	0x203A, // 0x9B -> single right-pointing angle quotation mark
	0x0153, // 0x9C -> latin small ligature oe
	0x009D, // 0x9D -> <control>
	0x017E, // 0x9E -> latin small letter z with caron
	0x0178, // 0x9F -> latin capital letter y with diaeresis
};

/**
* class used to store a position in the input buffer, when we
* speculatively look ahead.  Used during entity matching.
*/
class BufferStorePoint : public CleanupItem
{
public:
	BufferStorePoint(HTML5Tokenizer* tokenizer) : CleanupItem(), m_tokenizer(tokenizer)
	{
		m_original_buffer = m_tokenizer->m_current_buffer;
		m_original_position = m_tokenizer->m_position;
		m_tokenizer->SetIsLookingAheadL(FALSE, NULL); // to make doc.write handle buffers correctly
	}

	~BufferStorePoint() { Reset(); }

	void	Reset()
	{
		if (m_original_buffer->HasPosition(m_original_position))
		{
			m_tokenizer->m_current_buffer = m_original_buffer;
			m_tokenizer->m_position = m_original_position;
		}
		else // the old position has been replaced.  Find next position we have and use that
		{
			HTML5Buffer* buf = m_original_buffer->Suc();
			while (buf && buf->GetBufferLength() == 0)
				buf = buf->Suc();

			if (!buf)  // there are no more buffers with data, so use the original (and now empty) buffer
				buf = m_original_buffer;

			m_tokenizer->m_current_buffer = buf;
			m_tokenizer->m_position = buf->GetBufferStart();
		}

		m_tokenizer->m_remaining_data_length = m_tokenizer->m_current_buffer->GetLengthRemainingFromPosition(m_tokenizer->m_position);
	}

	virtual void Cleanup(int error)
	{
		CleanupItem::Cleanup(error);

		if (error == HTML5ParserStatus::FINISHED_DOC_WRITE)
			m_tokenizer->SetIsLookingAheadL(TRUE, NULL); // to make doc.write handle buffers correctly

		Reset();
	}

	unsigned Replace(const uni_char replacements[], unsigned replacements_length)
	{
		return m_original_buffer->Replace(m_original_position, m_tokenizer->m_current_buffer, m_tokenizer->m_position, replacements, replacements_length);
	}

private:
	HTML5Tokenizer*		m_tokenizer;
	HTML5Buffer*		m_original_buffer;
	const uni_char*		m_original_position;
};


unsigned long HTML5Tokenizer::CheckAndReplaceInvalidUnicode(unsigned long codepoint, BOOL is_preprocessing, unsigned line, unsigned chr)
{
	BOOL parse_error = FALSE;

	if (codepoint < 0xFF)
	{
		if (codepoint < 0x20 || codepoint > 0x7E) // just to quickly filter out the most normal characters
		{
			if (codepoint >= 0x7F && codepoint <= 0x9F)
			{
				if (!is_preprocessing)
				{
					OP_ASSERT(codepoint - 0x7F < ARRAY_SIZE(replacement_characters));
					codepoint = replacement_characters[codepoint - 0x7F];
				}
				parse_error = TRUE;
			}
			else if ((codepoint >= 0x01 && codepoint <= 0x08)
				|| (codepoint >= 0x0E && codepoint <= 0x1F)
				|| codepoint == 0x0B
				|| codepoint == 0x0D)
			{
				parse_error = TRUE;
			}
			else if (codepoint == 0 && !is_preprocessing)
			{
				codepoint = UNICODE_REPLACEMENT_CHARACTER;
				parse_error = TRUE;
			}
		}
	}
	else if ((codepoint >= 0xD800 && codepoint <= 0xDFFF)  // surrogate
		|| codepoint > MAX_UNICODE_POINT)
	{
		codepoint = UNICODE_REPLACEMENT_CHARACTER;
		parse_error = TRUE;
	}
	else if (codepoint >= 0xFDD0)
	{
		if ((codepoint <= 0xFDEF)
			|| codepoint == 0xFFFE
			|| codepoint == 0xFFFF)
		{
			parse_error = TRUE;
		}
		else if (codepoint > 0x10000)
		{
			for (unsigned long illegal = 0x1FFFE; illegal < 0x10FFFF; illegal += 0x10000)
				if (codepoint == illegal || codepoint == illegal + 1)
				{
					parse_error = TRUE;
					break;
				}
		}
	}

	if (parse_error)
		if (is_preprocessing)
			ReportTokenizerError(HTML5Parser::INVALID_CHARACTER, 1, line, chr);
		else
			ReportTokenizerError(HTML5Parser::INVALID_NUMERIC_ENTITY);

	return codepoint;
}

BOOL HTML5Tokenizer::ParseNumberEntityL(uni_char *replacements)
{
	int base = 10;

	if (IsAtEOF())
	{
		ReportTokenizerError(HTML5Parser::UNEXPECTED_EOF);
		return FALSE;
	}

	uni_char next_char = GetNextInputCharacterL();

	if (next_char == 'x' || next_char == 'X')
	{
		base = 16;
		ConsumeL();

		if (IsAtEOF())
		{
			ReportTokenizerError(HTML5Parser::UNEXPECTED_EOF);
			return FALSE;
		}

		next_char = GetNextInputCharacterL();
	}

	/** The biggest number we'll accept is 0x10FFFF, which in decimal is
	 *  1114111.  That means we won't need more than 7 digits.  If we get more
	 *  we'll just ignore the rest, and return the 0xFFFD (replacement character)
	 *  as the result, since it's out of range.
	 */
	uni_char number_buffer[8];  // ARRAY OK 2011-02-24 arneh
	unsigned current_digit = 0;

	BOOL skipping_leading_zeros = TRUE, has_seen_zero = FALSE;
	while (TRUE)
	{
		if (next_char >= '0' && next_char <= '9')
		{
			if (current_digit < 7)
				number_buffer[current_digit] = next_char;
		}
		else if (base == 16 &&
				 ((next_char >= 'A' && next_char <= 'F')
				 || (next_char >= 'a' && next_char <= 'f')))
		{
			if (current_digit < 7)
				number_buffer[current_digit] = next_char;
		}
		else
			break;

		// value can still be in range, even with a ton of leading zeros.  So
		// we only keep the first zero and skip the rest if the number starts
		// with leading zeros
		if (skipping_leading_zeros && next_char != '0')
			skipping_leading_zeros = FALSE;

		if (!skipping_leading_zeros)
			current_digit++;
		else
			has_seen_zero = TRUE;

		ConsumeL();
		if (IsAtEOF())
			break;

		next_char = GetNextInputCharacterL();
	}

	// if we have seen just zeros (which have been skipped), then add one to buffer, so that will be parsed
	if (current_digit == 0 && has_seen_zero)
	{
		number_buffer[current_digit] = '0';
		current_digit++;
	}

	if (current_digit == 0) // didn't get any digits
	{
		ReportTokenizerError(HTML5Parser::INVALID_NUMERIC_ENTITY);
		return FALSE;
	}

	if (current_digit > 7)  // it's too big to be a valid entity, so ignore the value
		current_digit = 0;

	number_buffer[current_digit] = 0;

	if (next_char == ';')
		ConsumeL();
	else
		ReportTokenizerError(HTML5Parser::EXPECTED_SEMICOLON_AT_END_OF_ENTITY);

	unsigned long codepoint = uni_strtoul(number_buffer, NULL, base);
	codepoint = CheckAndReplaceInvalidUnicode(codepoint, FALSE);
	HTML5EntityMatcher::GetUnicodeSurrogates(codepoint, replacements[0], replacements[1]);

	return TRUE;
}

BOOL HTML5Tokenizer::ParseNamedEntityL(const uni_char *&replacements, unsigned &rep_length, BOOL in_attribute)
{
	m_entity_matcher.Reset();
	HTML5Buffer* last_terminating_state_buffer = NULL;
	const uni_char* last_terminating_state_position = NULL;
	BOOL last_is_parse_error = FALSE;

	while (TRUE)
	{
		if (IsAtEOF())
			break;

		uni_char next_char = GetNextInputCharacterL();

		if (!m_entity_matcher.MatchNextChar(next_char))
			break;

		if (m_entity_matcher.IsTerminatingState())
		{
			rep_length = m_entity_matcher.GetReplacementCharacters(replacements);
			last_is_parse_error = next_char != ';';

			// remember position
			last_terminating_state_buffer = m_current_buffer;
			last_terminating_state_position = m_position;
			OP_ASSERT(m_current_buffer->HasPosition(m_position));
		}

		ConsumeL();
	}

	if (last_terminating_state_buffer)  // got a match
	{
		// reset position to where we got the last match
		m_current_buffer = last_terminating_state_buffer;
		m_position = last_terminating_state_position;
		OP_ASSERT(m_current_buffer->HasPosition(m_position));
		uni_char next_char = *m_position;
		m_remaining_data_length = m_current_buffer->GetLengthRemainingFromPosition(m_position);
		ConsumeL();

		if (last_is_parse_error)
			ReportTokenizerError(HTML5Parser::EXPECTED_SEMICOLON_AT_END_OF_ENTITY);

		if (in_attribute && !IsAtEOF() && next_char != ';')
		{
			next_char = GetNextInputCharacterL();
			// In attributes it's not a match if next character is = or ascii character or number
			if (next_char == '=' || is_ascii_alnum(next_char))
				return FALSE;
		}

		return TRUE;
	}
	else
	{
		ReportTokenizerError(HTML5Parser::INVALID_NAMED_ENTITY);
		rep_length = 0;
	}

	return FALSE;
}

unsigned HTML5Tokenizer::ReplaceCharacterEntityL(uni_char additional_allowed_character, BOOL in_attribute, BOOL replace_null)
{
	BufferStorePoint resetter(this);

	ConsumeL();  // &

	if (IsAtEOF())
		return 0;

	uni_char next_char = GetNextInputCharacterL();
	uni_char numeric_replacements[2]; /* ARRAY OK 2011-09-08 danielsp */
	const uni_char *replacements = NULL;
	unsigned replacements_length = 0;
	BOOL replace = FALSE;

	if (next_char == '<' || next_char == '&' || HTML5Parser::IsHTML5WhiteSpace(next_char))
		return 0;
	else if (additional_allowed_character && next_char == additional_allowed_character)
		return 0;
	else if (next_char == '#')
	{
		ConsumeL();  // #

		replace = ParseNumberEntityL(numeric_replacements);
		replacements = numeric_replacements;
		replacements_length = 2;
	}
	else
		replace = ParseNamedEntityL(replacements, replacements_length, in_attribute);

	if (replace)
	{
		uni_char repl[4];  /* ARRAY OK 2011-11-09 arneh */
		if (replace_null && replacements[0] == 0 && replacements[1] == 0)
		{
			op_memcpy(repl, replacements, MIN(sizeof(repl), replacements_length * sizeof(uni_char)));
			repl[1] = UNICODE_REPLACEMENT_CHARACTER;
			replacements = repl;
		}

		return resetter.Replace(replacements, replacements_length);
	}

	return 0;
}

BOOL HTML5Tokenizer::IsAppropriateEndTag()
{
	return m_appropriate_end_tag.Length() && m_html5_temp_buffer.Matches(m_appropriate_end_tag.GetBuffer(), TRUE);
}

HTML5Tokenizer::HTML5Tokenizer(HTML5Parser* parser, BOOL recognize_processing_instruction /* = FALSE */) :
	m_parser(parser),
	m_current_buffer(NULL),
	m_position(NULL),
	m_remaining_data_length(0),
	m_insertion_point_stack(NULL),
#ifdef DELAYED_SCRIPT_EXECUTION
	m_restore_skip(0),
#endif // DELAYED_SCRIPT_EXECUTION
	m_include_previous_char_in_line_pos(FALSE),
	m_state(DATA),
	m_switched_buffers(FALSE),
	m_token_emitted(FALSE),
	m_is_finished(FALSE),
	m_is_looking_ahead(FALSE),
	m_appropriate_end_tag(64),
	m_tokenizer_empty_buffer(FALSE),
	m_recognize_processing_instruction(recognize_processing_instruction),
	m_update_position(TRUE)
{
}

HTML5Tokenizer::~HTML5Tokenizer()
{
	while (m_insertion_point_stack)
	{
		InsertionPoint* ip = m_insertion_point_stack;
		m_insertion_point_stack = ip->m_next;
		OP_DELETE(ip);
	}
}

uni_char HTML5Tokenizer::GetNextInputCharacterSlowL()
{
	while (m_remaining_data_length < 1)
	{
		m_switched_buffers = TRUE;
		if (m_current_buffer->MustSignalEndOfBuffer())
		{
			m_current_buffer->SetMustSignalEndOfBuffer(FALSE);
			if (HTML5Buffer *buffer = m_current_buffer->Suc())
			{
				m_current_buffer = buffer;
				m_position = m_current_buffer->GetDataAtCurrentPosition(m_remaining_data_length);
			}

			LEAVE(HTML5ParserStatus::FINISHED_DOC_WRITE);
		}
		else
		{
			// If we have a next buffer, then switch to that
			if (HTML5Buffer* buffer = m_current_buffer->Suc())
			{
				m_current_buffer = buffer;
				m_position = m_current_buffer->GetDataAtCurrentPosition(m_remaining_data_length);
			}
			else
				LEAVE(HTML5ParserStatus::NEED_MORE_DATA);
		}
	}

	return *m_position;
}

BOOL HTML5Tokenizer::GetNextNonWhiteSpaceCharacterL(BOOL report_error, uni_char &next_char)
{
	do
	{
		if (IsAtEOF())
		{
			if (report_error)
				ReportTokenizerError(HTML5Parser::UNEXPECTED_EOF);
			RESET_TO_DATA_STATE;
			next_char = 0;
			return FALSE;
		}

		next_char = GetNextInputCharacterL();

		if (!HTML5Parser::IsHTML5WhiteSpace(next_char))
			break;

		ConsumeL(1);
	} while (TRUE);

	return TRUE;
}

BOOL HTML5Tokenizer::EmitErrorIfEOFL(HTML5Token& token)
{
	if (IsAtEOF())
	{
		if (!token.HasData())
			token.Reset(HTML5Token::CHARACTER);
		token.ConcatenateToDataL(m_html5_temp_buffer);
		m_html5_temp_buffer.Clear();

		if (token.HasData())
			return TRUE;

		ReportTokenizerError(HTML5Parser::UNEXPECTED_EOF);
		RESET_TOKEN_TO_EOF_L;
		RESET_TO_DATA_STATE;

		return TRUE;
	}

	return FALSE;
}

void HTML5Tokenizer::ConsumeL(unsigned number_to_consume)
{
	// Use temporary initially, since we may leave, and don't want
	// to change the member variables until we have consumed all we need
	unsigned remaining_length = m_remaining_data_length;
	const uni_char* position = m_position;
	HTML5Buffer* buffer = m_current_buffer;

	while (remaining_length < number_to_consume)  // this buffer doesn't contain enough
	{
		if (buffer->MustSignalEndOfBuffer())
		{
			buffer->SetMustSignalEndOfBuffer(FALSE);
			LEAVE(HTML5ParserStatus::FINISHED_DOC_WRITE);
		}

		buffer = buffer->Suc();  // do we have more buffers?
		if (buffer)
		{
			// consume all of current buffer, let's see how much remains to consume after that
			number_to_consume -= remaining_length;

			// switch to next buffer
			position = buffer->GetDataAtCurrentPosition(remaining_length);
		}
		else
		{
			LEAVE(HTML5ParserStatus::NEED_MORE_DATA);
		}
	}

	// We're not going to leave anymore, so we can safely change these now
	m_switched_buffers = TRUE;
	m_current_buffer = buffer;
	m_position = position;
	m_remaining_data_length = remaining_length;

	// this buffer contains enough.  consume what remains to be consumed from it
	m_position += number_to_consume;
	m_remaining_data_length -= number_to_consume;
}

BOOL HTML5Tokenizer::IsAtEOF() const
{
	if (m_remaining_data_length == 0)
	{
		HTML5Buffer *iter = m_current_buffer;
		unsigned remaining_length = m_remaining_data_length;
		while (iter && remaining_length == 0)
		{
			if (iter->MustSignalEndOfBuffer())
				break;
			else if (iter->IsLastBuffer())
				return TRUE;

			iter = iter->Suc();
			if (iter)
				iter->GetDataAtCurrentPosition(remaining_length);
		}
	}

	return FALSE;
}

void HTML5Tokenizer::SetIsLookingAheadL(BOOL is_looking_ahead, HTML5Buffer *buffer)
{
	if (is_looking_ahead)
	{
		if (!m_is_looking_ahead)
			PushInsertionPointL(buffer);
	}
	else
	{
		if (m_is_looking_ahead)
			PopInsertionPointL();
	}

	m_is_looking_ahead = is_looking_ahead;
}

BOOL HTML5Tokenizer::LookAheadIsL(const uni_char* input, const char* look_for, BOOL case_insensitive /* = FALSE */)
{
	HTML5Buffer* buffer = m_current_buffer;

	for (int i = 0; look_for[i]; i++, input++)
	{
		if (!buffer->HasPosition(input))
			if (buffer->IsLastBuffer())  // Never going to find it
				return FALSE;
			else
			{
				if (buffer->MustSignalEndOfBuffer())
				{
					buffer->SetMustSignalEndOfBuffer(FALSE);
					SetIsLookingAheadL(TRUE, buffer->Suc());
					LEAVE(HTML5ParserStatus::FINISHED_DOC_WRITE);
				}
				else
				{
					unsigned length = 0;
					while (length == 0)
					{
						if (buffer->Suc())
							buffer = buffer->Suc();
						else if (buffer->IsLastBuffer())
							return FALSE;
						else
							LEAVE(HTML5ParserStatus::NEED_MORE_DATA);

						input = buffer->GetDataAtCurrentPosition(length);
					}
				}
			}

		uni_char c = case_insensitive ? ascii_tolower(*input) : *input;
		if (c != look_for[i])
		{
			SetIsLookingAheadL(FALSE, NULL);
			return FALSE;
		}
	}

	SetIsLookingAheadL(FALSE, NULL);
	return TRUE;
}

void HTML5Tokenizer::HandleTokenizerErrorInDoctype(HTML5Token& token, HTML5Parser::ErrorCode error)
{
	ReportTokenizerError(error);
	token.SetForceQuirks(TRUE);
	m_state = DATA;
}

void HTML5Tokenizer::AddDataL(const uni_char* data, unsigned int length, BOOL is_last_buffer, BOOL is_placeholder_buffer, BOOL is_fragment)
{
	BOOL is_first_buffer = !is_fragment && !m_data_buffers.First();
	if (is_first_buffer)  // first data
	{
		// ignore BOM if first character
		if (length > 0 && CharsetDetector::StartsWithUTF16BOM(data))
		if (length > 0 && *data == 0xFEFF)
		{
			data++;
			length--;
		}
	}

	OpStackAutoPtr<HTML5Buffer> buffer(OP_NEW_L(HTML5Buffer, (this, FALSE, is_placeholder_buffer)));
	buffer->Into(&m_data_buffers);

	if (is_first_buffer)
		buffer->SetLineAndCharacterPosition(m_parser ? m_parser->GetFirstLineNumber() : 1, 0);

#ifdef DELAYED_SCRIPT_EXECUTION
	buffer->AddDataL(data, length, is_last_buffer, FALSE, m_restore_skip, is_fragment);

	if (m_restore_skip && length)
	{
		// Tokenizer restored to a previous position, but too far back
		// We need to skip past content we shouldn't have restored past
		if (m_restore_skip <= buffer->GetBufferLength())
		{
			buffer->IncreaseCurrentPosition(m_restore_skip, FALSE);
			m_restore_skip = 0;
		}
		else
		{
			buffer->IncreaseCurrentPosition(buffer->GetBufferLength(), FALSE);
			m_restore_skip -= buffer->GetBufferLength();
		}
	}
#else
	buffer->AddDataL(data, length, is_last_buffer, FALSE, 0, is_fragment);
#endif  // DELAYED_SCRIPT_EXECUTION

	if (m_remaining_data_length == 0 && m_current_buffer == buffer->Pred())
	{
		if (m_current_buffer && m_current_buffer->IsPlaceHolderBuffer())
		{
			// If we have any insertions points for the placeholder buffer, they
			// are really intended to point to this buffer, so move them
			for (InsertionPoint *ip = m_insertion_point_stack; ip; ip = ip->m_next)
				 if (ip->m_buffer == m_current_buffer)
					 ip->m_buffer = buffer.get();
		}

		m_switched_buffers = TRUE;
		m_current_buffer = buffer.get();
		m_position = m_current_buffer->GetDataAtCurrentPosition(m_remaining_data_length);
	}

	buffer.release();
}

void HTML5Tokenizer::InsertDataL(const uni_char* data, unsigned int length, BOOL add_newline)
{
	OpStackAutoPtr<HTML5Buffer> buffer(OP_NEW_L(HTML5Buffer, (this, TRUE, FALSE)));
	if (m_insertion_point_stack)
		buffer->Precede(m_insertion_point_stack->m_buffer);
	else
		buffer->Precede(m_current_buffer);

	buffer->SetMustSignalEndOfBuffer(!m_parser->IsPausing());
	buffer->AddDataL(data, length, FALSE, add_newline, 0, TRUE);

	if (!m_parser->IsBlocked() && !m_is_looking_ahead)
	{
		m_switched_buffers = TRUE;
		m_current_buffer = buffer.get();
		m_position = m_current_buffer->GetDataAtCurrentPosition(m_remaining_data_length);
	}

	buffer.release();
}

void HTML5Tokenizer::InsertPlaintextPreambleL()
{
	OP_ASSERT(m_current_buffer && !m_current_buffer->Pred());
	OpStackAutoPtr<HTML5Buffer> buffer(OP_NEW_L(HTML5Buffer, (this, TRUE, FALSE)));
	buffer->Precede(m_current_buffer);

	buffer->AddDataL(UNI_L("<pre>\n"), 6, FALSE, FALSE, 0, FALSE);

	m_current_buffer = buffer.get();
	m_position = m_current_buffer->GetDataAtCurrentPosition(m_remaining_data_length);

	buffer.release();
}

void HTML5Tokenizer::PushInsertionPointL(HTML5Buffer *buffer)
{
	InsertionPoint *ip = OP_NEW_L(InsertionPoint, ());
	ip->Set(buffer ? buffer : m_current_buffer);
	ip->m_next = m_insertion_point_stack;
	m_insertion_point_stack = ip;
}

void HTML5Tokenizer::PopInsertionPointL()
{
	OP_ASSERT(m_insertion_point_stack);
	InsertionPoint *ip = m_insertion_point_stack;
	m_insertion_point_stack = ip->m_next;
	OP_DELETE(ip);
}

BOOL HTML5Tokenizer::ResetEOBSignallingForNextBuffer()
{
	HTML5Buffer *iter = m_current_buffer;
	while (iter && !iter->MustSignalEndOfBuffer())
		iter = iter->Suc();

	if (iter)
	{
		iter->SetMustSignalEndOfBuffer(FALSE);
		return TRUE;
	}
	return FALSE;
}

#if defined DELAYED_SCRIPT_EXECUTION || defined SPECULATIVE_PARSER
OP_STATUS HTML5Tokenizer::StoreTokenizerState(HTML5ParserState* state)
{
	state->tokenizer_state = GetState();

	// doc.write buffers shouldn't store state, so find first non-doc.write buffer
	HTML5Buffer* buffer = m_current_buffer;
	for (; buffer && buffer->IsFromDocWrite(); buffer = buffer->Suc())
		/* empty body */;

	if (buffer)
	{
		state->stream_offset = buffer->GetPositionOffset();
		buffer->GetLineAndCharacterPosition(NULL, state->line_number, state->character_position);
	}
	else
	{
		state->stream_offset = state->line_number = state->character_position = 0;
	}

	return OpStatus::OK;
}

OP_STATUS HTML5Tokenizer::RestoreTokenizerState(HTML5ParserState* state, unsigned buffer_stream_position)
{
	SetState(static_cast<HTML5Tokenizer::State>(state->tokenizer_state));
	OP_ASSERT(m_current_buffer);  // this is created in the parser's RestoreState (with just "" as content)
	if (m_current_buffer)
	{
		OP_ASSERT(!m_current_buffer->IsFromDocWrite());  // should be buffer from original source we're changing
		m_current_buffer->SetLineAndCharacterPosition(state->line_number, state->character_position);
		m_current_buffer->SetBufferEndLineAndCharacterPosition(state->line_number, state->character_position);
		m_current_buffer->SetStreamPosition(buffer_stream_position);
	}

	m_restore_skip = state->stream_offset;
	m_token_emitted = TRUE;
	m_switched_buffers = TRUE;

	return OpStatus::OK;
}
#endif // defined DELAYED_SCRIPT_EXECUTION || defined SPECULATIVE_PARSER

#ifdef DELAYED_SCRIPT_EXECUTION
BOOL HTML5Tokenizer::HasTokenizerStateChanged(HTML5ParserState* state)
{
	return GetState() != static_cast<HTML5Tokenizer::State>(state->tokenizer_state);
}

BOOL HTML5Tokenizer::HasUnfinishedWriteData()
{
	HTML5Buffer *buf_iter = m_current_buffer;
	while (buf_iter)
	{
		if (buf_iter->IsFromDocWrite())
		{
			unsigned remaining_length;
			buf_iter->GetDataAtCurrentPosition(remaining_length);
			if (remaining_length > 0 || buf_iter->MustSignalEndOfBuffer())
				return TRUE;
		}

		buf_iter = buf_iter->GetNextBuffer();
	}
	return FALSE;
}
#endif // DELAYED_SCRIPT_EXECUTION

#if defined HTML5_STANDALONE || OUTPUT_TOKENS
#ifndef  HTML5_STANDALONE
# include "modules/encodings/encoders/utf8-encoder.h"
#endif
static void OutputStringEscaped(const uni_char* string, unsigned length = UINT_MAX)
{
	unsigned char buffer[2048]; /* ARRAY OK 2011-09-08 danielsp */

	if (!string)
		return;

	// Convert it to UTF-8 first
#ifdef HTML5_STANDALONE
	Ragnarok_UTF16toUTF8Converter conv;
#else
	UTF16toUTF8Converter conv;
#endif
	int read, converted_len;
	if (length == UINT_MAX)
		length = uni_strlen(string);
	converted_len = conv.Convert(string, length * sizeof(uni_char), buffer, 2048, &read);
	OP_ASSERT(read == (int)(length * sizeof(uni_char)));  // If this hits a too long string has been sent in

	for (const unsigned char* output = buffer; converted_len && *output; output++, converted_len--)
	{
		if (*output == '\n')
		{
			printf("\\n");
			continue;
		}
		else if (*output == '"' || *output == '\\')
			printf("\\");
		printf("%c", *output);
	}
}

static void OutputStringBuffer(const HTML5StringBuffer& buffer)
{
	HTML5StringBuffer::ContentIterator iter(buffer);
	unsigned length;
	while (const uni_char* value = iter.GetNext(length))
		OutputStringEscaped(value, length);
}

static void OutputToken(HTML5Token& token)
{
	switch (token.GetType())
	{
		case HTML5Token::DOCTYPE:
		{
			printf("[\"DOCTYPE\", \"");
			OutputStringEscaped(token.GetNameStr());
			printf("\", ");

			const uni_char* pubid = token.GetPublicIdentifier();
			if (token.HasPublicIdentifier())
			{
				printf("\"");
				OutputStringEscaped(pubid);
				printf("\", ");
			}
			else
				printf("null, ");

			const uni_char* sysid = token.GetSystemIdentifier();
			if (token.HasSystemIdentifier())
			{
				printf("\"");
				OutputStringEscaped(sysid);
				printf("\", ");
			}
			else
				printf("null, ");

			printf("%s]\n", token.DoesForceQuirks() ? "false" : "true");

			break;
		}
		case HTML5Token::START_TAG:
		{
			printf("[\"StartTag\", \"");
			OutputStringEscaped(token.GetNameStr());
			printf("\", {");

			for (unsigned i = 0; i < token.GetAttrCount(); i++)
			{
				HTML5Attr* attr = token.GetAttribute(i);
				printf("\"");
				OutputStringEscaped(attr->GetNameStr());
				printf("\" : \"");
				OutputStringBuffer(attr->GetValue());
				printf("\"");

				if (i != token.GetAttrCount() - 1)
					printf(", ");
			}

			printf("}%s]\n", token.IsSelfClosing() ? ", true" : "");

			break;
		}
		case HTML5Token::END_TAG:
		{
			printf("[\"EndTag\", \"");
			OutputStringEscaped(token.GetNameStr());
			printf("\"]\n");

			break;
		}
		case HTML5Token::COMMENT:
		{
			printf("[\"Comment\", \"");
			OutputStringBuffer(token.GetData());
			printf("\"]\n");

			break;
		}
		case HTML5Token::CHARACTER:
		{
			printf("[\"Character\", \"");
			OutputStringBuffer(token.GetData());
			printf("\"]\n");

			break;
		}
		case HTML5Token::ENDOFFILE:
			break;
		default:
		{
			printf("\"ParseError\"\n");

			break;
		}
	}
}
#endif // HTML5_STANDALONE

void HTML5Tokenizer::ReportTokenizerError(HTML5Parser::ErrorCode error, int offset, unsigned line, unsigned chr)
{
#ifdef HTML5_STANDALONE
	if (m_parser->GetOutputTokenizerResults())
	{
		HTML5Token parse_error_token;
		parse_error_token.Reset(HTML5Token::PARSE_ERROR);
		OutputToken(parse_error_token);
	}
#endif // HTML5_STANDALONE

	if (m_parser && m_parser->ReportErrors())
	{
		if (line == 0 && m_update_position)
			m_current_buffer->GetLineAndCharacterPosition(m_position, line, chr);

		m_parser->SignalErrorL(error, line, chr + offset);
	}
}

#ifdef _DEBUG
void HTML5Tokenizer::CheckIPReference(HTML5Buffer *buffer)
{
	InsertionPoint *ip = m_insertion_point_stack;
	while (ip && ip->m_buffer != buffer)
		ip = ip->m_next;

	OP_ASSERT(!ip);
}
#endif // _DEBUG

void HTML5Tokenizer::CleanUpBetweenTokensL()
{
	// delete all but last data buffer, or up to the first referenced one,
	// we don't need the previous ones anymore.
	const HTML5Buffer* first_still_referenced = m_html5_temp_buffer.GetEarliestBuffer();
	HTML5Buffer* buf = m_data_buffers.First();
	while (buf && buf != m_current_buffer)
	{
		if (first_still_referenced == buf)
			break;

		if (buf->MustSignalEndOfBuffer())
		{
			buf->SetMustSignalEndOfBuffer(FALSE);
			LEAVE(HTML5ParserStatus::FINISHED_DOC_WRITE);
		}
#ifdef _DEBUG
		CheckIPReference(buf);
#endif // _DEBUG
		OP_DELETE(buf); // The destructor will do Out()
		buf = m_data_buffers.First();
	}
}

void HTML5Tokenizer::Reset()
{
	m_data_buffers.Clear();
	m_current_buffer = NULL;
	m_position = NULL;
	m_remaining_data_length = 0;

	while (m_insertion_point_stack)
	{
		InsertionPoint* ip = m_insertion_point_stack;
		m_insertion_point_stack = ip->m_next;
		OP_DELETE(ip);
	}

	m_include_previous_char_in_line_pos = FALSE;
	m_state = DATA;
	m_switched_buffers = FALSE;
	m_token_emitted = FALSE;
	m_is_finished = FALSE;
	m_is_looking_ahead = FALSE;
	m_appropriate_end_tag.Clear();
	m_tokenizer_empty_buffer = FALSE;
	m_update_position = TRUE;
	m_html5_temp_buffer.Clear();
}

void HTML5Tokenizer::GetNextTokenL(HTML5Token& token)
{
	OP_PROBE7_L(OP_PROBE_HTML5TOKENIZER_GETNEXTTOKENL);

	if (!m_current_buffer)
	{
		if (m_tokenizer_empty_buffer)  // Just return an EOF token immediately
		{
			RESET_TOKEN_TO_EOF_L;
			return;
		}
		else
		{
			OP_ASSERT(FALSE);  // No data to parse has been added!
			LEAVE(HTML5ParserStatus::NEED_MORE_DATA);
		}
	}

	// If we returned a token the last time we were called, then we're returning a
	// new token now (and not continuing the previous), so we can clean up the
	// buffers the previous token used
	if (m_token_emitted)
	{
		if (m_switched_buffers && !m_current_buffer->IsFromDocWrite())
		{
			CleanUpBetweenTokensL();
			m_switched_buffers = FALSE;
		}
		token.Reset(HTML5Token::INVALID);

		if (m_update_position)
		{
			// Remember where we started parsing next token
			unsigned line, chr;
			m_current_buffer->GetLineAndCharacterPosition(NULL, line, chr);

			if (m_include_previous_char_in_line_pos)
				chr--;

			token.SetLineNum(line);
			token.SetLinePos(chr);
		}
		m_token_emitted = FALSE;
	}

	m_position = m_current_buffer->GetDataAtCurrentPosition(m_remaining_data_length);

	BOOL has_found_token = FALSE;
	State previous_state = m_state;
	while (!has_found_token)
	{
		const uni_char* previous_position = m_position;

		switch (m_state)
		{
		case DATA:
		case TAG_OPEN:
		case END_TAG_OPEN:
		case TAG_NAME:
			has_found_token = TokenizeMostCommonL(token);
			break;
		case CHARACTER_REFERENCE:
			has_found_token = TokenizeSpecialCharacterReferenceL(token);
			break;
		case RCDATA:
		case CHARACTER_REFERENCE_IN_RCDATA:
		case RAWTEXT:
		case SCRIPT_DATA:
		case SCRIPT_DATA_ESCAPE_START:
		case SCRIPT_DATA_ESCAPE_START_DASH:
		case SCRIPT_DATA_ESCAPED:
		case SCRIPT_DATA_ESCAPED_DASH:
		case SCRIPT_DATA_ESCAPED_DASH_DASH:
		case SCRIPT_DATA_DOUBLE_ESCAPED:
		case SCRIPT_DATA_DOUBLE_ESCAPED_DASH:
		case SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH:
			has_found_token = TokenizeSpecialDataL(token, token.GetData().GetContentLength() + m_html5_temp_buffer.GetContentLength());
			break;
		case RCDATA_LESS_THAN:
		case RCDATA_END_TAG_OPEN:
		case RCDATA_END_TAG_NAME:
		case RAWTEXT_LESS_THAN:
		case RAWTEXT_END_TAG_OPEN:
		case RAWTEXT_END_TAG_NAME:
		case SCRIPT_DATA_LESS_THAN:
		case SCRIPT_DATA_END_TAG_OPEN:
		case SCRIPT_DATA_END_TAG_NAME:
		case SCRIPT_DATA_ESCAPED_LESS_THAN:
		case SCRIPT_DATA_ESCAPED_END_TAG_OPEN:
		case SCRIPT_DATA_ESCAPED_END_TAG_NAME:
		case SCRIPT_DATA_DOUBLE_ESCAPE_START:
		case SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN:
		case SCRIPT_DATA_DOUBLE_ESCAPE_END:
			has_found_token = TokenizeSpecialDataL(token, token.GetData().GetContentLength() + m_html5_temp_buffer.GetContentLength());
			break;
		case BEFORE_ATTRIBUTE_NAME:
		case ATTRIBUTE_NAME:
		case AFTER_ATTRIBUTE_NAME:
		case BEFORE_ATTRIBUTE_VALUE:
			has_found_token = TokenizeAttributesL(token, 0);
			break;
		case ATTRIBUTE_VALUE_DOUBLE_QUOTED:
			has_found_token = TokenizeAttributesL(token, '"');
			break;
		case ATTRIBUTE_VALUE_SINGLE_QUOTED:
			has_found_token = TokenizeAttributesL(token, '\'');
			break;
		case ATTRIBUTE_VALUE_UNQUOTED:
		case AFTER_ATTRIBUTE_VALUE_QUOTED:
			has_found_token = TokenizeAttributesL(token, 0);
			break;
		case PLAINTEXT:
			has_found_token = TokenizePlaintextL(token);
			break;
		case SELF_CLOSING_START_TAG:
			has_found_token = TokenizeSelfClosingStartTagL(token);
			break;
		case MARKUP_DECLARATION_OPEN:
			has_found_token = TokenizeMarkupDeclarationOpenL(token);
			break;
		case BOGUS_COMMENT:
			has_found_token = TokenizeBogusCommentL(token);
			break;
		case COMMENT_START:
			has_found_token = TokenizeCommentStartL(token);
			break;
		case COMMENT_START_DASH:
			has_found_token = TokenizeCommentStartDashL(token);
			break;
		case COMMENT:
			has_found_token = TokenizeCommentL(token);
			break;
		case COMMENT_END_DASH:
			has_found_token = TokenizeCommentEndDashL(token);
			break;
		case COMMENT_END:
			has_found_token = TokenizeCommentEndL(token);
			break;
		case COMMENT_END_BANG:
			has_found_token = TokenizeCommentEndBangL(token);
			break;
		case DOCTYPE:
			has_found_token = TokenizeDoctypeL(token);
			break;
		case BEFORE_DOCTYPE_NAME:
			has_found_token = TokenizeBeforeDoctypeNameL(token);
			break;
		case DOCTYPE_NAME:
			has_found_token = TokenizeDoctypeNameL(token);
			break;
		case AFTER_DOCTYPE_NAME:
			has_found_token = TokenizeAfterDoctypeNameL(token);
			break;
		case AFTER_DOCTYPE_PUBLIC_KEYWORD:
			has_found_token = TokenizeAfterDoctypePublicKeywordL(token);
			break;
		case BEFORE_DOCTYPE_PUBLIC_IDENTIFIER:
			has_found_token = TokenizeBeforeDoctypePublicIdentifierL(token);
			break;
		case DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED:
			has_found_token = TokenizeDoctypePublicIdentifierQuotedL(token, '"');
			break;
		case DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED:
			has_found_token = TokenizeDoctypePublicIdentifierQuotedL(token, '\'');
			break;
		case AFTER_DOCTYPE_PUBLIC_IDENTIFIER:
			has_found_token = TokenizeAfterDoctypePublicIdentifierL(token);
			break;
		case BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
			has_found_token = TokenizeBetweenDoctypePublicAndSystemIdentifiersL(token);
			break;
		case AFTER_DOCTYPE_SYSTEM_KEYWORD:
			has_found_token = TokenizeAfterDoctypeSystemL(token);
			break;
		case BEFORE_DOCTYPE_SYSTEM_IDENTIFIER:
			has_found_token = TokenizeBeforeDoctypeSystemIdentifierL(token);
			break;
		case DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED:
			has_found_token = TokenizeDoctypeSystemIdentifierQuotedL(token, '"');
			break;
		case DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED:
			has_found_token = TokenizeDoctypeSystemIdentifierQuotedL(token, '\'');
			break;
		case AFTER_DOCTYPE_SYSTEM_IDENTIFIER:
			has_found_token = TokenizeAfterDoctypeSystemIdentifierL(token);
			break;
		case BOGUS_DOCTYPE:
			has_found_token = TokenizeBogusDoctypeL(token);
			break;
		case CDATA_SECTION:
			has_found_token = TokenizeCdataSectionL(token);
			break;
		}

		// store new position if we've changed state:
		if (previous_state != m_state)
		{
			m_current_buffer->SetCurrentPosition(m_position, m_update_position);
		}
		// Sanity check, make sure we always make some progress
		else if (!has_found_token && previous_position == m_position)
		{
			OP_ASSERT(!"At this point we have made no progress in the buffer, so we are probably in a infinite loop.");

			if (IsAtEOF())
			{
				if (token.GetType() == HTML5Token::INVALID)
				{
					RESET_TOKEN_TO_EOF_L;
				}
				else
					m_state = DATA;
			}
			else if (m_remaining_data_length < 1)
				LEAVE(HTML5ParserStatus::NEED_MORE_DATA);
			else
				// do something to not end up in infinite loop:
				ConsumeL(1);
		}
		previous_state = m_state;
	}

	switch (previous_state)
	{
	// These states don't know they've started a new token until they've already
	// consumed the first character of the next token
	case DATA:
	case RCDATA:
	case RAWTEXT:
	case SCRIPT_DATA:
		m_include_previous_char_in_line_pos = TRUE;
		break;
	default:
		m_include_previous_char_in_line_pos = FALSE;
		break;
	}

	if (token.GetType() == HTML5Token::START_TAG)
	{
		m_appropriate_end_tag.Clear();
		HTML5TokenBuffer *name = token.GetName();
		m_appropriate_end_tag.AppendL(name->GetBuffer(), name->Length());
		m_appropriate_end_tag.TerminateL();
		m_appropriate_end_tag.SetHashValue(name->GetHashValue());
	}
	else if (token.GetType() == HTML5Token::ENDOFFILE)
		m_is_finished = TRUE;

#ifdef HTML5_STANDALONE
	if (m_parser->GetOutputTokenizerResults())
		OutputToken(token);
#elif OUTPUT_TOKENS
	OutputToken(token);
#endif // HTML5_STANDALONE

	m_token_emitted = TRUE;

	m_current_buffer->SetCurrentPosition(m_position, m_update_position);
}

#define TOKENIZER_FLUSH_DATA if (data_start != m_position) \
	{ \
		if (token.GetType() == HTML5Token::INVALID) \
			token.Reset(HTML5Token::CHARACTER); \
		token.AppendToDataL(m_current_buffer, data_start, m_position - data_start); \
		data_start = m_position; \
	}

BOOL HTML5Tokenizer::TokenizeMostCommonL(HTML5Token& token)
{
	OP_PROBE7_L(OP_PROBE_HTML5TOKENIZER_TOKENIZEDATAL);

	uni_char next_char;
	const uni_char* data_start = m_position;

	while (m_remaining_data_length > 0)
	{
		next_char = GetNextInputCharacterL();

do_it_again:
		switch (m_state)
		{
		case DATA:
			if (next_char == '<')
			{
				m_state = TAG_OPEN;
				m_html5_temp_buffer.ResetL(m_current_buffer, m_position, 1);
				if (data_start != m_position || token.HasData())
				{
					TOKENIZER_FLUSH_DATA;
					ConsumeL();
					return TRUE;
				}
				ConsumeL();
			}
			else if (next_char == '&')
			{
				m_state = CHARACTER_REFERENCE;
				TOKENIZER_FLUSH_DATA;
				return token.HasData();
			}
			else
			{
				if (next_char == 0)
					ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);

				ConsumeL();

				if (m_position - data_start >= static_cast<int>(SplitTextLen))
				{
					TOKENIZER_FLUSH_DATA;
					return TRUE;
				}
			}
			continue;

		case TAG_OPEN:
			if (is_ascii_alpha(next_char))
			{
				m_state = TAG_NAME;
				token.Reset(HTML5Token::START_TAG);
				token.AppendToNameL(ascii_force_lower(next_char));
				ConsumeL();
			}
			else if (next_char == '/')
			{
				m_state = END_TAG_OPEN;
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				ConsumeL();
			}
			else if (next_char == '!')
			{
				m_state = MARKUP_DECLARATION_OPEN;
				ConsumeL();
				return FALSE;
			}
			else if (next_char == '?')
			{
				ReportTokenizerError(HTML5Parser::UNEXPECTED_CHARACTER);

				m_html5_temp_buffer.Clear();
				if (m_recognize_processing_instruction)
				{
					/* Processing instructions are parsed as bogus comments
					 * according to the HTML 5 spec.  But for some purposes
					 * we do want to know their contents (e.g. saving with
					 * files/as archive use it to find XML stylesheets), so
					 * this optional flag will tokenize them as start tags and
					 * with a type of PROCESSING_INSTRUCTION.
					 */
					token.Reset(HTML5Token::PROCESSING_INSTRUCTION);
					m_state = TAG_NAME;
					ConsumeL();
				}
				else  // this is how proc inst should be tokenized according to spec
				{
					m_state = BOGUS_COMMENT;
					return FALSE;
				}
			}
			else
			{
				ReportTokenizerError(HTML5Parser::EXPECTED_TAG_NAME);
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				m_state = DATA;
			}
			continue;

		case TAG_NAME:
			if (is_ascii_alpha(next_char))
				token.AppendToNameL(ascii_force_lower(next_char));
			else if (next_char == '>')
			{
				token.TerminateNameL();
				CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
			}
			else if (HTML5Parser::IsHTML5WhiteSpace(next_char))
			{
				token.TerminateNameL();
				CONSUME_AND_CONTINUE_IN_STATE(BEFORE_ATTRIBUTE_NAME);
			}
			else if (next_char == '/')
			{
				token.TerminateNameL();
				CONSUME_AND_CONTINUE_IN_STATE(SELF_CLOSING_START_TAG);
			}
			else if (next_char == 0)
				token.AppendToNameL(UNICODE_REPLACEMENT_CHARACTER);
			else
				token.AppendToNameL(next_char);

			ConsumeL();
			continue;

		case END_TAG_OPEN:
			if (is_ascii_alpha(next_char))
			{
				m_state = TAG_NAME;
				token.Reset(HTML5Token::END_TAG);
				token.AppendToNameL(ascii_force_lower(next_char));
				ConsumeL();
			}
			else if (next_char == '>')
			{
				ReportTokenizerError(HTML5Parser::EXPECTED_TAG_NAME);
				m_state = DATA;
				ConsumeL();
				data_start = m_position;
			}
			else
			{
				ReportTokenizerError(HTML5Parser::EXPECTED_TAG_NAME);

				m_html5_temp_buffer.Clear();
				m_state = BOGUS_COMMENT;
				return FALSE;
			}
			continue;
		}
	}

	// This means remaining length is 0
	switch (m_state)
	{
	case DATA:
		if (data_start != m_position)
		{
			if (token.GetType() == HTML5Token::INVALID)
				token.Reset(HTML5Token::CHARACTER);
			token.AppendToDataL(m_current_buffer, data_start, m_position - data_start);
		}

		if (token.HasData())
			return TRUE;

		if (m_current_buffer->IsLastBuffer())
		{
			RESET_TOKEN_TO_EOF_L;
			return TRUE;
		}

		next_char = GetNextInputCharacterL();
		data_start = m_position;
		goto do_it_again;
	case TAG_OPEN:
	case END_TAG_OPEN:
		if (m_current_buffer->IsLastBuffer())
		{
			// the < (and / for endtag)
			CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
			m_state = DATA;
			return TRUE;
		}
		m_current_buffer->SetCurrentPosition(m_position, m_update_position);
		next_char = GetNextInputCharacterL();
		goto do_it_again;
	case TAG_NAME:
		REPORT_ERROR_AND_CONTINUE_IN_DATA_STATE_IF_EOF(FALSE);
		m_current_buffer->SetCurrentPosition(m_position, m_update_position);
		next_char = GetNextInputCharacterL();
		goto do_it_again;
	}

	return TRUE;
}

#define TOKENIZER_FLUSH_ATTR_DATA if (data_start) \
	{ \
		token.AppendToLastAttributeValueL(m_current_buffer, data_start, m_position - data_start); \
		data_start = NULL; \
	}

BOOL HTML5Tokenizer::TokenizeAttributesL(HTML5Token& token, uni_char quote)
{
	uni_char next_char;
	const uni_char *data_start = NULL;

	while (m_remaining_data_length > 0)
	{
		next_char = GetNextInputCharacterL();

attr_again:
		switch (m_state)
		{
		case BEFORE_ATTRIBUTE_NAME:
			if (is_ascii_alpha(next_char))
			{
				if (token.GetType() == HTML5Token::END_TAG)
					ReportTokenizerError(HTML5Parser::ATTRIBUTE_IN_END_TAG);

				token.GetNewAttributeL()->AppendToNameL(ascii_force_lower(next_char));
				m_state = ATTRIBUTE_NAME;
			}
			else if (next_char == '/')
				CONSUME_AND_CONTINUE_IN_STATE(SELF_CLOSING_START_TAG);
			else if (next_char == '>')
				CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
			else if (next_char == 0)
			{
				ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
				token.GetNewAttributeL()->AppendToNameL(UNICODE_REPLACEMENT_CHARACTER);
				m_state = ATTRIBUTE_NAME;
			}
			else if (!HTML5Parser::IsHTML5WhiteSpace(next_char))
			{
				if (next_char == '"' || next_char == '\'' || next_char == '<' || next_char == '=')
					ReportTokenizerError(HTML5Parser::UNEXPECTED_CHARACTER);

				if (token.GetType() == HTML5Token::END_TAG)
					ReportTokenizerError(HTML5Parser::ATTRIBUTE_IN_END_TAG);

				if (!(next_char == '?' && m_recognize_processing_instruction && token.GetType() == HTML5Token::PROCESSING_INSTRUCTION))
				{
					token.GetNewAttributeL()->AppendToNameL(next_char);
					m_state = ATTRIBUTE_NAME;
				}
			}

			ConsumeL();
			continue;

		case ATTRIBUTE_NAME:
			if (is_ascii_alpha(next_char))
			{
				token.AppendToLastAttributeNameL(ascii_force_lower(next_char));
				ConsumeL();
				continue;
			}
			else if (next_char == '=')
				m_state = BEFORE_ATTRIBUTE_VALUE;
			else if (HTML5Parser::IsHTML5WhiteSpace(next_char))
				m_state = AFTER_ATTRIBUTE_NAME;
			else if (next_char == '>')
				m_state = DATA;
			else if (next_char == '/')
				m_state = SELF_CLOSING_START_TAG;
			else if (next_char == 0)
			{
				ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
				token.AppendToLastAttributeNameL(UNICODE_REPLACEMENT_CHARACTER);
				ConsumeL();
				continue;
			}
			else
			{
				if (next_char == '"' || next_char == '\'' || next_char == '<')
					ReportTokenizerError(HTML5Parser::UNEXPECTED_CHARACTER);

				token.AppendToLastAttributeNameL(next_char);
				ConsumeL();
				continue;
			}

			token.TerminateLastAttributeNameL();
			ConsumeL();

			// Check if we've already got this attribute
			if (token.GetType() == HTML5Token::START_TAG && token.CheckIfLastAttributeIsDuplicate())
				ReportTokenizerError(HTML5Parser::DUPLICATE_ATTRIBUTE);

			if (m_state == DATA || m_state == SELF_CLOSING_START_TAG)
				return m_state == DATA;
			continue;

		case AFTER_ATTRIBUTE_NAME:
			if (is_ascii_alpha(next_char))
			{
				if (token.GetType() == HTML5Token::END_TAG)
					ReportTokenizerError(HTML5Parser::ATTRIBUTE_IN_END_TAG, 0);

				token.GetNewAttributeL()->AppendToNameL(ascii_force_lower(next_char));
				m_state = ATTRIBUTE_NAME;
			}
			else if (next_char == '=')
				m_state = BEFORE_ATTRIBUTE_VALUE;
			else if (next_char == '>')
				CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
			else if (next_char == '/')
				CONSUME_AND_CONTINUE_IN_STATE(SELF_CLOSING_START_TAG);
			else if (next_char == 0)
			{
				ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
				token.GetNewAttributeL()->AppendToNameL(UNICODE_REPLACEMENT_CHARACTER);
				m_state = ATTRIBUTE_NAME;
			}
			else if (!HTML5Parser::IsHTML5WhiteSpace(next_char))
			{
				if (next_char == '"' || next_char == '\'' || next_char == '<')
					ReportTokenizerError(HTML5Parser::UNEXPECTED_CHARACTER);

				if (token.GetType() == HTML5Token::END_TAG)
					ReportTokenizerError(HTML5Parser::ATTRIBUTE_IN_END_TAG, 0);

				token.GetNewAttributeL()->AppendToNameL(next_char);
				m_state = ATTRIBUTE_NAME;
			}
			ConsumeL();
			continue;

		case BEFORE_ATTRIBUTE_VALUE:
			if (is_ascii_alpha(next_char))
			{
				token.AppendToLastAttributeValueL(m_current_buffer, m_position);
				m_state = ATTRIBUTE_VALUE_UNQUOTED;
			}
			else if (next_char == '"')
			{
				quote = next_char;
				m_state = ATTRIBUTE_VALUE_DOUBLE_QUOTED;
			}
			else if (next_char == '\'')
			{
				quote = next_char;
				m_state = ATTRIBUTE_VALUE_SINGLE_QUOTED;
			}
			else if (next_char == '&')
			{
				// don't consume
				m_state = ATTRIBUTE_VALUE_UNQUOTED;
				continue;
			}
			else if (next_char == '>')
			{
				ReportTokenizerError(HTML5Parser::EXPECTED_ATTRIBUTE_VALUE);
				CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
			}
			else if (!HTML5Parser::IsHTML5WhiteSpace(next_char))
			{
				if (next_char == '<' || next_char == '=')
					ReportTokenizerError(HTML5Parser::UNEXPECTED_CHARACTER);
				else if (next_char == '`')
					ReportTokenizerError(HTML5Parser::GOT_GRAVE_ACCENT_WHERE_APOSTROPHE_EXPECTED);
				else if (next_char == 0)
				{
					ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
					m_current_buffer->ReplaceNullAt(m_position);
				}

				token.AppendToLastAttributeValueL(m_current_buffer, m_position);
				m_state = ATTRIBUTE_VALUE_UNQUOTED;
			}

			ConsumeL();
			continue;

		case ATTRIBUTE_VALUE_DOUBLE_QUOTED:
		case ATTRIBUTE_VALUE_SINGLE_QUOTED:
			if (next_char == quote)
			{
				TOKENIZER_FLUSH_ATTR_DATA;
				m_state = AFTER_ATTRIBUTE_VALUE_QUOTED;
			}
			else
			{
				if (next_char == '&')
				{
					TOKENIZER_FLUSH_ATTR_DATA;
					m_current_buffer->SetCurrentPosition(m_position, m_update_position);
					ReplaceCharacterEntityL(quote, TRUE, TRUE);
				}
				else if (next_char == 0)
				{
					ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
					m_current_buffer->ReplaceNullAt(m_position);
				}

				if (!data_start)
					data_start = m_position;
			}

			ConsumeL();
			continue;

		case ATTRIBUTE_VALUE_UNQUOTED:
			if (is_ascii_alpha(next_char))
			{
				if (!data_start)
					data_start = m_position;
			}
			else if (HTML5Parser::IsHTML5WhiteSpace(next_char))
			{
				TOKENIZER_FLUSH_ATTR_DATA;
				m_state = BEFORE_ATTRIBUTE_NAME;
			}
			else if (next_char == '>')
			{
				TOKENIZER_FLUSH_ATTR_DATA;
				m_state = DATA;
				ConsumeL();
				return TRUE;
			}
			else
			{
				if (next_char == '&')
				{
					TOKENIZER_FLUSH_ATTR_DATA;
					m_current_buffer->SetCurrentPosition(m_position, m_update_position);
					ReplaceCharacterEntityL('>', TRUE, TRUE);
				}
				else if (next_char == 0)
				{
					ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
					m_current_buffer->ReplaceNullAt(m_position);
				}
				else if (next_char == '"' || next_char == '\'' || next_char == '<' || next_char == '=' || next_char == '`')
					ReportTokenizerError(HTML5Parser::UNEXPECTED_CHARACTER);

				if (!data_start)
					data_start = m_position;
			}

			ConsumeL();
			continue;

		case AFTER_ATTRIBUTE_VALUE_QUOTED:
			if (HTML5Parser::IsHTML5WhiteSpace(next_char))
			{
				m_state = BEFORE_ATTRIBUTE_NAME;
				ConsumeL();
			}
			else if (next_char == '/')
			{
				m_state = SELF_CLOSING_START_TAG;
				ConsumeL();
				return FALSE;
			}
			else if (next_char == '>')
			{
				m_state = DATA;
				ConsumeL();
				return TRUE;
			}
			else
			{
				ReportTokenizerError(HTML5Parser::UNEXPECTED_CHARACTER);
				m_state = BEFORE_ATTRIBUTE_NAME;
				// don't consume
			}
			continue;
		}
	}

	// here remaining length is 0
	TOKENIZER_FLUSH_ATTR_DATA;

	if (m_current_buffer->IsLastBuffer())
	{
		ReportTokenizerError(HTML5Parser::UNEXPECTED_EOF);
		m_state = DATA;
		return FALSE;
	}

	m_current_buffer->SetCurrentPosition(m_position, m_update_position);
	next_char = GetNextInputCharacterL();

	goto attr_again;
}

BOOL HTML5Tokenizer::TokenizeSpecialCharacterReferenceL(HTML5Token& token)
{
	unsigned replaced = ReplaceCharacterEntityL(0, FALSE, m_state == CHARACTER_REFERENCE_IN_RCDATA);
	if (replaced == 0)
		replaced = 1; // to add the & as character data if we didn't find a replacement

	if (token.GetType() == HTML5Token::INVALID)
		token.Reset(HTML5Token::CHARACTER);

	for (unsigned i = 0; i < replaced; i++)
	{
		GetNextInputCharacterL(); // to advance in the stream if the replacement is over multiple buffers
		token.AppendToDataL(m_current_buffer, m_position);
		if (!IsAtEOF())
			ConsumeL();
	}

	if (m_state == CHARACTER_REFERENCE_IN_RCDATA)
		m_state = RCDATA;
	else
		m_state = DATA;

	return FALSE;
}

#define RECONSUME_IN(state) \
	m_state = state; \
	goto do_more_special_data

BOOL HTML5Tokenizer::TokenizeSpecialDataL(HTML5Token& token, unsigned data_length)
{
	uni_char next_char;
	const uni_char* data_start = m_position;

	while (m_remaining_data_length > 0 && data_length < SplitTextLen)
	{
		next_char = GetNextInputCharacterL();
		data_length++;

do_more_special_data:
		switch (m_state)
		{
		case RCDATA:
			if (next_char == '<')
			{
				TOKENIZER_FLUSH_DATA;
				// According to the algorithm in the spec we clear the temp buffer in
				// RCDATA/RAWTEXT less than state.  We however do it here and include the
				// '<' (and possibly later '/') in the temp buffer to avoid having
				// to delete the data again if it turns out to be an end tag.
				m_html5_temp_buffer.ResetL(m_current_buffer, m_position, 1);
				m_state = RCDATA_LESS_THAN;
			}
			else if (next_char == '&')
			{
				TOKENIZER_FLUSH_DATA;
				RECONSUME_IN(CHARACTER_REFERENCE_IN_RCDATA);
			}
			else if (next_char == 0)
			{
				ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
				m_current_buffer->ReplaceNullAt(m_position);
			}
			ConsumeL();
			continue;

		case RCDATA_LESS_THAN:
			if (next_char == '/')
			{
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				m_state = RCDATA_END_TAG_OPEN;
				ConsumeL();
			}
			else
			{
				// this wasn't an end tag, so reconsume next_char in RCDATA
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(RCDATA);
			}
			continue;

		case RCDATA_END_TAG_OPEN:
			if (is_ascii_alpha(next_char))
			{
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				m_state = RCDATA_END_TAG_NAME;
				ConsumeL();
			}
			else
			{
				// this wasn't an end tag, so reconsume next_char in RCDATA
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(RCDATA);
			}
			continue;

		case RCDATA_END_TAG_NAME:
		case RAWTEXT_END_TAG_NAME:
		case SCRIPT_DATA_END_TAG_NAME:
		case SCRIPT_DATA_ESCAPED_END_TAG_NAME:
			{
				State next_state = BOGUS_COMMENT; // random state that we won't switch to
				if (is_ascii_alpha(next_char))
				{
					m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
					ConsumeL();
					continue;
				}
				else if (next_char == '>')
					next_state = DATA;
				else if (HTML5Parser::IsHTML5WhiteSpace(next_char))
					next_state = BEFORE_ATTRIBUTE_NAME;
				else if (next_char == '/')
					next_state = SELF_CLOSING_START_TAG;

				if (next_state != BOGUS_COMMENT && (!m_html5_temp_buffer.HasData() || IsAppropriateEndTag()))
				{
					if (token.HasData())
					{
						m_html5_temp_buffer.Clear();
						return TRUE;
					}

					ConsumeL();
					m_state = next_state;

					token.Reset(HTML5Token::END_TAG);
					HTML5TokenBuffer *name = token.GetName();
					name->Clear();
					name->AppendL(m_appropriate_end_tag.GetBuffer(), m_appropriate_end_tag.Length());
					name->SetHashValue(m_appropriate_end_tag.GetHashValue());

					return m_state == DATA; // Emit token on '>'
				}
			}

			// this wasn't an appropriate end tag, so reconsume next_char
			// in SCRIPT_DATA/SCRIPT_DATA_ESCAPED/RCDATA/RAWTEXT
			CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;

			if (m_state == SCRIPT_DATA_END_TAG_NAME)
				m_state = SCRIPT_DATA;
			else if (m_state == SCRIPT_DATA_ESCAPED_END_TAG_NAME)
				m_state = SCRIPT_DATA_ESCAPED;
			else if (m_state == RAWTEXT_END_TAG_NAME)
				m_state = RAWTEXT;
			else
				m_state = RCDATA;

			goto do_more_special_data;

		case CHARACTER_REFERENCE_IN_RCDATA:
			{
				data_length--; // increased at the beginning of the loop, but we haven't added anything
				unsigned replaced = ReplaceCharacterEntityL(0, FALSE, TRUE);
				if (replaced == 0)
					replaced = 1; // to add the & as character data if we didn't find a replacement

				if (token.GetType() == HTML5Token::INVALID)
					token.Reset(HTML5Token::CHARACTER);

				for (unsigned i = 0; i < replaced; i++)
				{
					GetNextInputCharacterL(); // to advance in the stream if the replacement is over multiple buffers
					token.AppendToDataL(m_current_buffer, m_position);
					data_length++;
					if (!IsAtEOF())
						ConsumeL();
				}
				data_start = m_position;
			}
			m_state = RCDATA;
			continue;

		case RAWTEXT:
			if (next_char == '<')
			{
				TOKENIZER_FLUSH_DATA;
				// According to the algorithm in the spec we clear the temp buffer in
				// RCDATA/RAWTEXT less than state.  We however do it here and include the
				// '<' (and possibly later '/') in the temp buffer to avoid having
				// to delete the data again if it turns out to be an end tag.
				m_html5_temp_buffer.ResetL(m_current_buffer, m_position, 1);
				m_state = RAWTEXT_LESS_THAN;
			}
			else if (next_char == 0)
			{
				ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
				m_current_buffer->ReplaceNullAt(m_position);
			}
			ConsumeL();
			continue;

		case RAWTEXT_LESS_THAN:
			if (next_char == '/')
			{
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				m_state = RAWTEXT_END_TAG_OPEN;
				ConsumeL();
			}
			else
			{
				// this wasn't an end tag, so reconsume next_char in RAWTEXT
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(RAWTEXT);
			}
			continue;

		case RAWTEXT_END_TAG_OPEN:
			if (is_ascii_alpha(next_char))
			{
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				m_state = RAWTEXT_END_TAG_NAME;
				ConsumeL();
			}
			else
			{
				// this wasn't an end tag, so reconsume next_char in RAWTEXT
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(RAWTEXT);
			}
			continue;

		case SCRIPT_DATA:
			if (next_char == '<')
			{
				TOKENIZER_FLUSH_DATA;
				// According to the algorithm in the spec we clear the temp buffer in
				// RCDATA/RAWTEXT less than state.  We however do it here and include the
				// '<' (and possibly later '/') in the temp buffer to avoid having
				// to delete the data again if it turns out to be an end tag.
				m_html5_temp_buffer.ResetL(m_current_buffer, m_position, 1);
				m_state = SCRIPT_DATA_LESS_THAN;
			}
			else if (next_char == 0)
			{
				ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
				m_current_buffer->ReplaceNullAt(m_position);
			}
			ConsumeL();
			continue;

		case SCRIPT_DATA_LESS_THAN:
			if (next_char == '/')
			{
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				m_state = SCRIPT_DATA_END_TAG_OPEN;
			}
			else if (next_char == '!')
			{
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				m_state = SCRIPT_DATA_ESCAPE_START;
			}
			else
			{
				// this wasn't an end tag, so reconsume next_char in SCRIPT_DATA
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(SCRIPT_DATA);
			}
			ConsumeL();
			continue;

		case SCRIPT_DATA_END_TAG_OPEN:
			if (is_ascii_alpha(next_char))
			{
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				m_state = SCRIPT_DATA_END_TAG_NAME;
				ConsumeL();
			}
			else
			{
				// this wasn't an end tag, so reconsume next_char in SCRIPT_DATA
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(SCRIPT_DATA);
			}
			continue;

		case SCRIPT_DATA_ESCAPE_START:
		case SCRIPT_DATA_ESCAPE_START_DASH:
			if (next_char == '-')
			{
				if (m_state == SCRIPT_DATA_ESCAPE_START)
					m_state = SCRIPT_DATA_ESCAPE_START_DASH;
				else
					m_state = SCRIPT_DATA_ESCAPED_DASH_DASH;
				ConsumeL();
			}
			else
			{
				// this wasn't a comment, so reconsume next_char in SCRIPT_DATA
				RECONSUME_IN(SCRIPT_DATA);
			}
			continue;

		case SCRIPT_DATA_ESCAPED:
		case SCRIPT_DATA_ESCAPED_DASH:
			if (next_char == '-')
			{
				if (m_state == SCRIPT_DATA_ESCAPED)
					m_state = SCRIPT_DATA_ESCAPED_DASH;
				else
					m_state = SCRIPT_DATA_ESCAPED_DASH_DASH;
			}
			else if (next_char == '<')
			{
				TOKENIZER_FLUSH_DATA;
				m_html5_temp_buffer.ResetL(m_current_buffer, m_position, 1);
				m_state = SCRIPT_DATA_ESCAPED_LESS_THAN;
			}
			else
			{
				if (next_char == 0)
				{
					ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
					m_current_buffer->ReplaceNullAt(m_position);
				}

				m_state = SCRIPT_DATA_ESCAPED;
			}
			ConsumeL();
			continue;

		case SCRIPT_DATA_ESCAPED_DASH_DASH:
			if (next_char == '<')
			{
				TOKENIZER_FLUSH_DATA;
				// We start the temporary buffer here; see explanation in TokenizeSpecialDataL()
				m_html5_temp_buffer.ResetL(m_current_buffer, m_position);
				m_state = SCRIPT_DATA_ESCAPED_LESS_THAN;
			}
			else if (next_char == '>')
				m_state = SCRIPT_DATA;
			else if (next_char != '-')
			{
				if (next_char == 0)
				{
					ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
					m_current_buffer->ReplaceNullAt(m_position);
				}

				m_state = SCRIPT_DATA_ESCAPED;
			}
			ConsumeL();
			continue;

		case SCRIPT_DATA_ESCAPED_LESS_THAN:
			if (next_char == '/')
			{
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				m_state = SCRIPT_DATA_ESCAPED_END_TAG_OPEN;
			}
			else if (is_ascii_alpha(next_char))
			{
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				m_state = SCRIPT_DATA_DOUBLE_ESCAPE_START;
			}
			else
			{
				// this wasn't an end tag , so reconsume next_char in SCRIPT_DATA_ESCAPED
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(SCRIPT_DATA_ESCAPED);
			}
			ConsumeL();
			continue;

		case SCRIPT_DATA_ESCAPED_END_TAG_OPEN:
			if (is_ascii_alpha(next_char))
			{
				// next_char will be appended to the name and token reset in a later state
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				m_state = SCRIPT_DATA_ESCAPED_END_TAG_NAME;
				ConsumeL();
			}
			else
			{
				// this wasn't an end tag, so reconsume next_char in SCRIPT_DATA_ESCAPED
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(SCRIPT_DATA_ESCAPED);
			}
			continue;

		case SCRIPT_DATA_DOUBLE_ESCAPE_START:
			if (HTML5Parser::IsHTML5WhiteSpace(next_char) || next_char == '/' || next_char == '>')
			{
				if (m_html5_temp_buffer.Matches(UNI_L("script"), TRUE))
					m_state = SCRIPT_DATA_DOUBLE_ESCAPED;
				else
					m_state = SCRIPT_DATA_ESCAPED;

				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
			}
			else if (is_ascii_alpha(next_char))
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
			else
			{
				// this wasn't an end tag, so reconsume next_char in SCRIPT_DATA_ESCAPED
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(SCRIPT_DATA_ESCAPED);
			}
			ConsumeL();
			continue;

		case SCRIPT_DATA_DOUBLE_ESCAPED:
		case SCRIPT_DATA_DOUBLE_ESCAPED_DASH:
			if (next_char == '-')
			{
				if (m_state == SCRIPT_DATA_DOUBLE_ESCAPED_DASH)
					m_state = SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH;
				else
					m_state = SCRIPT_DATA_DOUBLE_ESCAPED_DASH;
			}
			else if (next_char == '<')
			{
				TOKENIZER_FLUSH_DATA;
				// We start the temporary buffer here; see explanation in TokenizeSpecialDataL()
				m_html5_temp_buffer.ResetL(m_current_buffer, m_position);
				m_state = SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN;
			}
			else if (m_state == SCRIPT_DATA_DOUBLE_ESCAPED_DASH)
				m_state = SCRIPT_DATA_DOUBLE_ESCAPED;

			if (next_char == 0)
			{
				ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
				m_current_buffer->ReplaceNullAt(m_position);
			}
			ConsumeL();
			continue;

		case SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH:
			if (next_char == '<')
			{
				TOKENIZER_FLUSH_DATA;
				// We start the temporary buffer here; see explanation in TokenizeSpecialDataL()
				m_html5_temp_buffer.ResetL(m_current_buffer, m_position);
				m_state = SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN;
			}
			else if (next_char == '>')
				m_state = SCRIPT_DATA;
			else if (next_char != '-')
			{
				if (next_char == 0)
				{
					ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
					m_current_buffer->ReplaceNullAt(m_position);
				}

				m_state = SCRIPT_DATA_DOUBLE_ESCAPED;
			}
			ConsumeL();
			continue;

		case SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN:
			if (next_char == '/')
			{
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
				m_state = SCRIPT_DATA_DOUBLE_ESCAPE_END;
				ConsumeL();
			}
			else
			{
				// this wasn't an end tag, so reconsume next_char in SCRIPT_DATA_DOUBLE_ESCAPED
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(SCRIPT_DATA_DOUBLE_ESCAPED);
			}
			continue;

		case SCRIPT_DATA_DOUBLE_ESCAPE_END:
			if (HTML5Parser::IsHTML5WhiteSpace(next_char) || next_char == '/' || next_char == '>')
			{
				if (m_html5_temp_buffer.Matches(UNI_L("script"), TRUE))
					m_state = SCRIPT_DATA_ESCAPED;
				else
					m_state = SCRIPT_DATA_DOUBLE_ESCAPED;

				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
			}
			else if (is_ascii_alpha(next_char))
				m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
			else
			{
				// this wasn't an end tag, so reconsume next_char in SCRIPT_DATA_DOUBLE_ESCAPED
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				RECONSUME_IN(SCRIPT_DATA_DOUBLE_ESCAPED);
			}
			ConsumeL();
			continue;
		}
	}

	// end of this buffer
	switch (m_state)
	{
	case RCDATA:
	case RAWTEXT:
	case SCRIPT_DATA:
	case SCRIPT_DATA_ESCAPE_START:
	case SCRIPT_DATA_ESCAPE_START_DASH:
		TOKENIZER_FLUSH_DATA;
		if (m_remaining_data_length == 0)
		{
			if (m_current_buffer->IsLastBuffer())
			{
				if (!token.HasData())
					RESET_TOKEN_TO_EOF_L;
				return TRUE;
			}
			m_current_buffer->SetCurrentPosition(m_position, m_update_position);
			next_char = GetNextInputCharacterL();
			data_start = m_position;
			data_length++;
			goto do_more_special_data;
		}
		break;
	case RCDATA_LESS_THAN:
	case RCDATA_END_TAG_OPEN:
	case RCDATA_END_TAG_NAME:
	case RAWTEXT_LESS_THAN:
	case RAWTEXT_END_TAG_OPEN:
	case RAWTEXT_END_TAG_NAME:
	case SCRIPT_DATA_LESS_THAN:
	case SCRIPT_DATA_END_TAG_OPEN:
	case SCRIPT_DATA_END_TAG_NAME:
	case SCRIPT_DATA_ESCAPED_LESS_THAN:
	case SCRIPT_DATA_ESCAPED_END_TAG_OPEN:
	case SCRIPT_DATA_ESCAPED_END_TAG_NAME:
	case SCRIPT_DATA_DOUBLE_ESCAPE_START:
	case SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN:
	case SCRIPT_DATA_DOUBLE_ESCAPE_END:
		if (m_remaining_data_length == 0)
		{
			if (m_current_buffer->IsLastBuffer())
			{
				CONCATENATE_TEMP_BUFFER_AND_RESET_IF_NEEDED_L;
				if (!token.HasData())
					RESET_TOKEN_TO_EOF_L;
				return TRUE;
			}
			m_current_buffer->SetCurrentPosition(m_position, m_update_position);
			next_char = GetNextInputCharacterL();
			data_start = m_position;
			data_length++;
			goto do_more_special_data;
		}
		break;
	case CHARACTER_REFERENCE_IN_RCDATA:
		break;
	case SCRIPT_DATA_ESCAPED:
	case SCRIPT_DATA_ESCAPED_DASH:
	case SCRIPT_DATA_ESCAPED_DASH_DASH:
	case SCRIPT_DATA_DOUBLE_ESCAPED:
	case SCRIPT_DATA_DOUBLE_ESCAPED_DASH:
	case SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH:
		TOKENIZER_FLUSH_DATA;
		if (m_remaining_data_length == 0)
		{
			if (m_current_buffer->IsLastBuffer())
			{
				if (!token.HasData())
				{
					ReportTokenizerError(HTML5Parser::UNEXPECTED_EOF);
					RESET_TOKEN_TO_EOF_L;
				}
				return TRUE;
			}
			m_current_buffer->SetCurrentPosition(m_position, m_update_position);
			next_char = GetNextInputCharacterL();
			data_start = m_position;
			data_length++;
			goto do_more_special_data;
		}
		break;
	}

	return token.HasData();
}

BOOL HTML5Tokenizer::TokenizePlaintextL(HTML5Token& token)
{
	// find a non-empty buffer
	while (m_remaining_data_length == 0)
	{
		m_switched_buffers = TRUE;
		if (m_current_buffer->IsLastBuffer())
		{
			RESET_TOKEN_TO_EOF_L;
			return TRUE;
		}
		else if (HTML5Buffer* buffer = m_current_buffer->Suc())
		{
			if (m_current_buffer->MustSignalEndOfBuffer())
			{
				m_current_buffer->SetMustSignalEndOfBuffer(FALSE);
				m_current_buffer = buffer;
				m_position = m_current_buffer->GetDataAtCurrentPosition(m_remaining_data_length);
				LEAVE(HTML5ParserStatus::FINISHED_DOC_WRITE);
			}
			else
			{
				m_current_buffer = buffer;
				m_position = m_current_buffer->GetDataAtCurrentPosition(m_remaining_data_length);
			}
		}
		else
			LEAVE(HTML5ParserStatus::NEED_MORE_DATA);
	}

	// gobble up all data in the buffer and return it
	OP_ASSERT(m_remaining_data_length > 0);

	const uni_char *c = m_position;
	unsigned length = MIN(m_remaining_data_length, SplitTextLen);
	for (unsigned i = 0; i < length; i++, c++)
		if (*c == 0)
			m_current_buffer->ReplaceNullAt(c);

	if (token.GetType() == HTML5Token::INVALID)
		token.Reset(HTML5Token::CHARACTER);

	token.AppendToDataL(m_current_buffer, m_position, length);
	ConsumeL(length);

	return TRUE;
}

BOOL HTML5Tokenizer::TokenizeSelfClosingStartTagL(HTML5Token& token)
{
	REPORT_ERROR_AND_CONTINUE_IN_DATA_STATE_IF_EOF(FALSE);

	uni_char next_char = GetNextInputCharacterL();

	// End tags can't be self closing, so report error here if this is an end tag
	if (token.GetType() == HTML5Token::END_TAG)
		ReportTokenizerError(HTML5Parser::SELF_CLOSING_END_TAG);

	if (next_char == '>')
	{
		token.SetIsSelfClosing();
		m_state = DATA;
		ConsumeL();
		return TRUE;
	}
	else
	{
		ReportTokenizerError(HTML5Parser::EXPECTED_GREATER_THAN);
		m_state = BEFORE_ATTRIBUTE_NAME;
	}

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeMarkupDeclarationOpenL(HTML5Token& token)
{
	m_html5_temp_buffer.Clear();
	if (!IsAtEOF())
	{
		if (LookAheadIsL(m_position, "--"))
		{
			ConsumeL(2);
			token.Reset(HTML5Token::COMMENT);
			m_state = COMMENT_START;
			return FALSE;
		}
		else if (LookAheadIsL(m_position, "doctype", TRUE))
		{
			ConsumeL(7);
			m_state = DOCTYPE;
			return FALSE;
		}
		else if (m_parser)
		{
			const HTML5TreeBuilder* builder = m_parser->GetTreeBuilder();

			if (!builder->CurrentNodeIsInHTML_Namespace()
				&& LookAheadIsL(m_position, "[CDATA["))
			{
				ConsumeL(7);
				m_state = CDATA_SECTION;
				return FALSE;
			}
		}
	}

	ReportTokenizerError(HTML5Parser::INVALID_MARKUP_DECLARATION);
	m_state = BOGUS_COMMENT;

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeBogusCommentL(HTML5Token& token)
{
	if (token.GetType() == HTML5Token::INVALID)
		token.Reset(HTML5Token::COMMENT);

	while (TRUE)
	{
		if (m_remaining_data_length == 0)
		{
			if (m_current_buffer->IsLastBuffer())
				break;
			GetNextInputCharacterL();
			continue;
		}

		if (GetNextInputCharacterL() == '>')
		{
			ConsumeL();
			break;
		}
		else
		{
			if (*m_position == 0)
				m_current_buffer->ReplaceNullAt(m_position);
			token.AppendToDataL(m_current_buffer, m_position, 1);
			ConsumeL();
		}
	}

	m_state = DATA;

	return TRUE;
}

BOOL HTML5Tokenizer::TokenizeCommentStartL(HTML5Token& token)
{
	REPORT_ERROR_AND_CONTINUE_IN_DATA_STATE_IF_EOF(TRUE);

	uni_char next_char = GetNextInputCharacterL();

	if (next_char == '-')
	{
		// we add this to the temp buffer here instead of
		// adding the '-' in the next state so that we'll be in the correct state
		// and input position in case we leave later
		m_html5_temp_buffer.ResetL(m_current_buffer, m_position);
		CONSUME_AND_CONTINUE_IN_STATE(COMMENT_START_DASH);
	}
	else if (next_char == '>')
	{
		ReportTokenizerError(HTML5Parser::EXPECTED_COMMENT_CONTENT);
		CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
	}
	else
	{
		if (next_char == 0)
		{
			ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
			m_current_buffer->ReplaceNullAt(m_position);
		}

		token.AppendToDataL(m_current_buffer, m_position, 1);
		CONSUME_AND_CONTINUE_IN_STATE(COMMENT);
	}
}

BOOL HTML5Tokenizer::TokenizeCommentStartDashL(HTML5Token& token)
{
	REPORT_ERROR_AND_CONTINUE_IN_DATA_STATE_IF_EOF(TRUE);

	uni_char next_char = GetNextInputCharacterL();

	if (next_char == '-')
	{
		m_html5_temp_buffer.AppendL(m_current_buffer, m_position);
		CONSUME_AND_CONTINUE_IN_STATE(COMMENT_END);
	}
	else if (next_char == '>')
	{
		ReportTokenizerError(HTML5Parser::EXPECTED_COMMENT_CONTENT);
		CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
	}
	else
	{
		if (next_char == 0)
		{
			ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
			m_current_buffer->ReplaceNullAt(m_position);
		}

		m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
		token.ConcatenateToDataL(m_html5_temp_buffer);
		m_html5_temp_buffer.Clear();
		CONSUME_AND_CONTINUE_IN_STATE(COMMENT);
	}
}

BOOL HTML5Tokenizer::TokenizeCommentL(HTML5Token& token)
{
	const uni_char* data_start = NULL;

	while (TRUE)
	{
		REPORT_ERROR_AND_CONTINUE_IN_DATA_STATE_IF_EOF(TRUE);

		uni_char next_char = GetNextInputCharacterL();

		if (next_char == '-')
		{
			if (data_start)
				token.AppendToDataL(m_current_buffer, data_start, m_position - data_start);
			m_html5_temp_buffer.ResetL(m_current_buffer, m_position);

			CONSUME_AND_CONTINUE_IN_STATE(COMMENT_END_DASH);
		}
		else
		{
			if (next_char == 0)
			{
				ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
				m_current_buffer->ReplaceNullAt(m_position);
			}

			if (!data_start)
				data_start = m_position;

			if (m_remaining_data_length <= 1)
			{
				token.AppendToDataL(m_current_buffer, data_start, m_position - data_start + 1);
				data_start = NULL;
			}
		}

		ConsumeL();
	}

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeCommentEndDashL(HTML5Token& token)
{
	REPORT_ERROR_AND_CONTINUE_IN_DATA_STATE_IF_EOF(TRUE);

	uni_char next_char = GetNextInputCharacterL();

	if (next_char == '-')
	{
		m_html5_temp_buffer.AppendL(m_current_buffer, m_position, 1);
		CONSUME_AND_CONTINUE_IN_STATE(COMMENT_END);
	}
	else
	{
		if (next_char == 0)
		{
			ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
			m_current_buffer->ReplaceNullAt(m_position);
		}

		token.ConcatenateToDataL(m_html5_temp_buffer);  // appends the '-'
		m_html5_temp_buffer.Clear();
		token.AppendToDataL(m_current_buffer, m_position, 1);
		m_state = COMMENT;
	}

	ConsumeL();

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeCommentEndL(HTML5Token& token)
{
	REPORT_ERROR_AND_CONTINUE_IN_DATA_STATE_IF_EOF(TRUE);

	uni_char next_char = GetNextInputCharacterL();

	if (next_char == '>')
		CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
	else if (next_char == '!')
	{
		m_html5_temp_buffer.AppendL(m_current_buffer, m_position);
		ReportTokenizerError(HTML5Parser::EXPECTED_GREATER_THAN);
		m_state = COMMENT_END_BANG;
	}
	else if (next_char == '-')
	{
		// move first '-' in temp buffer to data, it's not part of comment end
		token.MoveCharacterToDataFrom(m_html5_temp_buffer);

		// append the latest '-' to temp buffer
		m_html5_temp_buffer.AppendL(m_current_buffer, m_position);
		m_current_buffer->SetCurrentPosition(m_position, m_update_position);
		ReportTokenizerError(HTML5Parser::EXPECTED_GREATER_THAN);
	}
	else
	{
		if (next_char == 0)
		{
			ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
			m_current_buffer->ReplaceNullAt(m_position);
		}

		ReportTokenizerError(HTML5Parser::EXPECTED_GREATER_THAN);
		token.ConcatenateToDataL(m_html5_temp_buffer);  // appends the '--'
		m_html5_temp_buffer.Clear();
		token.AppendToDataL(m_current_buffer, m_position, 1);
		m_state = COMMENT;
	}

	ConsumeL();

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeCommentEndBangL(HTML5Token& token)
{
	REPORT_ERROR_AND_CONTINUE_IN_DATA_STATE_IF_EOF(TRUE);

	uni_char next_char = GetNextInputCharacterL();

	if (next_char == '-')
	{
		token.ConcatenateToDataL(m_html5_temp_buffer);  // appends '--!' to data
		m_html5_temp_buffer.ResetL(m_current_buffer, m_position);  // but this latest '-' might be the end of a comment, so add to temp buffer
		m_state = COMMENT_END_DASH;
	}
	else if (next_char == '>')
		CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
	else
	{
		if (next_char == 0)
		{
			ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
			m_current_buffer->ReplaceNullAt(m_position);
		}

		token.ConcatenateToDataL(m_html5_temp_buffer);  // appends '--!' to data
		m_html5_temp_buffer.Clear();
		token.AppendToDataL(m_current_buffer, m_position, 1);
		m_state = COMMENT;
	}

	ConsumeL();

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeDoctypeL(HTML5Token& token)
{
	token.Reset(HTML5Token::DOCTYPE);

	if (IsAtEOF())
	{
		HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
		return TRUE;
	}

	uni_char next_char = GetNextInputCharacterL();

	if (HTML5Parser::IsHTML5WhiteSpace(next_char))
		CONSUME_AND_CONTINUE_IN_STATE(BEFORE_DOCTYPE_NAME);
	else
	{
		// don't consume
		HandleTokenizerErrorInDoctype(token, HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_NAME);
		m_state = BEFORE_DOCTYPE_NAME;
	}

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeBeforeDoctypeNameL(HTML5Token& token)
{
	uni_char next_char;
	if (!GetNextNonWhiteSpaceCharacterL(FALSE, next_char))
	{
		HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
		return TRUE;
	}

	next_char = ascii_tolower(next_char);
	if (is_ascii_lower(next_char))
	{
		token.Reset(HTML5Token::DOCTYPE);
		token.AppendToNameL(next_char);
		m_state = DOCTYPE_NAME;
	}
	else if (next_char == '>')
	{
		HandleTokenizerErrorInDoctype(token, HTML5Parser::DOCTYPE_EXPECTED_NAME);
		CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
	}
	else
	{
		if (next_char == 0)
		{
			ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
			token.AppendToNameL(UNICODE_REPLACEMENT_CHARACTER);
		}
		else
			token.AppendToNameL(next_char);
		m_state = DOCTYPE_NAME;
	}

	ConsumeL();

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeDoctypeNameL(HTML5Token& token)
{
	while (TRUE)
	{
		if (IsAtEOF())
		{
			token.TerminateNameL();
			HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
			return TRUE;
		}

		uni_char next_char = GetNextInputCharacterL();
		next_char = ascii_tolower(next_char);

		if (HTML5Parser::IsHTML5WhiteSpace(next_char))
		{
			token.TerminateNameL();
			CONSUME_AND_CONTINUE_IN_STATE(AFTER_DOCTYPE_NAME);
		}
		else if (next_char == '>')
		{
			token.TerminateNameL();
			CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
		}
		else if (next_char == 0)
		{
			ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
			token.AppendToNameL(UNICODE_REPLACEMENT_CHARACTER);
		}
		else
			token.AppendToNameL(next_char);

		ConsumeL();
	}

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeAfterDoctypeNameL(HTML5Token& token)
{
	while (TRUE)
	{
		if (IsAtEOF())
		{
			HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
			return TRUE;
		}

		uni_char next_char = GetNextInputCharacterL();

		if (HTML5Parser::IsHTML5WhiteSpace(next_char))
		{
			// Do nothing, just stay in this state
		}
		else if (next_char == '>')
			CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
		else if (LookAheadIsL(m_position, "public", TRUE))
		{
			ConsumeL(6);
			m_state = AFTER_DOCTYPE_PUBLIC_KEYWORD;
			return FALSE;
		}
		else if (LookAheadIsL(m_position, "system", TRUE))
		{
			ConsumeL(6);
			m_state = AFTER_DOCTYPE_SYSTEM_KEYWORD;
			return FALSE;
		}
		else
		{
			ReportTokenizerError(HTML5Parser::BOGUS_DOCTYPE);
			token.SetForceQuirks(TRUE);

			CONSUME_AND_CONTINUE_IN_STATE(BOGUS_DOCTYPE);
		}

		ConsumeL();
	}

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeAfterDoctypePublicKeywordL(HTML5Token& token)
{
	if (IsAtEOF())
	{
		HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
		return TRUE;
	}

	uni_char next_char = GetNextInputCharacterL();

	if (HTML5Parser::IsHTML5WhiteSpace(next_char))
		CONSUME_AND_CONTINUE_IN_STATE(BEFORE_DOCTYPE_PUBLIC_IDENTIFIER);
	else if (next_char == '"')
	{
		token.SetHasPublicIdentifier();
		ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_PUBLIC_IDENTIFIER);
		CONSUME_AND_CONTINUE_IN_STATE(DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED);
	}
	else if (next_char == '\'')
	{
		token.SetHasPublicIdentifier();
		ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_PUBLIC_IDENTIFIER);
		CONSUME_AND_CONTINUE_IN_STATE(DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED);
	}
	else if (next_char == '>')
	{
		HandleTokenizerErrorInDoctype(token, HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_PUBLIC_IDENTIFIER);
		ConsumeL();
		return TRUE;
	}
	else
	{
		ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_PUBLIC_IDENTIFIER);
		token.SetForceQuirks(TRUE);

		CONSUME_AND_CONTINUE_IN_STATE(BOGUS_DOCTYPE);
	}
}

BOOL HTML5Tokenizer::TokenizeBeforeDoctypePublicIdentifierL(HTML5Token& token)
{
	while (TRUE)
	{
		if (IsAtEOF())
		{
			HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
			return TRUE;
		}

		uni_char next_char = GetNextInputCharacterL();

		if (HTML5Parser::IsHTML5WhiteSpace(next_char))
			/* do nothing, stay in state */;
		else if (next_char == '"')
		{
			token.SetHasPublicIdentifier();
			CONSUME_AND_CONTINUE_IN_STATE(DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED);
		}
		else if (next_char == '\'')
		{
			token.SetHasPublicIdentifier();
			CONSUME_AND_CONTINUE_IN_STATE(DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED);
		}
		else if (next_char == '>')
		{
			HandleTokenizerErrorInDoctype(token, HTML5Parser::DOCTYPE_EXPECTED_PUBLIC_IDENTIFIER);
			ConsumeL();
			return TRUE;
		}
		else
		{
			ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_PUBLIC_IDENTIFIER);
			token.SetForceQuirks(TRUE);

			CONSUME_AND_CONTINUE_IN_STATE(BOGUS_DOCTYPE);
		}

		ConsumeL();
	}

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeDoctypePublicIdentifierQuotedL(HTML5Token& token, char quote)
{
	while (TRUE)
	{
		if (IsAtEOF())
		{
			token.TerminatePublicIdentifierL();
			HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
			return TRUE;
		}

		uni_char next_char = GetNextInputCharacterL();

		if (next_char == quote)
		{
			token.TerminatePublicIdentifierL();
			CONSUME_AND_CONTINUE_IN_STATE(AFTER_DOCTYPE_PUBLIC_IDENTIFIER);
		}
		else if (next_char == '>')
		{
			token.TerminatePublicIdentifierL();
			HandleTokenizerErrorInDoctype(token, HTML5Parser::EXPECTED_END_QUOTE);
			ConsumeL();
			return TRUE;
		}
		else if (next_char == 0)
		{
			ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
			token.AppendToPublicIdentifierL(UNICODE_REPLACEMENT_CHARACTER);
		}
		else
			token.AppendToPublicIdentifierL(next_char);

		ConsumeL();
	}

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeAfterDoctypePublicIdentifierL(HTML5Token& token)
{
	if (IsAtEOF())
	{
		HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
		return TRUE;
	}

	uni_char next_char = GetNextInputCharacterL();

	if (HTML5Parser::IsHTML5WhiteSpace(next_char))
		CONSUME_AND_CONTINUE_IN_STATE(BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS);
	else if (next_char == '>')
		CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
	else if (next_char == '"')
	{
		token.SetHasSystemIdentifier();
		ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_SYSTEM_IDENTIFIER);
		m_state = DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
	}
	else if (next_char == '\'')
	{
		token.SetHasSystemIdentifier();
		ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_SYSTEM_IDENTIFIER);
		m_state = DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
	}
	else
	{
		ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_SYSTEM_IDENTIFIER);
		token.SetForceQuirks(TRUE);
		m_state = BOGUS_DOCTYPE;
	}

	ConsumeL();

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeBetweenDoctypePublicAndSystemIdentifiersL(HTML5Token& token)
{
	while (TRUE)
	{
		if (IsAtEOF())
		{
			HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
			return TRUE;
		}

		uni_char next_char = GetNextInputCharacterL();

		if (HTML5Parser::IsHTML5WhiteSpace(next_char))
			/* Do nothing, stay in state */;
		else if (next_char == '>')
			CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
		else if (next_char == '"')
		{
			token.SetHasSystemIdentifier();
			CONSUME_AND_CONTINUE_IN_STATE(DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED);
		}
		else if (next_char == '\'')
		{
			token.SetHasSystemIdentifier();
			CONSUME_AND_CONTINUE_IN_STATE(DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED);
		}
		else
		{
			ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SYSTEM_IDENTIFIER);
			token.SetForceQuirks(TRUE);
			m_state = BOGUS_DOCTYPE;
		}

		ConsumeL();
	}

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeAfterDoctypeSystemL(HTML5Token& token)
{
	if (IsAtEOF())
	{
		HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
		return TRUE;
	}

	uni_char next_char = GetNextInputCharacterL();

	if (HTML5Parser::IsHTML5WhiteSpace(next_char))
		CONSUME_AND_CONTINUE_IN_STATE(BEFORE_DOCTYPE_SYSTEM_IDENTIFIER);
	else if (next_char == '"')
	{
		token.SetHasSystemIdentifier();
		ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_SYSTEM_IDENTIFIER);
		CONSUME_AND_CONTINUE_IN_STATE(DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED);
	}
	else if (next_char == '\'')
	{
		token.SetHasSystemIdentifier();
		ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_SYSTEM_IDENTIFIER);
		CONSUME_AND_CONTINUE_IN_STATE(DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED);
	}
	else if (next_char == '>')
	{
		HandleTokenizerErrorInDoctype(token, HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_SYSTEM_IDENTIFIER);
		ConsumeL();
		return TRUE;
	}
	else
	{
		ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SPACE_BEFORE_SYSTEM_IDENTIFIER);
		token.SetForceQuirks(TRUE);

		CONSUME_AND_CONTINUE_IN_STATE(BOGUS_DOCTYPE);
	}
}

BOOL HTML5Tokenizer::TokenizeBeforeDoctypeSystemIdentifierL(HTML5Token& token)
{
	while (TRUE)
	{
		if (IsAtEOF())
		{
			HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
			return TRUE;
		}

		uni_char next_char = GetNextInputCharacterL();

		if (HTML5Parser::IsHTML5WhiteSpace(next_char))
			/* Do nothing, stay in state */;
		else if (next_char == '"')
		{
			token.SetHasSystemIdentifier();
			CONSUME_AND_CONTINUE_IN_STATE(DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED);
		}
		else if (next_char == '\'')
		{
			token.SetHasSystemIdentifier();
			CONSUME_AND_CONTINUE_IN_STATE(DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED);
		}
		else if (next_char == '>')
		{
			HandleTokenizerErrorInDoctype(token, HTML5Parser::DOCTYPE_EXPECTED_SYSTEM_IDENTIFIER);
			ConsumeL();
			return TRUE;
		}
		else
		{
			ReportTokenizerError(HTML5Parser::DOCTYPE_EXPECTED_SYSTEM_IDENTIFIER);
			token.SetForceQuirks(TRUE);

			CONSUME_AND_CONTINUE_IN_STATE(BOGUS_DOCTYPE);
		}

		ConsumeL();
	}

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeDoctypeSystemIdentifierQuotedL(HTML5Token& token, char quote)
{
	while (TRUE)
	{
		if (IsAtEOF())
		{
			token.TerminateSystemIdentifierL();
			HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
			return TRUE;
		}

		uni_char next_char = GetNextInputCharacterL();

		if (next_char == quote)
		{
			token.TerminateSystemIdentifierL();
			CONSUME_AND_CONTINUE_IN_STATE(AFTER_DOCTYPE_SYSTEM_IDENTIFIER);
		}
		else if (next_char == '>')
		{
			token.TerminateSystemIdentifierL();
			HandleTokenizerErrorInDoctype(token, HTML5Parser::EXPECTED_END_QUOTE);
			ConsumeL();
			return TRUE;
		}
		else if (next_char == 0)
		{
			ReportTokenizerError(HTML5Parser::INVALID_CHARACTER);
			token.AppendToSystemIdentifierL(UNICODE_REPLACEMENT_CHARACTER);
		}
		else
			token.AppendToSystemIdentifierL(next_char);

		ConsumeL();
	}

	return FALSE;
}


BOOL HTML5Tokenizer::TokenizeAfterDoctypeSystemIdentifierL(HTML5Token& token)
{
	if (IsAtEOF())
	{
		HandleTokenizerErrorInDoctype(token, HTML5Parser::UNEXPECTED_EOF);
		return TRUE;
	}

	uni_char next_char = GetNextInputCharacterL();

	if (HTML5Parser::IsHTML5WhiteSpace(next_char))
		/* Do nothing, stay in state */;
	else if (next_char == '>')
		CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);
	else
	{
		ReportTokenizerError(HTML5Parser::EXPECTED_GREATER_THAN);
		m_state = BOGUS_DOCTYPE;
	}

	ConsumeL();

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeBogusDoctypeL(HTML5Token& token)
{
	while (TRUE)
	{
		if (IsAtEOF())
		{
			RESET_TO_DATA_STATE;
			return TRUE;
		}

		if (GetNextInputCharacterL() == '>')
			CONSUME_EMIT_AND_CONTINUE_IN_STATE(DATA);

		ConsumeL();
	}

	return FALSE;
}

BOOL HTML5Tokenizer::TokenizeCdataSectionL(HTML5Token& token)
{
	const uni_char* data_start = NULL;

	if (token.GetType() == HTML5Token::INVALID)
		token.Reset(HTML5Token::CHARACTER);

	OP_ASSERT(token.GetType() == HTML5Token::CHARACTER);

	while (TRUE)
	{
		if (IsAtEOF())
		{
			if (data_start)
				token.AppendToDataL(m_current_buffer, data_start, m_position - data_start);

			RESET_TO_DATA_STATE;
			return TRUE;
		}

		if (data_start && m_remaining_data_length < 3)
		{
			// GetNextInputCharacterL may leave, so add what we have so far, so we don't have to do it again
			token.AppendToDataL(m_current_buffer, data_start, m_position - data_start);
			m_current_buffer->SetCurrentPosition(m_position, m_update_position);
			data_start = NULL;

			if (m_remaining_data_length == 0)
				return TRUE;
		}

		if (GetNextInputCharacterL() == ']')  // Optimization to not have to call LookAheadIsL until we need to
		{
			if (LookAheadIsL(m_position, "]]>", FALSE))  // end of CDATA
			{
				if (data_start)
					token.AppendToDataL(m_current_buffer, data_start, m_position - data_start);

				ConsumeL(3);
				m_state = DATA;

				return TRUE;
			}
		}

		if (!data_start)
			data_start = m_position;

		ConsumeL();
	}

	return FALSE;
}
