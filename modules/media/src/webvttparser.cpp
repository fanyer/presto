/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_HTML_SUPPORT

/*
 * Implementation of parsing algorithm for WebVTT
 *
 * http://dev.w3.org/html5/webvtt/#parsing
 */

#include "modules/logdoc/htm_elm.h"

#include "modules/media/src/webvttparser.h"
#include "modules/media/mediatrack.h"

class WebVTT_Buffer
{
public:
	WebVTT_Buffer(const uni_char* s, unsigned len)
	{
		m_state.buf = s;
		m_state.len = len;
	}
	WebVTT_Buffer(const TempBuffer& tb)
	{
		m_state.buf = tb.GetStorage();
		m_state.len = tb.Length();
	}

	uni_char CurrentChar() const { return *m_state.buf; }
	bool IsEmpty() const { return m_state.len == 0; }

	bool Scan(uni_char c)
	{
		if (*m_state.buf != c)
			return false;

		Shift();
		return true;
	}
	bool Scan(const char* s, unsigned slen);

	void Shift() { m_state.buf++, m_state.len--; }

	void ScanWS();
	void ScanNonWS();

	unsigned ScanUInt(unsigned& value);

	struct State
	{
		const uni_char* buf;
		unsigned len;
	};

	const State& Mark() { return m_state; }
	const State& Save() { return m_state; }
	void Restore(const State& s) { m_state = s; }

	bool MatchFromMark(const State& mark, const char* s, unsigned slen);

private:
	State m_state;
};

bool WebVTT_Buffer::Scan(const char* s, unsigned slen)
{
	if (m_state.len < slen)
		return false;

	while (*s)
	{
		if (*m_state.buf != *s)
			return false;

		s++;
		Shift();
	}

	return true;
}

bool WebVTT_Buffer::MatchFromMark(const State& mark, const char* s, unsigned slen)
{
	unsigned d = m_state.buf - mark.buf;
	if (slen != d)
		return false;

	return uni_strncmp(mark.buf, s, slen) == 0;
}

static inline bool IsWhitespace(uni_char c)
{
	// Valid whitespace characters except for CR since it is
	// normalized away early (WebVTT parser algorithm, step 1).
	return c == 0x20 || c == 0x09 || c == 0x0c || c == 0x0a;
}

void WebVTT_Buffer::ScanWS()
{
	while (m_state.len)
	{
		if (!IsWhitespace(*m_state.buf))
			break;

		Shift();
	}
}

void WebVTT_Buffer::ScanNonWS()
{
	while (m_state.len)
	{
		if (IsWhitespace(*m_state.buf))
			break;

		Shift();
	}
}

static inline bool IsDecDigit(uni_char c)
{
	return c >= '0' && c <= '9';
}

unsigned WebVTT_Buffer::ScanUInt(unsigned& value)
{
	unsigned count = 0;

	value = 0;

	while (m_state.len)
	{
		uni_char c = *m_state.buf;

		if (!IsDecDigit(c))
			break;

		if (value < UINT_MAX / 10)
			value = value * 10 + (c - '0');
		else
			value = UINT_MAX;

		Shift();
		count++;
	}

	return count;
}

OP_BOOLEAN WebVTT_InputStream::CollectLine(TempBuffer& line)
{
	unsigned len = m_length;
	const uni_char* buf = m_buffer;

	while (len)
	{
		uni_char c = *buf;
		if (c == 0x0d || c == 0x0a)
			break;

		if (c == 0x00)
		{
			// Flush what we currently have collected.
			RETURN_IF_ERROR(line.Append(m_buffer, buf - m_buffer));

			// Replace with "replacement character".
			RETURN_IF_ERROR(line.Append(0xFFFD));

			// Consume and update state.
			m_buffer = ++buf;
			m_length = --len;
		}
		else
		{
			++buf;
			--len;
		}
	}

	if (buf > m_buffer)
	{
		RETURN_IF_ERROR(line.Append(m_buffer, buf - m_buffer));

		m_buffer = buf;
		m_length = len;
	}

	if (m_length != 0 || m_last_buffer)
		return OpBoolean::IS_TRUE;

	return OpBoolean::IS_FALSE;
}

WebVTT_InputStream::EOLStatus WebVTT_InputStream::ConsumeEOL()
{
	unsigned len = m_length;
	const uni_char* buf = m_buffer;

	if (len && *buf == 0x0d)
		buf++, len--;

	if (len == 0)
	{
		if (!m_last_buffer)
			return NEED_DATA;
	}
	else if (*buf == 0x0a)
		buf++, len--;

	if (m_buffer != buf)
	{
		m_buffer = buf;
		m_length = len;
		return CONSUMED_EOL;
	}

	return NOT_EOL;
}

OP_BOOLEAN WebVTT_Parser::CollectLine(WebVTT_InputStream* is)
{
	m_line.Clear();

	OP_BOOLEAN success = is->CollectLine(m_line);
	if (success == OpBoolean::IS_FALSE)
	{
		// If the input stream drained and it wasn't because of EOF,
		// set restart state and wait for more data.
		m_restart_state = m_parse_state;
		m_parse_state = STATE_COLLECT_LINE;
	}

	return success;
}

