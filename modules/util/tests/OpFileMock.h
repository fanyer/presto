/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_UTIL_TESTS_OPFILEMOCK_H
#define MODULES_UTIL_TESTS_OPFILEMOCK_H
#ifdef SELFTEST
#include "modules/util/opfile/opfile.h"
#include "modules/hardcore/base/opstatus.h"
#include "modules/opdata/OpData.h"


/** A mock implementation of OpFile for testing classes that
 * use this interface.
 *
 * May be incomplete, feel free to implement more faked/mocked methods.
 **/
class OpFileMock : public OpFile
{
public:
	OpFileMock() :
		openCalls(0), openLastMode(0), openReturn(OpStatus::OK),
		isOpenReturn(TRUE),
		closeCalls(0), closeReturn(OpStatus::OK),
		writeCalls(0), writeLastData(), writeLastLen(0), writeReturn(OpStatus::OK),
		isWritableReturn(TRUE)
	{}
	virtual OP_STATUS Open(int mode)
	{
		openCalls++;
		openLastMode = mode;
		return openReturn;
	}

	virtual BOOL IsOpen() const
	{
		return isOpenReturn;
	}

	virtual OP_STATUS Close()
	{
		closeCalls++;
		isOpenReturn = FALSE;
		return closeReturn;
	}

	virtual OP_STATUS Write(const void* data, OpFileLength len)
	{
		writeCalls++;
		RETURN_IF_ERROR(writeLastData.SetCopyData((char*) data, static_cast<size_t>(len)));
		writeLastLen = len;
		return writeReturn;
	}

	virtual BOOL IsWritable() const
	{
		return isWritableReturn;
	}

	void reset()
	{
		openCalls = 0;
		openLastMode = 0;
		openReturn = OpStatus::OK;
		isOpenReturn = TRUE;
		closeCalls = 0;
		closeReturn = OpStatus::OK;
		writeCalls = 0;
		writeLastData.Clear();
		writeLastLen = 0;
		writeReturn = OpStatus::OK;
		isWritableReturn = TRUE;
	}

	int openCalls;
	int openLastMode;
	OP_STATUS openReturn;

	BOOL isOpenReturn;

	int closeCalls;
	OP_STATUS closeReturn;

	int writeCalls;
	OpData writeLastData;
	OpFileLength writeLastLen;
	OP_STATUS writeReturn;

	BOOL isWritableReturn;
};

#endif // SELFTEST
#endif // MODULES_UTIL_TESTS_OPFILEMOCK_H
