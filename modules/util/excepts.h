/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/*
 * Designer/architect: Petter Reinholdtsen <pere@opera.com>
 */

/*
 * Implement exception handling abstraction layer:
 *   LEAVE(int i);
 *   TRAP(error, block);
 *   TRAPD(int error, block)
 *   OpStackAutoPtr<type> ptr;
 * In addition, the CleanupStack::Push() and ...::Pop() functions are
 * implemented on some platforms.
 */

#ifndef MODULES_UTIL_EXCEPTS_H
#define MODULES_UTIL_EXCEPTS_H

#include "modules/util/opautoptr.h"

#ifndef THROWCLAUSE
# if defined _WIN32_WCE && defined ARM
#  define THROWCLAUSE
# else
#  define THROWCLAUSE throw()
# endif
#endif

#ifdef EPOC
# include <e32cmn.h>
# include <e32std.h>
# define LEAVE(i) User::Leave(i)
# ifdef __LEAVE_EQUALS_THROW__
#  define USE_CXX_EXCEPTIONS
# endif

/**
 * Cleanup stack item class to approximate behaviour on other platforms.
 */
class CleanupItem : public CBase
{
public:
	// Dummy function. CBase has never added itself to CleanupStack directly,
	// so it cannot remove itself here.
	virtual void Cleanup(int /*error*/)
	{
	}
};

#endif

#if defined(USE_CXX_EXCEPTIONS)

#ifdef MEMTEST
# include "modules/memtools/mem_util.h"
#endif

#ifdef _DEBUG
extern void leave_trapped();
# define CATCH_TRAP() leave_trapped()
#else
# define CATCH_TRAP()
#endif // _DEBUG

#ifndef TRAP
#define TRAP(var, block) \
	do{ \
		try { \
			do { block; } while (0); \
			var = OpStatus::OK; \
		} catch (OP_STATUS i__) { \
			CATCH_TRAP(); \
			var = i__; \
		} \
	} while(0)
#endif // TRAP

#ifndef TRAPD
#define TRAPD(var, block) \
	OP_STATUS var; \
	TRAP(var, block)
#endif // TRAPD

#ifndef LEAVE
#define LEAVE(i) \
	do { \
		throw ((OP_STATUS) i) \
	} while(0)
#endif // LEAVE

// Basically just a rename of OpAutoPtr
template <class T>
class OpStackAutoPtr : public OpAutoPtr<T>
{
public:
	explicit OpStackAutoPtr(T* ptr = 0) THROWCLAUSE : OpAutoPtr<T>(ptr) {}
	OpStackAutoPtr(OpStackAutoPtr& rhs) THROWCLAUSE : OpAutoPtr<T>(rhs) {}
	OpStackAutoPtr& operator=(OpStackAutoPtr& rhs) THROWCLAUSE
	{
		return static_cast<OpStackAutoPtr&>(OpAutoPtr<T>::operator=(rhs));
	}
};

template <class T>
class OpStackAnchor
{
public:
	OpStackAnchor(const T* ref) {}

	/// Make this template compatible with other platforms.
	void Cleanup(int error) {}
	/// Make this template compatible with other platforms.
	void Close() {}
};

template <class T>
class OpHeapArrayAnchor
{
public:
	OpHeapArrayAnchor(void)   : m_ptr(NULL) {}
	OpHeapArrayAnchor(T* ptr) : m_ptr(ptr)  {}

	~OpHeapArrayAnchor()
	{
		Clear();
	}

	T* release()
	{
		T* ptr = m_ptr;
		m_ptr = 0;
		return ptr;
	}

	void reset(T* ptr)
	{
		m_ptr = ptr;
	}

	void Clear(void)
	{
		if (m_ptr != NULL)
		{
			OP_DELETEA(m_ptr);
			m_ptr = NULL;
		}
	}

	T *Get(void) const
	{
		return m_ptr;
	}

	void Cleanup(int error) {}
	void Close()            {}