bool WebVTT_Parser::ConsumeEOL(WebVTT_InputStream* is)
{
	WebVTT_InputStream::EOLStatus eol_status = is->ConsumeEOL();
	if (eol_status == WebVTT_InputStream::CONSUMED_EOL)
		m_line_no++;

	return (eol_status == WebVTT_InputStream::CONSUMED_EOL ||
			eol_status == WebVTT_InputStream::NOT_EOL);
}

// http://dev.w3.org/html5/webvtt/#collect-a-webvtt-timestamp
bool WebVTT_Parser::ParseTimestamp(WebVTT_Buffer& b, double& ts)
{
	if (b.IsEmpty())
		return false;

	unsigned value1, value2, value3, value4;
	unsigned num_digits;

	num_digits = b.ScanUInt(value1);
	if (num_digits == 0)
		return false;

	bool most_sign_units_is_hours = (num_digits != 2 || value1 > 59);

	if (b.IsEmpty() || !b.Scan(':'))
		return false;

	num_digits = b.ScanUInt(value2);
	if (num_digits != 2)
		return false;

	if (most_sign_units_is_hours || !b.IsEmpty() && b.CurrentChar() == ':')
	{
		if (b.IsEmpty() || !b.Scan(':'))
			return false;

		num_digits = b.ScanUInt(value3);
		if (num_digits != 2)
			return false;
	}
	else
	{
		value3 = value2;
		value2 = value1;
		value1 = 0;
	}

	if (b.IsEmpty() || !b.Scan('.'))
		return false;

	num_digits = b.ScanUInt(value4);
	if (num_digits != 3)
		return false;

	if (value2 > 59 || value3 > 59)
		return false;

	ts = value1 * 60.0 * 60.0;
	ts += value2 * 60.0;
	ts += value3;
	ts += value4 / 1000.0;
	return true;
}

void WebVTT_Parser::ParseSettings(const uni_char* settings, unsigned settings_len, MediaTrackCue* cue)
{
	OP_ASSERT(m_curr_cue == NULL);
	m_curr_cue = cue;

	WebVTT_Buffer b(settings, settings_len);
	ParseSettings(b);

	m_curr_cue = NULL;
}

// http://dev.w3.org/html5/webvtt/#parse-the-webvtt-settings
void WebVTT_Parser::ParseSettings(WebVTT_Buffer& b)
{
	while (!b.IsEmpty())
	{
		b.ScanWS();

		if (b.IsEmpty())
			break;

		uni_char c = b.CurrentChar();
		b.Shift();

		bool is_position = false;
		switch (c)
		{
		case 'v':
			if (b.Scan("ertical:", 8))
			{
				WebVTT_Buffer::State mark = b.Mark();

				b.ScanNonWS();

				if (b.MatchFromMark(mark, "rl", 2))
					m_curr_cue->SetDirection(CUE_DIRECTION_VERTICAL);
				else if (b.MatchFromMark(mark, "lr", 2))
					m_curr_cue->SetDirection(CUE_DIRECTION_VERTICAL_LR);
			}
			break;

		case 'l':
			if (b.Scan("ine:", 4))
			{
				bool is_negative = b.Scan('-');
				unsigned line_pos;
				unsigned num_digits = b.ScanUInt(line_pos);
				bool is_percentage = b.Scan('%');

				if (!b.IsEmpty() && !IsWhitespace(b.CurrentChar()))
					goto otherwise;

				if (num_digits == 0 || is_percentage && is_negative)
					break;

				if (is_percentage && line_pos > 100)
					break;

				// Clamp the range of linePosition to [INT_MIN, INT_MAX].
				int signed_line_pos;
				if (line_pos > INT_MAX)
					signed_line_pos = is_negative ? INT_MIN : INT_MAX;
				else
				{
					signed_line_pos = line_pos;
					if (is_negative)
						signed_line_pos = -signed_line_pos;
				}

				m_curr_cue->SetSnapToLines(!is_percentage);
				m_curr_cue->SetLinePosition(signed_line_pos);
			}
			break;

		case 'p':
			is_position = b.Scan("osition:", 8);
			/* fall through */
		case 's':
			if (is_position || c == 's' && b.Scan("ize:", 4))
			{
				unsigned num_value;
				unsigned num_digits = b.ScanUInt(num_value);

				if (b.IsEmpty())
					break;

				if (!b.Scan('%'))
					goto otherwise;

				if (!b.IsEmpty() && !IsWhitespace(b.CurrentChar()))
					goto otherwise;

				if (num_digits == 0)
					break;

				if (num_value > 100)
					break;

				if (is_position)
					m_curr_cue->SetTextPosition(num_value);
				else
					m_curr_cue->SetSize(num_value);
			}
			break;

		case 'a':
			if (b.Scan("lign:", 5))
			{
				WebVTT_Buffer::State mark = b.Mark();

				b.ScanNonWS();

				if (b.MatchFromMark(mark, "start", 5))
					m_curr_cue->SetAlignment(CUE_ALIGNMENT_START);
				else if (b.MatchFromMark(mark, "middle", 6))
					m_curr_cue->SetAlignment(CUE_ALIGNMENT_MIDDLE);
				else if (b.MatchFromMark(mark, "end", 3))
					m_curr_cue->SetAlignment(CUE_ALIGNMENT_END);
			}
			break;

		otherwise:
		default:
			b.ScanNonWS();
			break;
		}
	}
}

