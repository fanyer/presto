/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "modules/ecmascript/json_listener.h"
#include "modules/ecmascript/jsonlite/json_lexer.h"

class JSONParser
{
	OpJSONLexer::Input m_input;
	OpJSONLexer::Lexer m_lexer;
	OpJSONLexer::Parser m_parser;

	const uni_char *m_data;
	unsigned m_length;
	JSONListener *m_listener;

public:
	JSONParser(JSONListener *listener);
	OP_STATUS Parse(const uni_char *data, unsigned length);

private:
	OP_STATUS ParseList(BOOL is_object);
	OP_STATUS ParseObject();
	OP_STATUS ParseArray();
	OP_STATUS ParseValue();
	OP_STATUS ParseString(BOOL is_attribute = FALSE);
	OP_STATUS ParseNumber();
};

#endif // JSON_PARSER_H
