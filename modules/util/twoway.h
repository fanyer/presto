/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_UTIL_TWOWAY_H
#define MODULES_UTIL_TWOWAY_H
#include "modules/util/simset.h"

class TwoWayPointer_Base;

/**
 * TwoWayPointer_Target MUST be inherited by all classes that are to be used as
 * type T in the TwoWayPointer<T> template class-family.
 *
 * Functionality provided:
 * - The class maintains a list of all TwoWay pointers that points to the object
 *   it is part of.
 * - When this object is destroyed all TwoWay pointers are NULLed, and it is
 *   there will be no dangling pointers (as far as the TwoWay pointers are
 *   concerned).
 * - A TwoWayPointer_Target MAY send action signals to all TwoWayPointers that
 *   will allow pointers of the TwoWayPointer_WithAction template to perform
 *   specific actions on request from the target. See TwoWayPointer_WithAction
 *   for more information.
 *
 * All Action codes to SignalActionL SHOULD start at
 * TwoWayPointer_Target::TWOWAYPOINTER_ACTION_USER+0
 */
class TwoWayPointer_Target
{
private:
	friend class TwoWayPointer_Base;

	/** Back-pointer list.
	 *
	 * List of TwoWayPointers that point to this.
	 */
	Head	pointers;

	/**
	 * Registers point as a pointer that points to this target
	 */
	void Register(TwoWayPointer_Base *point);

public:
	/** Detach from every TwoWayPointer that points here. */
	~TwoWayPointer_Target();

	/** Action codes for SignalActionL().
	 *
	 * Both this class and TwoWayPointer have SignalActionL() methods.
	 */
	enum
	{
		/** Action code reserved for destruction. */
		TWOWAYPOINTER_ACTION_DESTROY = 0,
		/** Action codes less than this are reserved for the system. */
		TWOWAYPOINTER_ACTION_USER = 0x1000
	};

public:
	/** Send an action signal to all pointers that points to this target
	 *
	 * @param action Identifies the action to take.  Action code
	 * TwoWayPointer_Target::TWOWAYPOINTER_ACTION_DESTROY (zero) is reserved for
	 * the destructor and MUST NEVER be used in a call to this function.  Codes
	 * in the range TwoWayPointer_Target::TWOWAYPOINTER_ACTION_DESTROY to
	 * TwoWayPointer_Target::TWOWAYPOINTER_ACTION_USER-1 are reserved for future
	 * use (presently 0-0x0FFF, inclusive).
	 */
	void SignalActionL(uint32 action);
};

/** Base class for TwoWayPointers
 *
 * Performs actual registration with target, and automatically
 * deregisters when the object is destroyed.
 *
 * The target may send an action signal that can be optionally
 * handled by the parent class.
 */
class TwoWayPointer_Base : private Link
{
	friend class TwoWayPointer_Target; // Needs to be able to Into(), and Link is private.
	void RawRelease() { if (InList()) Out(); }

public:
	/** Destructor. Performs automatice release from target */
	virtual ~TwoWayPointer_Base() { RawRelease(); }

protected:
	/** Release this object from the target */
	virtual void Internal_Release() { RawRelease(); }

	/** Register this object as one that points to the target obj */
	void BaseRegister(TwoWayPointer_Target *obj)
	{
		RawRelease();
		if (obj) obj->Register(this);
	}

	/** Perform the action requested by the target
	 *
	 * @param action A TwoWayPointer_Target::TWOWAYPOINTER_ACTION_* code.
	 * Action code TWOWAYPOINTER_ACTION_DESTROY (zero) is used when the target
	 * object is destroyed.  Codes in the range
	 * TwoWayPointer_Target::TWOWAYPOINTER_ACTION_DESTROY to
	 * TwoWayPointer_Target::TWOWAYPOINTER_ACTION_USER-1 are reserved and MUST
	 * NOT be used in calls to this function
	 */
	virtual void SignalActionL(uint32 action);
};
// Inline outside class to avoid unused variable warning:
inline void TwoWayPointer_Base::SignalActionL(uint32 action) {}
// Can only compile after TwoWayPointer_Base's declaration:
inline void TwoWayPointer_Target::Register(TwoWayPointer_Base *point)
{
	if (point)
		point->Into(&pointers);
}

