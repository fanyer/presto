/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5TOKENIZER_H
#define HTML5TOKENIZER_H

#include "modules/logdoc/src/html5/html5buffer.h"
#include "modules/logdoc/src/html5/html5entities.h"
#include "modules/logdoc/html5parser.h"

class HTML5Buffer;
class HTML5Token;
class HTML5Attr;

#define UNICODE_REPLACEMENT_CHARACTER (0xFFFD)


/**
 * Class which implements a tokenizer according to the HTML 5 specification.
 *
 * It's run by feeding it data buffers with AddData, then repeatedly calling
 * GetNextTokenL, until a EndOfFile token is returned.
 */
class HTML5Tokenizer
{
public:
	friend class BufferStorePoint;

	enum State
	{
		DATA,
		TAG_OPEN,
		END_TAG_OPEN,
		TAG_NAME,
		CHARACTER_REFERENCE,

		RCDATA,
		RCDATA_LESS_THAN,
		RCDATA_END_TAG_OPEN,
		RCDATA_END_TAG_NAME,
		CHARACTER_REFERENCE_IN_RCDATA,

		RAWTEXT,
		RAWTEXT_LESS_THAN,
		RAWTEXT_END_TAG_OPEN,
		RAWTEXT_END_TAG_NAME,

		SCRIPT_DATA,
		SCRIPT_DATA_LESS_THAN,
		SCRIPT_DATA_END_TAG_OPEN,
		SCRIPT_DATA_END_TAG_NAME,
		SCRIPT_DATA_ESCAPE_START,
		SCRIPT_DATA_ESCAPE_START_DASH,
		SCRIPT_DATA_ESCAPED,
		SCRIPT_DATA_ESCAPED_DASH,
		SCRIPT_DATA_ESCAPED_DASH_DASH,
		SCRIPT_DATA_ESCAPED_LESS_THAN,
		SCRIPT_DATA_ESCAPED_END_TAG_OPEN,
		SCRIPT_DATA_ESCAPED_END_TAG_NAME,
		SCRIPT_DATA_DOUBLE_ESCAPE_START,
		SCRIPT_DATA_DOUBLE_ESCAPED,
		SCRIPT_DATA_DOUBLE_ESCAPED_DASH,
		SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH,
		SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN,
		SCRIPT_DATA_DOUBLE_ESCAPE_END,

		BEFORE_ATTRIBUTE_NAME,
		ATTRIBUTE_NAME,
		AFTER_ATTRIBUTE_NAME,
		BEFORE_ATTRIBUTE_VALUE,
		ATTRIBUTE_VALUE_DOUBLE_QUOTED,
		ATTRIBUTE_VALUE_SINGLE_QUOTED,
		ATTRIBUTE_VALUE_UNQUOTED,
		AFTER_ATTRIBUTE_VALUE_QUOTED,

		PLAINTEXT,
		SELF_CLOSING_START_TAG,
		MARKUP_DECLARATION_OPEN,

		BOGUS_COMMENT,
		COMMENT_START,
		COMMENT_START_DASH,
		COMMENT,
		COMMENT_END_DASH,
		COMMENT_END,
		COMMENT_END_BANG,
		COMMENT_END_SPACE,

		DOCTYPE,
		BEFORE_DOCTYPE_NAME,
		DOCTYPE_NAME,
		AFTER_DOCTYPE_NAME,
		AFTER_DOCTYPE_PUBLIC_KEYWORD,
		BEFORE_DOCTYPE_PUBLIC_IDENTIFIER,
		DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED,
		DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED,
		AFTER_DOCTYPE_PUBLIC_IDENTIFIER,
		BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS,
		AFTER_DOCTYPE_SYSTEM_KEYWORD,
		BEFORE_DOCTYPE_SYSTEM_IDENTIFIER,
		DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED,
		DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED,
		AFTER_DOCTYPE_SYSTEM_IDENTIFIER,
		BOGUS_DOCTYPE,

		CDATA_SECTION
	};

