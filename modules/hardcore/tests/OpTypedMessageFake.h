/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_HARDCORE_TESTS_OPTYPEDMESSAGEFAKE_H
#define MODULES_HARDCORE_TESTS_OPTYPEDMESSAGEFAKE_H
#ifdef SELFTEST
#include "modules/hardcore/component/OpTypedMessage.h"

/** A fake implementation of OpTypedMessage for testing classes that
 * use this interface.
 *
 * May be incomplete, feel free to implement more faked/mocked methods.
 **/
class OpTypedMessageFake : public OpTypedMessage
{
public:
	OpTypedMessageFake() : OpTypedMessage(0, OpMessageAddress(), OpMessageAddress(), 0)
	{}

#ifdef OPDATA_STRINGSTREAM
	virtual OpUniStringStream& ToStream(OpUniStringStream& in) const
	{
		in << UNI_L("OpTypedMessageFake");
		return in;
	}
#endif

	virtual OP_STATUS Serialize(OpData&) const
	{
		return OpStatus::OK;
	}

	virtual const char* GetTypeString() const
	{
		return "OpTypedMessageFake";
	}
};

#endif // SELFTEST
#endif // MODULES_HARDCORE_TESTS_OPTYPEDMESSAGEFAKE_H
