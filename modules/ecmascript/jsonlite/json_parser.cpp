#include "core/pch.h"

#ifdef ES_JSON_LITE

#include "modules/ecmascript/json_parser.h"

JSONParser::JSONParser(JSONListener *listener)
	: m_parser(m_lexer)
	, m_listener(listener)
{
	OP_ASSERT(listener);
}

OP_STATUS
JSONParser::Parse(const uni_char *data, unsigned length)
{
	m_data = data;
	m_length = length;
	RETURN_IF_ERROR(m_input.Construct(m_data, m_length)); // TODO(jhoff): Is it OK to construct multiple times?
	RETURN_IF_ERROR(m_lexer.Construct(&m_input));
	return ParseValue();
}

OP_STATUS
JSONParser::ParseList(BOOL is_object) // helper method for comma separated lists used by object and array
{
	OpJSONLexer::Lexer::Token t;
	for (;;)
	{
		m_lexer.LT(1, t);
		if (t == OpJSONLexer::Lexer::T_CURLY_BRACKET_END ||
			t == OpJSONLexer::Lexer::T_BRACKET_END)
		 return OpStatus::OK;
		if (is_object)
		{
			RETURN_IF_ERROR(ParseString(TRUE));
			RETURN_IF_ERROR(m_lexer.Match(OpJSONLexer::Lexer::T_COLON));
			RETURN_IF_ERROR(ParseValue());
		}
		else
			RETURN_IF_ERROR(ParseValue());
		m_lexer.LT(1, t);
		if (t == OpJSONLexer::Lexer::T_CURLY_BRACKET_END ||
			t == OpJSONLexer::Lexer::T_BRACKET_END)
		 return OpStatus::OK;
		RETURN_IF_ERROR(m_lexer.Match(OpJSONLexer::Lexer::T_COMMA));
	}
}

OP_STATUS
JSONParser::ParseObject()
{
	RETURN_IF_ERROR(m_lexer.Match(OpJSONLexer::Lexer::T_CURLY_BRACKET_START));
	RETURN_IF_ERROR(m_listener->EnterObject());
	RETURN_IF_ERROR(ParseList(TRUE));
	RETURN_IF_ERROR(m_lexer.Match(OpJSONLexer::Lexer::T_CURLY_BRACKET_END));
	RETURN_IF_ERROR(m_listener->LeaveObject());
	return OpStatus::OK;
}

OP_STATUS
JSONParser::ParseArray()
{
	RETURN_IF_ERROR(m_lexer.Match(OpJSONLexer::Lexer::T_BRACKET_START));
	RETURN_IF_ERROR(m_listener->EnterArray());
	RETURN_IF_ERROR(ParseList(FALSE));
	RETURN_IF_ERROR(m_lexer.Match(OpJSONLexer::Lexer::T_BRACKET_END));
	RETURN_IF_ERROR(m_listener->LeaveArray());
	return OpStatus::OK;
}

OP_STATUS
JSONParser::ParseValue()
{
	OpJSONLexer::Lexer::Token t;
	m_lexer.LT(1, t);
	if (m_input.GetState().cur == m_data + m_length)
		return OpStatus::OK;
	switch (t)
	{
	case OpJSONLexer::Lexer::T_CURLY_BRACKET_START:
		return ParseObject();
	case OpJSONLexer::Lexer::T_BRACKET_START:
		return ParseArray();
	case OpJSONLexer::Lexer::T_REAL:
	case OpJSONLexer::Lexer::T_INTEGER:
	case OpJSONLexer::Lexer::T_EXPONENT:
	case OpJSONLexer::Lexer::T_FRACTION:
		return ParseNumber();
	case OpJSONLexer::Lexer::T_STRING:
		return ParseString();
	case OpJSONLexer::Lexer::T_NULL:
		m_lexer.Match(t);
		m_listener->Null();
		return OpStatus::OK;
	case OpJSONLexer::Lexer::T_TRUE:
		m_lexer.Match(t);
		m_listener->Bool(TRUE);
		return OpStatus::OK;
	case OpJSONLexer::Lexer::T_FALSE:
		m_lexer.Match(t);
		m_listener->Bool(FALSE);
		return OpStatus::OK;
	case OpJSONLexer::Lexer::T_WS:
		m_lexer.Match(t);
		return ParseValue();
	default:
		return OpStatus::ERR;
	}
}

OP_STATUS
JSONParser::ParseString(BOOL is_attribute)
{
	OpString str;
	RETURN_IF_ERROR(m_parser.ParseString(str));
	if (is_attribute)
		m_listener->AttributeName(str);
	else
		m_listener->String(str);
	return OpStatus::OK;
}

OP_STATUS
JSONParser::ParseNumber()
{
	double val;
	RETURN_IF_ERROR(m_parser.ParseDouble(val));
	m_listener->Number(val);
	return OpStatus::OK;
}

#endif // ES_JSON_LITE
