/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Luca Venturi
**
** This file contains several macros and some classes that did not find a better place.
*/


#ifndef CACHE_UTILS_H

#define CACHE_UTILS_H

// Class that define a segment of file stored on disk; usually storages will just host all the file or nothing,
// but Multimedia Storages can host more than one, without covering all the file
struct StorageSegment
{
	// Content start of the segment. The position on the disk can be different
	OpFileLength content_start;
	// Bytes available on this segment.
	OpFileLength content_length;
};

/// Generic class implementing an iterator.
/// When the iterator is created, it can be iterated only once
/// Example of use:
/// 					while(ptr=iterator.Next(ops)) { /** Manage ptr */ }
template <class T>
class ListIterator
{
public:
	/// Return the next element, NULL if the last one is reached
	/// Call to Next() after the list is finished are legal, and they will return NULL
	virtual T* Next()=0;
};

/// Iterator that is able to delete the last element returned. This has two main advantages:
/// - The user does not have to know what operations are required to perform the delete correctly
/// - It is possible to keep a different internal representation of what has been returned.
/// Both these points are important to simplify the containers management
template <class T>
class ListIteratorDelete: public ListIterator<T>
{
public:
	/// Delete the last element returned by Next()
	virtual OP_STATUS DeleteLast()=0;
};

// Class that delete a pointer when the destructor is called
template <class T>
class SafePointer
{
private:
	// Pointer
	T *ptr;
	// TRUE if it is an array
	BOOL is_array;
	
public:
	/**
		Constructor. The pointer WILL be deleted on destruction.
		@param  pointer Pointer to the resource
		@param vector TRUE if the pointer is a vector (meaningfull only for owned pointers);
	*/
	SafePointer(T *pointer, BOOL vector) { ptr=pointer; is_array=vector; }
	/// Return the pointer
	T *GetPointer() { return ptr; }
	// Destructor
	~SafePointer()
	{
		if(is_array)
			OP_DELETEA(ptr);
		else
			OP_DELETE(ptr);
	}
};

// Utility macro to reduce the complexity: traverse all the contexts
// No references count performed
#define CACHE_CTX_WHILE_BEGIN_NOREF	{					\
	Context_Manager *manager=ctx_main;				\
	Context_Manager *next_manager=NULL;				\
	OP_ASSERT(manager);								\
	while(manager) {								\
		if(manager==ctx_main)						\
			next_manager = (Context_Manager *) context_list.First();	\
		else										\
			next_manager = (Context_Manager *) (manager->Suc());	\
		
// Utility macro to reduce the complexity
// No references count performed
#define CACHE_CTX_WHILE_END_NOREF					\
		manager=next_manager;						\
	}												\
}

// Utility macro to reduce the complexity
// No references count performed
#define CACHE_CTX_WHILE_END_NOREF_FROZEN			\
		manager=next_manager;						\
		if(!manager)								\
		{											\
			manager=frozen_manager;					\
			frozen_manager=NULL;					\
		}											\
	}												\
}

// Utility macro to reduce the complexity: traverse all the contexts
// References count performed
#define CACHE_CTX_WHILE_BEGIN_REF	{					\
	Context_Manager *manager=ctx_main;				\
	Context_Manager *next_manager=NULL;				\
	OP_ASSERT(manager);								\
	while(manager) {								\
		manager->IncReferencesCount();				\
		if(manager==ctx_main)						\
			next_manager = (Context_Manager *) context_list.First();	\
		else										\
			next_manager = (Context_Manager *) (manager->Suc());	\
		
// Utility macro to reduce the complexity
// References count performed
#define CACHE_CTX_WHILE_END_REF						\
		manager->DecReferencesCount();				\
		manager=next_manager;						\
	}												\
}

