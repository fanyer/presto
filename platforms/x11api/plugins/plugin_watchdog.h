/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PLUGIN_WATCHDOG_H
#define PLUGIN_WATCHDOG_H

#include <pthread.h>

/** @brief A watchdog class that watches from a separate thread
 * This is a watchdog timer that will kill the current process (using
 * POSIX's _exit()) if it has not been kicked at any point during
 * the specified watchdog timeout.
 */
class PluginWatchdog
{
public:
	PluginWatchdog() : m_shared_data(NULL) {}
	~PluginWatchdog();

	/** Initialize the watchdog. Call before calling any of the other functions
	 * below.
	 */
	OP_STATUS Init();

	/** Start watching and kill the process if the timeout is reached without
	 * receiving any kicks. Keep watching until Stop() is called or the object
	 * is destroyed.
	 * @param timeout Timeout in ms, must be > 0.
	 */
	OP_STATUS Start(unsigned timeout);

	/** Stop the watchdog.
	 */
	void Stop();

	/** Kick the watchdog. To not kill the process, this function should be
	 * called at least once before the timeout expires.
	 */
	void Kick();

private:
	static void* WatchLoop(void* data);
	static OP_STATUS Evaluate(int status);

	struct SharedData
	{
		unsigned timeout;
		pthread_cond_t cond;
		pthread_mutex_t mutex;

		SharedData() : timeout(0) {}
	};

	SharedData* m_shared_data;
	pthread_t m_thread_id;
};

#endif // PLUGIN_WATCHDOG_H
