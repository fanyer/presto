/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_PROPERTY_H
#define OP_PROPERTY_H

#include "adjunct/desktop_util/adt/opdelegatenotifier.h"
#include "modules/hardcore/mem/mem_man.h"

class OpINT32Vector;

/**
 * Property-related primitives used by OpProperty to compare and assign values
 */
namespace OpPropertyFunctions
{
	// Helper
	inline void ReportIfError(OP_STATUS status)
		{ if (OpStatus::IsError(status)) g_memory_manager->RaiseCondition(status); }

	// Generic comparison and assignment
	template <typename Lhs, typename Rhs>
		inline BOOL Equals(const Lhs& lhs, const Rhs& rhs) { return lhs == rhs; }

	template <typename Lhs, typename Rhs>
		inline void Assign(Lhs& lhs, const Rhs& rhs) { lhs = rhs; }

	// Overrides for specific types
	BOOL Equals(const OpINT32Vector& lhs, const OpINT32Vector& rhs);
	void Assign(OpINT32Vector& lhs, const OpINT32Vector& rhs);
	inline void Assign(OpString& lhs, const OpStringC& rhs) { ReportIfError(lhs.Set(rhs)); }
	inline void Assign(OpString8& lhs, const OpStringC8& rhs) { ReportIfError(lhs.Set(rhs)); }
}



/**
 * Helps generate OpProperty implementations for different value types.
 *
 * @param T the property value type
 * @param ArgT the type used in preference of T for function arguments
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @see OpProperty
 */
template <typename T, typename ArgT>
class OpPropertyBase : public OpDelegateNotifier<ArgT>
{
public:
	typedef T ValueType;
	typedef ArgT ArgType;
	typedef ArgT ResultType;

	/**
	 * Constructs OpPropertyBase and value-initializes m_value.
	 *
	 * If @a T is a class type, the default constructor for @a T is called to
	 * initialize m_value.  Otherwise, m_value is zero-initialized.
	 */
	OpPropertyBase() : m_value() {}
	OpPropertyBase(ArgType value) : m_value(value) {}

	virtual ~OpPropertyBase() {}

	/**
	 * @return property value
	 */
	ResultType Get() const { return m_value; }

	/**
	 * Sets the property value.  If @a value is not equal to the current value,
	 * all subscribers are notified of value change.
	 *
	 * @param value the new value
	 */
	void Set(ArgType value)
	{
		if (Assign(value))
		{
			this->NotifyAll(m_value);
		}
	}

protected:
	/**
	 * Makes sure the property value is @a value.
	 *
	 * @param value the new value
	 * @return @c TRUE iff property value has changed
	 */
	virtual BOOL Assign(ArgType value)
	{
		const BOOL equal = OpPropertyFunctions::Equals(m_value, value);
		if (!equal)
		{
			OpPropertyFunctions::Assign(m_value, value);
		}
		return !equal;
	}

private:
	ValueType m_value;
};


/**
 * A property is an object providing read/write access to its value and
 * subscriptions to value change notifications.  Subscriptions are based on
 * OpDelegate, which promotes loose coupling by not requiring subscriber to
 * implement a specific interface.
 *
 * The contents of an OpProperty object are value-initialized at construction,
 * see OpPropertyBase::OpPropertyBase().
 *
 * Example:
 * @code
 *	class Data
 *	{
 * 	public:
 * 		OpProperty<int> m_int;
 * 	};
 *
 *	class Subscriber
 *	{
 *	public:
 *		explicit Subscriber(Data& data)
 *		{
 *			data.m_int.Subscribe(MAKE_DELEGATE(*this, &Subscriber::OnDataChanged));
 *		}
 *		~Subscriber()
 *		{
 *			data.m_int.Unsubscribe(this);
 *		}
 *
 * 		// Will be called each time data.m_int changes value:
 *		void OnDataChanged(int new_value)  {}
 *	};
 * @endcode
 *
 * @param T the value type.  Must be default constructible.  Preferably, it
 *		should also be equality comparable and "="-assignable from another T
 *		instance -- otherwise, you will need specializations as is the case for
 *		OpString and OpString8.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 * @see OpDelegate
 * @see OpFilteredProperty
 */
template <typename T>
class OpProperty : public OpPropertyBase<T, T>
{
public:
	OpProperty() {}
	OpProperty(T value) : OpPropertyBase<T, T>(value) {}
};

template <>
class OpProperty<OpString> : public OpPropertyBase<OpString, const OpStringC&>
{
};

template <>
class OpProperty<OpString8> : public OpPropertyBase<OpString8, const OpStringC8&>
{
};

template <>
class OpProperty<OpINT32Vector>
		: public OpPropertyBase<OpINT32Vector, const OpINT32Vector&>
{
};


/**
 * Collects general-purpose property value filters.
 */
struct OpPropertyFilters
{
	void Strip(OpString& string)
		{ string.Strip(); }
};


#endif // OP_PROPERTY_H
