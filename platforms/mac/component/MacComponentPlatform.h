/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MAC_COMPONENT_PLATFORM_H
#define MAC_COMPONENT_PLATFORM_H

#include "platforms/posix_ipc/posix_ipc_component_platform.h"

class MacComponentPlatform : public PosixIpcComponentPlatform
{
public:
	MacComponentPlatform();
	virtual ~MacComponentPlatform();
	
	enum SocketDir {
		READ,
		WRITE
	};
	
	virtual int  Run();
	virtual void Exit();

	OP_STATUS SendMessage(OpTypedMessage* message);

	void Read();
	void Write();

private:
	BOOL InitSocket(SocketDir dir, int fd);
	void DestroySocket(SocketDir dir);

	static void SocketCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info);

	void				*m_timer;
	CFSocketRef			m_socket[2];			// One read and one write
	CFRunLoopSourceRef	m_run_loop_source[2];	// One read and one write
};

#endif // MAC_COMPONENT_PLATFORM_H
