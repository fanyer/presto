/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef TESTSUITE_SELFTESTMEMORYCLEANUPUTIL_H
#define TESTSUITE_SELFTESTMEMORYCLEANUPUTIL_H

#ifdef SELFTEST

/** Simple util class for handling deleting objects in asynchronous selftests.
 *
 * As 'finally' clause doesn't work in asynchronous selftests and in larger tests it can become
 * troublesome to keep track of all the objects and their lifetime. This class simplifies handling 
 * heap allocated objects by scheduling their deletion in the end of selftest group is completed.
 */
class SelftestMemoryCleanupUtil
{
public:
	/** Schedules object to be deleted when Cleanup is called.
	 * 
	 * @param object - object which will be scheduled for deletion. If this function fails(OOM) then the object
	 * will be deleted immidietly. This function returns ERR_NO_MEMORY also if object is NULL, so NULL check can be 
	 * ommited and just check the result of this function.
	 * NOTE: As deallocation uses OP_DELETE object must be allocted by OP_NEW(not malloc, or array new allocator)
	 * @return OK or ERR_NO_MEMORY
	 */
	template<class Type>
	OP_STATUS DeleteAfterTest(Type* object)
	{
		if (!object)
			return OpStatus::ERR_NO_MEMORY;
		DeletableBase* wrapper = OP_NEW(Deletable<Type>, (object));
		if (!wrapper)
		{
			OP_DELETE(object);
			return OpStatus::ERR_NO_MEMORY;
		}
		OP_STATUS error = m_deletables.Insert(0, wrapper);
		if (OpStatus::IsError(error))
			OP_DELETE(wrapper);
		return error;
	}

	/** Deletes all objects scheduled for deletion
	*
	* This is called by selftest engine after finishing every test group.
	*/
	void Cleanup() { m_deletables.DeleteAll(); }

	/** Returns number of items scheduled for cleanup
	 *
	 * For testing purposes only
	 */
	UINT32 ItemsToCleanup() { return m_deletables.GetCount(); }
private:
	/// Wrapper interface for deletable object
	class DeletableBase{
	public:
		virtual ~DeletableBase() {}
	};

	/// Implementation of wrapper for deletable object which calls proper destructor
	template<class WrappedType>
	class Deletable : public DeletableBase
	{
	public:
		Deletable(WrappedType* wrapped_object) : m_wrapped_object(wrapped_object) {}
		virtual ~Deletable() { OP_DELETE(m_wrapped_object); }
	private:
		WrappedType* m_wrapped_object;
	};

	OpAutoVector<DeletableBase> m_deletables;
};

#endif // SELFTEST
#endif // !TESTSUITE_TESTUTILS_H