// http://dev.w3.org/html5/webvtt/#collect-webvtt-cue-timings-and-settings
bool WebVTT_Parser::ParseTimingsAndSettings(const TempBuffer& line)
{
	WebVTT_Buffer b(line);

	// Step 3
	b.ScanWS();

	// Step 4
	double start_ts;
	if (!ParseTimestamp(b, start_ts))
		return false;

	m_curr_cue->SetStartTime(start_ts);

	// Step 5
	b.ScanWS();

	// Steps 6-8
	if (!b.Scan("-->", 3))
		return false;

	// Step 9
	b.ScanWS();

	// Step 10
	double end_ts;
	if (!ParseTimestamp(b, end_ts))
		return false;

	m_curr_cue->SetEndTime(end_ts);

	// Step 11
	ParseSettings(b);

	return true;
}

enum
{
	WVTT_TOK_STRING,
	WVTT_TOK_START_TAG,
	WVTT_TOK_END_TAG,
	WVTT_TOK_TIMESTAMP_TAG
};

void WebVTT_Parser::AddClass(TempBuffer& buffer)
{
	size_t class_len = buffer.Length();
	if (class_len == 0)
		return;

	if (m_classes.Length() != 0)
		m_classes.Append(' ');

	m_classes.Append(buffer.GetStorage(), class_len);
}

// http://dev.w3.org/html5/webvtt/#webvtt-start-tag-annotation-state
void WebVTT_Parser::SetAnnotation(TempBuffer& buffer)
{
	// Skip leading and trailing whitespace and replace consecutive
	// runs of whitespace with a single ' ' (0x20).
	WebVTT_Buffer b(buffer);

	b.ScanWS();

	while (!b.IsEmpty())
	{
		uni_char c = b.CurrentChar();

		b.Shift();

		if (IsWhitespace(c))
		{
			b.ScanWS();

			if (b.IsEmpty())
				break;

			c = ' ';
		}

		m_annotation.Append(c);
	}
}

// http://dev.w3.org/html5/webvtt/#webvtt-cue-text-tokenizer
int WebVTT_Parser::GetCueToken(WebVTT_Buffer& b)
{
	TempBuffer buffer;

	ResetTokenizer();

	while (true)
	{
		switch (m_token_state)
		{
		case WVTT_DATA_STATE:
			{
				if (b.IsEmpty())
					return WVTT_TOK_STRING;

				uni_char c = b.CurrentChar();
				switch (c)
				{
				case '&':
					buffer.Clear();
					buffer.Append(c);
					m_token_state = WVTT_ESC_STATE;
					break;

				case '<':
					{
						if (m_token_result.Length() == 0)
							m_token_state = WVTT_TAG_STATE;
						else
							return WVTT_TOK_STRING;
					}
					break;

				default:
					m_token_result.Append(c);
					break;
				}
			}
			break;

		case WVTT_ESC_STATE:
			{
				if (b.IsEmpty() || b.CurrentChar() == '<')
				{
					m_token_result.Append(buffer.GetStorage(), buffer.Length());
					return WVTT_TOK_STRING;
				}

				uni_char c = b.CurrentChar();
				if (c == ';')
				{
					const uni_char* b = buffer.GetStorage();
					unsigned blen = buffer.Length();

					if (blen == 4 && uni_strncmp(b, "&amp", 4) == 0)
						m_token_result.Append('&');
					else if (blen == 3 && uni_strncmp(b, "&lt", 3) == 0)
						m_token_result.Append('<');
					else if (blen == 3 && uni_strncmp(b, "&gt", 3) == 0)
						m_token_result.Append('>');
					else if (blen == 4 && uni_strncmp(b, "&lrm", 4) == 0)
						m_token_result.Append(0x200e);
					else if (blen == 4 && uni_strncmp(b, "&rlm", 4) == 0)
						m_token_result.Append(0x200f);
					else if (blen == 5 && uni_strncmp(b, "&nbsp", 5) == 0)
						m_token_result.Append(0x00a0);
					else
					{
						m_token_result.Append(b, blen);
						m_token_result.Append(';');
					}

					m_token_state = WVTT_DATA_STATE;
				}
				else if (c >= 'a' && c <= 'z' ||
						 c >= 'A' && c <= 'Z' ||
						 c >= '0' && c <= '9')
				{
					buffer.Append(c);
				}
				else
				{
					m_token_result.Append(buffer.GetStorage(), buffer.Length());
					buffer.Clear();

					if (c == '&')
					{
						buffer.Append(c);
					}
					else
					{
						m_token_result.Append(c);
						m_token_state = WVTT_DATA_STATE;
					}
				}
			}
			break;

		case WVTT_TAG_STATE:
			{
				OP_ASSERT(m_token_result.Length() == 0);

				if (b.IsEmpty())
					// Empty start tag
					return WVTT_TOK_START_TAG;

				uni_char c = b.CurrentChar();
				if (IsWhitespace(c))
					m_token_state = WVTT_START_TAG_ANNOTATE_STATE;
				else if (c == '.')
					m_token_state = WVTT_START_TAG_CLASS_STATE;
				else if (c == '/')
					m_token_state = WVTT_END_TAG_STATE;
				else if (c >= '0' && c <= '9')
				{
					m_token_result.Append(c);
					m_token_state = WVTT_TIMESTAMP_TAG_STATE;
				}
				else if (b.Scan('>'))
				{
					// Empty start tag
					return WVTT_TOK_START_TAG;
				}
				else
				{
					m_token_result.Append(c);
					m_token_state = WVTT_START_TAG_STATE;
				}
			}
			break;

		case WVTT_START_TAG_STATE:
			{
				if (b.IsEmpty())
					return WVTT_TOK_START_TAG;

				OP_ASSERT(buffer.Length() == 0);

				uni_char c = b.CurrentChar();
				if (IsWhitespace(c))
				{
					if (c == '\n')
						buffer.Append(c);

					m_token_state = WVTT_START_TAG_ANNOTATE_STATE;
				}
				else if (c == '.')
					m_token_state = WVTT_START_TAG_CLASS_STATE;
				else if (b.Scan('>'))
					return WVTT_TOK_START_TAG;
				else
					m_token_result.Append(c);
			}
			break;

		case WVTT_START_TAG_CLASS_STATE:
			{
				if (b.IsEmpty())
				{
					AddClass(buffer);
					return WVTT_TOK_START_TAG;
				}

				uni_char c = b.CurrentChar();
				if (IsWhitespace(c))
				{
					AddClass(buffer);
					buffer.Clear();
					if (c == '\n')
						buffer.Append(c);

					m_token_state = WVTT_START_TAG_ANNOTATE_STATE;
				}
				else if (c == '.')
				{
					AddClass(buffer);
					buffer.Clear();
				}
				else if (b.Scan('>'))
				{
					AddClass(buffer);
					return WVTT_TOK_START_TAG;
				}
				else
				{
					buffer.Append(c);
				}
			}
			break;

		case WVTT_START_TAG_ANNOTATE_STATE:
			{
				if (b.IsEmpty() || b.Scan('>'))
				{
					SetAnnotation(buffer);
					return WVTT_TOK_START_TAG;
				}

				buffer.Append(b.CurrentChar());
			}
			break;

		case WVTT_END_TAG_STATE:
		case WVTT_TIMESTAMP_TAG_STATE:
			{
				if (b.IsEmpty() || b.Scan('>'))
				{
					if (m_token_state == WVTT_END_TAG_STATE)
						return WVTT_TOK_END_TAG;
					else
						return WVTT_TOK_TIMESTAMP_TAG;
				}

				m_token_result.Append(b.CurrentChar());
			}
			break;
		}

		b.Shift();
	}
}

