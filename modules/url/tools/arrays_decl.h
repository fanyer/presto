/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef __URL_ARRAYS_DECL_H__
#define __URL_ARRAYS_DECL_H__

#ifndef CONST_ARRAY_SIZE

/** These defintions are used in the module header files
 *
 *  For a name "foo" this defines the variable g_foo that 
 *	must be used in all access to the const array.
 *  The naming is used to avoid preprocessor trouble
 *
 *	DECLARE_MODULE_CONST_ARRAY(my_structname, foo)
 *	
 *	This defines a module member "m_foo" when there is 
 *	no complex global support.
 *
 *	There must also be an entry in the module header file that defines 
 *		#ifndef HAS_COMPLEX_GLOBALS
 *		# define g_foo	CONST_ARRAY_GLOBAL_NAME(module, foo)
 *		#endif
 *  
 *  where module is the name of the module.
 *  The use of the ifdef is used to avoid preprocessor problems
 */
#ifdef HAS_COMPLEX_GLOBALS
#define DECLARE_MODULE_CONST_ARRAY(structname, name)
#define DECLARE_MODULE_CONST_ARRAY_PRESIZED(structname, name, presize)
#define CONST_ARRAY_GLOBAL_NAME(modulename, name) g_##name
#define DECLARE_MODULE_CONST_ITEM(structname, name)
#define CONST_ITEM_GLOBAL_NAME(modulename, name) g_##name
#else
#ifndef GLOBAL_CONST_WRAPPER_BASE
#define GLOBAL_CONST_WRAPPER_BASE
#endif

#define CONST_ARRAY_WRAPPER_STRUCT(structname, name) \
  	struct _cda_##name##_st GLOBAL_CONST_WRAPPER_BASE {   \
		structname *m_array;    \
		size_t  m_size;         \
		BOOL	m_extern;		\
								\
		_cda_##name##_st(); \
		virtual ~_cda_##name##_st();\
		virtual void InitL(int phase=0);\
		virtual void Destroy();\
	private:\
		size_t FillTable();\
	}

#define CONST_ARRAY_WRAPPER_STRUCT_PRESIZED(structname, name, presize) \
  	struct _cda_##name##_st GLOBAL_CONST_WRAPPER_BASE {   \
		structname m_array[presize];    \
		size_t  m_size;         \
								\
		_cda_##name##_st():m_size(presize){}; \
		virtual ~_cda_##name##_st();\
		virtual void InitL(int phase=0);\
		virtual void Destroy();\
	private:\
		size_t FillTable();\
	}


#  define DECLARE_MODULE_CONST_ARRAY(structname, name)   \
	CONST_ARRAY_WRAPPER_STRUCT(structname, name);   \
	struct _cda_##name##_st m_cda_##name

#  define DECLARE_MODULE_CONST_ARRAY_PRESIZED(structname, name, presized)   \
	CONST_ARRAY_WRAPPER_STRUCT_PRESIZED(structname, name, presized);   \
	struct _cda_##name##_st m_cda_##name


#define CONST_ARRAY_GLOBAL_NAME(modulename, name) g_opera-> modulename##_module.m_cda_##name .m_array 

#define CONST_ITEM_WRAPPER_STRUCT(structname, name)\
	struct _cda_##name##_st  GLOBAL_CONST_WRAPPER_BASE {   \
		structname m_item;\
								\
		_cda_##name##_st(){}; \
		virtual void InitL(int phase=0);\
		virtual void Destroy(){};\
	}

#  define DECLARE_MODULE_CONST_ITEM(structname, name)   \
	CONST_ITEM_WRAPPER_STRUCT(structname, name);\
	struct _cda_##name##_st m_cdi_##name

#define CONST_ITEM_GLOBAL_NAME(modulename, name) g_opera->modulename##_module.m_cdi_##name .m_item

#endif

#endif //CONST_ARRAY_SIZE

#endif // __URL_ARRAYS_DECL_H__


