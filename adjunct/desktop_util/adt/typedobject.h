/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef TYPED_OBJECT_H
#define TYPED_OBJECT_H

/** @brief Keeps a type ID for a class so that dynamic downcasting is possible and safe
  *
  * To use this, let your classes derive from TypedObject and use the
  * IMPLEMENT_TYPEDOBJECT macro (see below).
  *
  * Then, if you have an object pointer/reference of type TypedObject,
  * you can downcast it using GetTypedObject(), eg.
  *
  *   void CrazyFunction(TypedObject* object)
  *   {
  *       MyClass* typed_object = object->GetTypedObject<MyClass>();
  *       if (typed_object)
  *           // do stuff
  *   }
  */

/** Macro that defines required type function in classes that derive from TypedObject
  * @param SuperClass The superclass, class this class is derived from
  *
  * @example Usage Use this macro at the start of a class declaration:
  *
  *   class MyClass : public TypedObject
  *   {
  *   	IMPLEMENT_TYPEDOBJECT(TypedObject);
  *     public:
  *   	  // other functions go here
  *   };
  *
  *   class MyDerivedClass : public MyClass
  *   {
  *   	IMPLEMENT_TYPEDOBJECT(MyClass);
  *   	public:
  *   	  // other functions go here
  *   };
  */
#define IMPLEMENT_TYPEDOBJECT(SuperClass) \
	protected: \
		virtual BOOL HasTypeId(TypedObject::TypeId type) const { return type == this->GetTypeId(this) || SuperClass::HasTypeId(type); }

class TypedObject
{
public:
	virtual ~TypedObject() {}

	/** Get this object casted to the specified type
	  * @param Type Type that we need
	  * @return Pointer to object if the requested type was the type of this object, NULL otherwise
	  */
	template<typename Type>
	Type* GetTypedObject() { return IsOfType<Type>() ? static_cast<Type*>(this) : 0; }

	template<typename Type>
	const Type* GetTypedObject() const { return IsOfType<Type>() ? static_cast<const Type*>(this) : 0; }

	/** Get an object casted to the specified type
	  * @param InputType Type that we have
	  * @param OutputType Type that we need
	  * @param object Pointer to object
	  * @return Pointer to object if @a object was not @c NULL and @a OutputType
	  * 		was the type of @a object, @c NULL otherwise
	  */
	template<typename OutputType, typename InputType>
	static OutputType* GetTypedObject(InputType* object) { return object ? object->template GetTypedObject<OutputType>() : 0; }

	template<typename OutputType, typename InputType>
	static const OutputType* GetTypedObject(const InputType* object) { return object ? object->template GetTypedObject<OutputType>() : 0; }

	/** Check if this object is of the specified type
	  * @param Type Compare with this type
	  * @param object (optional) Compare with type of this object
	  * @return Whether the object was of the requested type
	  */
	template<typename Type>
	BOOL IsOfType(const Type* object = 0) const { return HasTypeId(GetTypeId<Type>()); }

protected:
	typedef void* TypeId;

	/** Get the type id for a certain type, used in implementation of IsOfType()
	  * @param Type Get type id for this type
	  * @param object (optional) Get type id of type of this object
	  */
	template<typename Type>
	static TypeId GetTypeId(const Type* object = 0) { static char m_type_id; return &m_type_id; }

	/** Check if an object is of certain type
	  * @param type Type to check for
	  * @return Whether this object is of requested type
	  *
	  * @note Use IMPLEMENT_TYPEDOBJECT to implement this function
	  */
	virtual inline BOOL HasTypeId(TypeId type) const = 0;
};

// Default implementation of IsOfType
BOOL TypedObject::HasTypeId(TypedObject::TypeId type) const { return type == GetTypeId(this); }

#endif // TYPED_OBJECT_H
