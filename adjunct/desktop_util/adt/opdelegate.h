/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_DELEGATE_H
#define OP_DELEGATE_H


/**
 * The common interface of all OpDelegate classes with a specific member
 * function argument and result types.
 *
 * @param ArgT member function argument type
 * @param ResultT member function result type
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @author Arjan van Leeuwen (arjanl)
 */
template <typename ArgT, typename ResultT = void>
class OpDelegateBase
{
public:
	typedef ArgT ArgType;
	typedef ResultT ResultType;

	virtual ~OpDelegateBase()  {}

	/**
	 * Supports invoking the delegate object like a normal function.
	 *
	 * @param arg the argument to pass to the member function
	 */
	ResultT operator()(ArgT arg)
		{ return Invoke(arg); }

	/**
	 * @param object an object
	 * @return @c true iff this delegate represents @a object
	 */
	bool Represents(const void* object) const
		{ return object == GetTarget(); }

protected:
	virtual ResultT Invoke(ArgT arg) = 0;
	virtual const void* GetTarget() const = 0;
};


/**
 * A delegate is an object that calls a member function on behalf of another
 * object (the target).
 *
 * In many cases, MAKE_DELEGATE is the most convenient way to create OpDelegate
 * instances.
 *
 * Example:
 * @code
 * 	class TargetClass
 * 	{
 * 	public:
 * 		void MemberFunction(int& arg)  {}
 * 	};
 *
 *  TargetClass target;
 *
 *  // This is equivalent to "target.MemberFunction(12);"
 *  (*MAKE_DELEGATE(target, &TargetClass::MemberFunction))(12);
 * @endcode
 *
 * @param T target object type
 * @param ArgT member function argument type
 * @param ResultT member function result type
 * @param FunctionPtr pointer to a member function in @a T
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @author Arjan van Leeuwen (arjanl)
 *
 * @see MAKE_DELEGATE
 */
template <typename T, typename ArgT, typename ResultT,
		 ResultT (T::*FunctionPtr)(ArgT)>
class OpDelegate : public OpDelegateBase<ArgT, ResultT>
{
public:
	/**
	 * Constructor.
	 *
	 * @param target the target object
	 */
	explicit OpDelegate(T& target) : m_target(&target)  {}

protected:
	virtual ResultT Invoke(ArgT arg)
		{ return (m_target->*FunctionPtr)(arg); }

	virtual const void* GetTarget() const
		{ return m_target; }

private:
	T* const m_target;
};



/**
 * A factory of OpDelegates.  Used in combination with MakeDelegateFactory to
 * make the MAKE_DELEGATE shortcut possible.
 *
 * @see OpDelegate
 * @see MakeDelefateFactory
 * @see MAKE_DELEGATE
 */
template <typename T, typename ArgT, typename ResultT>
struct OpDelegateFactory
{
	template <ResultT (T::*FunctionPtr)(ArgT)>
	OpDelegate<T, ArgT, ResultT, FunctionPtr>* MakeDelegate(T& target) const
	{
		typedef OpDelegate<T, ArgT, ResultT, FunctionPtr> Delegate;
		return OP_NEW(Delegate, (target));
	}
};


/**
 * Convenience function that lets you instantiate OpDelegateFactory without
 * specifying template arguments.
 *
 * @param function pointer to a member function in @a T
 * @return an OpDelegateFactory instance for creating OpDelegate objects able to
 * 		call @a function on a @a T instance
 *
 * @see MAKE_DELEGATE
 */
template <typename T, typename ArgT, typename ResultT>
OpDelegateFactory<T, ArgT, ResultT> MakeDelegate(
		ResultT (T::*function)(ArgT))
{
	return OpDelegateFactory<T, ArgT, ResultT>();
}


/**
 * A shortcut to instantiating OpDelegate without specifying template arguments
 * or specifying the member function pointer twice.
 *
 * @param target the target object
 * @param function pointer to a member function in the @a target's class
 * @return an OpDelegate instance able to call @a function on @a target
 */
#define MAKE_DELEGATE(target, function) \
	MakeDelegate(function).MakeDelegate<function>(target)


#endif // OP_DELEGATE_H
