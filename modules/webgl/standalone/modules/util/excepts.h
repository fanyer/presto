/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2001-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
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
 * In addition, the CleanupStack::Push() and ..::Pop() functions are
 * implemented on some platforms.
 */

#ifndef EXCEPTS_H
#define EXCEPTS_H

#include "opautoptr.h"

#ifndef THROWCLAUSE
#  if defined _WIN32_WCE && defined ARM
#    define THROWCLAUSE 
#  else
#    define THROWCLAUSE throw()
#  endif
#endif

#if defined(USE_CXX_EXCEPTIONS)

#ifdef _DEBUG
extern void leave_trapped();
#define CATCH_TRAP() leave_trapped()
#else
#define CATCH_TRAP()
#endif // _DEBUG

#define TRAP(var, block) \
do{\
     try { \
       do { block; } while (0); \
       var = OpStatus::OK; \
     } catch (OP_STATUS i__) { \
	   CATCH_TRAP(); \
       var = i__; \
     }\
}while(0)
#define TRAPD(var, block) \
     OP_STATUS var; \
     TRAP(var, block)
#define LEAVE(i) throw ((OP_STATUS)i)

// Rename template (is there another way?
#define OpStackAutoPtr OpAutoPtr
template <class T>
class OpStackAnchor
{
public:
	OpStackAnchor(const T* ref) {};

	/// Make this template compatible with other platforms.
	void Cleanup(int error) {};
	/// Make this template compatible with other platforms.
	void Close() {};
};

template <class T>
class OpHeapArrayAnchor
{
public:
	OpHeapArrayAnchor(T* ref)
		: m_ref(ref)
	{
	};

	~OpHeapArrayAnchor()
	{
		delete [] m_ref;
	};

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

	void Cleanup(int error)	{};
	void Close() {};
private:
	T* m_ref;
};

#elif defined(EPOC)

extern void LEAVE(int);

// TRAP and TRAPD is already defined in EPOC API

template <class T>
class OpStackAutoPtr
{
private:
	class Wrapper
    {
    public:
        void Close() // Called upon cleanup
        {
            delete ptr;
        }
		~Wrapper()
		{
            delete ptr;
		};
    public:
	    T* ptr;
    };
public:
	explicit OpStackAutoPtr(T *aPtr)
    {
        CleanupClosePushL(iWrapper);
        iWrapper.ptr=aPtr;
    };
    ~OpStackAutoPtr()
    {
		// Just Pop() rather than PopAndDestroy() because the Wrapper
		// destructor takes care of deleting the pointer in normal
		// circumstances.
        CleanupStack::Pop(); // iWrapper
    }
	T* get() const { return iWrapper.ptr; };
    T& operator*() const { return *iWrapper.ptr; };
    T* operator->() const { return iWrapper.ptr; };
	T* release() { T* ptr=iWrapper.ptr; iWrapper.ptr=0; return ptr; };
    void reset(T* ptr=0) 
	{
		if (iWrapper.ptr != ptr)
		{
			delete iWrapper.ptr;
			iWrapper.ptr = ptr;
		}
	}

private:
    Wrapper iWrapper;
};

template <class T>
class OpStackAnchor
{
private:
	class Wrapper
    {
    public:
        void Close() // Called upon cleanup
		{
            ptr->T::~T();
		};
    public:
		const T* ptr;
    };
public:
    explicit OpStackAnchor(const T* aPtr)
    {
        CleanupClosePushL(iWrapper);
        iWrapper.ptr=aPtr;
    };
    ~OpStackAnchor()
    {
		// This is Pop() and not PopAndDestroy() because the anchored
		// object's destructor is called as per normal if the Stack
		// Anchor's destructor is called.
        CleanupStack::Pop(); // iWrapper
    }
private:
    Wrapper iWrapper;
};

template <class T>
class OpHeapArrayAnchor
{
private:
	class Wrapper
    {
    public:
		void Close()
		{
            delete [] ptr;
		};
    public:
		T* ptr;
    };
public:
	explicit OpHeapArrayAnchor(T* aPtr)
    {
        iWrapper.ptr=aPtr;
        CleanupClosePushL(iWrapper);
    };
    ~OpHeapArrayAnchor()
    {
		// PopAndDestroy() rather than Pop() because nobody else
		// performs destruction on the controlled object
        CleanupStack::PopAndDestroy(); // iWrapper
    }
	T* release()
	{
		T* ptr=iWrapper.ptr;
		iWrapper.ptr=0;
		return ptr;
	}

