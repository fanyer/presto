/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef HC_MESSAGE_LOGGING

#include "modules/hardcore/logging/OpReadableLogFormatter.h"

#include "modules/hardcore/component/OpTimeProvider.h"
#include "modules/hardcore/component/OpTypedMessage.h"

OpReadableLogFormatter::OpReadableLogFormatter(OpTimeProvider* timeProvider) :
	m_timeProvider(timeProvider), m_runningIndex(0)
{
}

OP_STATUS OpReadableLogFormatter::FormatBeginLogging(OpUniStringStream& inout)
{
	inout << UNI_L("Starting message log, current time: ");
	if(m_timeProvider)
		inout << m_timeProvider->GetRuntimeMS();
	else
		inout << UNI_L("Unknown");
	inout  << UNI_L("\n");
	return OpStatus::OK;
}

OP_STATUS OpReadableLogFormatter::FormatEndLogging(OpUniStringStream& inout)
{
	inout << UNI_L("Ending message log, current time: ");
	if(m_timeProvider)
		inout << m_timeProvider->GetRuntimeMS();
	else
		inout << UNI_L("Unknown");
	inout  << UNI_L("\n");
	return OpStatus::OK;
}

OP_STATUS OpReadableLogFormatter::FormatStartedDispatching(
	OpUniStringStream& inout,
	const OpTypedMessage& message)
{
	Prefix(inout);
	OP_STATUS status = Push(++m_runningIndex);
	inout << UNI_L("[New dispatch #: ") << GetCurrentDispatchIndex() << UNI_L("]")
	   << UNI_L(": Dispatching ") << message
	   << UNI_L("\n");
	return status;
}

OP_STATUS OpReadableLogFormatter::FormatFinishedDispatching(
	OpUniStringStream& inout,
	const OpTypedMessage&)
{
	Prefix(inout);
	inout << UNI_L("[Done dispatching #: ") << GetCurrentDispatchIndex()
		  << UNI_L("]\n");
	Pop();
	return OpStatus::OK;
}

OP_STATUS OpReadableLogFormatter::FormatLog(
	OpUniStringStream& inout,
	const OpTypedMessage& message,
	const uni_char* action)
{
	Prefix(inout);
	inout << UNI_L(": ") << action << UNI_L(" ") << message << UNI_L("\n");
	return OpStatus::OK;
}

void OpReadableLogFormatter::Prefix(OpUniStringStream& inout) const
{
	if(m_timeProvider)
		inout << UNI_L("[") << m_timeProvider->GetRuntimeMS() << UNI_L("]");
	for(INT32 i = 0; i < GetIndendation(); ++i)
		inout << UNI_L("-");
	inout << UNI_L("[Result of dispatch #: ") << GetCurrentDispatchIndex()
		  << UNI_L("]");
}

OP_STATUS OpReadableLogFormatter::Push(INT32 newDispatchIndex)
{
	return m_dispatchIndexStack.Add(newDispatchIndex);
}

void OpReadableLogFormatter::Pop()
{
	UINT32 stackSize = m_dispatchIndexStack.GetCount();
	if(stackSize == 0)
		return;
	m_dispatchIndexStack.Remove(stackSize - 1);
}

INT32 OpReadableLogFormatter::GetCurrentDispatchIndex() const
{
	INT32 stackSize = m_dispatchIndexStack.GetCount();
	if (stackSize == 0)
		return 0;
	return m_dispatchIndexStack.Get(stackSize - 1);
}

INT32 OpReadableLogFormatter::GetIndendation() const
{
	return m_dispatchIndexStack.GetCount();
}

#endif // HC_MESSAGE_LOGGING
