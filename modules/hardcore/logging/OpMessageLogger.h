/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_HARDCORE_LOGGING_OPMESSAGELOGGER_H
#define MODULES_HARDCORE_LOGGING_OPMESSAGELOGGER_H

#ifdef HC_MESSAGE_LOGGING
class OpTypedMessage;

/** Interface for an OpTypedMessage logger.
 *
 * The implementer shall log dispatching and performing various actions on
 * OpTypedMessages. Dispatching is handled separately to allow producing
 * tree-like log structures, with traceable message flow histories.
 *
 * The implementer is free to choose the medium and format of the log, for
 * example a text file, ecmascript DOM or an input to an external tool.
 *
 * Failures to log are expected to be either handled internally, quietly ignored
 * or logged within the medium (ex. "Unable to log, reason: ..."). This is because
 * it's not expected from the application code to contain extra logic for handling
 * errors within the optional, debug-only logging system. The application treats
 * the logging facilities as "fire and forget".
 */
class OpMessageLogger
{
public:
	/** Called after the message has been taken off the message queue and
	 * scheduled for dispatching in OpComponentManager::DispatchMessage().
	 *
	 * @param msg Pointer to the message that will be dispatched momentarily.
	 */
	virtual void BeforeDispatch(const OpTypedMessage* msg) = 0;

	/** Called after OpComponentManager::DispatchMessage() has returned.
	 *
	 * @param msg Pointer to the message that has just been dispatched.
	 */
	virtual void AfterDispatch(const OpTypedMessage* msg) = 0;

	/** Called when an arbitrary action (like "Sending", "Adding to inbox" etc.)
	 * is performed on the message.
	 *
	 * @param msg Pointer to message upon which the action is performed.
	 * @param action Null-terminated string description of the action.
	 */
	virtual void Log(const OpTypedMessage* msg, const uni_char* action) = 0;

	/** Checks whether the logger is able to log.
	 *
	 * A logger may be unable to log if, for example, there's an OOM situation,
	 * the medium is unavailable or some invariant is broken. In such situation,
	 * the implementation is free to ignore calls to AfterDispatch(),
	 * BeforeDispatch() or Log() by returning immediately.
	 *
	 * @return true if able to log, false otherwise.
	 */
	virtual bool IsAbleToLog() const = 0;

	virtual ~OpMessageLogger() {}
};

#endif // HC_MESSAGE_LOGGING
#endif // MODULES_HARDCORE_LOGGING_OPMESSAGELOGGER_H