	OpHeapArrayAnchor &operator=(T *ptr)
	{
		Clear();
		m_ptr = ptr;
		return *this;
	}

	T &operator[](int index)
	{
		return m_ptr[index];
	}

	operator BOOL () const
	{
		return m_ptr != NULL;
	}

private:
	T* m_ptr;
};

#elif defined(EPOC)

#ifdef __LEAVE_EQUALS_THROW__
# error "This section must not be compiled if __LEAVE_EQUALS_THROW__ is defined"
#endif

// TRAP and TRAPD is already defined in EPOC API

template <class T>
class OpStackAutoPtr
{
private:
	class Wrapper
	{
	public:
		Wrapper()
		{
			iLeaving = EFalse;
		}

		void Close() // Called upon cleanup
		{
			OP_DELETE(ptr);
			ptr = NULL;
			iLeaving = ETrue; // User::Leave() has already taken us of cleanup stack

		}

		~Wrapper()
		{
			OP_DELETE(ptr);
			ptr = NULL;
		}

	public:
		T* ptr;
		TBool iLeaving;
	};

public:
	explicit OpStackAutoPtr(T *aPtr = 0)
	{
		CleanupClosePushL(iWrapper);
		iWrapper.ptr = aPtr;
	}

	~OpStackAutoPtr()
	{
		// in a normal return we have to take iWrapper off the cleanup stack
		if (!iWrapper.iLeaving)
		{
			// Just Pop() rather than PopAndDestroy() because the Wrapper
			// destructor takes care of deleting the pointer in normal
			// circumstances.
			CleanupStack::Pop(); // iWrapper
		}
	}

	T* get()        const { return  iWrapper.ptr; }
	T& operator*()  const { return *iWrapper.ptr; }
	T* operator->() const { return  iWrapper.ptr; }
	T* release()
	{
		T* ptr = iWrapper.ptr;
		iWrapper.ptr = 0;
		return ptr;
	}
	void reset(T* ptr = 0)
	{
		if (iWrapper.ptr != ptr)
		{
			OP_DELETE(iWrapper.ptr);
			iWrapper.ptr = ptr;
		}
	}

private:
	Wrapper iWrapper;
};

#ifdef __SUPPORT_CPP_EXCEPTIONS__
// This implementation of OpStackAnchor is for Symbian9 where standard C++ exceptions are supported.
// It must empty, because C++ destructors are called, otherwise there could be a double deletion in case of exception.
template <class T>
class OpStackAnchor
{
public:
	explicit OpStackAnchor(const T* /*aPtr*/) {}
};

#else
// This implementation of OpStackAnchor is for pre-Symbian9 where standard C++ exceptions are NOT supported.
template <class T>
class OpStackAnchor
{
private:
	class Wrapper
	{
	public:
		Wrapper()
		{
			iLeaving = EFalse;
		}
		void Close() // Called upon cleanup
		{
			ptr->T::~T();
			iLeaving = ETrue;
		};
	public:
		T* ptr;
		TBool iLeaving;
	}

public:
	explicit OpStackAnchor(T* aPtr)
	{
		CleanupClosePushL(iWrapper);
		iWrapper.ptr = aPtr;
	}
	~OpStackAnchor()
	{
		// This is Pop() and not PopAndDestroy() because the anchored
		// object's destructor is called as per normal if the Stack
		// Anchor's destructor is called.
		if (iWrapper.iLeaving == EFalse)
		{
			CleanupStack::Pop(); // iWrapper
		}
	}

private:
	Wrapper iWrapper;
};
#endif  // __SUPPORT_CPP_EXCEPTIONS__

template <class T>
class OpHeapArrayAnchor
{
private:
	class Wrapper
	{
	public:
		Wrapper()
		{
			iLeaving = EFalse;
		}
		void Close() // called by User::Leave()
		{
			OP_DELETEA(ptr);
			iLeaving = ETrue;
		}
	public:
		T* ptr;
		TBool iLeaving;
    };
public:
	OpHeapArrayAnchor(void)
	{
		iWrapper.ptr = NULL;
	}