WVTT_Node::~WVTT_Node()
{
	WVTT_Node* child = FirstChild();
	while (child)
	{
		WVTT_Node* next = child->Suc();
		OP_DELETE(child);
		child = next;
	}

	if (m_type == WVTT_TEXT)
		OP_DELETEA(m_data.text);
	else if (m_type == WVTT_TIMESTAMP)
		/* nothing to delete */;
	else if (m_type != WVTT_UNKNOWN)
	{
		OP_DELETEA(m_data.attrs.classes);
		OP_DELETEA(m_data.attrs.annotation);
	}
}

OP_STATUS
WVTT_Node::Construct(WebVTT_NodeType t, const uni_char* classes, unsigned classes_len)
{
	m_type = t;

	m_data.attrs.classes = NULL;
	m_data.attrs.annotation = NULL;

	return UniSetStrN(m_data.attrs.classes, classes, classes_len);
}

OP_STATUS
WVTT_Node::Construct(const uni_char* s, unsigned slen)
{
	m_type = WVTT_TEXT;
	m_data.text = NULL;

	return UniSetStrN(m_data.text, s, slen);
}

void
WVTT_Node::Construct(double ts)
{
	m_type = WVTT_TIMESTAMP;
	m_data.timestamp = ts;
}

WVTT_Node*
WVTT_Node::Next() const
{
	if (FirstChild())
		return FirstChild();

	for (const WVTT_Node* leaf = this; leaf; leaf = leaf->Parent())
		if (leaf->Suc())
			return leaf->Suc();

	return NULL;
}

WVTT_Node*
WVTT_Node::NextSibling() const
{
	for (const WVTT_Node* leaf = this; leaf; leaf = leaf->Parent())
		if (leaf->Suc())
			return leaf->Suc();

	return NULL;
}

static HTML_ElementType WVTTNodeTypeToDOMHTMLType(WebVTT_NodeType node_type)
{
	switch (node_type)
	{
	case WVTT_C:
		return Markup::HTE_SPAN;
	case WVTT_I:
		return Markup::HTE_I;
	case WVTT_B:
		return Markup::HTE_B;
	case WVTT_U:
		return Markup::HTE_U;
	case WVTT_V:
		return Markup::HTE_SPAN;
	case WVTT_TEXT:
		return Markup::HTE_TEXT;
	case WVTT_TIMESTAMP:
		return Markup::HTE_PROCINST;
	default:
		OP_ASSERT(!"Unmappable nodetype");
	case WVTT_RUBY:
	case WVTT_RT:
		return Markup::HTE_UNKNOWN;
	}
}

