/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MAC_POSIX_BINARY_SELECTOR
#define MAC_POSIX_BINARY_SELECTOR

#include "platforms/posix_ipc/pi/posix_ipc_binary_selector.h"

#ifdef NS4P_COMPONENT_PLUGINS

class MacPosixBinarySelector : public PosixIpcBinarySelector
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
		ARG_COUNT
	};

	char* m_args[ARG_COUNT+1];
	OpString8 m_binary;
	OpString8 m_token;
	OpString8 m_logfolder;
};

#endif // NS4P_COMPONENT_PLUGINS

#endif // MAC_POSIX_BINARY_SELECTOR
