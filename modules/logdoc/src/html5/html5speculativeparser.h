/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5SPECULATIVEPARSER_H
#define HTML5SPECULATIVEPARSER_H

#ifdef SPECULATIVE_PARSER

#include "modules/logdoc/inline_finder.h"
#include "modules/url/url_loading.h"

/**
 * A speculative resource parser that starts when the regular HTML5 parser is
 * blocked, waiting on an external script.
 */
class HTML5SpeculativeParser
	: public InlineFinder::LinkHandler
{
public:
	/**
	 * Creates the speculative parser.
	 *
	 * @param[out] parser The created speculative parser or NULL.
	 * @param[in] url URL of the document we should parse. It must refer to a
	 *                (X)HTML, SVG or WML document.
	 * @param[in] base_url URL used as base for inlines.
	 * @param[in] logdoc The logical document for the URL.
	 *
	 * @return Normal OOM values.
	 */

	static OP_STATUS Make(HTML5SpeculativeParser *&parser, URL url, URL base_url, LogicalDocument *logdoc);

	~HTML5SpeculativeParser();

	/**
	 * Append data to the existing stream of data to parse.
	 *
	 * @param buffer Pointer to the start of the buffer to append. Not NULL.
	 * @param length Length, in uni_chars of the buffer.
	 * @param end_of_data Set to TRUE if this is the last data to be appended.
	 *
	 * Leaves on OOM.
	 */
	void AppendDataL(const uni_char *buffer, unsigned length, BOOL end_of_data);
	OP_STATUS AppendData(const uni_char *buffer, unsigned length, BOOL end_of_data) { TRAPD(status, AppendDataL(buffer, length, end_of_data)); return status; }

	/**
	 * Parse the contents of the added buffers.
	 *
	 * Can Leave with OOM values or HTML5ParserStatus.
	 */
	void ParseL();
	OP_PARSER_STATUS Parse() { TRAPD(pstat, ParseL()); return pstat; }

	/**
	 * Restores the state of the parser. Used when we (re)start in the middle of
	 * the stream.
	 *
	 * @param state The new state.
	 * @param buffer_stream_position The position in the stream where this
	 *        buffer starts.
	 * @param base_url URL used as base for inlines.
	 */
	OP_STATUS RestoreState(HTML5ParserState* state, unsigned buffer_stream_position, URL base_url)
	{
		m_inline_finder->SetBaseURL(base_url);
		return m_inline_finder->RestoreTokenizerState(state, buffer_stream_position);
	}

	/**
	 * Returns TRUE if the parser has finished parsing the last buffer in the
	 * data stream.
	 */
	BOOL IsFinished() { return m_is_finished; }

	/**
	 * Returns value of the current position in the stream. Since this value
	 * combines a non preprocessed position with a preprocessed one it is only
	 * comparable to a position returned from from the same type of function.
	 */
	unsigned GetStreamPosition() { return m_inline_finder->GetStreamPosition(); }

	virtual void LinkFoundL(URL &link_url, Markup::Type type, HTML5TokenWrapper *token);
	virtual void LinkListFoundL(Head &link_list);

private:

	HTML5SpeculativeParser(LogicalDocument *logdoc, URL url);

	InlineFinder *m_inline_finder;
	LogicalDocument *m_logdoc;
	URL m_url;
	BOOL m_is_finished;
};

#endif // SPECULATIVE_PARSER
#endif // HTML5SPECULATIVEPARSER_H
