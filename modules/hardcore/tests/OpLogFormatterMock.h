/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_HARDCORE_TESTS_OPLOGFORMATTERMOCK_H
#define MODULES_HARDCORE_TESTS_OPLOGFORMATTERMOCK_H
#ifdef SELFTEST

#ifdef HC_MESSAGE_LOGGING
#include "modules/hardcore/logging/OpLogFormatter.h"
#include "modules/hardcore/base/opstatus.h"
#include "modules/util/opstring.h"

/** A mock implementation of OpLogFormatter for testing classes that
 * use this interface.
 *
 * May be incomplete, feel free to implement more faked/mocked methods.
 **/
class OpLogFormatterMock : public OpLogFormatter
{
public:
	OpLogFormatterMock() :
		formatBeginLoggingResult(OpStatus::OK),
		formatStartedDispatchingResult(OpStatus::OK),
		formatFinishedDispatchingResult(OpStatus::OK),
		formatLogResult(OpStatus::OK),
		formatEndLoggingResult(OpStatus::OK)
	{}

	virtual OP_STATUS FormatBeginLogging(OpUniStringStream& inout)
	{
		inout << UNI_L("FormatBeginLogging result");
		return formatBeginLoggingResult;
	}

	virtual OP_STATUS FormatStartedDispatching(
		OpUniStringStream& inout,
		const OpTypedMessage& message)
	{
		OpString16 typeString;
		RETURN_IF_ERROR(typeString.SetFromUTF8(message.GetTypeString()));
		inout << UNI_L("FormatStartedDispatching result ")
			  << typeString.CStr();
		return formatStartedDispatchingResult;
	}

	virtual OP_STATUS FormatFinishedDispatching(
		OpUniStringStream& inout,
		const OpTypedMessage& message)
	{
		OpString16 typeString;
		RETURN_IF_ERROR(typeString.SetFromUTF8(message.GetTypeString()));
		inout << UNI_L("FormatFinishedDispatching result ")
			  << typeString.CStr();
		return formatFinishedDispatchingResult;
	}

	virtual OP_STATUS FormatLog(
		OpUniStringStream& inout,
		const OpTypedMessage& message,
		const uni_char* action)
	{
		OpString16 typeString;
		RETURN_IF_ERROR(typeString.SetFromUTF8(message.GetTypeString()));
		inout << UNI_L("FormatLog result ") << action << UNI_L(" ")
			  << typeString.CStr();
		return formatLogResult;
	}

	virtual OP_STATUS FormatEndLogging(OpUniStringStream& inout)
	{
		inout << UNI_L("FormatEndLogging result");
		return formatEndLoggingResult;
	}

	OP_STATUS formatBeginLoggingResult;
	OP_STATUS formatStartedDispatchingResult;
	OP_STATUS formatFinishedDispatchingResult;
	OP_STATUS formatLogResult;
	OP_STATUS formatEndLoggingResult;
};
#endif // HC_MESSAGE_LOGGING
#endif // SELFTEST
#endif // MODULES_HARDCORE_TESTS_OPLOGFORMATTERMOCK_H
