/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_PROTOBUF_OUTPUT_COMMON_H
#define OP_PROTOBUF_OUTPUT_COMMON_H

#ifdef PROTOBUF_SUPPORT

#include "modules/util/adt/opvector.h"
#include "modules/protobuf/src/protobuf_wireformat.h"

class OpProtobufInstanceProxy;
class OpProtobufField;

class OpProtobufOutput
{
public:
	static int GetFieldCount(const OpProtobufInstanceProxy &instance, int field_idx, const OpProtobufField &field);
};

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_OUTPUT_COMMON_H
