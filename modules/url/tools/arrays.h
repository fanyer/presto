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

#ifndef __URL_ARRAYS_H__
#define __URL_ARRAYS_H__

#include "modules/url/tools/arrays_decl.h"

#ifndef CONST_ARRAY_SIZE

# undef CONST_ARRAY
# undef PREFIX_CONST_ARRAY
# undef CONST_ENTRY
# undef CONST_END
# undef CONST_ARRAY_INIT
# undef CONST_ARRAY_SIZE
# undef CONST_DOUBLE_ARRAY_SIZE

/* Usage:
 *
 * For name "foo" this defines a const array by name g_foo
 * 
 * In the code:
 *	- Use CONST_ARRAY or PREFIX_CONST_ARRAY to declare the array
 *  - Use CONST_ENTRY and variants for each item, define macros for complex structures. Do not use commas between elements
 *	- Use CONST_END to end the array
 *
 *  CONST_ARRAY(foo, bar)
 *  CONST_DOUBLE_ENTRY(foo1, val1, foo2, val2)
 *  CONST_DOUBLE_ENTRY(foo1, val3, foo2, val4)
 *	CONST_END(bar)
 *
 *	In the module header file 
 *  - add #include "modules/url/tools/array_decl.h"
 *  - add an entry "DECLARE_MODULE_CONST_ARRAY(my_structname, foo);" in the module class for each array variable foo
 *  - add an entry 
 *		#ifndef HAS_COMPLEX_GLOBALS
 *		# define g_foo	CONST_ARRAY_GLOBAL_NAME(module, foo)
 *		#endif
 *  
 *    where module is the name of the module.
 *
 *  In the module object constructor (NOTE: DO NOT USE in the module header file)
 *	- add CONST_ARRAY_CONSTRUCT(foo);
 *
 *	In the Module Initilizer function (alternatively whereever you use "foo") (NOTE: DO NOT USE in the module header file)
 *	- add CONST_ARRAY_INIT_L(foo); 
 *
 *	In the Module destroy function  (NOTE: DO NOT USE in the module header file)
 *	- add CONST_ARRAY_SHUTDOWN(foo); 
 * 
 *
 *  For single entry variables
 *
 *	- Use CONST_ITEM or PREFIX_CONST_ITEM to declare the item
 *  - Use CONST_ENTRY_SINGLE and variants for each item, define macros for complex structures. Do not use commas between elements
 *	- Use CONST_ENTRY_END to end the item
 *
 *  CONST_ITEM(foo, bar)
 *  CONST_ENTRY_SINGLE(foo1, val1)
 *  CONST_ENTRY_SIGNLE(foo2, val2)
 *	CONST_END
 *
 *	In the module header file 
 *  - add #include "modules/url/tools/array_decl.h"
 *  - add an entry "DECLARE_MODULE_CONST_ITEM(my_structname, foo);" in the module class for each array variable foo
 *  - add an entry 
 *		#ifndef HAS_COMPLEX_GLOBALS
 *		# define g_foo	CONST_ARRAY_GLOBAL_NAME(module, foo)
 *		#endif
 *    alternatively
 *		# define foo	CONST_ARRAY_GLOBAL_NAME(module, foo)
 *	  if the variable is *not* known by the g_foo name 
 *  
 *    where module is the name of the module.
 *
 *	In the Module Initilizer function (alternatively whereever you use "foo") (NOTE: DO NOT USE in the module header file)
 *	- add CONST_ITEM_INIT(foo); 
 *
 *
 *  NOTE: NOTE: *ALWAYS* keep the intialization sequence in the same order as in the original file, in case pointers are needed to reference other structures
 */
    