static HTML_ElementType WVTTNodeTypeToRenderingType(WebVTT_NodeType node_type)
{
	switch (node_type)
	{
	case WVTT_C:
		return Markup::CUEE_C;
	case WVTT_I:
		return Markup::CUEE_I;
	case WVTT_B:
		return Markup::CUEE_B;
	case WVTT_U:
		return Markup::CUEE_U;
	case WVTT_V:
		return Markup::CUEE_V;
	case WVTT_TEXT:
		return Markup::HTE_TEXT;

	default:
		// Timestamps should not be included in the rendering fragment.
	case WVTT_TIMESTAMP:
		OP_ASSERT(!"Unmappable nodetype");
	case WVTT_RUBY:
	case WVTT_RT:
		return Markup::HTE_UNKNOWN;
	}
}

static OP_STATUS
SerializeTimestamp(double timestamp, TempBuffer& buffer)
{
	unsigned hours = op_double2uint32(timestamp / 3600.0);
	unsigned remainder = static_cast<unsigned>(1000.0 * op_fmod(timestamp, 3600.0));
	unsigned millis = remainder % 1000;
	remainder /= 1000;
	unsigned minutes = remainder / 60;
	unsigned seconds = remainder % 60;

	return buffer.AppendFormat(UNI_L("%02u:%02u:%02u.%03u"), hours, minutes, seconds, millis);
}

HTML_Element*
WVTT_Node::CloneForDOM(HLDocProfile* hld_profile, int ns_idx)
{
	// 'class', 'title', potential XML-name + terminator
	HtmlAttrEntry attr_list[4];
	HTML_ElementType html_type = ns_idx != NS_IDX_HTML ?
		WVTTNodeTypeToRenderingType(m_type) :
		WVTTNodeTypeToDOMHTMLType(m_type);

	// Map localName that we lack HTML_ElementTypes for.
	const uni_char* name = NULL;
	if (m_type == WVTT_RT)
		name = UNI_L("rt");
	else if (m_type == WVTT_RUBY)
		name = UNI_L("ruby");

	LogdocXmlName elm_name;

	int idx = 0;

	if (html_type == Markup::HTE_UNKNOWN && name)
	{
		attr_list[idx].ns_idx = SpecialNs::NS_LOGDOC;
		attr_list[idx].attr = Markup::LOGA_XML_NAME;
		attr_list[idx].is_id = FALSE;
		attr_list[idx].is_special = TRUE;
		attr_list[idx].is_specified = FALSE;
		RETURN_VALUE_IF_ERROR(elm_name.SetName(name, uni_strlen(name), FALSE), NULL);
		attr_list[idx].value = reinterpret_cast<uni_char*>(&elm_name);
		attr_list[idx].value_len = 0;
		idx++;
	}

	TempBuffer ts_buffer;
	if (m_type == WVTT_TIMESTAMP)
	{
		RETURN_VALUE_IF_ERROR(SerializeTimestamp(GetTimestamp(), ts_buffer), NULL);

		// target=timestamp
		attr_list[idx].attr = Markup::HA_TARGET;
		attr_list[idx].ns_idx = NS_IDX_DEFAULT;
		attr_list[idx].value = UNI_L("timestamp");
		attr_list[idx].value_len = 9;
		idx++;

		// data=HH+:MM:SS.mmm
		attr_list[idx].attr = Markup::HA_CONTENT;
		attr_list[idx].ns_idx = NS_IDX_DEFAULT;
		attr_list[idx].value = ts_buffer.GetStorage();
		attr_list[idx].value_len = ts_buffer.Length();
		idx++;
	}

	if (HasClass() && m_data.attrs.classes)
	{
		attr_list[idx].attr = Markup::HA_CLASS;
		attr_list[idx].ns_idx = ns_idx != NS_IDX_HTML ? NS_IDX_HTML : NS_IDX_DEFAULT;
		attr_list[idx].value = m_data.attrs.classes;
		attr_list[idx].value_len = uni_strlen(m_data.attrs.classes);
		idx++;
	}

	if (HasAnnotation())
	{
		const uni_char* annotation = m_data.attrs.annotation;
		if (!annotation)
			annotation = UNI_L("");

		if (ns_idx != NS_IDX_HTML)
		{
			attr_list[idx].attr = Markup::HA_XML;
			attr_list[idx].name = UNI_L("voice");
			attr_list[idx].name_len = 5;
		}
		else
		{
			attr_list[idx].attr = Markup::HA_TITLE;
		}
		attr_list[idx].value = annotation;
		attr_list[idx].value_len = uni_strlen(annotation);
		idx++;
	}

	HTML_Element* element = NEW_HTML_Element();
	if (!element)
		return NULL;

	OP_STATUS status;
	if (html_type == Markup::HTE_TEXT)
	{
		OP_ASSERT(idx == 0);

		status = element->Construct(hld_profile, m_data.text,
									m_data.text ? uni_strlen(m_data.text) : 0);
	}
	else
	{
		OP_ASSERT(idx <= 3);

		status = element->Construct(hld_profile, ns_idx, html_type, attr_list);
	}

	if (OpStatus::IsError(status))
	{
		DELETE_HTML_Element(element);
		element = NULL;
	}
	return element;
}

