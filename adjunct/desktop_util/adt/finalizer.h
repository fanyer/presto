/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef FINALIZER_H
#define FINALIZER_H

#include "adjunct/desktop_util/adt/bindablefunction.h"

/**
 * Calls a function at destruction.
 *
 * By far the most common reason to need a finalizer is when one is serious
 * about releasing resources (other than memory, for which we already have smart
 * pointers, smart vectors, smart lists, etc.).
 *
 * Say you have to release a Windows registry handle before exiting from a
 * method.  With Finalizer, one is able to get by with something like:
 * @code
 *	OP_STATUS SomeClass::SomeMethod()
 *	{
 *		HKEY handle = AcquireHandle();
 *		Finalizer finalizer(BindableFunction(&RegCloseKey, handle));
 *		// ...
 *		RETURN_IF_ERROR(...);
 *		// ...
 *		return OpStatus::OK;
 *	}
 * @endcode
 * Whatever the control path, SomeMethod() never returns without releasing
 * the handle.
 *
 * One may not be so lucky as to have a ready-to-use function they can hand on
 * to Finalizer.  This introduces a slight complication, but it can still be
 * argued that the benefits outweigh the extra effort:
 * @code
 *	struct ReleaseResource
 *	{
 *		void operator()(Resource* res)  { ... }
 *	};
 *
 *	OP_STATUS SomeClass::AnotherMethod()
 *	{
 *		Resource res = AcquireResource();
 *		Finalizer finalizer(BindableFunction(ReleaseResource(), &res));
 *
 *		// ...
 *		RETURN_IF_ERROR(...);
 *		// ...
 *		return OpStatus::OK;
 *	}
 * @endcode
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class Finalizer
{
public:
	explicit Finalizer(const BindableFunction& function) : m_function(function)
		{}

	~Finalizer()
		{ m_function.Call(); }

private:
	BindableFunction m_function;
};

#endif // FINALIZER_H