# define CONST_ARRAY_NON_STATIC
# ifdef HAS_COMPLEX_GLOBALS
#  define CONST_ARRAY(structname, name, modulename) PREFIX_CONST_ARRAY(CONST_ARRAY_NON_STATIC, structname, name, modulename)
#  define PREFIX_CONST_ARRAY(prefix, structname, name, modulename) prefix structname const g_##name[] = {
#  define PREFIX_CONST_ARRAY_PRESIZED(structname, name, modulename) structname const g_##name[] = {

#  define CONST_ITEM(structname, name, modulename) PREFIX_CONST_ITEM(CONST_ARRAY_NON_STATIC, structname, name, modulename)
#  define PREFIX_CONST_ITEM(prefix, structname,name, modulename) prefix structname const g_##name =

#  define CONST_ENTRY(x)								x,
#  define CONST_DOUBLE_ENTRY(name1,x,name2,y)			{x,y},
#  define CONST_TRIPLE_ENTRY(name1,x,name2,y,name3,z)   {x,y,z},
#  define CONST_QUAD_ENTRY(name1,x,name2,y,name3,z,name4,u)   {x,y,z,u},
#  define CONST_PENT_ENTRY(name1,val1,name2,val2,name3,val3,name4,val4,name5,val5) {val1,val2,val3,val4,val5},

// For larger entries
#  define CONST_ENTRY_BLOCK_START {
#  define CONST_ENTRY_START(name, x) { x
#  define CONST_ENTRY_SINGLE(name, x) , x
#  define CONST_ENTRY_END		},

#  define CONST_ENTRY_SEP		,

#  define CONST_END(name)            };

/** Call this from the module initialization */
#  define CONST_ARRAY_INIT_L(name) ((void)0)
/** Call this from the module initialization for a phases init, use init_phase as variable name  (0 = all, 1= alloc, 2= init) */
#  define CONST_ARRAY_PHASE_INIT_L(name) ((void)0)
/** Call this from the module shutdown */
#  define CONST_ARRAY_SHUTDOWN(name) ((void)0)
/** Call this to get the size of the array */
#  define CONST_ARRAY_SIZE(modulename, name) ARRAY_SIZE(g_##name )

#  define CONST_ITEM_ENTRY_START(name, x) { x
#  define CONST_ITEM_ENTRY_SINGLE(name, x) , x
#  define CONST_ITEM_ENTRY_END		};
#  define CONST_ITEM_ENTRY_END2		}

/** Call this from the module initialization */
#  define CONST_ITEM_INIT(name)	((void)0)
/** Call this from the module initialization for a phases init, use init_phase as variable name  (0 = all, 1= alloc, 2= init) */
#  define CONST_ITEM_PHASE_INIT(name) ((void)0)
/** Call this from the module initialization */
#  define CONST_ITEM_SHUTDOWN(name) ((void)0)

// local file/function variation, used for short-term arrays.

#  define LOCAL_CONST_ARRAY(structname, name) LOCAL_PREFIX_CONST_ARRAY(CONST_ARRAY_NON_STATIC, structname, name)
#  define LOCAL_PREFIX_CONST_ARRAY(prefix, structname,name) prefix structname const name[] = {

/** Call this from the function initialization */
#  define LOCAL_CONST_ARRAY_INIT_L(name) ((void)0)
#  define LOCAL_CONST_ARRAY_INIT(name, return_value) ((void)0)
#  define LOCAL_CONST_ARRAY_INIT_VOID(name) ((void)0)

/** Call this from the function initialization if phased init is desired (0 = all, 1= alloc, 2= init) */
#  define LOCAL_CONST_ARRAY_PHASE_INIT_L(name, phase) ((void)0)
#  define LOCAL_CONST_ARRAY_PHASE_INIT(name, return_value, phase) ((void)0)
#  define LOCAL_CONST_ARRAY_PHASE_INIT_VOID(name, phase) ((void)0)

/** Call this from the module shutdown */
#  define LOCAL_CONST_ARRAY_SHUTDOWN(name) ((void)0)
/** Call this to get the size of the array */
#  define LOCAL_CONST_ARRAY_SIZE(name) ARRAY_SIZE( name )
#  define LOCAL_DECLARE_CONST_ARRAY(structname, name) ((void)0)


