/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef WINDOWS_TLS
#define WINDOWS_TLS

/** A class that holds thread-local variables */
class WindowsTLS
{
public:
	WindowsTLS() : component_manager(NULL), cleanup_stack(NULL) {}

	/** @return The WindowsTLS for the calling thread
	 */
	static WindowsTLS* Get() { return static_cast<WindowsTLS*>(TlsGetValue(s_index)); }

	/** Initialize thread-local variables and attach the first (calling) thread
	 * @note should be called only once when the process is starting and before
	 * calling any of the other functions are called
	 * @return true on success, false otherwise
	 */
	static bool Init();

	/** Attach a new thread (makes sure TLS is initialized for the new thread)
	 * @note Should be called in the context of the thread being attached
	 * @return true on success, false otherwise
	 */
	static bool AttachThread();

	/** Detach a thread
	 * @note Should be called in the context of the thread being detached
	 */
	static void DetachThread();

	/** Destroy thread-local variables, should be called only once when
	 * process is exiting. 
	 */
	static void Destroy();

	class OpComponentManager* component_manager;
	class CleanupItem* cleanup_stack;

private:
	static DWORD s_index;
};

#endif // WINDOWS_TLS
