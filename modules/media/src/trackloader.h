/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef TRACKLOADER_H
#define TRACKLOADER_H

#ifdef MEDIA_HTML_SUPPORT

#include "modules/console/opconsoleengine.h"
#include "modules/display/writingsystemheuristic.h"
#include "modules/logdoc/helistelm.h"

#include "modules/media/src/webvttparser.h"

class MediaTrack;

class TrackLoaderListener
{
public:
	virtual void OnTrackLoaded(HEListElm* hle, MediaTrack* track) = 0;
	virtual void OnTrackLoadError(HEListElm* hle, MediaTrack* track) = 0;
};

class TrackLoader :
	public WebVTT_ParserListener
{
public:
	TrackLoader(const URL& url,
				TrackLoaderListener* listener,
				MediaTrack* track);
	virtual ~TrackLoader() { OP_DELETE(m_url_dd); }

	URL* GetURL() { return &m_load_url; }

	// WebVTT_ParserListener
	void OnCueDecoded(MediaTrackCue* cue);
	void OnParseError(const WebVTT_Parser::ParseError& error);
	void OnParsingComplete() {}

	void HandleData(HEListElm* hle);
	void HandleRedirect(HEListElm* hle);

private:
	void HandleFailed(HEListElm* hle);
#ifdef OPERA_CONSOLE
	void ConsoleMsg(OpConsoleEngine::Severity severity, const uni_char* msg,
					unsigned line_no = 0, const uni_char* line = NULL);
#endif // OPERA_CONSOLE

	WebVTT_Parser m_parser;

	URL m_load_url;
	URL_DataDescriptor* m_url_dd;
	MediaTrack* m_track;
	TrackLoaderListener* m_listener;

	WritingSystemHeuristic m_wsys_heur;
	bool m_run_heuristics;
#ifdef OPERA_CONSOLE
	bool m_reported_encoding_error;
	unsigned m_reported_errors;
	static const unsigned MAX_REPORTED_ERRORS = 10;
#endif // OPERA_CONSOLE
};

#endif // MEDIA_HTML_SUPPORT
#endif // !TRACKLOADER_H
