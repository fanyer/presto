/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MAC_TLS
#define MAC_TLS

#include <pthread.h>

/** A class that holds thread-local variables */
class MacTLS
{
public:
	MacTLS() : component_manager(0), cleanup_stack(0) {}

	/** @return The MacTLS for the calling thread
	 */
	static MacTLS* Get() { MacTLS* instance = GetInternal(); return instance ? instance : &s_null_instance; }

	/** Initialize thread-local variables and attach the first (calling) thread
	 * @note should be called only once when the process is starting and before
	 * calling any of the other functions are called
	 * @return 0 on success or other values on failure
	 */
	static int Init();

	/** Attach a new thread (makes sure TLS is initialized for the new thread)
	 * @note Should be called in the context of the thread being attached
	 * @return 0 on success or other values on failure
	 */
	static int AttachThread();

	/** Destroy thread-local variables, should be called only once when
	 * process is exiting. 
	 */
	static void Destroy();

	class OpComponentManager* component_manager;
	class CleanupItem* cleanup_stack;

private:
	static MacTLS* GetInternal() { return static_cast<MacTLS*>(pthread_getspecific(s_key)); }
	static void DetachThread(void* data);

	static MacTLS s_null_instance;
	static pthread_key_t s_key;
};

#endif // MAC_TLS