	explicit OpHeapArrayAnchor(T* aPtr)
	{
		iWrapper.ptr = aPtr;
		CleanupClosePushL(iWrapper);
	}
	~OpHeapArrayAnchor()
	{
		// normal return (not User::Leave())
		if (!iWrapper.iLeaving) 
		{
			// PopAndDestroy() rather than Pop() because nobody else
			// performs destruction on the controlled object
			CleanupStack::PopAndDestroy(); // iWrapper
		}
	}
	T* release()
	{
		T* ptr = iWrapper.ptr;
		iWrapper.ptr = 0;
		return ptr;
	}

	void reset(T* ref)
	{
		if (iWrapper.ptr != ref)
		{
			OP_DELETEA(iWrapper.ptr);
			iWrapper.ptr = ref;
		}
	}

	T *Get(void) const
	{
		return iWrapper.ptr;
	}

	OpHeapArrayAnchor &operator=(T *aPtr)
	{
		reset(aPtr);
		return *this;
	}

	T &operator[](int index)
	{
		return iWrapper.ptr[index];
	}

	operator BOOL () const
	{
		return iWrapper.ptr != NULL;
	}

private:
	Wrapper iWrapper;
};

#else // not EPOC and not USE_CXX_EXCEPTIONS

#include "modules/util/opautoptr.h"

/**
 * Cleanup stack item used with the OpStackAutoPtr template.  Keeps a
 * thread specific list of items as a simple linked list.
 */
class CleanupItem
{
private:
	/**
	 * Pointer to the previous item on the cleanup stack, or null if
	 * this item is the last item on the stack.
	 */
	CleanupItem *next;

	/**
	 * Dummy copy constructors
	 */
	CleanupItem(const CleanupItem&);
	CleanupItem& operator=(const CleanupItem&);

protected:
	friend class User; // Allow access to the cleanup stack item

public:
	CleanupItem();
	virtual ~CleanupItem();
	virtual void Cleanup(int error);
	static void CleanupAll(int error);
};

class CleanupCatcher : public CleanupItem
{
public:
	jmp_buf catcher;
	int error;

	CleanupCatcher() : error(0) {}

	virtual void Cleanup(int error);
};

/**
 * This class is an OpAutoPtr template for use on the program stack.
 * It is compatible with the OpAutoPtr implementation, which is
 * equivalent to Standard C++ auto_ptr.
 *
 * @see CleanupItem, OpAutoPtr, auto_ptr
 */
template <class T>
class OpStackAutoPtr : public CleanupItem
{
private:
	OpAutoPtr<T> ptr;
public:
	explicit OpStackAutoPtr(T* _ptr = 0) : CleanupItem(), ptr(_ptr) {}
	virtual void Cleanup(int error) { ptr.reset(0); CleanupItem::Cleanup(error); }

	T* get() const THROWCLAUSE { return ptr.get(); }
	T& operator*() const THROWCLAUSE { return ptr.operator*(); }
	T* operator->() const THROWCLAUSE { return ptr.operator->(); }
	T* release() THROWCLAUSE { return ptr.release(); }
	void reset(T* _ptr = 0) THROWCLAUSE { ptr.reset(_ptr); }
};

/**
 * This class is an template to anchor objects used on the program
 * stack to the cleanup stack.
 *
 * @see CleanupItem
 */
template <class T>
class OpStackAnchor : public CleanupItem
{
	T* const m_ref;
public:
	OpStackAnchor(T* ref)
		: CleanupItem(), m_ref(ref)
	{
#ifdef MEMTEST
		oom_remove_anchored_object((void *) ref);
#endif // MEMTEST
	}
	virtual void Cleanup(int error)
	{
		CleanupItem::Cleanup(error);
		m_ref->T::~T();
	}
	/// Make this template compatible with other platforms.
	void Close() {}
};

