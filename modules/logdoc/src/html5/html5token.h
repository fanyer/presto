/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5TOKEN_H
#define HTML5TOKEN_H

#include "modules/logdoc/src/html5/html5attr.h"
#include "modules/logdoc/src/html5/html5buffer.h"
#include "modules/logdoc/src/html5/html5tokenbuffer.h"


class HTML5Token
{
public:
	enum TokenType
	{
		DOCTYPE,
		START_TAG,
		END_TAG,
		COMMENT,
		CHARACTER,
		ENDOFFILE,
		PROCESSING_INSTRUCTION,
		PARSE_ERROR,
		INVALID
	};

	HTML5Token()
		: m_type(INVALID)
		, m_line_num(1)
		, m_line_pos(1)
		, m_self_closing(NOT_SELF_CLOSING)
		, m_name(64)
		, m_attributes(NULL)
		, m_attributes_length(0)
		, m_number_of_attributes(0) 
		, m_last_attribute_is_valid(TRUE) {}
	~HTML5Token() { OP_DELETEA(m_attributes); }

	/**
	 * Secondary constructor. But can also be used to reset the token when reusing the parser.
	 * When used to initialize a token that will copy another, lengths should be set to 0 to
	 * avoid allocating buffers that will be deleted in the copying process anyway.
	 *
	 * @param[in] init_name_len The initial size of the name buffer. If 0, no buffer will be allocated.
	 * @param[in] init_attr_len The initial size of the attribute array. Will do nothing if an
	 * existing buffer is already larger or equal to this size.
	 */
	void			InitializeL(unsigned init_name_len = 64, unsigned init_attr_len = HTML5Token::kAttrChunkSize);
	
	/// Clears token and resets it to start a new token of given type
	void			Reset(TokenType type)
	{
		m_type = type;
		m_doctype_info.m_force_quirks = FALSE;
		m_self_closing = NOT_SELF_CLOSING;

		m_name.Clear();
		m_doctype_info.m_public_identifier.Clear();
		m_doctype_info.m_system_identifier.Clear();
		m_number_of_attributes = 0;

		m_data.Clear();
	}

	
	/// @return The type of token
	TokenType		GetType() const { return m_type; }

	/// @returns The line number in the stream the token starts
	unsigned		GetLineNum() const { return m_line_num; }
	void			SetLineNum(unsigned line) { m_line_num = line; }
	/// @returns The position on the line the token starts
	unsigned		GetLinePos() const { return m_line_pos; }
	void			SetLinePos(unsigned pos) { m_line_pos = pos; }

	// For DOCTYPE tokens
	BOOL			HasPublicIdentifier() const { return m_doctype_info.m_has_public_identifier; }
	BOOL			HasSystemIdentifier() const { return m_doctype_info.m_has_system_identifier; }
	void			SetHasPublicIdentifier() { m_doctype_info.m_has_public_identifier = TRUE; }
	void			SetHasSystemIdentifier() { m_doctype_info.m_has_system_identifier = TRUE; }

	const uni_char*	GetPublicIdentifier() const { return m_doctype_info.m_public_identifier.GetBuffer(); }
	void			GetPublicIdentifier(const uni_char *&pub_id, unsigned &pub_id_length) const { pub_id = m_doctype_info.m_public_identifier.GetBuffer(); pub_id_length = m_doctype_info.m_public_identifier.Length(); }
	const uni_char*	GetSystemIdentifier() const { return m_doctype_info.m_system_identifier.GetBuffer(); }
	void			GetSystemIdentifier(const uni_char *&sys_id, unsigned &sys_id_length) const { sys_id = m_doctype_info.m_system_identifier.GetBuffer(); sys_id_length = m_doctype_info.m_system_identifier.Length(); }
	void			AppendToPublicIdentifierL(uni_char character) { m_doctype_info.m_has_public_identifier = TRUE; m_doctype_info.m_public_identifier.AppendL(character); }
	void			TerminatePublicIdentifierL() { m_doctype_info.m_public_identifier.TerminateL(); }
	void			AppendToSystemIdentifierL(uni_char character) { m_doctype_info.m_has_system_identifier = TRUE; m_doctype_info.m_system_identifier.AppendL(character); }
	void			TerminateSystemIdentifierL() { m_doctype_info.m_system_identifier.TerminateL(); }

	BOOL			DoesForceQuirks() const { return m_doctype_info.m_force_quirks; }
	void			SetForceQuirks(BOOL force_quirks) { m_doctype_info.m_force_quirks = force_quirks; }