	void reset(T* ref)
	{
		iWrapper.ptr = ref;
	}

private:
    Wrapper iWrapper;
};

#else // not EPOC and not USE_CXX_EXCEPTIONS

#include <setjmp.h>

#include "opautoptr.h"

/**
 * Cleanup stack item used with the OpStackAutoPtr template.  Keeps a
 * thread specific list of items as a simple linked list.
 */
class CleanupItem
{
private:
	/**
	 * Pointer to last cleanup item on stack (program stack, not push
	 * stack).
	 */
	static CleanupItem *cleanupstack;

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

	/**
	 * Return the latest cleanup stack item pushed in this threads
	 * cleanup stack.
	 *
	 * This is not thread safe at the moment, and must be modifed to
	 * not use a global variable, but a different variable in each
	 * thread.
	 * 
	 * @see SetLastStackItem()
	 */
	static CleanupItem *GetLastStackItem() { return cleanupstack; };

	/**
	 * Set the latest clean stack item for the current thread.
	 *
	 * This is not thread safe at the moment.  See GetLastStackItem()
	 * for more info.
	 *
	 * @see GetLastStackItem()
	 */
	static void SetLastStackItem(CleanupItem *item) { cleanupstack = item; };
public:
	CleanupItem();
	virtual ~CleanupItem();
	virtual void Cleanup(int error);
};

class CleanupCatcher
	: public CleanupItem
{
private:
#ifdef ENABLE_CLEANUPSTACK
	int stackheigth;
#endif
public:
	jmp_buf catcher;
	int error;

	CleanupCatcher();
	~CleanupCatcher();
	void Cleanup(int error);
};

/**
 * This class is an OpAutoPtr template for use on the program stack.
 * It is compatible with the OpAutoPtr implementation, which is
 * equivalent to Standard C++ auto_ptr.
 *
 * @see CleanupItem, OpAutoPtr, auto_ptr
 */
template <class T>
class OpStackAutoPtr
	: public CleanupItem
{
private:
	OpAutoPtr<T> ptr;
public:
	OpStackAutoPtr(T* _ptr) : CleanupItem(), ptr(_ptr) {};
	void Cleanup(int error) { ptr.reset(0); CleanupItem::Cleanup(error); };

	T* get() const THROWCLAUSE { return ptr.get(); };
	T& operator*() const THROWCLAUSE { return ptr.operator*(); };
	T* operator->() const THROWCLAUSE { return ptr.operator->(); };
	T* release() THROWCLAUSE { return ptr.release(); };
	void reset(T* _ptr=0) THROWCLAUSE { ptr.reset(_ptr); };
};

/**
 * This class is an template to anchor objects used on the program
 * stack to the cleanup stack.
 *
 * @see CleanupItem
 */

template <class T>
class OpStackAnchor
	: public CleanupItem
{
	const T* m_ref;
public:
	OpStackAnchor(const T* ref)
		: CleanupItem(), m_ref(ref)
	{
#ifdef MEMTEST
		oom_remove_anchored_object((void *) ref);
#endif // MEMTEST
	};
	void Cleanup(int error)
	{
		CleanupItem::Cleanup(error);
#ifndef MHW_STB // FIXME: Compiler bug on MHW? /k
		m_ref->T::~T();
#endif // !MHW_STB
	};
	/// Make this template compatible with other platforms.
	void Close() {};
};

template <class T>
class OpHeapArrayAnchor
	: public CleanupItem
{
public:
	OpHeapArrayAnchor(T* ref)
		: CleanupItem(), m_ref(ref)
	{
	};

	~OpHeapArrayAnchor()
	{
		delete [] m_ref;
		m_ref = 0;
	};

	T* release()
	{
		T* ref = m_ref;
		m_ref = 0;
		return ref;
	};

	void reset(T* ref)
	{
		m_ref = ref;
	}
	
	void Cleanup(int error)
	{
		CleanupItem::Cleanup(error); 
		delete [] m_ref;
		m_ref = 0;
		error = 0; // keep compiler quiet
	};
	void Close() {};
private:
	T* m_ref;
};

