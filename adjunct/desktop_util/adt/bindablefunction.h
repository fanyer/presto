/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
#ifndef BINDABLE_FUNCTION_H
#define BINDABLE_FUNCTION_H

#include "modules/util/opautoptr.h"


/**
 * A helper interface used in BindableFunction implementation.
 */
class FunctionImpl
{
public:
	virtual ~FunctionImpl()  {}
	virtual FunctionImpl* Clone() const = 0;
	virtual void Call() = 0;
};

/**
 * The FunctionImpl implementation used by BindableFunction when adapting
 * a no-arg functor.
 */
template <typename Functor>
class NoArgFunctionImpl : public FunctionImpl
{
public:
	explicit NoArgFunctionImpl(Functor functor) : m_functor(functor)
		{}

	virtual NoArgFunctionImpl* Clone() const
		{ return OP_NEW(NoArgFunctionImpl, (*this)); }

	virtual void Call()
		{ m_functor(); }

private:
	Functor m_functor;
};

/**
 * The FunctionImpl implementation used by BindableFunction when adapting
 * a 1-arg functor.
 */
template <typename Functor, typename Arg1>
class OneArgFunctionImpl : public FunctionImpl
{
public:
	OneArgFunctionImpl(Functor functor, const Arg1& arg1)
		: m_functor(functor), m_arg1(arg1)
	{}

	virtual OneArgFunctionImpl* Clone() const
		{ return OP_NEW(OneArgFunctionImpl, (*this)); }

	virtual void Call()
		{ m_functor(m_arg1); }

private:
	Functor m_functor;
	Arg1 m_arg1;
};

/**
 * The FunctionImpl implementation used by BindableFunction when adapting
 * a 2-arg functor.
 */
template <typename Functor, typename Arg1, typename Arg2>
class TwoArgFunctionImpl : public FunctionImpl
{
public:
	TwoArgFunctionImpl(Functor functor, const Arg1& arg1, const Arg2& arg2)
		: m_functor(functor), m_arg1(arg1), m_arg2(arg2)
	{}

	virtual TwoArgFunctionImpl* Clone() const
		{ return OP_NEW(TwoArgFunctionImpl, (*this)); }

	virtual void Call()
		{ m_functor(m_arg1, m_arg2); }

private:
	Functor m_functor;
	Arg1 m_arg1;
	Arg2 m_arg2;
};


/**
 * Adapts any functor so that it can be called with a single Call().  The real
 * purpose is having a single interface for calling functors with zero or more
 * arguments.  If the adaptee does accept arguments, they are bound to the
 * BindableFunction instance at construction.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @see Finalizer for usage examples
 */
class BindableFunction
{
public:
	template <typename Functor>
	explicit BindableFunction(Functor functor)
		: m_impl(OP_NEW(NoArgFunctionImpl<Functor>, (functor)))
	{}

	template <typename Functor, typename Arg1>
	BindableFunction(Functor functor, const Arg1& arg1)
		: m_impl(OP_NEW((OneArgFunctionImpl<Functor, Arg1>), (functor, arg1)))
	{}

	template <typename Functor, typename Arg1, typename Arg2>
	BindableFunction(Functor functor, const Arg1& arg1, const Arg2& arg2)
		: m_impl(OP_NEW((TwoArgFunctionImpl<Functor, Arg1, Arg2>),
					(functor, arg1, arg2)))
	{}

	// Add support for more function arguments here at will...

	BindableFunction(const BindableFunction& rhs)
		: m_impl(rhs.m_impl->Clone())
	{
	}
	BindableFunction& operator=(const BindableFunction& rhs)
	{
		if (this != &rhs)
		{
			m_impl.reset(rhs.m_impl->Clone());
		}
		return *this;
	}

	void Call()
		{ m_impl->Call(); }

private:
	OpAutoPtr<FunctionImpl> m_impl;
};

#endif // BINDABLE_FUNCTION_H