	HTML5Tokenizer(HTML5Parser* parser, BOOL recognize_processing_instruction = FALSE);
	~HTML5Tokenizer();

	/// Clears all state from the tokenizer, like deleting it and newing it with same parameters to constructor
	void	Reset();

	State	GetState() const { return m_state; }
	void	SetState(State state) { m_state = state; }

	/// @returns TRUE if this tokenizer has returned an EOF token, or SetFinished has been called (on abort)
	BOOL	IsFinished() { return m_is_finished; }
	void	SetFinished() { m_is_finished = TRUE; }

	/// If update is TRUE the source position will be updated and stored in the token
	void	SetUpdatePosition(BOOL update) { m_update_position = update; }

	/**
	 * Adds data for the tokenizer to work on.  This can be called several
	 * times if the input is split across several buffers.  That last buffer
	 * must have the is_last_buffer parameter set to TRUE (and non of the others
	 * should have it set).
	 *
	 * @param is_placeholder_buffer A placeholder buffer is an empty buffer DSE
	 * inserts on recovery until the real data is inserted later. Should be true
	 * if this add is such placeholder data, FALSE otherwise.
	 * @param is_fragment Set to TRUE if this is a fragment (innerHTML) which
	 * will result in less preprocessing.
	 *
	 * Will LEAVE on OOM, in which case the latest buffer was not added.
	 */
	void	AddDataL(const uni_char* data, unsigned int length, BOOL is_last_buffer, BOOL is_placeholder_buffer, BOOL is_fragment);

	/**
	 * Inserts data for the tokenizer to work on before or after the current
	 * buffer. Used for data written by document.write(). This can be called
	 * several times if the input is split across several buffers.
	 *
	 * Will LEAVE on OOM, in which case the latest buffer was not added.
	 */
	void	InsertDataL(const uni_char* data, unsigned int length, BOOL add_newline);

	/**
	 ' Used to for tokenizing plain text documents. Inserts a <pre> before
	 * the text data stream. WARNING! Must only be called before tokenization
	 * has started.
	 */
	void	InsertPlaintextPreambleL();

	BOOL	HasData() { return m_current_buffer != NULL; }

	void	CloseLastBuffer() { if (m_data_buffers.Last()) m_data_buffers.Last()->SetIsLastBuffer(); else m_tokenizer_empty_buffer = TRUE; }

	/**
	 * Push/PopInsertionPointL() are used to set the insertion point for
	 * document.write() as described by the HTML 5 spec when starting
	 * and ending running and execution of a script
	 * @param buffer The insertion point buffer, using current if NULL
	 */
	void	PushInsertionPointL(HTML5Buffer *buffer);
	void	PopInsertionPointL();

	BOOL	ResetEOBSignallingForNextBuffer();

	/**
	 * Returns next token in the input.  In case there is not enough data in the
	 * input buffers the tokenizer has so far (and AddData has not been called with
	 * a TRUE is_last_buffer parameter) then this function will leave with a
	 * HTML5ParserStatus::NEED_MORE_DATA status.  The caller should then call
	 * AddDataL with additional input data, before calling GetNextTokenL again
	 * with the SAME token used in the previous call, since some state is stored
	 * in the token.
	 *
	 * The function will also LEAVE on OOM, in which case it should also be called again
	 * with the same token if more memory is made available.
	 *
	 * If the function doesn't LEAVE, the token should always be valid.
	 *
	 * @returns The next token in the stream.
	 */
	void	GetNextTokenL(HTML5Token& token);

	/// If the codepoint is an invalid codepoint (for HTML) the characer it should be
	/// replaced by (according to the HTML 5 specification) is returned.
	unsigned long	CheckAndReplaceInvalidUnicode(unsigned long codepoint, BOOL is_preprocessing, unsigned line = 0, unsigned chr = 0);

