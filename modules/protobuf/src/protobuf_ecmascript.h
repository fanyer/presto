/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef OP_PROTOBUF_ES_INPUT_H
#define OP_PROTOBUF_ES_INPUT_H

#ifdef PROTOBUF_SUPPORT

#ifdef PROTOBUF_ECMASCRIPT_SUPPORT

#include "modules/protobuf/src/protobuf_wireformat.h"
#include "modules/protobuf/src/opvaluevector.h"

class OpProtobufInstanceProxy;
class OpProtobufField;

class ES_Object;
class ES_Runtime;

class OpESInputStream
{
public:
	OpESInputStream();
	OP_STATUS Construct(ES_Object *obj, ES_Runtime *runtime);

	OP_STATUS Read(OpProtobufInstanceProxy &instance);

private:
	ES_Object *root_object;
	ES_Runtime *es_runtime;
};

class OpESOutputStream
{
public:
	OpESOutputStream(ES_Object *&root, ES_Runtime *runtime);

	OP_STATUS Write(const OpProtobufInstanceProxy &instance);

private:
	ES_Object  *&root_object;
	ES_Runtime *runtime;
};

// JSON serializer and parser

class OpESToJSON
{
public:
	OpESToJSON();
	OP_STATUS Construct(ES_Object *obj, ES_Runtime *runtime);

	OP_STATUS Write(ByteBuffer &out);

private:
	ES_Object *root_object;
	ES_Runtime *es_runtime;
};

class OpJSONToES
{
public:
	OpJSONToES(ES_Object *&root, ES_Runtime *runtime);
	~OpJSONToES();
	OP_STATUS Construct(const ByteBuffer &in);

	OP_STATUS Read();

private:
	TempBuffer  buffer;
	ES_Object  *&root_object;
	ES_Runtime *runtime;
};

#endif // PROTOBUF_ECMASCRIPT_SUPPORT
#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_ES_INPUT_H
