/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_SMARTPTR_H
#define MODULES_UTIL_SMARTPTR_H

#include "modules/util/simset.h"

/**
 *  OpReferenceCounter is the base class used for all classes that
 *  will be referenced by OpSmartPointer instances.
 */
class OpReferenceCounter
{
private:
	unsigned int reference_counter;
public:
#ifdef _DEBUG
#ifdef YNP_WORK
	Head debug_list;
#endif
#endif

	/** Default constructor */
	OpReferenceCounter(){reference_counter = 0;}

	/** Copy Constructor. Works like Default constructor */
	OpReferenceCounter(OpReferenceCounter &){reference_counter = 0;}

	/** Destructor */
	virtual ~OpReferenceCounter(){
#ifdef _DEBUG
#ifdef YNP_WORK
		//OP_ASSERT(debug_list.Empty());
#endif
#endif
		//OP_ASSERT(reference_counter == 0);
	}

	/** Assignment operator. Does not do anything */
	OpReferenceCounter &operator =(OpReferenceCounter &){return *this;}

	/** Increment reference count*/
	unsigned int Increment_Reference(){return ++reference_counter;}

	/** Decrement reference count. Cannot decrement below 0*/
	unsigned int Decrement_Reference(){if (reference_counter){ reference_counter--;} return reference_counter;}

	/** Returns the current reference count for this object */
	unsigned int Get_Reference_Count() const {return reference_counter;}
};

/**
 *  SmartPointer template class
 *
 *  This class takes care of all housekeeping concerning reference counting.
 *
 *  NOTE This implementation DOES NOT delete the target if the reference
 *  count is reduced to zero. Cleanup is assumed to be done by designated
 *  code.
 *
 *	@param	T	Class of type OpReferenceCounter
 */
template<class T> class OpSmartPointerNoDelete
#ifdef _DEBUG
#ifdef YNP_WORK
 : public Link
#endif
#endif
{
private:
	T	*object;

public:

	/** Default Constructor */
	OpSmartPointerNoDelete(): object(NULL){};

	/**
	 *	Copy Constructor
	 *
	 *	@param ptr	Another Smart pointer, after this call this object will
	 *				also reference the object ptr references
	 */
	OpSmartPointerNoDelete(const OpSmartPointerNoDelete &ptr): object(NULL){SetNewValue((T *) ptr.object);};

	/**
	 *	Constructor with pointer argument
	 *
	 *	@param	ptr	This object will be pointing to the object ptr is pointing to
	 */
	OpSmartPointerNoDelete(T *ptr): object(NULL){SetNewValue((T *) ptr);};

	/** Destructor, the reference counter of the current object (if any) will be  decremented */
	~OpSmartPointerNoDelete(){SetNewValue(NULL);};

	/** Returns the current pointer of this object */
	operator T *(){return object;}

	/** Returns the current pointer of this object */
	operator const T *() const {return object;}

	/** Returns the current pointer of this object */
	T *operator ->(){return object;}

	/** Returns the current pointer of this object */
	const T *operator ->() const {return object;}

	/** Returns a reference to the currently referenced object
	 *	NOTE: This function DOES NOT take into account NULL pointers,
	 *	but in debug mode an OP_ASSERT will be triggered.
	 */
	T &operator *(){OP_ASSERT(object); return *object;}

	/** Returns a reference to the currently referenced object
	 *	NOTE: This function DOES NOT take into account NULL pointers,
	 *	but in debug mode an OP_ASSERT will be triggered.
	 */
	const T &operator *() const {OP_ASSERT(object); return *object;}

	/**
	 *	Equality operator. Returns TRUE (non-zero) if the pointers of both this
	 *	object and ptr are the same, FALSE (zero) if they are not.
	 */
	BOOL operator ==(const OpSmartPointerNoDelete &ptr) const {return (object == ptr.object);}

	/**
	 *	Equality operator. Returns TRUE (non-zero) if the pointer of this
	 *	object and ptr are the same, FALSE (zero) if they are not.
	 */
	BOOL operator ==(T *ptr) const {return (object == ptr);}

	/**
	 *	Inequality operator. Returns TRUE (non-zero) if the pointers of both this
	 *	object and ptr are not the same, FALSE (zero) if they are the same.
	 */
	BOOL operator !=(const OpSmartPointerNoDelete &ptr) const {return (object != ptr.object);}

	/**
	 *	Inequality operator. Returns TRUE (non-zero) if the pointer of this
	 *	object and ptr are not the same, FALSE (zero) if they are the same.
	 */
	BOOL operator !=(T *ptr) const {return (object != ptr);}

	/**
	 *	Assignment operator
	 *
	 *	This object will be set to point at the same object that ptr is pointing to, with
	 *	reference counters appropriately set for the old and new objects
	 *
	 *	@param	ptr		Reference to the smartpointer holding the new pointer value. The object is not updated
	 *
	 *	@return		Reference to this Object
	 */
	OpSmartPointerNoDelete &operator =(const OpSmartPointerNoDelete &ptr){SetNewValue((T *) ptr.object);return *this;};

	/**
	 *	Assignment operator
	 *
	 *	This object will be set to point at the same object that ptr is pointing to, with
	 *	reference counters appropriately set for the old and new objects
	 *
	 *	@param	ptr		Pointer to the object that this object will point to.
	 *
	 *	@return		Reference to this Object
	 */
	OpSmartPointerNoDelete &operator =(T *ptr){SetNewValue((T *) ptr); return *this;};

private:
	/**
	 *  Decrements the reference counter of the previous object,
	 *  increments the reference counte of the new object, and assigns it to
	 *  the object pointer.

	 *	@param  new_value	Pointer to a OpReferenceCounter object.
	 *						This pointer-value will be assigned to target
	 *						If non-NULL the reference counter of the referenced object
	 *						will be incremented.
	 */
	void SetNewValue(T *new_value)
	{
#if defined (_DEBUG) && defined(YNP_WORK)
		if(InList())
			Out();
		if(new_value)
			Into(&new_value->debug_list);
#endif // _DEBUG && YNP_WORK

		if(object)
		{
			object->Decrement_Reference();
		}
		object = new_value;
		if(object)
		{
			object->Increment_Reference();
		}
	}
};