	unsigned		GetLastBufferStartOffset() { return m_current_buffer ? m_current_buffer->GetStreamPosition() : 0; }
	unsigned		GetLastBufferPosition() { return m_current_buffer ? m_current_buffer->GetCurrentPosition() : 0; }

#if defined DELAYED_SCRIPT_EXECUTION || defined SPECULATIVE_PARSER
	OP_STATUS		StoreTokenizerState(HTML5ParserState* state);
	OP_STATUS		RestoreTokenizerState(HTML5ParserState* state, unsigned buffer_stream_position);
#endif // defined DELAYED_SCRIPT_EXECUTION || defined SPECULATIVE_PARSER

#ifdef DELAYED_SCRIPT_EXECUTION
	BOOL			HasTokenizerStateChanged(HTML5ParserState* state);
	/**
	 * Used by DSE to see if there is more data in the tokenizer that needs to
	 * be parsed before finishing the current delayed script and continuing to
	 * the next.
	 * @returns TRUE if there is unfinished write data in the tokenizer.
	 */
	BOOL			HasUnfinishedWriteData();
#endif // DELAYED_SCRIPT_EXECUTION

#ifdef DEBUG_ENABLE_OPASSERT
	unsigned	GetIPStackDepth()
	{
		unsigned depth = 0;
		InsertionPoint *iter = m_insertion_point_stack;
		while (iter)
		{
			depth++;
			iter = iter->m_next;
		}
		return depth;
	}
#endif // DEBUG_ENABLE_OPASSERT

private:

	class InsertionPoint
	{
	public:
		InsertionPoint() : m_buffer(NULL), m_next(NULL) {}

		HTML5Buffer*	m_buffer;
		InsertionPoint*	m_next;

		void	Set(HTML5Buffer *buffer) { OP_ASSERT(buffer); m_buffer = buffer; }
	};

	BOOL	EmitErrorIfEOFL(HTML5Token& token);

	/// Returns the next character in input (without consuming anything)
	uni_char	PeekAtNextInputCharacter() { OP_ASSERT(m_remaining_data_length > 0); return *m_position; }

	/// Returns the next character in input, and consumes it (i.e. moves current position)
	uni_char 	GetNextInputCharacterL() { if (m_remaining_data_length >= 1) return *m_position; else return GetNextInputCharacterSlowL(); }
	uni_char 	GetNextInputCharacterSlowL();

	/// Skips white space characters (consumes them), then returns and consumes the next
	/// non-white space character
	/// @param[OUT] next_char Will contain the next non-whitespace character after call.
	/// @returns TRUE if a non-whitespace character was found, otherwise FALSE
	BOOL	GetNextNonWhiteSpaceCharacterL(BOOL report_error, uni_char &next_char);

	/// Consumes one character (i.e. moves current position in input).
	/// May LEAVE with HTML5ParserStatus::NEED_MORE_DATA
	void	ConsumeL() {	if (m_remaining_data_length < 1) ConsumeL(1); else { m_position++; m_remaining_data_length--; } }

	/// Consumes a specified amount of characters from the input buffer.
	/// May LEAVE with HTML5ParserStatus::NEED_MORE_DATA
	void	ConsumeL(unsigned number_to_consume);

	BOOL	IsAtEOF() const;

	void	SetIsLookingAheadL(BOOL is_looking_ahead, HTML5Buffer *buffer);

	/**
	 * Checks if the string starting at input matches look_for.  Returns
	 * TRUE if it does.
	 * Will LEAVE with HTML5ParserStatus::NEED_MORE_DATA if it needs more input data
	 * to determine if there is a match.
	 *
	 * @param input   Pointer to a position in input current_buffer where we should look
	 *                for string match.  MUST be pointer to valid position in m_current_buffer!
	 * @param look_for  Character sequence to look for.  MUST be in lowercase if case_insensitive is TRUE
	 * @param case_insensitive  If TRUE then the match will be done case_insensitively
	 * @return TRUE if there is a match
	 */
	BOOL 	LookAheadIsL(const uni_char* input, const char* look_for, BOOL case_insensitive = FALSE);