// Utility macro to reduce the complexity: traverse all the contexts
// References count performed
// This version also check frozen contexts
#define CACHE_CTX_WHILE_BEGIN_REF_FROZEN	{		\
	Context_Manager *manager=ctx_main;				\
	Context_Manager *next_manager=NULL;				\
	Context_Manager *frozen_manager=(Context_Manager *)frozen_contexts.First();	\
	OP_ASSERT(manager);								\
	while(manager) {								\
		manager->IncReferencesCount();				\
		if(manager==ctx_main)						\
			next_manager = (Context_Manager *) context_list.First();	\
		else										\
			next_manager = (Context_Manager *) (manager->Suc());	\
		
// Utility macro to reduce the complexity
// References count performed
#define CACHE_CTX_WHILE_END_REF_FROZEN				\
		manager->DecReferencesCount();				\
		manager=next_manager;						\
		if(!manager)								\
		{											\
			manager=frozen_manager;					\
			frozen_manager=NULL;					\
		}											\
	}												\
}

// Call the method in every context, then continue
#define CACHE_CTX_CALL_ALL(METHOD_CALL)	 			\
	CACHE_CTX_WHILE_BEGIN_REF							\
		manager->METHOD_CALL;						\
	CACHE_CTX_WHILE_END_REF

// Call the method in every context and get the first error, then continue
#define CACHE_CTX_CALL_ALL_OPS(OPS, METHOD_CALL)	\
	CACHE_CTX_WHILE_BEGIN_REF							\
		OP_STATUS temp=manager->METHOD_CALL;		\
		if(!OpStatus::IsSuccess(temp) && OpStatus::IsSuccess(OPS))	\
			OPS=temp;								\
	CACHE_CTX_WHILE_END_REF
	
// Call the method in the context with the right contextid; return void
// Reference count protection applies
#define CACHE_CTX_FIND_ID_VOID_REF(CTX_ID, METHOD_CALL)	 {		\
	Context_Manager *manager = FindContextManager(CTX_ID);	\
	if (manager)	{								\
		manager->IncReferencesCount();				\
		manager->METHOD_CALL;						\
		manager->DecReferencesCount();				\
		return;										\
	}												\
	OP_ASSERT(FALSE); /*Cache context not found"*/	\
}

// Call only the method in the context with the right contextid.
#define CACHE_CTX_FIND_ID_VOID(CTX_ID, METHOD_CALL)	 {		\
	Context_Manager *manager = FindContextManager(CTX_ID);	\
	if (manager)	{								\
		manager->METHOD_CALL;						\
		return;										\
	}												\
	OP_ASSERT(FALSE); /*Cache context not found"*/	\
}

// Call only the method in the context with the right contextid (with return);
// ERR is what to return when the context id is not found
#define CACHE_CTX_FIND_ID_RETURN(CTX_ID, METHOD_CALL, ERR)	 {	\
	Context_Manager *manager = FindContextManager(CTX_ID);		\
	if (manager)	{								\
		return manager->METHOD_CALL;				\
	}												\
	OP_ASSERT(FALSE); /*Cache context not found*/	\
	return ERR;										\
}

// Parameter used only for selftests
#ifdef SELFTEST
	#define SELFTEST_PARAM_COMMA_BEFORE(DEF) , DEF
#else
	#define SELFTEST_PARAM_COMMA_BEFORE(DEF)
#endif

// Operator and OEM cache macros
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	#define OEM_EXT_OPER_CACHE_MNG(DEF) DEF
	#define OEM_EXT_OPER_CACHE_MNG_BEFORE(DEF) ,DEF
#else
	#define OEM_EXT_OPER_CACHE_MNG(DEF)
	#define OEM_EXT_OPER_CACHE_MNG_BEFORE(DEF)
#endif

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_CACHE_OPERATION
	#define OEM_CACHE_OPER(DEF) DEF
	#define OEM_CACHE_OPER_AFTER(DEF) DEF,
#else
	#define OEM_CACHE_OPER(DEF)
	#define OEM_CACHE_OPER_AFTER(DEF)
#endif

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_CACHE_OPERATION && defined  __OEM_OPERATOR_CACHE_MANAGEMENT
	#define OPER_CACHE_COMMA() ,
#else
	#define OPER_CACHE_COMMA() 
#endif


#endif
