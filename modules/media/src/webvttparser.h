/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef WEBVTT_PARSER_H
#define WEBVTT_PARSER_H

#include "modules/util/tempbuf.h"

enum WebVTT_NodeType
{
	WVTT_ROOT,
	WVTT_C,
	WVTT_I,
	WVTT_B,
	WVTT_U,
	WVTT_RUBY,
	WVTT_RT,
	WVTT_V,
	WVTT_TEXT,
	WVTT_TIMESTAMP,

	WVTT_UNKNOWN /* Keep this last */
};

class WVTT_Node
{
	friend class WVTT_TreeBuilder;
public:
	WVTT_Node() :
		m_type(WVTT_UNKNOWN),
		m_parent(NULL),
		m_suc(NULL),
		m_firstchild(NULL) {}
	~WVTT_Node();

	WebVTT_NodeType Type() const { return m_type; }

	WVTT_Node* Parent() const { return m_parent; }
	WVTT_Node* FirstChild() const { return m_firstchild; }
	WVTT_Node* Suc() const { return m_suc; }
	WVTT_Node* Next() const;
	WVTT_Node* NextSibling() const;

	const uni_char* GetText() const { return m_data.text; }
	double GetTimestamp() const { return m_data.timestamp; }
	const uni_char* GetClass() const { return m_data.attrs.classes; }
	const uni_char* GetAnnotation() const { return m_data.attrs.annotation; }

	OP_STATUS SetAnnotation(const uni_char* annotation, unsigned annotation_len)
	{
		return UniSetStrN(m_data.attrs.annotation, annotation, annotation_len);
	}

	OP_STATUS Construct(WebVTT_NodeType t, const uni_char* classes = NULL, unsigned classes_len = 0);
	OP_STATUS Construct(const uni_char* s, unsigned slen);
	void Construct(double ts);

	/** Clone to a HTML_Element. Used by getCueAsHTML. */
	HTML_Element* CloneForDOM(HLDocProfile* hld_profile, int ns_idx);

	/** Clone an entire subtree to a HTML_Element fragment.
	 *
	 * Create a HTML_Element subtree rooted at this node and attach it
	 * to the 'root' HTML_Element passed. If this node is a WVTT_ROOT,
	 * a new HTML_Element will not be created, but instead the 'root'
	 * HTML_Element will be used.
	 *
	 * Passing NS_IDX_HTML will yield a subtree suitable for the
	 * getCueAsHTML call, while NS_IDX_CUE will yield one suitable for
	 * rendering.
	 */
	OP_STATUS CloneSubtreeForDOM(HLDocProfile* hld_profile, HTML_Element* root,
								 int ns_idx);

private:
	bool HasClass() const
	{
		return m_type == WVTT_C ||
			m_type == WVTT_I ||
			m_type == WVTT_B ||
			m_type == WVTT_U ||
			m_type == WVTT_RUBY ||
			m_type == WVTT_RT ||
			m_type == WVTT_V;
	}
	bool HasAnnotation() const
	{
		return m_type == WVTT_V;
	}

	union
	{
		double timestamp;
		uni_char* text;
		struct
		{
			uni_char* classes;
			uni_char* annotation;
		} attrs;
	} m_data;
	WebVTT_NodeType m_type;

	WVTT_Node* m_parent;
	WVTT_Node* m_suc;
	WVTT_Node* m_firstchild;
};

class WebVTT_InputStream
{
public:
	WebVTT_InputStream() :
		m_buffer(NULL), m_length(0), m_last_buffer(false) {}

	void SetBuffer(const uni_char* b, unsigned l, bool lb)
	{
		m_buffer = b;
		m_length = l;
		m_last_buffer = lb;
	}

	void Shift() { m_buffer++, m_length--; }
	uni_char GetChar() const { return *m_buffer; }

	/** Collect a line, not including the end-of-line characters.
	 *
	 * @return IS_FALSE if the input drained and it wasn't because of
	 * EOF, otherwise IS_TRUE or ERR_NO_MEMORY.
	 */
	OP_BOOLEAN CollectLine(TempBuffer& line);

	enum EOLStatus
	{
		NEED_DATA,		/**< Ran out of data trying to consume the EOL. */
		CONSUMED_EOL,	/**< Consumed a EOL. */
		NOT_EOL			/**< There was data, but we are not looking at a EOL. */
	};

	/** Consume end-of-line characters.
	 *
	 * @return status according to EOLStatus.
	 */
	EOLStatus ConsumeEOL();

	unsigned Remaining() const { return m_length; }