	/**
	 * Call when next character in input buffer is a & (in a state where entities
	 * are allowed).  Will try to match the next input to a entity (named or numeric)
	 * and if it does, will replace the entity in the input buffer with the correct
	 * character.
	 * Will LEAVE with HTML5ParserStatus::NEED_MORE_DATA if it needs more input data
	 * to determine if there is a match.
	 *
	 * @param additionally_allowed_character  As defined in HTML 5 specification
	 * @param in_attribute  Should be TRUE if the entity is inside and attribute value (where somewhat different rules are applied)
	 * @param replace_null  If TRUE, all NULL values will be replaced by the replacement character
	 * @return The number of inserted characters or 0 if no matching entity found.
	 */
	unsigned	ReplaceCharacterEntityL(uni_char additionally_allowed_character, BOOL in_attribute, BOOL replace_null);

	/**
	 *  Parses a numeric entity at current input position, returns TRUE if it matches
	 *  Returns TRUE if a valid numeric entity was matched.  If so then the
	 *  resulting character is placed in high and low.  High will be 0 if the character
	 *  can be represented by a single uni_char, otherwise high and low will contain
	 *  the high and low uni_chars of a surrogate pair.
	 */
	BOOL	ParseNumberEntityL(uni_char *replacements);
	/// As for ParseNumberEntityL, but for named entities
	BOOL	ParseNamedEntityL(const uni_char *&replacements, unsigned &rep_length, BOOL in_attribute);

	/// Checks if m_html5_temp_buffer matches the stored tags name.  See HTML5 spec
	BOOL	IsAppropriateEndTag();

	void	HandleTokenizerErrorInDoctype(HTML5Token& token, HTML5Parser::ErrorCode error);
	void	ReportTokenizerError(HTML5Parser::ErrorCode error, int offset = 0, unsigned line = 0, unsigned chr = 0);

#ifdef _DEBUG
	void	CheckIPReference(HTML5Buffer *buffer);
#endif // _DEBUG
	/// Frees buffers we don't need anymore
	void	CleanUpBetweenTokensL();

	/**
	 * Tokenize functions, one for most states (a few states share a function).
	 *
	 * Will return TRUE when a token is ready to be returned, otherwise keep
	 * calling the appropriate function for the now current state until
	 * a function returns TRUE.
	 *
	 * Will LEAVE on OOM, or when more input data is needed.
	 *
	 * When LEAVING this is implemented so that it will restart in the same state, and
	 * hence the same function will be called again when more data is available (or memory
	 * has been freed, depending on why it LEAVED last time).
	 * To keep this working the functions should only Consume the characters it needs to
	 * when it's clear that it will not leave and that a state has changed or that it
	 * has stored what it has so far in the token.  This differs a little from the HTML 5
	 * specification, which tells us to consume characters immediately.  But from outside
	 * the tokenizer the result should look the same.
	 *
	 * The current position in input buffer is stored to HTML5Buffer when we have Consumed
	 * the characters if we return normally.  If we leave then it will not be stored, and
	 * hence we will restart tokenizing from the same position after being called again
	 * after LEAVING.
	 */

	BOOL	TokenizeMostCommonL(HTML5Token& token);
	BOOL	TokenizeAttributesL(HTML5Token& token, uni_char quote);

	BOOL	TokenizeSpecialCharacterReferenceL(HTML5Token& token);

