/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COCOA_OPERA_LISTENER_H
#define COCOA_OPERA_LISTENER_H

#include "modules/hardcore/component/OpComponentPlatform.h"
#include "platforms/posix_ipc/posix_ipc_process_manager.h"

class CocoaOperaListener : public OpComponentPlatform
{
public:
	CocoaOperaListener(BOOL tracking=FALSE, void*killDate=NULL);
	virtual ~CocoaOperaListener();
	void SetTracking(BOOL tracking);
	void* KillDate() { return m_killDate; }
	static CocoaOperaListener* GetListener() {return s_current_opera_listener;}
	bool  IsInsideProcessEvents() { return m_insideProcessEvents; }

protected:
	// Implementation of OpComponentPlatform
	virtual void RequestRunSlice(unsigned int limit);
	virtual OP_STATUS RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type);
	virtual OP_STATUS SendMessage(OpTypedMessage* message);
	virtual OP_STATUS ProcessEvents(unsigned int timeout, EventFlags flags);

private:
	void* m_data;
	void* m_killDate;
	void* m_killTimer;
	bool m_insideProcessEvents;
	static PosixIpcProcessManager s_process_manager;
	CocoaOperaListener* m_previous_listener;
	static CocoaOperaListener* s_current_opera_listener;
};

extern CocoaOperaListener* gCocoaOperaListener;

#endif // COCOA_OPERA_LISTENER_H