OP_STATUS
WVTT_Node::CloneSubtreeForDOM(HLDocProfile* hld_profile, HTML_Element* root,
							  int ns_idx)
{
	HTML_Element* element = NULL;

	if (m_type != WVTT_ROOT)
	{
		// Don't include timestamp elements in the rendering tree.
		if (ns_idx != NS_IDX_HTML && m_type == WVTT_TIMESTAMP)
			return OpStatus::OK;

		element = CloneForDOM(hld_profile, ns_idx);
		if (!element)
			return OpStatus::ERR_NO_MEMORY;

		element->Under(root);
	}
	else
	{
		element = root;
	}

	for (WVTT_Node* child = FirstChild(); child; child = child->Suc())
		RETURN_IF_ERROR(child->CloneSubtreeForDOM(hld_profile, element, ns_idx));

	return OpStatus::OK;
}

WVTT_Node*
WebVTT_Parser::CreateNode(WebVTT_NodeType node_type)
{
	WVTT_Node* node = OP_NEW(WVTT_Node, ());
	if (!node)
		return NULL;

	OP_STATUS status = OpStatus::OK;
	switch (node_type)
	{
	case WVTT_C:
	case WVTT_I:
	case WVTT_B:
	case WVTT_U:
	case WVTT_RUBY:
	case WVTT_RT:
		status = node->Construct(node_type, m_classes.GetStorage(), m_classes.Length());
		break;

	case WVTT_V:
		status = node->Construct(node_type, m_classes.GetStorage(), m_classes.Length());
		if (OpStatus::IsSuccess(status) && m_annotation.Length())
			status = node->SetAnnotation(m_annotation.GetStorage(), m_annotation.Length());
		break;

	default:
		OP_ASSERT(!"Unknown node type");
	}

	// Don't expect anything but OOM errors here.
	OP_ASSERT(OpStatus::IsSuccess(status) || OpStatus::IsMemoryError(status));

	if (OpStatus::IsError(status))
	{
		OP_DELETE(node);
		node = NULL;
	}
	return node;
}

/** Map from tag-name (string) to WVTT_Node node type.
 *
 * Unrecognized tags map to WVTT_UNKNOWN.
 */
static inline WebVTT_NodeType
MapStringToNodeType(const uni_char* str, unsigned str_len)
{
	if (str_len == 0)
		return WVTT_UNKNOWN;

	if (str_len == 1)
	{
		switch (str[0])
		{
		case 'c':
			return WVTT_C;
		case 'i':
			return WVTT_I;
		case 'b':
			return WVTT_B;
		case 'u':
			return WVTT_U;
		case 'v':
			return WVTT_V;
		}
	}
	else if (str_len >= 2 && str[0] == 'r')
	{
		// 'ruby'
		if (str_len == 4 && str[1] == 'u' && str[2] == 'b' && str[3] == 'y')
			return WVTT_RUBY;
		// 'rt'
		else if (str_len == 2 && str[1] == 't')
			return WVTT_RT;
	}
	return WVTT_UNKNOWN;
}

class WVTT_TreeBuilder
{
public:
	WVTT_TreeBuilder(WVTT_Node* root)
		: m_current(root), m_lastchild(NULL) {}

	void AppendNode(WVTT_Node* node)
	{
		node->m_parent = m_current;

		if (m_lastchild)
			m_lastchild->m_suc = node;
		else
			m_current->m_firstchild = node;

		m_lastchild = node;
	}

	void PushNode(WVTT_Node* node)
	{
		AppendNode(node);

		m_current = node;
		m_lastchild = NULL;
	}

	void PopNode()
	{
		OP_ASSERT(m_current && m_current->Parent());
		m_lastchild = m_current;
		m_current = m_current->Parent();
	}

	bool CurrentMatches(WebVTT_NodeType node_type) const
	{
		return m_current->Type() == node_type;
	}

private:
	WVTT_Node* m_current;
	WVTT_Node* m_lastchild;
};

