/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Test class for the new service API in STP/1.
** Jan Borsodi
*/

#ifndef OT_SCOPE_TEST_SERVICE_H
#define OT_SCOPE_TEST_SERVICE_H

#ifdef SELFTEST
#ifdef SCOPE_SUPPORT
#include "modules/scope/src/generated/g_scope_test_service_interface.h"
#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/scope_transport.h"

class OtScopeTestService
	: public OtScopeTestService_SI
{
public:
	OtScopeTestService(const uni_char* id = NULL)
		: OtScopeTestService_SI(id)
		, data_value(0)
		, event_id(0)
	{}

	OP_STATUS Clear();

	unsigned int data_value;
	OpString     data_name;
	unsigned int event_id;
	int last_async_command_tag;


	// For Command_AsyncCommand
	unsigned GetLastAsyncCommandTag() const { return last_async_command_tag; }
	OP_STATUS AsyncCommandCallback(unsigned async_tag, int value);

	// Request/Response functions
	OP_STATUS DoGetData(OutData &out);
	OP_STATUS DoSetData(const InData &in);
	OP_STATUS DoAdd(const InAdd &in, OutAdd &out);
	OP_STATUS DoNop();
	OP_STATUS DoGetRepeatedData(const GetRepeatedDataArg &in, RepeatedData &out);
	OP_STATUS DoFormatError(const FormatErrorArg &in);
	OP_STATUS DoAsyncCommand(const AsyncArgs &in, unsigned int async_tag);
	OP_STATUS DoTestMixedStrings(const MixedStringType &in, MixedStringType &out);
	OP_STATUS DoTestMixedBytes(const MixedByteType &in, MixedByteType &out);
	OP_STATUS DoInspectElement(const InspectElementArg &in);
};

#endif // SCOPE_SUPPORT
#endif // SELFTEST

#endif // OT_SCOPE_TEST_SERVICE_H