/** TwoWayPointer template.
 *
 * This class points to the class T (derived from TwoWayPointer_Target), and
 * allows the target object to automatically NULL this pointer when it is
 * destroyed.
 *
 * The target may also send action signals to the pointers (the default template
 * ignores these calls)
 *
 * The API is close the OpStackAutoPtr template class, but reset DOES NOT delete
 * the target.
 */
template <class T>
class TwoWayPointer : public TwoWayPointer_Base
{
private:
	T *m_target;

public:
	/** Create pointer that points to obj */
	TwoWayPointer(T *obj=NULL) : m_target(NULL) { Internal_Register(obj); }

	/** Destructor */
	virtual ~TwoWayPointer() { m_target = NULL; }

	/** Get the pointer value */
	T *get() const { return m_target; }

	/** NULL the pointer, but return the original */
	T *release() { T *p1 = m_target; Internal_Release(); return p1; }

	/** Reassign the new value obj as the target */
	void reset(T *obj=NULL) { Internal_Register(obj); }

	/** Conversion operator */
	operator T*() const { return m_target; }

#if 0
	/* This causes compile errors on many of our less capable compilers; and we
	 * seem to be able to live without it. */
	/** Conversion operator */
	operator const T*() const { return m_target; }
#endif

	/** Pointer member dereferencing */
	T *operator ->() const { return m_target; }

	/** Assign a new value to the pointer */
	TwoWayPointer &operator =(T *obj) { Internal_Register(obj); return *this; }

protected:
	/** Perform release of the pointer */
	virtual void Internal_Release()
		{ m_target = NULL; TwoWayPointer_Base::Internal_Release(); }

	/** Register a new target, releasing the old */
	virtual void Internal_Register(T *obj) { m_target = obj; BaseRegister(obj); }
};

/** Action base-class. */
class TwoWayAction_Target
{
public:
	virtual ~TwoWayAction_Target() {}
	/** The action to perform.
	 *
	 * @param source The target of the action.
	 * @param action The action code for what action to perform; see
	 * TwoWayPointer_Target::SignalActionL().
	 */
	virtual void TwoWayPointerActionL(TwoWayPointer_Target *source, uint32 action) = 0;
};

/** Extends TwoWayPointer to interact with signals triggering actions.
 *
 * TwoWayPointer_WithAction_Basic has the same funcitonality as TwoWayPointer,
 * but allows the owner of the pointer to catch signal actions from another
 * object, and perform operations based on a signal code.
 */
template <class T>
class TwoWayPointer_WithAction : public TwoWayPointer<T>
{
private:
	TwoWayAction_Target *m_agent;

public:
	/** Create pointer that points to obj, no action object */
	TwoWayPointer_WithAction(T *obj=NULL)
		: TwoWayPointer<T>(obj), m_agent(NULL) {}

	/** Create pointer that points to obj, with an action object */
	TwoWayPointer_WithAction(TwoWayAction_Target *act_obj, T *obj=NULL)
		: TwoWayPointer<T>(obj), m_agent(act_obj) {}

	/** Destructor */
	virtual ~TwoWayPointer_WithAction() { m_agent = NULL; }

	/** Set a new action object */
	void SetActionObject(TwoWayAction_Target *act_obj) { m_agent = act_obj; }

	/** Assign a new value to the pointer */
	TwoWayPointer<T> &operator =(T *obj) { return TwoWayPointer<T>::operator =(obj); }
protected:
	/** Handle an action.
	 *
	 * Catches the SignalAction and calls the TwoWayPointerActionL function in
	 * the action object with the current pointer value as one of the arguments
	 * @param action Identifies what action to take.  See TwoWayAction_Target.
	 */
	virtual void SignalActionL(uint32 action)
		{ if (m_agent) m_agent->TwoWayPointerActionL(this->get(), action); }
};

#endif // !MODULES_UTIL_TWOWAY_H
