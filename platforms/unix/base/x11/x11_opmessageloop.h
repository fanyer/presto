/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_OPMESSAGELOOP_H__
#define __X11_OPMESSAGELOOP_H__

#include "modules/hardcore/component/OpComponentPlatform.h"
#include "modules/util/adt/oplisteners.h"
#include "platforms/posix/posix_selector.h"
#include "platforms/utilix/x11types.h"

class X11CbEvent;
class X11CbObject;
class X11Widget;
class PosixIpcProcessManager;

class X11LowLatencyCallback
{
public:
	virtual ~X11LowLatencyCallback() {};
	virtual void LowLatencyCallback(UINTPTR data) = 0;
};

class X11OpMessageLoop: public OpComponentPlatform, public PosixSelectListener
{
public:
	X11OpMessageLoop(PosixIpcProcessManager& process_manager);
	~X11OpMessageLoop();

	// Implementation of OpComponentPlatform
	virtual void RequestRunSlice(unsigned int limit);
	virtual OP_STATUS RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type);
	virtual OP_STATUS SendMessage(OpTypedMessage* message);
	virtual OP_STATUS ProcessEvents(unsigned int timeout, EventFlags flags);
	virtual void OnComponentCreated(OpMessageAddress address);
	virtual void OnComponentDestroyed(OpMessageAddress address) {}

	// Implementation of PosixSelectListener
	void OnReadReady(int fd) {}
	void OnError(int fd, int err=0) {}
	void OnDetach(int fd) {}

	// Implementation specific methods:

	/** Initialize the loop
	 * @note must be run before running other functions in here, but after
	 * initializing g_opera
	 */
	OP_STATUS Init();

	/** Run the main loop until StopAll() is called
	 */
	void MainLoop();

	/**
	 * Run the message loop for the specified time or until StopCurrent() is called
	 *
	 * @param timeout Number of milliseconds. UINT_MAX means no timeout
	 *
	 * @return This function always return OpStatus::OK
	 */
	void NestLoop(unsigned timeout = UINT_MAX);

	/**
	 * Calls NestLoop() with a timeout of 1000 ms.
	 * @deprecated  DO NOT CALL THIS METHOD, results will be unexpected.
	 *
	 * Results can be unexpected and it's definitely not guaranteed that this
	 * will 'dispatch all posted messages'. There's only one known use case
	 * of this method which will be removed when the printing code in core
	 * is rewritten to become sane.
	 */
	DEPRECATED(void DispatchAllPostedMessagesNow());

	/**
	 * Stop the current message loop. Even if there are several calls to
	 * @ref NestLoop() on the stack with the same X11OpMessageLoop object, only the
	 * topmost call will be requested to stop.
	 */
	void StopCurrent() { m_stop_current = true; }

	/**
	* Instructs all loops (regardless of nesting level) to stop. This function
	* will cause Opera to exit from all sorts of loop nesting. Shall only be
	* used during program termination.
	*/
	void StopAll() { m_stop_all = true; }

	/**
	 * These are used to send the original XEvents to windowless plugins,
	 * since the core events have missing information
	 */
	enum PluginEvent
	{
		KEY_EVENT,
		BUTTON_EVENT,
		MOTION_EVENT,
		FOCUS_EVENT,
		CROSSING_EVENT,
		PLUGIN_EVENT_COUNT
	};
	XEvent GetPluginEvent(PluginEvent event);

	void HandleXEvent(XEvent *e, X11Widget* widget);

	/** cbobj->LowLatencyCallback(data) will be called as soon as
	 * possible after g_op_time_info->GetRuntimeMS() returns a value
	 * at least as big as 'when'.
	 */
	static OP_STATUS PostLowLatencyCallbackAbsoluteTime(X11LowLatencyCallback *cbobj, UINTPTR data, double when);

	/** cbobj->LowLatencyCallback(data) will be called as soon as
	 * possible after at least 'delay' milliseconds have passed.
	 */
	static OP_STATUS PostLowLatencyCallbackDelayed(X11LowLatencyCallback * cbobj, UINTPTR data, double delay);

	/** cbobj->LowLatencyCallback(data) will be called as soon as
	 * possible.
	 */
	static OP_STATUS PostLowLatencyCallbackNoDelay(X11LowLatencyCallback * cbobj, UINTPTR data);

	/** Cancel all outstanding low latency callbacks registered for
	 * 'cbobj'.
	 */
	static void CancelLowLatencyCallbacks(X11LowLatencyCallback * cbobj);

	/** Trigger any low latency callbacks that are ready to be
	 * executed.  It may be a good idea to call this every now and
	 * then when doing something that may take a significant amount of
	 * time.
	 */
	static void TriggerLowLatencyCallbacks();

	/**
	 * Returns the current X timestamp
	 */
	static X11Types::Time GetXTime() { return s_xtime; }

	/**
	 * Sets the current X timer stamp
	 */
	static void SetXTime(X11Types::Time xtime) { s_xtime = xtime; }

	/**
	 * This method is called by the SIGCHLD signal handler.  So it
	 * MUST BE signal-safe!
	 */
	static void ReceivedSIGCHLD() { s_sigchld_received = true; };

	/**
	 * Retrieve the sequence number of a request that has been handled
	 * before any X event that is currently handled or will be handled
	 * in the future.
	 *
	 * So any X event handled now or in the future will reflect the
	 * effects of all requests with sequence numbers equal to or
	 * earlier than this function's return value.  However, it is
	 * possible that requests with later sequence numbers have been
	 * handled as well.  If that is important to you, you should look
	 * at the 'serial' member of the X event you are handling instead
	 * (If it is important to you, you are probably handling an
	 * event).
	 *
	 * Note that X has a function LastKnownRequestProcessed().  The
	 * problem with this function is that it (probably) returns the
	 * sequence number from the last message it received.  Which can
	 * be significantly later than the next message returned by
	 * XNextEvent().
	 */
	static unsigned long GetXRecentlyProcessedRequestNumber() { return s_xseqno; };

	/**
	 * Returns the current mouse position wrt to the root window
	 */
	static OpPoint GetRootPointerPosition() { return s_xroot; }

	/**
	 * Returns a pointer to the single instance of the message loop object
	 */
	static X11OpMessageLoop *Self() { return s_self; }

private:
	void PollPlatformEvents(unsigned waitms);
	bool ProcessXEvents();
	void SavePluginEvent(const XEvent& xevent);
	void ReapZombies();

	unsigned m_stop_backlog;
	bool m_stop_current;
	bool m_stop_all;

	XEvent* m_plugin_events;

	PosixIpcProcessManager& m_process_manager;

	/* g_op_time_info->GetRuntimeMS() of the first time we got a
	 * SIGCHLD after the last time we actually called wait().
	 *
	 * 0 when not set.
	 */
	double m_first_sigchld;

	/* g_op_time_info->GetRuntimeMS() of the last time we got a
	 * SIGCHLD.
	 *
	 * 0 when not set.
	 */
	double m_last_sigchld;

	static X11Types::Time s_xtime;
	static unsigned long s_xseqno;
	static OpPoint s_xroot;
	static X11OpMessageLoop* s_self;
	static List<class X11LowLatencyCallbackElm> s_pending_low_latency_callbacks;

	/** Set by the SIGCHLD handler.  You really don't need to know
	 * anything more :)
	 */
	static volatile bool s_sigchld_received;
};

#endif // __X11_OPMESSAGELOOP_H__
