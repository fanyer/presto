/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef SCOPE_DEFAULT_MESSAGE_H
#define SCOPE_DEFAULT_MESSAGE_H

#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/protobuf/src/protobuf_message.h"

class OpScopeDefaultMessage
{
public:
	OpScopeDefaultMessage()
		: encoded_size_(-1)
	{}
	enum MetaInfo {
		FieldCount  = 0
	};

	static const OpProtobufMessage *GetMessageDescriptor();

	mutable int encoded_size_;
};


#endif // SCOPE_DEFAULT_MESSAGE_H
