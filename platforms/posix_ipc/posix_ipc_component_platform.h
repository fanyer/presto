/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UNIX_COMPONENT_PLATFORM_H
#define UNIX_COMPONENT_PLATFORM_H

#include "modules/hardcore/component/OpComponent.h"

class IpcHandle;

/** @brief An implementation of OpComponentPlatform for Opera sub-processes
 */
class PosixIpcComponentPlatform : public OpComponentPlatform
{
public:
	/**
	 * Create a new unix component instead of the default one (Main process).
	 * This is used for sub-processes.
	 *
	 * Returns NULL if a component could not be created.
	 */
	static OP_STATUS Create(PosixIpcComponentPlatform*& component, const char* process_token);

	virtual ~PosixIpcComponentPlatform();

	/**
	 * Run the component.
	 * @returns the return value which will be returned by main().
	 */
	virtual int Run() { return ProcessMessages(); }

	/** Exit the main run loop this component is in
	  */
	virtual void Exit();

	// Implementation of OpComponentPlatform:
	virtual void RequestRunSlice(unsigned int limit);
	virtual OP_STATUS RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type);
	virtual OP_STATUS SendMessage(OpTypedMessage* message);
	virtual OP_STATUS ProcessEvents(unsigned int timeout, EventFlags flags);
	virtual void OnComponentDestroyed(OpMessageAddress address);

protected:
	/**
	 * Function implemented by the platform that can be used to create custom components
	 * and component platforms.
	 * @param type Type of component to create
	 * @return Component created, or NULL on error
	 */
	static PosixIpcComponentPlatform* CreateComponent(OpComponentType type);

	PosixIpcComponentPlatform();
	int ProcessMessages();

	IpcHandle* m_ipc;
	double m_timeout;
	unsigned m_nesting_level;
	bool m_stop_running;
};

#endif // UNIX_COMPONENT_PLATFORM_H