// http://dev.w3.org/html5/webvtt/#webvtt-cue-text-parsing-rules
WVTT_Node*
WebVTT_Parser::ParseCueText(const uni_char* cue_text, unsigned cue_text_len)
{
	WebVTT_Buffer b(cue_text, cue_text_len);

	WVTT_Node* cue_root = OP_NEW(WVTT_Node, ());
	if (!cue_root)
		return NULL;

	// This way of calling Construct cannot lead to errors.
	OpStatus::Ignore(cue_root->Construct(WVTT_ROOT));

	WVTT_TreeBuilder builder(cue_root);

	bool oom = false;
	while (!oom)
	{
		if (b.IsEmpty())
			break;

		int token = GetCueToken(b);

		const uni_char* val_str = m_token_result.GetStorage();
		unsigned val_len = m_token_result.Length();

		switch (token)
		{
		case WVTT_TOK_STRING:
			{
				WVTT_Node* text = OP_NEW(WVTT_Node, ());
				if (!text || OpStatus::IsError(text->Construct(val_str, val_len)))
				{
					OP_DELETE(text);
					oom = true;
					break;
				}

				builder.AppendNode(text);
			}
			break;

		case WVTT_TOK_START_TAG:
			{
				WebVTT_NodeType node_type = MapStringToNodeType(val_str, val_len);

				// 'rt' only allowed under 'ruby'.
				if (node_type == WVTT_RT && !builder.CurrentMatches(WVTT_RUBY))
					node_type = WVTT_UNKNOWN;

				if (node_type != WVTT_UNKNOWN)
					if (WVTT_Node* node = CreateNode(node_type))
						builder.PushNode(node);
					else
						oom = true;
			}
			break;

		case WVTT_TOK_END_TAG:
			{
				WebVTT_NodeType node_type = MapStringToNodeType(val_str, val_len);

				// Matching start/end tags always close.
				bool pop_current = builder.CurrentMatches(node_type);

				if (!pop_current)
				{
					// 'ruby' auto-closes 'rt'.
					if (node_type == WVTT_RUBY && builder.CurrentMatches(WVTT_RT))
					{
						// Close 'rt'.
						builder.PopNode();
						pop_current = true;
					}
				}

				if (pop_current)
					builder.PopNode();
			}
			break;

		case WVTT_TOK_TIMESTAMP_TAG:
			{
				double tstamp;
				WebVTT_Buffer sb(val_str, val_len);
				if (ParseTimestamp(sb, tstamp) && sb.IsEmpty())
				{
					WVTT_Node* tstamp_node = OP_NEW(WVTT_Node, ());
					if (!tstamp_node)
					{
						oom = true;
						break;
					}

					tstamp_node->Construct(tstamp);

					builder.AppendNode(tstamp_node);
				}
			}
			break;
		}
	}

	if (oom)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);

		OP_DELETE(cue_root);
		cue_root = NULL;
	}
	return cue_root;
}

void WebVTT_Parser::DiscardCurrentCue()
{
	OP_DELETE(m_curr_cue);
	m_curr_cue = NULL;
}

