/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_PROTOBUF_INPUT_H
#define OP_PROTOBUF_INPUT_H

#ifdef PROTOBUF_SUPPORT

#include "modules/protobuf/src/protobuf_wireformat.h"
#include "modules/protobuf/src/protobuf_input_common.h"
#include "modules/protobuf/src/protobuf_data.h"
#include "modules/protobuf/src/opvaluevector.h"
#include "modules/protobuf/src/json_tools.h"

#ifdef PROTOBUF_XML_SUPPORT
# include "modules/xmlutils/xmlfragment.h"
#endif // PROTOBUF_SUPPORT

class OpProtobufInstanceProxy;
class OpProtobufField;

class OpProtobufInputStream
{
public:
	OpProtobufInputStream();
	OP_STATUS Construct(const OpProtobufDataRange &range);
	OP_STATUS Construct(const unsigned char *in, int len);

	OP_STATUS Read(OpProtobufInstanceProxy &instance);

private:
	OP_STATUS ReadMessage(OpProtobufInstanceProxy &instance);
	OP_STATUS ReadField(OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);

	OP_STATUS ReadVarInt32(INT32 &out);
	OP_STATUS ReadVarInt64(INT64 &out);
	OP_STATUS ReadFixed32(UINT32 &out);
	OP_STATUS ReadFixed64(UINT64 &out);
	OP_STATUS ReadDouble(double &out);
	OP_STATUS ReadFloat(float &out);

	OpProtobufDataRange range;
};

#ifdef PROTOBUF_JSON_SUPPORT

class OpJSONInputStream
{
public:
	OpJSONInputStream();
	OP_STATUS Construct(const uni_char *in, int len);

	OP_STATUS Read(OpProtobufInstanceProxy &instance);

	const OpProtobufError &GetError() const { return error; }

private:
	OP_STATUS ReadMessage(OpProtobufInstanceProxy &instance);
	OP_STATUS ReadField(OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field, BOOL embed = FALSE, BOOL first = TRUE);

	typedef OpScopeJSON::Input Input;
	typedef OpScopeJSON::Lexer Lexer;
	typedef OpScopeJSON::Parser Parser;

	Input  input;
	Lexer  lexer;
	Parser parser;
	Lexer::MatchResult match;
	OpProtobufError error;
};

#endif // PROTOBUF_JSON_SUPPORT

#ifdef PROTOBUF_XML_SUPPORT

class OpXMLInputStream
{
public:
	OpXMLInputStream();
	OP_STATUS Construct(const char *in, int len);

	OP_STATUS Read(OpProtobufInstanceProxy &instance);

	BOOL HasErrorObject() const { return has_error; }
	const OpProtobufError &GetError() const { return error; }

private:
	OP_STATUS ReadMessage(XMLFragment &xml, OpProtobufInstanceProxy &instance);
	OP_STATUS ReadField(XMLFragment &xml, OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field, const uni_char *name);

	BOOL        is_valid;
	BOOL        has_error;
	OpString    text;
	XMLFragment xml;

	OpProtobufError error;
};

#endif // PROTOBUF_XML_SUPPORT

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_INPUT_H
