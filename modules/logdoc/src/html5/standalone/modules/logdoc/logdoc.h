/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LOGDOC_H
#define LOGDOC_H

#include "modules/logdoc/markup.h"

class H5Element;
class HTML5Parser;

typedef int OP_PARSING_STATUS;

class ParsingStatus : public OpStatus
{
public:
	enum
	{
		NEED_MORE_DATA = USER_SUCCESS + 1,
		EXECUTE_SCRIPT = USER_SUCCESS + 2
	};
};

class LogicalDocument
{
public:
	class Script
	{
	public:
		Script() : m_parsing_ready(TRUE) {}

		BOOL	IsReadyForParsing() { return m_parsing_ready; }

	private:
		BOOL	m_parsing_ready;
	};

	LogicalDocument()
		: m_root(NULL)
		, m_parser(NULL)
		, m_blocking_script(NULL)
		, m_strict_mode(FALSE)
		, m_quirks_line_height_mode(FALSE)
		, m_doctype_name(NULL)
		, m_doctype_public_id(NULL)
		, m_doctype_system_id(NULL)
#ifdef HTML5_STANDALONE
		, m_tokenize_only(FALSE)
#endif // HTML5_STANDALONE
		{}
	~LogicalDocument();

	H5Element*		GetRoot() { return m_root; }

	OP_PARSING_STATUS	Parse(Markup::Type context_elm_type, const uni_char *buffer, unsigned buffer_length, BOOL end_of_data);
	OP_PARSING_STATUS	ParseFragment(H5Element *context_elm, const uni_char *buffer, unsigned buffer_length);
	OP_STATUS			AddParsingData(const uni_char* data, unsigned data_length);
	OP_PARSING_STATUS	ContinueParsing();

	OP_STATUS		SetDoctype(const uni_char *name, const uni_char *pub_id, const uni_char *sys_id);
	const uni_char*	GetDoctypeName() { return m_doctype_name; }
	const uni_char*	GetDoctypePubId() { return m_doctype_public_id; }
	const uni_char*	GetDoctypeSysId() { return m_doctype_system_id; }

	void			CheckQuirksMode();

	Script*			GetBlockingScript() { return m_blocking_script; }

#ifdef HTML5_STANDALONE
	void			SetTokenizeOnly() { m_tokenize_only = TRUE; }
	HTML5Parser*	GetParser() { return m_parser; }
#endif // HTML5_STANDALONE

private:
	H5Element*		m_root;
	HTML5Parser*	m_parser;

	Script*			m_blocking_script;

	BOOL			m_strict_mode;
	BOOL			m_quirks_line_height_mode;

	uni_char*       m_doctype_name;
	uni_char*       m_doctype_public_id;
	uni_char*       m_doctype_system_id;

#ifdef HTML5_STANDALONE
	BOOL			m_tokenize_only;
#endif // HTML5_STANDALONE
};

#endif // LOGDOC_H
