/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_HARDCORE_LOGGING_OPLOGFORMATTER_H
#define MODULES_HARDCORE_LOGGING_OPLOGFORMATTER_H
#ifdef HC_MESSAGE_LOGGING
#include "modules/opdata/OpStringStream.h"

class OpTypedMessage;

/**
 * Formats logging activities in an appropriate manner.
 *
 * Implementations will produce output in an implementation-specific
 * format or protocol. For human-readable logs, a readable log formatter
 * will be supplied, while for a remote logging server, a formatter that
 * outputs to json, protobuf or another protocol may be more suitable.
 *
 * A formatter should make no assumptions about what kind of a logger will
 * use it.
 *
 * It is permitted for a formatter to have state, ex. for elaborate logging
 * of message history, with parent and children messages.
 *
 * @note All output is currently limited to UniStrings. Pure binary formats are
 * not supported at the moment. Before considering changing this, assert that
 * you really need logs as binary blobs...
 */
class OpLogFormatter
{
public:
	/**
	 * Called at the beginning of logging.
	 *
	 * Should produce an initial, opening log message.
	 *
	 * @param inout The implementation shall write its formatted log here.
	 * @return OpStatus:OK if everything went smoothly, an appropriate error
	 * status otherwise.
	 */
	virtual OP_STATUS FormatBeginLogging(OpUniStringStream& inout) = 0;

	/**
	 * Called when a message is handed of to dispatching.
	 *
	 * Should produce a suitable log message. The functionality is handled separately
	 * from the generic FormatLog() method to allow logging message hierarchies,
	 * ie. "This dispatch has caused the following actions".
	 *
	 * @param inout The implementation shall write its formatted log here.
	 * @param message Message associated with action.
	 * @return OpStatus:OK if everything went smoothly, an appropriate error
	 * status otherwise.
	 */
	virtual OP_STATUS FormatStartedDispatching(
		OpUniStringStream& inout,
		const OpTypedMessage& message) = 0;

	/**
	 * Called when a message has finished going through the dispatching process
	 * and is nearing the end of its life.
	 *
	 * Should produce a suitable log message. The functionality is handled separately
	 * from the generic FormatLog() method to allow logging message hierarchies,
	 * ie. "This dispatch has caused the following actions".
	 *
	 * @param inout The implementation shall write its formatted log here.
	 * @param message Message associated with action.
	 * @return OpStatus:OK if everything went smoothly, an appropriate error
	 * status otherwise.
	 */
	virtual OP_STATUS FormatFinishedDispatching(
		OpUniStringStream& inout,
		const OpTypedMessage& message) = 0;

	/**
	 * Called when a generic action is being performed on a message, ex.
	 * "Sending", "Adding to inbox" or "Delaying".
	 *
	 * Should produce a suitable log message.
	 *
	 * @param inout The implementation shall write its formatted log here.
	 * @param message Message associated with action.
	 * @param action The action that is being performed on @a message.
	 * @return OpStatus:OK if everything went smoothly, an appropriate error
	 * status otherwise.
	 */
	virtual OP_STATUS FormatLog(
		OpUniStringStream& inout,
		const OpTypedMessage& message,
		const uni_char* action) = 0;

	/**
	 * Called at the end of logging.
	 *
	 * Should produce a final, closing log message. No further calls to
	 * Format* methods are expected after this method returns.
	 *
	 * @param inout The implementation shall write its formatted log here.
	 * @return OpStatus:OK if everything went smoothly, an appropriate error
	 * status otherwise.
	 */
	virtual OP_STATUS FormatEndLogging(OpUniStringStream& inout) = 0;

	virtual ~OpLogFormatter() {}
};

#endif // HC_MESSAGE_LOGGING
#endif // MODULES_HARDCORE_LOGGING_OPLOGFORMATTER_H