#ifdef MEMTEST
# ifndef _BUILDING_OPERA_
#  define _BUILDING_OPERA_
# endif
# include "modules/util/simset.h"
# include "dbug/dbug.h"

void oom_add_unanchored_object(char *allocation_address, void *object_address);
void oom_delete_unanchored_object(void *object_address);
void oom_remove_anchored_object(void *object_address);
void oom_report_and_remove_unanchored_objects();
extern int *global_oom_dummy;

#define OOM_REPORT_UNANCHORED_OBJECT() \
    do { \
        int dummy; \
        char *caller_address = 0; \
        if (( (void*) this > (void*) &dummy && (void*) this < (void*) global_oom_dummy) || \
			( (void*) this < (void*) &dummy && (void*) this > (void*) global_oom_dummy)) { \
            GET_CALLER(caller_address); \
            oom_add_unanchored_object(caller_address, this); \
        } \
    } while (0)

class oom_UnanchoredObject : public Link
{
public:
	oom_UnanchoredObject(char* alloc_address, void *object_address, int depth) :
		allocation_address(alloc_address),
		address(object_address),
		trap_depth(depth)
		{}
	char *allocation_address;
	void *address;
	int trap_depth;
};

class oom_TrapInfo : public Link
{
public:
	oom_TrapInfo();
	~oom_TrapInfo();
	
	char* m_call_address;
	BOOL m_is_left;
};

#endif

#if defined(MEMTEST) && defined(PARTIAL_LTH_MALLOC)
# include "modules/memtools/happy-malloc.h"
# define TRAP(var, block) \
    do {\
        SAVE_MALLOC_STATE(); \
        oom_TrapInfo _oom_trap; \
        CleanupCatcher _catcher; \
        if (0 == setjmp(_catcher.catcher)) { \
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
  do {\
    CleanupCatcher _catcher; \
    if (0 == setjmp(_catcher.catcher)) { \
      do { block; } while (0); \
      var = OpStatus::OK; \
      OpStatus::Ignore(var); \
    } else { \
      var = _catcher.error; \
    } \
  } while (0)
#endif // MEMTEST
#define TRAPD(var, block) \
  OP_STATUS var; \
  TRAP(var, block)
#define LEAVE(i) User::Leave(i)

class User
{
public:
	static void Leave(int error);
	static void LeaveNoMemory();
	static int LeaveIfError(int error);
};

/******************** Emulate EPOC cleanup stack API *********************/

#ifdef ENABLE_CLEANUPSTACK  // Awaiting rewrite to get rid of <vector>

#include <vector>

class CBase { public: virtual ~CBase() {};};

class CleanupStack
{
private:
	class CleanupStackItem;
public:
	static void PushL(const CleanupStackItem &item);
	static void PushL(CBase* cbaseptr);
	static void Pop();
	static void PopAndDestroy();
protected:
	/**
	 * Return the current height of the cleanup stack.  Used when
	 * cleaning up the stack
	 * 
	 * @see CleanupCatcher::CleanupCatcher(), CleanupUntil()
	 */
	static int StackHeight();

	/**
	 * Clean up all items on the cleanup stack until the number of
	 * items is the level given.Used when cleaning up the stack
	 * 
	 * @see CleanupCatcher::Cleanup(), StackHeight()
	 */
	static void CleanupUntil(unsigned int level);

	friend class CleanupCatcher;
private:
	class CleanupStackItem
	{
		CBase *cbaseptr;
	public:
		CleanupStackItem(CBase *cbaseptr = 0);
		void Cleanup();
	};
	static std::vector<CleanupStackItem> stack;
};

#endif // ENABLE_CLEANUPSTACK

#endif // not EPOC and not USE_CXX_EXCEPTIONS

/* Shorthand for use of OpStackAnchor<> */

#define ANCHOR(type,var) \
	OpStackAnchor< type > anchor_variable_ ## var (&var)

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

#define LEAVE_IF_ERROR( expr )                                                \
   do { OP_STATUS LEAVE_IF_ERROR_TMP = expr;                                  \
        if (OpStatus::IsError(LEAVE_IF_ERROR_TMP))							\
		{																		\
		LEAVE(LEAVE_IF_ERROR_TMP);												\
		}																	\
   } while(0)		

#define LEAVE_IF_NULL( expr ) \
	do { \
		if (NULL == (expr)) \
			LEAVE(OpStatus::ERR_NO_MEMORY); \
	} while(0)

#define LEAVE_IF_FATAL( expr )                                                \
   do { OP_STATUS LEAVE_IF_FATAL_TMP = expr;                                  \
        if (OpStatus::IsFatal(LEAVE_IF_FATAL_TMP))  \
	{	\
		LEAVE(LEAVE_IF_FATAL_TMP);\
	}	\
   } while(0)		

#define RETURN_IF_ERROR( expr )                                                 \
   do { OP_STATUS RETURN_IF_ERROR_TMP = expr;                                   \
        if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
   {	\
	   return RETURN_IF_ERROR_TMP; \
   }	\
   } while(0)

#define RETURN_IF_MEMORY_ERROR( expr )                   \
   do                                                    \
   {                                                     \
       OP_STATUS RETURN_IF_ERROR_TMP = expr;             \
       if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP)) \
       {                                                 \
	       return RETURN_IF_ERROR_TMP;                   \
       }                                                 \
   } while(0)