template <class T>
class OpHeapArrayAnchor : public CleanupItem
{
public:
	OpHeapArrayAnchor(void)   : CleanupItem(), m_ref(0)   {}
	OpHeapArrayAnchor(T* ref) : CleanupItem(), m_ref(ref) {}

	virtual ~OpHeapArrayAnchor()
	{
		OP_DELETEA(m_ref);
		m_ref = 0;
	}

	T* release()
	{
		T* ref = m_ref;
		m_ref = 0;
		return ref;
	}

	void reset(T* ref)
	{
		m_ref = ref;
	}
	
	void Clear(void)
	{
		if (m_ref != 0)
		{
			OP_DELETEA(m_ref);
			m_ref = 0;
		}
	}

	virtual void Cleanup(int error)
	{
		CleanupItem::Cleanup(error);
		OP_DELETEA(m_ref);
		m_ref = 0;
		error = 0; // keep compiler quiet
	}
	void Close() {}

	T *Get(void) const
	{
		return m_ref;
	}

	OpHeapArrayAnchor &operator=(T *ptr)
	{
		Clear();
		m_ref = ptr;
		return *this;
	}

	T &operator[](int index)
	{
		return m_ref[index];
	}

	operator BOOL () const
	{
		return m_ref != NULL;
	}
private:
	T* m_ref;
};

#if defined(MEMTEST) && defined(PARTIAL_LTH_MALLOC)
# include "modules/memtools/happy-malloc.h"
# define TRAP(var, block) \
	do { \
		SAVE_MALLOC_STATE(); \
		oom_TrapInfo _oom_trap; \
		CleanupCatcher _catcher; \
		if (0 == op_setjmp(_catcher.catcher)) { \
			do { block; } while (0); \
			var = OpStatus::OK; \
			OpStatus::Ignore(var); \
		} else { \
			var = _catcher.error; \
		} \
		RESET_MALLOC_STATE(); \
	} while (0)
#else // !MEMTEST
# define TRAP(var, block) \
	do { \
		CleanupCatcher _catcher; \
		if (0 == op_setjmp(_catcher.catcher)) { \
			do { block; } while (0); \
			var = OpStatus::OK; \
			OpStatus::Ignore(var); \
		} else { \
			var = (OpStatus::ErrorEnum)_catcher.error; \
		} \
	} while (0)
#endif // MEMTEST
#define TRAPD(var, block) \
	OP_STATUS var; \
	TRAP(var, block)
#define LEAVE(i) \
	do { \
		User::Leave(i); \
	} while (0)

class User
{
public:
#ifdef USE_DEBUGGING_OP_STATUS
	static void Leave(OP_STATUS error);
#endif // USE_DEBUGGING_OP_STATUS
	static void Leave(int error);
	static void LeaveNoMemory();
	static int LeaveIfError(int error);
};

#endif // not EPOC and not USE_CXX_EXCEPTIONS

/* Shorthand for use of OpStackAnchor<> */

#define ANCHOR(type, var) \
	OpStackAnchor< type > anchor_variable_ ## var (&var)

#define ANCHOR_PTR(type, var) \
	OpStackAutoPtr< type > anchor_ptr_variable_ ## var (var)

#define ANCHOR_PTR_RELEASE(var) \
	anchor_ptr_variable_ ## var.release()

/// Similar to TRAPD, ANCHORD defines and anchors the variable in one go
#define ANCHORD(type, var) \
	type var; \
	ANCHOR(type, var)

/**
 * Access macros to make the OpHeapArrayAnchor easier to use.
 */
#define ANCHOR_ARRAY(type, variable) \
	OpHeapArrayAnchor<type> anchor_array_variable_ ## variable (variable)

#define ANCHOR_ARRAY_RELEASE(variable) \
	anchor_array_variable_ ## variable.release()

#define ANCHOR_ARRAY_RESET(variable) \
	anchor_array_variable_ ## variable.reset(variable)

