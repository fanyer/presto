/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_HARDCORE_LOGGING_OPREADABLELOGFORMATTER_H
#define MODULES_HARDCORE_LOGGING_OPREADABLELOGFORMATTER_H

#ifdef HC_MESSAGE_LOGGING
#include "modules/hardcore/logging/OpLogFormatter.h"

#include "modules/util/adt/opvector.h"

class OpTypedMessage;
class OpTimeProvider;

/**
 * An implementation of OpLogFormatter that produces human-readable logs.
 *
 * Each logged action has a time associated with it (if a time provider is
 * supplied at creation). The format contains indentation with '-' (minus)
 * signs for describing message dispatch hierarchy.
 * The formatter assigns internal unique
 * numbers to subsequent dispatches and maintains a stack of them. This allows
 * to trace which dispatch has triggered which actions.
 * The logs are expressed in a tree-like format with indentation and dispatch indexes
 * to allow following this "dispatch stack".
 */
class OpReadableLogFormatter : public OpLogFormatter
{
public:
	/**
	 * Constructor.
	 *
	 * @param timeProvider An entity that is able to provide time information.
	 * If NULL, time information will not be added to the output.
	 *
	 */
	OpReadableLogFormatter(OpTimeProvider* timeProvider);

	virtual OP_STATUS FormatBeginLogging(OpUniStringStream& inout);

	virtual OP_STATUS FormatStartedDispatching(
		OpUniStringStream& inout,
		const OpTypedMessage& message);

	virtual OP_STATUS FormatFinishedDispatching(
		OpUniStringStream& inout,
		const OpTypedMessage& message);

	virtual OP_STATUS FormatLog(
		OpUniStringStream& inout,
		const OpTypedMessage& message,
		const uni_char* action);

	virtual OP_STATUS FormatEndLogging(OpUniStringStream& inout);

private:
	/** Add the common log string that precedes the message description.
	 *
	 * Things like current time, indentation, dispatch trigger.
	 *
	 * @param inout StringStream to which the common strings are added.
	 */
	void Prefix(OpUniStringStream& inout) const;

	/** Push new dispatch index onto internal stack.
	 *
	 * @param newDispatchIndex Index that goes on stack.
	 * @return OpStatus::Ok if operation was succsessfull
	 */
	OP_STATUS Push(INT32 newDispatchIndex);

	/** Pop the top dispatch index from stack.
	 */
	void Pop();

	/** Get the index from top of the stack.
	 *
	 * @return Most recent dispatch index.
	 */
	INT32 GetCurrentDispatchIndex() const;

	/** Get the amount of indentation to prepend.
	 *
	 * @return The number of indentation characters to use. Equivalent to the
	 * depth of the dispatch stack.
	 */
	INT32 GetIndendation() const;
	OpTimeProvider* m_timeProvider;

	/// Contains unique indexes associated with subsequent dispatches.
	OpINT32Vector m_dispatchIndexStack;

	/// Continually incremented for unique dispatch indexes.
	INT32 m_runningIndex;
};

#endif // HC_MESSAGE_LOGGING
#endif // MODULES_HARDCORE_LOGGING_OPREADABLELOGFORMATTER_H