	/**
	 * Called to tokenize RCDATA, RAWTEXT and script data.
	 * @param data_length Length of data already in token and
	 *        pending in the HTML 5 temp buffer.h
	 */
	BOOL	TokenizeSpecialDataL(HTML5Token& token, unsigned data_length);
	BOOL	TokenizeCharacterReferenceInAttributeL(HTML5Token& token);
	BOOL	TokenizeAfterAttributeValueQuotedL(HTML5Token& token);
	BOOL	TokenizePlaintextL(HTML5Token& token);
	BOOL	TokenizeSelfClosingStartTagL(HTML5Token& token);
	BOOL	TokenizeMarkupDeclarationOpenL(HTML5Token& token);
	BOOL	TokenizeBogusCommentL(HTML5Token& token);
	BOOL	TokenizeCommentStartL(HTML5Token& token);
	BOOL	TokenizeCommentStartDashL(HTML5Token& token);
	BOOL	TokenizeCommentL(HTML5Token& token);
	BOOL	TokenizeCommentEndDashL(HTML5Token& token);
	BOOL	TokenizeCommentEndL(HTML5Token& token);
	BOOL	TokenizeCommentEndBangL(HTML5Token& token);
	BOOL	TokenizeDoctypeL(HTML5Token& token);
	BOOL	TokenizeBeforeDoctypeNameL(HTML5Token& token);
	BOOL	TokenizeDoctypeNameL(HTML5Token& token);
	BOOL	TokenizeAfterDoctypeNameL(HTML5Token& token);
	BOOL	TokenizeAfterDoctypePublicKeywordL(HTML5Token& token);
	BOOL	TokenizeBeforeDoctypePublicIdentifierL(HTML5Token& token);
	BOOL	TokenizeDoctypePublicIdentifierQuotedL(HTML5Token& token, char quote);
	BOOL	TokenizeAfterDoctypePublicIdentifierL(HTML5Token& token);
	BOOL	TokenizeBetweenDoctypePublicAndSystemIdentifiersL(HTML5Token& token);
	BOOL	TokenizeAfterDoctypeSystemL(HTML5Token& token);
	BOOL	TokenizeBeforeDoctypeSystemIdentifierL(HTML5Token& token);
	BOOL	TokenizeDoctypeSystemIdentifierQuotedL(HTML5Token& token, char quote);
	BOOL	TokenizeAfterDoctypeSystemIdentifierL(HTML5Token& token);
	BOOL	TokenizeBogusDoctypeL(HTML5Token& token);
	BOOL	TokenizeCdataSectionL(HTML5Token& token);


	HTML5Parser*	m_parser;
	/// m_position will point into this buffer.  It may have next and previous buffers,
	/// depending on how much has been added with AddData and freed by
	/// CleanUpBetweenTokens and RemoveEmptyBuffers
	HTML5Buffer*	m_current_buffer;
	AutoDeleteList<HTML5Buffer>
					m_data_buffers;
	/// Input position during the current call to GetNextTokenL
	const uni_char* m_position;
	/// Remaining length from m_position to end of current buffer
	unsigned 		m_remaining_data_length;

	InsertionPoint*	m_insertion_point_stack;

#if defined DELAYED_SCRIPT_EXECUTION || defined SPECULATIVE_PARSER
	/** When restoring a tokenizer to a previous position (during DSE or
	 *  speculative parsing) we may have skipped too far back. This tells us how
	 *  much of the buffer to skip to where we want to be.
	 */
	unsigned		m_restore_skip;
#endif // defined DELAYED_SCRIPT_EXECUTION || defined SPECULATIVE_PARSER

	/// Some states consume first character in next token before returning the token
	/// To keep the line position of the tokens correct we have to adjust for that
	BOOL			m_include_previous_char_in_line_pos;

	State 			m_state;
	/// We're in a state where we can clear old input data buffers (all preceding m_
	/// current_buffer) because no one uses them anymore
	BOOL			m_switched_buffers;
	BOOL			m_token_emitted;
	BOOL			m_is_finished;  ///< has returned a EOF token
	HTML5EntityMatcher	m_entity_matcher;
	/// Used to indicate where to restart when lookahead is interrupted by end of buffers
	BOOL			m_is_looking_ahead;
	HTML5TokenBuffer	m_appropriate_end_tag;
	BOOL			m_tokenizer_empty_buffer;
	BOOL			m_recognize_processing_instruction;
	BOOL			m_update_position;

	/**
	 *  This is the temp buffer which is speced in the HTML 5 tokenizer algoritm.
	 *  It is used in RCData and Rawtext when we're tokenizing something which
	 *  may be a tagname, but which we want to return as character data if turns
	 *  out not to be.
	 */
	HTML5StringBuffer	m_html5_temp_buffer;
};

#endif // HTML5TOKENIZER_H