#define RETURN_VALUE_IF_ERROR( expr, val )                        \
   do { OP_STATUS RETURN_IF_ERROR_TMP = expr;                     \
        if (OpStatus::IsError(RETURN_IF_ERROR_TMP))		\
	{	\
		return val; \
	}	\
   } while(0)

#define RETURN_VOID_IF_ERROR( expr )                        \
   do { OP_STATUS RETURN_IF_ERROR_TMP = expr;               \
        if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
	{	\
		return; \
	}	\
   } while(0)

#define RAISE_AND_RETURN_IF_ERROR( expr )                   \
   do { OP_STATUS RETURN_IF_ERROR_TMP = expr;               \
        if (OpStatus::IsError(RETURN_IF_ERROR_TMP))	\
		{			   	\
				 g_memory_manager->RaiseCondition(RETURN_IF_ERROR_TMP);	\
				return RETURN_IF_ERROR_TMP; \
		}		\
   } while(0)

#define RAISE_AND_RETURN_VALUE_IF_ERROR( expr, val )            \
   do { OP_STATUS RETURN_IF_ERROR_TMP = expr;                   \
        if (OpStatus::IsError(RETURN_IF_ERROR_TMP)){			\
			g_memory_manager->RaiseCondition(RETURN_IF_ERROR_TMP);	\
			return val;	\
		}					\
   } while(0)

#define RAISE_AND_RETURN_VOID_IF_ERROR( expr )              \
   do { OP_STATUS RETURN_IF_ERROR_TMP = expr;               \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)){		\
		g_memory_manager->RaiseCondition(RETURN_IF_ERROR_TMP);	\
		return; \
		}		\
   } while(0)

#define TRAP_AND_RETURN(var, expr)               \
   do { TRAPD(var, expr);                         \
        if (OpStatus::IsError(var)) \
		{	\
		return var;   \
		}	\
   } while(0)

#define RETURN_IF_LEAVE(expr) \
	do { TRAPD(var, expr);                         \
		if (OpStatus::IsError(var)) \
		{\
		return var;   \
		}\
	} while(0)

#define TRAP_AND_RETURN_VALUE_IF_ERROR(var, expr, val )               \
   do { TRAP(var, expr);                         \
        if (OpStatus::IsError(var)) \
		{	\
		return val;   \
		}	\
   } while(0)

#define TRAP_AND_RETURN_VOID_IF_ERROR(var, expr)               \
   do { TRAP(var, expr);                         \
        if (OpStatus::IsError(var))		\
   {	\
		return;   \
	}	\
   } while(0)

#define RAISE_IF_ERROR( expr )              \
   do { OP_STATUS RETURN_IF_ERROR_TMP = expr;               \
		if (OpStatus::IsError(RETURN_IF_ERROR_TMP)){		\
		g_memory_manager->RaiseCondition(RETURN_IF_ERROR_TMP);	\
		}		\
   } while(0)

#ifndef RAISE_IF_MEMORY_ERROR
#define RAISE_IF_MEMORY_ERROR( expr )                           \
   do                                                           \
   {                                                            \
       OP_STATUS RETURN_IF_ERROR_TMP = expr;                    \
       if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP))        \
       {		                                                \
	       g_memory_manager->RaiseCondition(RETURN_IF_ERROR_TMP);	\
	   }                                                        \
   } while(0)
#endif


#endif // EXCEPTS_H