/**
 *  SmartPointer template class
 *
 *  This class takes care of all housekeeping concerning reference counting.
 *
 *  NOTE: This implementation deletes the target if the reference
 *  count is reduced to zero.
 *
 *	@param	T	Class of type OpReferenceCounter
 */
template<class T> class OpSmartPointerWithDelete
#ifdef _DEBUG
#ifdef YNP_WORK
 : public Link
#endif
#endif
{
private:
	T	*object;

public:

	/** Default Constructor */
	OpSmartPointerWithDelete(): object(NULL){};

	/**
	 *	Copy Constructor
	 *
	 *	@param ptr	Another Smart pointer, after this call this object will
	 *				also reference the object ptr references
	 */
	OpSmartPointerWithDelete(const OpSmartPointerWithDelete &ptr): object(NULL){SetNewValue((T *) ptr.object);};

	/**
	 *	Constructor with pointer argument
	 *
	 *	@param	ptr	This object will be pointing to the object ptr is pointing to
	 */
	OpSmartPointerWithDelete(T *ptr): object(NULL){SetNewValue((T *) ptr);};

	/** Destructor, the reference counter of the current object (if any) will be  decremented */
	~OpSmartPointerWithDelete(){SetNewValue(NULL);};

	/** Returns the current pointer of this object */
	operator T *(){return object;}

	/** Returns the current pointer of this object */
	operator const T *() const {return object;}

	/** Returns the current pointer of this object */
	T *operator ->(){return object;}

	/** Returns the current pointer of this object */
	const T *operator ->() const {return object;}

	/** Returns a reference to the currently referenced object
	 *	NOTE: This function DOES NOT take into account NULL pointers,
	 *	but in debug mode an OP_ASSERT will be triggered.
	 */
	T &operator *() const {OP_ASSERT(object); return *object;}

	/**
	 *	Equality operator. Returns TRUE (non-zero) if the pointers of both this
	 *	object and ptr are the same, FALSE (zero) if they are not.
	 */
	BOOL operator ==(const OpSmartPointerWithDelete &ptr) const {return (object == ptr.object);}

	/**
	 *	Equality operator. Returns TRUE (non-zero) if the pointer of this
	 *	object and ptr are the same, FALSE (zero) if they are not.
	 */
	BOOL operator ==(T *ptr) const {return (object == ptr);}

	/**
	 *	Inequality operator. Returns TRUE (non-zero) if the pointers of both this
	 *	object and ptr are not the same, FALSE (zero) if they are the same.
	 */
	BOOL operator !=(const OpSmartPointerWithDelete &ptr) const {return (object != ptr.object);}

	/**
	 *	Inequality operator. Returns TRUE (non-zero) if the pointer of this
	 *	object and ptr are not the same, FALSE (zero) if they are the same.
	 */
	BOOL operator !=(T *ptr) const {return (object != ptr);}

	/**
	 *	Assignment operator
	 *
	 *	This object will be set to point at the same object that ptr is pointing to, with
	 *	reference counters appropriately set for the old and new objects
	 *
	 *	@param	ptr		Reference to the smartpointer holding the new pointer value. The object is not updated
	 *
	 *	@return		Reference to this Object
	 */
	OpSmartPointerWithDelete &operator =(const OpSmartPointerWithDelete &ptr){SetNewValue((T *) ptr.object);return *this;};

	/**
	 *	Assignment operator
	 *
	 *	This object will be set to point at the same object that ptr is pointing to, with
	 *	reference counters appropriately set for the old and new objects
	 *
	 *	@param	ptr		Pointer to the object that this object will point to.
	 *
	 *	@return		Reference to this Object
	 */
	OpSmartPointerWithDelete &operator =(T *ptr){SetNewValue((T *) ptr); return *this;};

private:
	/**
	 *  Decrements the reference counter of the previous object,
	 *  increments the reference counte of the new object, and assigns it to
	 *  the object pointer.
	 *
	 *	If the reference count of the present object reaches zero, it is deleted.
	 *
	 *	@param  new_value	Pointer to a OpReferenceCounter object.
	 *						This pointer-value will be assigned to target
	 *						If non-NULL the reference counter of the referenced object
	 *						will be incremented.
	 */
	void SetNewValue(T *new_value)
	{
#if defined (_DEBUG) && defined(YNP_WORK)
		if(InList())
			Out();
		if(new_value)
			Into(&new_value->debug_list);
#endif // _DEBUG && YNP_WORK

		if(new_value)
		{
			new_value->Increment_Reference();
		}
		if(object)
		{
			if(object->Decrement_Reference() == 0)
				OP_DELETE(object);
		}
		object = new_value;
	}
};

#endif // !MODULES_UTIL_SMARTPTR_H
