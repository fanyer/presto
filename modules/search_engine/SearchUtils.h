/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef SEARCHUTILS_H
#define SEARCHUTILS_H

#ifndef CHECK_RESULT
#  if defined(__GNUC__)
#    if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#      define CHECK_RESULT(X) X __attribute__((warn_unused_result))
#    endif
#  endif
#endif
#ifndef CHECK_RESULT
#  define CHECK_RESULT(X) X
#endif // CHECK_RESULT

#ifndef RETURN_OOM_IF_NULL
#define RETURN_OOM_IF_NULL( expr ) \
	do { \
		if (NULL == (expr)) \
			return OpStatus::ERR_NO_MEMORY; \
	} while(0)
#endif

#define INVALID_FILE_LENGTH ((OpFileLength)-1)

/**
 * Class declaring private copy constructor and assignment operator.
 * Inherit from this class if you want to prevent accidental assignment to
 * a class variable, which may result in the destructor being called twice
 * on the same data. It should typically be used whenever there is a
 * destructor that does some non-trivial cleanup.
 */
class NonCopyable
{
protected:
	NonCopyable() {}
private:
	// Disable implicit copy constructor and assignment
	NonCopyable(const NonCopyable&);
	NonCopyable& operator=(const NonCopyable&);
};

/**
 * Class for detecting loops during iterations.
 * This particular implementation is especially useful if the operation for
 * advancing to the next element is costly, or if the iterator itself has a
 * large state, such as when traversing data on disk. Usage:
 * 
 * <pre>
 * LoopDetector<Link*> loopDetector;
 * for (p = First(); p; p = p->Next()) {
 *    //...
 *    RETURN_IF_ERROR(loopDetector.CheckNext(p));
 * }
 * </pre>
 */
template <class T, T nullValue=0> class LoopDetector
{
public:
	LoopDetector() : n(0) { prevId = nullValue; }

	void Reset() { n = 0; }

	/**
	 * Check next element in a sequence to detect if there is a loop
	 * @return OK, or ERR if the element has been seen before (loop detected)
	 */
	CHECK_RESULT(OP_STATUS CheckNext(T id))
	{
		if (n > 0 && id == prevId)
		{
			OP_ASSERT(!"Loop detected, probably a corrupt file. Contact search_engine owner if reproducible");
			return OpStatus::ERR;
		}
		n++;
		if ((n & (n-1)) == 0) // Update prevId at n==1,2,4,8,16,32...
			prevId = id;
		return OpStatus::OK;
	}

private:
	int n;
	T prevId;
};

#endif // SEARCHUTILS_H
