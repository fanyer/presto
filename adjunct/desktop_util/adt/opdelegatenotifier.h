/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_DELEGATE_NOTIFIER_H
#define OP_DELEGATE_NOTIFIER_H

#include "adjunct/desktop_util/adt/opdelegate.h"
#include "adjunct/desktop_util/adt/optightvector.h"

template <typename Arg>
class OpDelegateNotifier
{
public:
	typedef OpDelegateBase<Arg> Delegate;

	/**
	 * Subscribes an object via the object's delegate.
	 *
	 * @param delegate represents the subscriber.  Ownership is transferred.
	 * @return status
	 */
	OP_STATUS Subscribe(Delegate* delegate);

	/**
	 * Cancels all of an object's subscriptions.
	 *
	 * @param subscriber the subscriber
	 */
	void Unsubscribe(const void* subscriber);

protected:
	void NotifyAll(Arg value);

private:
	OpAutoTightVector<Delegate*> m_delegates;
};



template <typename Arg>
OP_STATUS OpDelegateNotifier<Arg>::Subscribe(Delegate* delegate)
{
	if (delegate == NULL)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	const OP_STATUS result = m_delegates.Add(delegate);
	if (OpStatus::IsError(result))
	{
		OP_DELETE(delegate);
	}

	return result;
}

template <typename Arg>
void OpDelegateNotifier<Arg>::Unsubscribe(const void* subscriber)
{
	for (UINT32 i = m_delegates.GetCount(); i-- > 0; )
	{
		if (m_delegates[i]->Represents(subscriber))
		{
			m_delegates.Remove(i);
		}
	}
}

template <typename Arg>
void OpDelegateNotifier<Arg>::NotifyAll(Arg value)
{
	for (UINT32 i = 0; i < m_delegates.GetCount(); ++i)
	{
		(*m_delegates[i])(value);
	}
}

#endif // OP_DELEGATE_NOTIFIER_H