	bool IsEOF() const { return m_last_buffer && m_length == 0; }

private:
	const uni_char* m_buffer;
	unsigned m_length;
	bool m_last_buffer;
};

class MediaTrackCue;
class WebVTT_ParserListener;
class WebVTT_Buffer;

class WebVTT_Parser
{
public:
	WebVTT_Parser() :
		m_line_no(1),
		m_curr_cue(NULL),
		m_prev_start_ts(0),
		m_parse_state(STATE_OPTIONAL_BOM),
		m_listener(NULL) {}
	~WebVTT_Parser() { DiscardCurrentCue(); }

	void SetListener(WebVTT_ParserListener* l) { m_listener = l; }

	OP_STATUS Parse(WebVTT_InputStream* is);

	void Reset()
	{
		m_line_no = 1;
		DiscardCurrentCue();
		m_prev_start_ts = 0;
		m_parse_state = STATE_OPTIONAL_BOM;
	}

	bool HasCompleted() const { return m_parse_state == STATE_END; }

	WVTT_Node* ParseCueText(const uni_char* cue_text, unsigned cue_text_len);
	void ParseSettings(const uni_char* settings, unsigned settings_len, MediaTrackCue* cue);

	enum ErrorSeverity
	{
		Fatal,
		Informational
	};

	struct ParseError
	{
		ParseError(ErrorSeverity s,
				   const uni_char* msg,
				   const uni_char* l, unsigned lno)
			: message(msg), line(l), line_no(lno), severity(s) {}

		const uni_char* message;
		const uni_char* line;
		unsigned line_no;
		ErrorSeverity severity;
	};

private:
	OP_BOOLEAN CollectLine(WebVTT_InputStream* is);

	/** Consume a EOL if we are current looking at one.
	 *
	 * @return false if more data is needed, otherwise true.
	 *         Note that a true result does not mean a EOL was
	 *         actually consumed.
	 */
	bool ConsumeEOL(WebVTT_InputStream* is);

	void DiscardCurrentCue();
	void ResetTokenizer()
	{
		m_token_state = WVTT_DATA_STATE;
		m_token_result.Clear();
		m_annotation.Clear();
		m_classes.Clear();
	}

	bool ParseTimingsAndSettings(const TempBuffer& line);
	bool ParseTimestamp(WebVTT_Buffer& b, double& ts);
	void ParseSettings(WebVTT_Buffer& b);

	int GetCueToken(WebVTT_Buffer& b);

	WVTT_Node* CreateNode(WebVTT_NodeType node_type);
	void AddClass(TempBuffer& buffer);
	void SetAnnotation(TempBuffer& buffer);

	TempBuffer m_line;
	unsigned m_line_no;
	bool m_line_already_collected;
	TempBuffer m_cue_text;

	MediaTrackCue* m_curr_cue;
	double m_prev_start_ts; /* Start timestamp of previous cue. */

	enum ParseState
	{
		STATE_COLLECT_LINE,
		STATE_OPTIONAL_BOM,
		STATE_MAGIC,
		STATE_MAGIC_SKIP_NL,
		STATE_SKIP_HEADER,
		STATE_CUE_LOOP,
		STATE_CREATE_CUE,
		STATE_TIMINGS_OR_IDENTIFIER,
		STATE_TIMINGS_SKIP_NL,
		STATE_TIMINGS,
		STATE_CUE_TEXT,
		STATE_CUE_TEXT_LOOP,
		STATE_CUE_TEXT_PROC,
		STATE_BAD_CUE,
		STATE_BAD_CUE_LOOP,
		STATE_BAD_CUE_SKIP_LINE,
		STATE_END
	};
	ParseState m_parse_state;
	ParseState m_restart_state;

	TempBuffer m_token_result;
	TempBuffer m_annotation;
	TempBuffer m_classes;

	enum
	{
		WVTT_DATA_STATE,
		WVTT_ESC_STATE,
		WVTT_TAG_STATE,
		WVTT_START_TAG_STATE,
		WVTT_START_TAG_CLASS_STATE,
		WVTT_START_TAG_ANNOTATE_STATE,
		WVTT_END_TAG_STATE,
		WVTT_TIMESTAMP_TAG_STATE
	} m_token_state;

	WebVTT_ParserListener* m_listener;
};

class WebVTT_ParserListener
{
public:
	virtual void OnCueDecoded(MediaTrackCue* cue) = 0;
	virtual void OnParseError(const WebVTT_Parser::ParseError& error) = 0;
	virtual void OnParsingComplete() = 0;
};

#endif // WEBVTT_PARSER_H
