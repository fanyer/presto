/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef NS4P_HOOKED_H
#define NS4P_HOOKED_H

#include "modules/otl/list.h"

/**
 * Hooks are class-local listeners created to reduce state members and turn
 *
 *    ...
 *    m_state = X;
 *    ...
 *
 *    ...
 *    if (m_state == X)
 *        Foo();
 *    ...
 *
 * into
 *
 *    ...
 *    m_event_hook.Append(&DoX());
 *    ...
 *
 *    ...
 *    RunHooks(m_event_hook);
 *    ...
 *
 * where DoX() is a protected method.
 */
template <class T>
class Hook
{
public:
	/**
	 * Hook method.
	 *
	 * When run by a hook, returning an error will prevent any later hook methods from being
	 * called. See Runhook().
	 */
	typedef OP_STATUS (T::*HookMethod)();

	/**
	 * Append hook method to hook.
	 *
	 * @param hook Hook to append method to.
	 * @param method Method to append.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS Append(HookMethod method) { return m_methods.Append(method); }

	/**
	 * Prepend hook method to hook.
	 *
	 * @param hook Hook to prepend method to.
	 * @param method Method to prepend.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS Prepend(HookMethod method) { return m_methods.Prepend(method); }

	/**
	 * Remove hook method from hook.
	 *
	 * @param hook Hook to remove method from.
	 * @param method Method to remove.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_SUCH_RESOURCE if method was not found.
	 */
	OP_STATUS Remove(HookMethod method) { return m_methods.PopItem(method) ? OpStatus::OK : OpStatus::ERR_NO_SUCH_RESOURCE; }

	typename OtlList<HookMethod>::ConstIterator Begin() const { return m_methods.Begin(); }
	typename OtlList<HookMethod>::ConstIterator End() const { return m_methods.End(); }

protected:
	OtlList<HookMethod> m_methods;
};

template <class T>
class Hooked
{
protected:
	/**
	 * Call all hook methods added to a hook.
	 *
	 * If a hook method returns an error, no further hook methods will be run.
	 *
	 * @param hook The hook whose methods are to be called.
	 *
	 * @return OpStatus::OK on success, otherwise return value from the first
	 * hook method that failed.
	 */
	OP_STATUS RunHook(const Hook<T>& hook);
};

template <class T> OP_STATUS
Hooked<T>::RunHook(const Hook<T>& hook)
{
	/* Make a local copy of the hook method list to allow the hook methods to manipulate the list. */
	OtlList<typename Hook<T>::HookMethod> methods_copy;
	for (typename OtlList<typename Hook<T>::HookMethod>::ConstIterator method = hook.Begin(); method != hook.End(); ++method)
		methods_copy.Append(*method);

	/* Invoke hook methods. */
	for (typename OtlList<typename Hook<T>::HookMethod>::ConstIterator method = methods_copy.Begin(); method != methods_copy.End(); ++method)
		RETURN_IF_ERROR((static_cast<T*>(this)->*(*method))());

	/* Implementation note: The last hook method is allowed to delete 'this', hence it is NOT safe
	 * to perform any member interaction after all hooks are run. */

	return OpStatus::OK;
}

#endif // NS4P_HOOKED_H