// http://dev.w3.org/html5/webvtt/#webvtt-parser-algorithm
OP_STATUS WebVTT_Parser::Parse(WebVTT_InputStream* is)
{
	while (true)
	{
		switch (m_parse_state)
		{
		case STATE_COLLECT_LINE:
		{
			// Special state for resuming line collection when the input
			// stream drains. m_restart_state should have been set to
			// the state to return to when collection finishes. (Handled
			// by CollectLine)
			OP_BOOLEAN status = is->CollectLine(m_line);
			if (status == OpBoolean::IS_FALSE)
				return OpStatus::OK;

			RETURN_IF_ERROR(status);

			m_parse_state = m_restart_state;
		}
		break;

		case STATE_OPTIONAL_BOM:
			// This will trigger if we invoke the parser with no data
			OP_ASSERT(is->Remaining() != 0);

			// Step 3
			// Here we would strip the BOM, but the CharacterDecoder
			// in the URL_DataDescriptor already did that so just
			// ignore this step and move on.
			// if (is->GetChar() == 0xfeff)
			//	is->Shift();

			// Step 4
			m_line_already_collected = false;

			m_parse_state = STATE_MAGIC;

			// Step 5
			RETURN_IF_ERROR(CollectLine(is));
			break;

		case STATE_MAGIC:
			{
				// Step 6-8
				WebVTT_Buffer b(m_line);
				if (!b.Scan("WEBVTT", 6) ||
					!(b.IsEmpty() || b.Scan(0x20) || b.Scan(0x09)))
				{
					m_listener->OnParseError(ParseError(Fatal,
														UNI_L("A WebVTT file must begin with the string \"WEBVTT\"."),
														m_line.GetStorage(), m_line_no));

					m_parse_state = STATE_END;
					return OpStatus::ERR;
				}

				m_parse_state = STATE_MAGIC_SKIP_NL;
			}
			// fall through

		case STATE_MAGIC_SKIP_NL:
			// Step 9
			if (is->IsEOF())
			{
				m_parse_state = STATE_END;
				break;
			}

			// Step 10
			if (!ConsumeEOL(is))
				return OpStatus::OK;

			m_parse_state = STATE_SKIP_HEADER;

			// Step 11
			RETURN_IF_ERROR(CollectLine(is));
			break;

		case STATE_SKIP_HEADER:
			// Step 12
			if (is->IsEOF())
			{
				m_parse_state = STATE_END;
				break;
			}

			// Step 13
			if (!ConsumeEOL(is))
				return OpStatus::OK;

			if (m_line.Length() != 0)
			{
				// Step 14
				if (uni_strstr(m_line.GetStorage(), "-->"))
				{
					// Step 16 (not setting 'already collected line'
					// flag - instead going straight to the creation
					// step).
					m_parse_state = STATE_CREATE_CUE;
				}
				else
				{
					// Step 15, 11
					RETURN_IF_ERROR(CollectLine(is));

					// Remain in this state
				}
			}
			else
			{
				m_parse_state = STATE_CUE_LOOP;
			}
			break;

		case STATE_CUE_LOOP:
			m_parse_state = STATE_CREATE_CUE;

			// Step 16
			if (m_line_already_collected)
				break;

			// Step 18
			RETURN_IF_ERROR(CollectLine(is));
			break;

		case STATE_CREATE_CUE:
			// Step 19
			if (m_line.Length() == 0)
			{
				// Step 17
				if (is->IsEOF())
				{
					m_parse_state = STATE_END;
					break;
				}

				if (!ConsumeEOL(is))
					return OpStatus::OK;

				m_parse_state = STATE_CUE_LOOP;
				break;
			}

			// Steps 20-29
			OP_ASSERT(m_curr_cue == NULL);
			m_curr_cue = OP_NEW(MediaTrackCue, ());
			if (!m_curr_cue)
				return OpStatus::ERR_NO_MEMORY;

			m_parse_state = STATE_TIMINGS_OR_IDENTIFIER;
			// fall through

		case STATE_TIMINGS_OR_IDENTIFIER:
			// Step 30
			if (uni_strstr(m_line.GetStorage(), "-->"))
			{
				m_parse_state = STATE_TIMINGS;
				break;
			}

			// Step 31
			m_curr_cue->SetIdentifier(StringWithLength(m_line));

			// Step 32
			if (is->IsEOF())
			{
				DiscardCurrentCue();

				m_parse_state = STATE_END;
				break;
			}

			m_parse_state = STATE_TIMINGS_SKIP_NL;
			// fall through

		case STATE_TIMINGS_SKIP_NL:
			// Step 33
			if (!ConsumeEOL(is))
				return OpStatus::OK;

			m_parse_state = STATE_TIMINGS;

			// Step 34
			RETURN_IF_ERROR(CollectLine(is));
			break;

		case STATE_TIMINGS:
			// Step 35
			if (m_line.Length() == 0)
			{
				DiscardCurrentCue();

				m_parse_state = STATE_CUE_LOOP;
				break;
			}

			// Step 36
			m_line_already_collected = false;

			// Step 37
			if (!ParseTimingsAndSettings(m_line))
			{
				m_listener->OnParseError(ParseError(Informational,
													UNI_L("Parsing the timing and settings failed, the cue will be discarded."),
													m_line.GetStorage(), m_line_no));

				m_parse_state = STATE_BAD_CUE;
				break;
			}
			else
			{
				double curr_start_ts = m_curr_cue->GetStartTime();

				if (curr_start_ts < m_prev_start_ts)
					m_listener->OnParseError(ParseError(Informational,
														UNI_L("The cue started before the previous cue. Cues should be sorted by start time offset."),
														m_line.GetStorage(), m_line_no));

				m_prev_start_ts = curr_start_ts;
			}

			// Step 38
			m_cue_text.Clear();

			m_parse_state = STATE_CUE_TEXT_LOOP;
			// fall through

		case STATE_CUE_TEXT_LOOP:
			// Step 39
			if (is->IsEOF())
			{
				m_parse_state = STATE_CUE_TEXT_PROC;
				break;
			}

			// Step 40
			if (!ConsumeEOL(is))
				return OpStatus::OK;

			m_parse_state = STATE_CUE_TEXT;

			// Step 41
			RETURN_IF_ERROR(CollectLine(is));
			break;

		case STATE_CUE_TEXT:
			// Step 42
			if (m_line.Length() == 0)
			{
				m_parse_state = STATE_CUE_TEXT_PROC;
				break;
			}

			// Step 43
			if (uni_strstr(m_line.GetStorage(), "-->"))
			{
				m_line_already_collected = true;
				m_parse_state = STATE_CUE_TEXT_PROC;
				break;
			}

			// Step 44
			if (m_cue_text.Length() != 0)
				m_cue_text.Append(0x0a);

			// Step 45
			m_cue_text.Append(m_line.GetStorage(), m_line.Length());

			// Step 46
			m_parse_state = STATE_CUE_TEXT_LOOP;
			break;

		case STATE_CUE_TEXT_PROC:
			// Step 47
			RETURN_IF_ERROR(m_curr_cue->SetText(StringWithLength(m_cue_text)));

			// Step 48
			m_listener->OnCueDecoded(m_curr_cue);
			m_curr_cue = NULL;

			// Step 49
			m_parse_state = STATE_CUE_LOOP;
			break;

		case STATE_BAD_CUE:
			// Step 50
			DiscardCurrentCue();

			m_parse_state = STATE_BAD_CUE_LOOP;
			// fall through

		case STATE_BAD_CUE_LOOP:
			// Steps 51
			if (is->IsEOF())
			{
				m_parse_state = STATE_END;
				break;
			}

			// Steps 52
			if (!ConsumeEOL(is))
				return OpStatus::OK;

			m_parse_state = STATE_BAD_CUE_SKIP_LINE;

			// Step 53
			RETURN_IF_ERROR(CollectLine(is));
			break;

		case STATE_BAD_CUE_SKIP_LINE:
			// Step 55
			if (m_line.Length() == 0)
			{
				m_parse_state = STATE_CUE_LOOP;
				break;
			}

			// Step 54
			if (uni_strstr(m_line.GetStorage(), "-->"))
			{
				m_line_already_collected = true;
				m_parse_state = STATE_CUE_LOOP;
				break;
			}

			// Step 56
			m_parse_state = STATE_BAD_CUE_LOOP;
			break;

		case STATE_END:
			// Step 57
			m_listener->OnParsingComplete();
			return OpStatus::OK;
		}
	}
}

#endif // MEDIA_HTML_SUPPORT
