/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_HTML_SUPPORT

#include "modules/doc/frm_doc.h"
#include "modules/doc/loadinlineelm.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/logdoc/helistelm.h"

#include "modules/media/mediatrack.h"
#include "modules/media/src/mediasourceutil.h"
#include "modules/media/src/trackloader.h"

TrackLoader::TrackLoader(const URL& url,
						 TrackLoaderListener* listener,
						 MediaTrack* track) :
	m_load_url(url),
	m_url_dd(NULL),
	m_track(track),
	m_listener(listener),
	m_run_heuristics(track->GetScript() == WritingSystem::Unknown)
#ifdef OPERA_CONSOLE
	, m_reported_encoding_error(false)
	, m_reported_errors(0)
#endif // OPERA_CONSOLE
{
	m_parser.SetListener(this);
}

void TrackLoader::HandleFailed(HEListElm* hle)
{
	OP_DELETE(m_url_dd);
	m_url_dd = NULL;

	m_listener->OnTrackLoadError(hle, m_track);
}

void TrackLoader::HandleRedirect(HEListElm* hle)
{
	// If we actually managed to start parsing before the redirect
	// (unlikely) delete the (now old) data descriptor, reset the
	// parser and release any potential results.

	OP_DELETE(m_url_dd);
	m_url_dd = NULL;

	m_parser.Reset();

	m_track->Clear();
}

void TrackLoader::HandleData(HEListElm* hle)
{
	if (m_parser.HasCompleted())
	{
		OP_ASSERT(m_url_dd == NULL);
		return;
	}

	URL track_url = m_load_url.GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);
	if (track_url.IsEmpty())
		track_url = m_load_url;

	if (!m_url_dd)
	{
		// This is the first time we try handling data, so check if
		// the request is successful (e.g. 200 OK for HTTP)
		if (!IsSuccessURL(track_url))
		{
			HandleFailed(hle);
			return;
		}

#ifdef CORS_SUPPORT
		if (LoadInlineElm* lie = hle->GetLoadInlineElm())
			if (lie->IsCrossOriginRequest() && !hle->IsCrossOriginAllowed())
			{
				HandleFailed(hle);
				return;
			}
#endif // CORS_SUPPORT

		unsigned short charset_id = 0;
		OpStatus::Ignore(g_charsetManager->GetCharsetID("utf-8", &charset_id));

		OP_STATUS status = hle->CreateURLDataDescriptor(m_url_dd, g_main_message_handler,
														URL::KFollowRedirect, FALSE, TRUE,
														NULL, URL_TEXT_CONTENT, charset_id);
		if (OpStatus::IsError(status))
		{
			HandleFailed(hle);
			return;
		}

		// Since we force UTF-8, there is little point in trying to
		// "guess" the charset.
		m_url_dd->StopGuessingCharset();
	}

	unsigned long read;
	BOOL more;
	do
	{
		read = m_url_dd->RetrieveData(more);

#ifdef OPERA_CONSOLE
		if (!m_reported_encoding_error)
		{
			int first_invalid_char = m_url_dd->GetFirstInvalidCharacterOffset();
			if (first_invalid_char != -1)
			{
				ConsoleMsg(OpConsoleEngine::Information, UNI_L("Invalid UTF-8 encoding."));

				m_reported_encoding_error = true;
			}
		}
#endif // OPERA_CONSOLE

		const uni_char* buffer = reinterpret_cast<const uni_char*>(m_url_dd->GetBuffer());
		unsigned long buflen = m_url_dd->GetBufSize();

		WebVTT_InputStream istream;
		bool last_buffer = !more && track_url.Status(URL::KNoRedirect) == URL_LOADED;
		istream.SetBuffer(buffer, UNICODE_DOWNSIZE(buflen), last_buffer);

		OP_STATUS status = m_parser.Parse(&istream);
		if (OpStatus::IsError(status))
		{
			HandleFailed(hle);
			more = FALSE;
		}
		else
		{
			unsigned long bytes_to_consume = buflen - UNICODE_SIZE(istream.Remaining());
			if (bytes_to_consume == 0)
			{
				// No bytes were read or the URL is still loading.
				OP_ASSERT(read == 0 || track_url.Status(URL::KNoRedirect) != URL_LOADED);
				break;
			}

			m_url_dd->ConsumeData(bytes_to_consume);
		}
	}
	while (read > 0 && more);

	if (!more && track_url.Status(URL::KNoRedirect) == URL_LOADED)
	{
		if (!m_parser.HasCompleted())
		{
			HandleFailed(hle);
			return;
		}

		m_listener->OnTrackLoaded(hle, m_track);

		OP_DELETE(m_url_dd);
		m_url_dd = NULL;
	}
}

void TrackLoader::OnCueDecoded(MediaTrackCue* cue)
{
	if (m_run_heuristics)
	{
		const StringWithLength& cue_text = cue->GetText();
		m_wsys_heur.Analyze(cue_text.string, cue_text.length);

		WritingSystem::Script guessed = m_wsys_heur.GuessScript();

		if (m_track->GetScript() != guessed)
			m_track->SetScript(guessed);

		m_run_heuristics = !!m_wsys_heur.WantsMoreText();
	}

	OP_STATUS status = m_track->AddParsedCue(cue);
	if (OpStatus::IsError(status))
	{
		RAISE_IF_MEMORY_ERROR(status);
		OP_DELETE(cue);
	}
}

#ifdef OPERA_CONSOLE
static void ConsoleMsgL(OpConsoleEngine::Severity severity, const uni_char* msg,
						unsigned line_no, const uni_char* line, const URL& url,
						bool last_error)
{
	OpConsoleEngine::Message m(OpConsoleEngine::Media, severity);
	ANCHOR(OpConsoleEngine::Message, m);
	url.GetAttributeL(URL::KUniName, m.url);
	m.context.SetL("WebVTT parser");

	OP_STATUS status;
	if (line)
		status = m.message.AppendFormat("%s\n\nLine %u:\n  %s", msg, line_no, line);
	else
		status = m.message.AppendFormat("%s", msg);
	if (OpStatus::IsSuccess(status) && last_error)
		status = m.message.AppendFormat("\n\nNo more errors will be reported for this file.");

	LEAVE_IF_ERROR(status);

	g_console->PostMessageL(&m);
}

void TrackLoader::ConsoleMsg(OpConsoleEngine::Severity severity, const uni_char* msg,
							 unsigned line_no /* = 0 */, const uni_char* line /* = NULL */)
{
	if (g_console->IsLogging() && m_reported_errors < MAX_REPORTED_ERRORS)
	{
		TRAPD(status, ConsoleMsgL(severity, msg, line_no, line, m_load_url,
								  ++m_reported_errors == MAX_REPORTED_ERRORS));
		RAISE_IF_MEMORY_ERROR(status);
	}
}
#endif // OPERA_CONSOLE

void TrackLoader::OnParseError(const WebVTT_Parser::ParseError& error)
{
#ifdef OPERA_CONSOLE
	OpConsoleEngine::Severity severity;
	switch (error.severity)
	{
	case WebVTT_Parser::Fatal:
		severity = OpConsoleEngine::Error;
		break;
	default:
	case WebVTT_Parser::Informational:
		severity = OpConsoleEngine::Information;
		break;
	}

	ConsoleMsg(severity, error.message, error.line_no, error.line);
#endif // OPERA_CONSOLE
}

#endif // MEDIA_HTML_SUPPORT