	// For DOCTYPE and tag tokens
	HTML5TokenBuffer*	GetName() { return &m_name; }
	const uni_char*		GetNameStr() const { return m_name.GetBuffer(); }
	unsigned			GetNameLength() const { return m_name.Length(); }
	/**
	 * Used to override the name tokenized from the original stream. That is normally not a good
	 * idea but in the case of an image tag, it should be treated as an img tag, so we introduced
	 * this way of forcing another name. Do not use unless you have no other alternative.
	 */
	void			SetNameL(const uni_char *new_name, unsigned name_len) { m_name.Clear(); m_name.AppendAndHashL(new_name, name_len); }
	void			SetNameL(const HTML5TokenBuffer *new_name) { m_name.CopyL(new_name); }
	void			AppendToNameL(const uni_char character) { m_name.AppendAndHashL(character); }
	void			TerminateNameL() { m_name.TerminateL(); }

	// For start and end tag tokens
	/// Returns TRUE if the token is self closing (<close/>)
	BOOL			IsSelfClosing() const { return m_self_closing != NOT_SELF_CLOSING; }
	BOOL			IsSelfClosingAcked() { return m_self_closing == ACKED_SELF_CLOSING; }
	void			SetIsSelfClosing() { m_self_closing = SELF_CLOSING; }
	void			AckSelfClosing() { m_self_closing = ACKED_SELF_CLOSING; }

	virtual unsigned int	GetAttrCount() const { return m_number_of_attributes; }
	HTML5Attr*		GetAttribute(unsigned int idx) const { return &m_attributes[idx]; }
	/// Allocates space if necessary and returns a pointer to a new attribute
	HTML5Attr*		GetNewAttributeL();
	void			AppendToLastAttributeNameL(uni_char character) { if ((m_type == START_TAG || m_type == PROCESSING_INSTRUCTION) && m_last_attribute_is_valid) GetLastAttribute()->AppendToNameL(character); }
	void			TerminateLastAttributeNameL() { GetLastAttribute()->GetName()->TerminateL(); }
	void			AppendToLastAttributeValueL(const HTML5Buffer* buffer, const uni_char* string, unsigned length = 1) { if ((m_type == START_TAG || m_type == PROCESSING_INSTRUCTION) && m_last_attribute_is_valid) GetLastAttribute()->AppendToValueL(buffer, string, length); }

	/**
	 * Checks and removes last attribute if the name is a duplicate of a previous attribute
	 * Sets the m_last_attribute_is_valid flag to FALSE, so that any attempts to add to
	 * the attribute's value (or name) is ignored.
	 * @returns TRUE if the last added attribute already existed in the attribute list.
	 */
	BOOL			CheckIfLastAttributeIsDuplicate();
	
	// For comment and character tokens
	const HTML5StringBuffer&	GetData() const { return m_data; }
	
	BOOL			HasData() const { return m_data.HasData(); }
	void			AppendToDataL(const HTML5Buffer* buffer, const uni_char* data, unsigned length = 1) { m_data.AppendL(buffer, data, length); }
	void			ConcatenateToDataL(const HTML5StringBuffer& concatenate) { m_data.ConcatenateL(concatenate); }
	void			MoveCharacterToDataFrom(HTML5StringBuffer& move_from) { m_data.MoveCharacterFrom(move_from); }
	
	BOOL			IsWSOnly(BOOL include_null_char) const;

private:
	struct DoctypeInfo
	{
		DoctypeInfo()
			: m_force_quirks(FALSE)
			, m_has_public_identifier(FALSE)
			, m_has_system_identifier(FALSE)
			, m_public_identifier(128)
			, m_system_identifier(128)
		{}

		BOOL m_force_quirks, m_has_public_identifier, m_has_system_identifier;
		HTML5TokenBuffer	m_public_identifier;
		HTML5TokenBuffer	m_system_identifier;
	};

	enum SelfClosingState
	{
		NOT_SELF_CLOSING,
		SELF_CLOSING,
		ACKED_SELF_CLOSING
	};

	static const unsigned kAttrChunkSize = 16;

	HTML5Attr*		GetLastAttribute() const { OP_ASSERT(m_number_of_attributes >= 1 && m_number_of_attributes <= m_attributes_length); return &m_attributes[m_number_of_attributes-1]; }

	TokenType			m_type;
	unsigned int		m_line_num;
	unsigned int		m_line_pos;
	HTML5StringBuffer	m_data;
	SelfClosingState	m_self_closing;
	BOOL				m_force_quirks;
	HTML5TokenBuffer	m_name;
	DoctypeInfo			m_doctype_info;
	HTML5Attr*			m_attributes;
	unsigned			m_attributes_length;
	unsigned			m_number_of_attributes;
	BOOL				m_last_attribute_is_valid;
};

#endif // HTML5TOKEN_H