#define ANCHOR_ARRAY_DELETE(variable) \
	do { \
		anchor_array_variable_ ## variable.release(); \
		delete [] variable; \
	} while (0)

/**
 * Various convenience macros
 */
#define LEAVE_IF_ERROR(expr) \
	do { \
		OP_STATUS LEAVE_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(LEAVE_IF_ERROR_TMP)) \
			LEAVE(LEAVE_IF_ERROR_TMP); \
	} while (0)

#define LEAVE_IF_NULL(expr) \
	do { \
		if (NULL == (expr)) \
			LEAVE(OpStatus::ERR_NO_MEMORY); \
	} while (0)

#define LEAVE_IF_FATAL(expr) \
	do { \
		OP_STATUS LEAVE_IF_FATAL_TMP = expr; \
		if (OpStatus::IsFatal(LEAVE_IF_FATAL_TMP)) \
			LEAVE(LEAVE_IF_FATAL_TMP); \
	} while (0)

#define RETURN_IF_ERROR(expr) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
			return RETURN_IF_ERROR_TMP; \
	} while (0)

#define RETURN_OOM_IF_NULL(expr) \
	do { \
		if (NULL == (expr)) \
			return OpStatus::ERR_NO_MEMORY; \
	} while (0)

#define RETURN_VALUE_IF_NULL(expr, val) \
	do { \
		if (NULL == (expr)) \
			return val; \
	} while (0)

#define RETURN_IF_MEMORY_ERROR(expr) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP)) \
			return RETURN_IF_ERROR_TMP; \
	} while (0)

#define RETURN_VALUE_IF_ERROR(expr, val) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
			return val; \
	} while (0)

#define RETURN_VOID_IF_ERROR(expr) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
			return; \
	} while (0)

#define RAISE_AND_RETURN_IF_ERROR(expr) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) { \
			g_memory_manager->RaiseCondition(RETURN_IF_ERROR_TMP); \
			return RETURN_IF_ERROR_TMP; \
		} \
	} while (0)

#define RAISE_AND_RETURN_VALUE_IF_ERROR(expr, val) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) { \
			g_memory_manager->RaiseCondition(RETURN_IF_ERROR_TMP); \
			return val; \
		} \
	} while (0)

#define RAISE_AND_RETURN_VOID_IF_ERROR(expr) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) { \
			g_memory_manager->RaiseCondition(RETURN_IF_ERROR_TMP); \
			return; \
		} \
	} while (0)

#define TRAP_AND_RETURN(var, expr) \
	do { \
		TRAPD(var, expr); \
		if (OpStatus::IsError(var)) \
			return var; \
	} while (0)

#define RETURN_IF_LEAVE(expr) \
	do { \
		TRAPD(OP_STATUS_RETURN_IF_LEAVE_TMP, expr); \
		if (OpStatus::IsError(OP_STATUS_RETURN_IF_LEAVE_TMP)) \
			return OP_STATUS_RETURN_IF_LEAVE_TMP; \
	} while (0)

#define TRAP_AND_RETURN_VALUE_IF_ERROR(var, expr, val) \
	do { \
		TRAP(var, expr); \
		if (OpStatus::IsError(var)) \
			return val; \
	} while (0)

#define TRAP_AND_RETURN_VOID_IF_ERROR(var, expr) \
	do { \
		TRAP(var, expr); \
		if (OpStatus::IsError(var)) \
			return; \
	} while (0)

#define RAISE_IF_ERROR(expr) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
			g_memory_manager->RaiseCondition(RETURN_IF_ERROR_TMP); \
	} while (0)

#ifndef RAISE_IF_MEMORY_ERROR
#define RAISE_IF_MEMORY_ERROR(expr) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP)) \
			g_memory_manager->RaiseCondition(RETURN_IF_ERROR_TMP); \
	} while (0)
#endif

#endif // !MODULES_UTIL_EXCEPTS_H