# else

#define COMPLEX_EMPTY_CLASS_SCOPE

#  define COMPLEX_CONST_ARRAY_DEF_CONSTRUCT(structname, name, class_scope) \
	class_scope _cda_##name##_st::_cda_##name##_st():m_array(NULL), m_size(0), m_extern(FALSE){} \

#  define COMPLEX_CONST_ARRAY_CONSTRUCT_PREDEF_ARRAY(structname, name, class_scope, external_array) \
	class_scope _cda_##name##_st::_cda_##name##_st():m_array(external_array), m_size(ARRAY_SIZE(external_array)), m_extern(TRUE){} \

#  define COMPLEX_CONST_ARRAY_SETUP(structname, name, class_scope) \
	class_scope _cda_##name##_st::~_cda_##name##_st(){if(m_array) Destroy();}\
	void class_scope _cda_##name##_st::InitL(int phase)\
			{\
				if(m_array != NULL && phase <2)\
					return;\
				if(phase <2) \
				{\
					if(!m_extern)\
					{\
						size_t alloc_count = FillTable();\
						if(!alloc_count)\
							alloc_count = 1;\
						m_array = new (ELeave) structname[alloc_count];\
						m_size = alloc_count;\
					}\
					if(phase == 1)	return;\
				}\
				m_size = FillTable();\
			}\
			void class_scope _cda_##name##_st::Destroy()\
			{\
				if(!m_extern)\
					delete [] m_array;\
				m_array = NULL;\
				m_size = 0;\
			}\
			size_t class_scope _cda_##name##_st::FillTable()\
			{ structname *local=m_array; size_t i=0;

#  define COMPLEX_CONST_ARRAY_SETUP_PRESIZED(structname, name, class_scope) \
	class_scope _cda_##name##_st::~_cda_##name##_st(){}\
	void class_scope _cda_##name##_st::Destroy(){}\
	void class_scope _cda_##name##_st::InitL(int phase)\
			{\
				if(phase == 1)	return;\
				m_size = FillTable();\
			}\
			size_t class_scope _cda_##name##_st::FillTable()\
			{ structname *local=m_array; size_t i=0;

#  define PREFIX_CONST_ARRAY(prefix, structname, name, modulename) \
	COMPLEX_CONST_ARRAY_DEF_CONSTRUCT(structname, name, modulename##_MODULE_CLASS_NAME::)\
	COMPLEX_CONST_ARRAY_SETUP(structname, name, modulename##_MODULE_CLASS_NAME::)

#  define CONST_ARRAY(structname, name, modulename) PREFIX_CONST_ARRAY(CONST_ARRAY_NON_STATIC, structname, name, modulename)

#  define PREFIX_CONST_ARRAY_PREDEF_ARRAY(structname, name, modulename, external_array) \
	COMPLEX_CONST_ARRAY_CONSTRUCT_PREDEF_ARRAY(structname, name, modulename##_MODULE_CLASS_NAME:: , external_array)\
	COMPLEX_CONST_ARRAY_SETUP(structname, name, modulename##_MODULE_CLASS_NAME::)

#  define CONST_ARRAY_PREDEF_ARRAY(structname, name, modulename, external_array) \
			PREFIX_CONST_ARRAY_PREDEF_ARRAY(CONST_ARRAY_NON_STATIC, structname, name, modulename, external_array)

#  define PREFIX_CONST_ARRAY_PRESIZED(structname, name, modulename) \
	COMPLEX_CONST_ARRAY_SETUP_PRESIZED(structname, name, modulename##_MODULE_CLASS_NAME::)

#  define CONST_ARRAY_PRESIZED(structname, name, modulename) \
			PREFIX_CONST_ARRAY_PRESIZED(CONST_ARRAY_NON_STATIC, structname, name, modulename)

#ifdef _DEBUG
#  define TEST_CONST_ARRAY_SIZE if(i>=m_size){OP_ASSERT(!"This CONST_ARRAY is not properly sized. Overflow"); LEAVE(OpStatus::ERR_OUT_OF_RANGE);}
#else
#  define TEST_CONST_ARRAY_SIZE ((void)0)
#endif

#  define CONST_ENTRY(x)           if (local){TEST_CONST_ARRAY_SIZE; *(local++) = x;} i++;
#  define CONST_DOUBLE_ENTRY(name1,x,name2,y)           if (local){TEST_CONST_ARRAY_SIZE; local->name1 = x;local->name2 = y;local++;} i++;
#  define CONST_TRIPLE_ENTRY(name1,x,name2,y,name3,z)   if (local){TEST_CONST_ARRAY_SIZE; local->name1 = x;local->name2 = y;\
																	local->name3 = z;local++;} i++;
#  define CONST_QUAD_ENTRY(name1,x,name2,y,name3,z,name4,u)   if (local){TEST_CONST_ARRAY_SIZE; local->name1 = x;\
																	local->name2 = y;local->name3 = z; local->name4 = u;local++;} i++;
#  define CONST_PENT_ENTRY(name1,val1,name2,val2,name3,val3,name4,val4,name5,val5) \
	if (local) { TEST_CONST_ARRAY_SIZE; \
		local->name1 = val1; local->name2 = val2; local->name3 = val3; \
		local->name4 = val4; local->name5 = val5; local++; \
	} i++;

// For larger entries
#  define CONST_ENTRY_BLOCK_START {
#  define CONST_ENTRY_START(name, x) 		if (local){ TEST_CONST_ARRAY_SIZE; local->name = x;
#  define CONST_ENTRY_SINGLE(name, x) if (local) local->name = x;
#  define CONST_ENTRY_END		if (local) local ++;} i++;

#  define CONST_ENTRY_SEP		

#  define CONST_END(name) return i;}

/** Call this from the module initialization */
#  define CONST_ARRAY_INIT_L(name) m_cda_##name.InitL()
/** Call this from the module initialization for a phases init, use init_phase as variable name  (0 = all, 1= alloc, 2= init) */
#  define CONST_ARRAY_PHASE_INIT_L(name) m_cda_##name.InitL(init_phase)
/** Call this from the module destruction */
#  define CONST_ARRAY_SHUTDOWN(name) m_cda_##name.Destroy()
/** Call this to get the size of the array */
#  define CONST_ARRAY_SIZE(modulename, name) (g_opera->modulename##_module.m_cda_##name .m_size)

/* **** Constant variable items **** */

#  define COMPLEX_CONST_ITEM(prefix, structname, name, class_scope) \
		void class_scope _cda_##name##_st::InitL(int phase)\
			{	if(phase == 1) return;

#  define PREFIX_CONST_ITEM(prefix, structname, name, modulename) \
		COMPLEX_CONST_ITEM(prefix, structname, name,  modulename##_MODULE_CLASS_NAME::);

#ifndef CONST_ITEM_GLOBAL_NAME
#define CONST_ITEM_GLOBAL_NAME(modulename, name) g_opera->modulename##_module.m_cdi_##name .m_item
#endif

#  define CONST_ITEM_ENTRY_START(name, x) 		m_item.name = x;
#  define CONST_ITEM_ENTRY_SINGLE(name, x)		m_item.name = x;
#  define CONST_ITEM_ENTRY_END		}
#  define CONST_ITEM_ENTRY_END2		}

/** Call this from the module initialization */
#  define CONST_ITEM_INIT(name) m_cdi_##name.InitL()
/** Call this from the module initialization for a phases init, use init_phase as variable name  (0 = all, 1= alloc, 2= init) */
#  define CONST_ITEM_PHASE_INIT(name) m_cdi_##name.InitL(init_phase)
/** Call this from the module initialization */
#  define CONST_ITEM_SHUTDOWN(name) ((void)0)

// local file/function variation, used for short-term arrays.

#  define LOCAL_CONST_ARRAY(structname, name) LOCAL_PREFIX_CONST_ARRAY(CONST_ARRAY_NON_STATIC, structname, name)
#  define LOCAL_PREFIX_CONST_ARRAY(prefix, structname,name) \
			CONST_ARRAY_WRAPPER_STRUCT(structname, name);   \
			COMPLEX_CONST_ARRAY_DEF_CONSTRUCT(structname, name, COMPLEX_EMPTY_CLASS_SCOPE)\
			COMPLEX_CONST_ARRAY_SETUP( structname, name, COMPLEX_EMPTY_CLASS_SCOPE)

#  define LOCAL_FINAL_SETUP(name) name = l_cda_i_##name.m_array

/** Call this from the function initialization */
#  define LOCAL_CONST_ARRAY_INIT_L(name) l_cda_i_##name.InitL(0); LOCAL_FINAL_SETUP(name)
#  define LOCAL_CONST_ARRAY_INIT(name, return_value) TRAPD(cda_err_##name , l_cda_i_##name.InitL(0)); \
				if(OpStatus::IsError(cda_err_##name )) return return_value; \
				LOCAL_FINAL_SETUP(name)
#  define LOCAL_CONST_ARRAY_INIT_VOID(name) TRAPD(cda_err_##name , l_cda_i_##name.InitL(0)); \
				if(OpStatus::IsError(cda_err_##name )) return; \
				LOCAL_FINAL_SETUP(name)

/** Call this from the function initialization if phased init is desired (0 = all, 1= alloc, 2= init) */
#  define LOCAL_CONST_ARRAY_PHASE_INIT_L(name, phase) l_cda_i_##name.InitL(phase); LOCAL_FINAL_SETUP(name)
#  define LOCAL_CONST_ARRAY_PHASE_INIT(name, return_value, phase) TRAPD(cda_err_##name , l_cda_i_##name.InitL(phase));\
				if(OpStatus::IsError(cda_err_##name )) return return_value;\
				LOCAL_FINAL_SETUP(name)m
#  define LOCAL_CONST_ARRAY_PHASE_INIT_VOID(name, phase) TRAPD(cda_err_##name , l_cda_i_##name.InitL(phase));\
				if(OpStatus::IsError(cda_err_##name )) return;\
				LOCAL_FINAL_SETUP(name)

/** Call this from the module shutdown, not necessary when using LOCAL_DECLARE_CONST_ARRAY */
#  define LOCAL_CONST_ARRAY_SHUTDOWN(name) l_cda_i_##name.Destroy(); name = NULL
/** Call this to get the size of the array */
#  define LOCAL_CONST_ARRAY_SIZE(name) (l_cda_i_##name.m_size)
/** Declare variables. Includes shutdown */
#  define LOCAL_DECLARE_CONST_ARRAY(structname, name) \
		structname * name = NULL; \
		_cda_##name##_st l_cda_i_##name; \
		ANCHOR(_cda_##name##_st , l_cda_i_##name )


# endif // HAS_COMPLEX_GLOBALS

# define CONST_DOUBLE_ARRAY(structname, name, modulename) CONST_ARRAY(structname, name, modulename)
# define PREFIX_CONST_DOUBLE_ARRAY(prefix, structname,name, modulename) PREFIX_CONST_ARRAY(prefix, structname,name, modulename) 
# define CONST_DOUBLE_END(name)            CONST_END(name)

# define CONST_DOUBLE_ARRAY_INIT_L(name) CONST_ARRAY_INIT_L(name)
# define CONST_DOUBLE_ARRAY_SIZE(module_name, name) CONST_ARRAY_SIZE(module_name, name)
#endif

#endif
