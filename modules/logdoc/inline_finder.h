/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef INLINE_FINDER_H
#define INLINE_FINDER_H

#if defined MHTML_ARCHIVE_SAVE_SUPPORT || defined LOGDOC_INLINE_FINDER_SUPPORT

#include "modules/logdoc/datasrcelm.h"
#include "modules/logdoc/src/html5/html5tokenizer.h"
#include "modules/logdoc/logdoc.h"

class URL;
class Window;
class Head;
class HTML5TokenWrapper;
struct HTML5ParserState;
class HLDocProfile;

/*
 * The LinkFinder searches a document and calls the callback LinkHandler with
 * each inline found in the document.
 *
 * It is however not perfect.  Among other limitations, scripts will not run, so
 * anything loaded from scripts will not be fetched, and no layout will be run,
 * so that elements which normally won't show (display: none) may still be
 * loaded.  This is a design decision to keep the prefetcher fairly lightweight.
 *
 * It assumes that the document and all it's inlines are already loaded and in
 * cache.
 */
class InlineFinder
{
public:
	class LinkHandler
	{
	public:
		virtual ~LinkHandler() {}

		/* Called for most inlines found
		 * @param link_url The URL just found.  Note that you may get a callback
		 *                 for the same URL several times if it appears several times
		 *                 in the source.
		 * @param type The element type the inline was associated with.
		 */
		virtual void LinkFoundL(URL& link_url, Markup::Type type, HTML5TokenWrapper *token) = 0;

		/// Called when there is an entire list of links found at once (mostly from CSS files)
		/// @param link_list Will be a list of URLLink elements with the links
		virtual void LinkListFoundL(Head& link_list) = 0;
	};

	/**
	 * Creates the inline finder.
	 * @param[out] finder The created finder or NULL on OOM.
	 * @param[in] url URL of the document wherein inlines is searched for. It
	 *            must refer to a (X)HTML, SVG or WML document.
	 * @param[in] callback The callback object which is notified each time an
     *            inline is found.
	 * @param[in] only_safe_links If true it only finds inlines that are safe to
	 *            save on disk.
	 * @return Normal OOM values.
	 */
	static OP_STATUS Make(InlineFinder*& finder,  URL& url, LinkHandler* callback, BOOL only_safe_links);

	~InlineFinder();

	/**
	 * Feed data to the finder.
	 *
	 * @param buffer Start of the buffer.
	 * @param length Length of buffer.
	 * @param end_of_data Set to true if this is the last call to this function.
	 */
	void AppendDataL(const uni_char *buffer, unsigned length, BOOL end_of_data);

	/**
	 * Sets the doctype used during scanning for inlines.
	 * @see LogicalDocument::SetDoctype()
	 */
	OP_STATUS SetDoctype(const uni_char *name, const uni_char *pub, const uni_char *sys);

	/**
	 * Sets the base URL.
	 *
	 * @param base_url The URL to use as base.
	 */
	void SetBaseURL(URL base_url) { m_base_url = base_url; }

#ifdef SPECULATIVE_PARSER
	/**
	 * Restores the state of the tokenizer. Used when we start in the middle of the stream.
	 * @param state The new state.
	 * @param buffer_stream_position The position in the stream where this buffer starts.
	 */
	OP_STATUS RestoreTokenizerState(HTML5ParserState* state, unsigned buffer_stream_position);
#endif // SPECULATIVE_PARSER

	/**
	 * Returns value of the current position in the stream. Since this value
	 * combines a non preprocessed position with a preprocessed one it is only
	 * comparable to a position returned from from the same type of function.
	 */
	unsigned 	GetStreamPosition()
	{
		return m_tokenizer.GetLastBufferStartOffset() + m_tokenizer.GetLastBufferPosition();
	}

	/**
	 * Parses the document at the given URL for links.  URL must refer to a
	 * (X)HTML, SVG or WML document.
	 * @param url  The document to search for inlines
	 * @param callback  The callback object which will be notified with each link found, must not be NULL
	 * @param window    Window,  if any, which was used for loading the page
	 * @param only_safe_links    If TRUE then the callback will be called only for URLContentTypes
	 *                           considered safe for the element it is found in.  The inline
	 *                           must already be loaded for URLType to be known, if it's not
	 *                           loaded it will be considered unsafe
	 * @see LinkFinder
	 */
	static void ParseLoadedURLForInlinesL(URL& url, LinkHandler* callback, Window* window, BOOL only_safe_links=FALSE);

	/**
	 * Non leaving version of ParseLoadedURLForInlinesL().
	 */
	static OP_STATUS ParseLoadedURLForInlines(URL& url, LinkHandler* callback, Window* window, BOOL only_safe_links=FALSE)
	{
		TRAPD(status, ParseLoadedURLForInlinesL(url, callback, window, only_safe_links)); return status;
	}

	/**
	 * Start the actual parsing. Leaves with HTML5ParserStatus::NEED_MORE_DATA when 
	 * it runs out of data and expects more.
	 */
	void ParseForInlinesL();

private:
	InlineFinder(URL url, LinkHandler* callback, BOOL only_safe_links);

	HTML5Tokenizer m_tokenizer;
	HTML5TokenWrapper m_token;
	LogicalDocument m_dummy_logdoc;
	DataSrc m_style_src;
	URL m_url;
	URL m_base_url;
	HLDocProfile* m_dummy_profile;
	LinkHandler* m_callback;
	BOOL m_only_safe_links;
	BOOL m_doctype_found;
	BOOL m_in_style;
};

#endif // defined MHTML_ARCHIVE_SAVE_SUPPORT || defined API_LOGDOC_INLINE_FINDER

#endif /*LINK_FINDER_H*/
