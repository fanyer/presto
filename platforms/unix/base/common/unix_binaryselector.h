/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UNIX_BINARY_SELECTOR
#define UNIX_BINARY_SELECTOR

#include "platforms/posix_ipc/pi/posix_ipc_binary_selector.h"

class UnixBinarySelector : public PosixIpcBinarySelector
{
public:
	OP_STATUS Init(OpComponentType component, const char* token, const char* logfolder);

	// Implementing PosixIpcBinarySelector
	virtual const char* GetBinaryPath();
	virtual char *const * GetArgsArray();

private:
	enum Arguments {
		ARG_BINARY,
		ARG_MULTIPROC,
		ARG_MULTIPROC_DATA,
		ARG_LOGFOLDER,
		ARG_LOGFOLDER_DATA,
		ARG_TYPE,
		ARG_TYPE_DATA,
		ARG_COUNT
	};

	const char* m_args[ARG_COUNT+1];
	OpString8 m_binary;
	OpString8 m_token;
	OpString8 m_logfolder;
};

#endif // UNIX_BINARY_SELECTOR
