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

#ifndef __ROOT_ARRAYS_H__
#define __ROOT_ARRAYS_H__


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
 *  - add an entry "DECLARE_MODULE_CONST_ARRAY(my_structname, foo);" in the module class for each variable foo
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
 */
    

# define CONST_ARRAY_NON_STATIC
#  define CONST_ARRAY(structname, name, modulename) PREFIX_CONST_ARRAY(CONST_ARRAY_NON_STATIC, structname, name, modulename)
#  define PREFIX_CONST_ARRAY(prefix, structname,name, modulename) prefix structname const g_##name[] = {

#  define CONST_ENTRY(x)								x,
#  define CONST_DOUBLE_ENTRY(name1,x,name2,y)			{x,y},
#  define CONST_TRIPLE_ENTRY(name1,x,name2,y,name3,z)   {x,y,z},
#  define CONST_QUAD_ENTRY(name1,x,name2,y,name3,z,name4,u)   {x,y,z,u},

// For larger entries
#  define CONST_ENTRY_START(name, x) { x
#  define CONST_ENTRY_SINGLE(name, x) , x
#  define CONST_ENTRY_END		},

#  define CONST_END(name)            };

/** Call this from the module initialization */
#  define CONST_ARRAY_INIT_L(name) ((void)0)
/** Call this from the module shutdown */
#  define CONST_ARRAY_SHUTDOWN(name) ((void)0)
/** Call this to get the size of the array */
#  define CONST_ARRAY_SIZE(modulename, name) (sizeof(g_##name )/sizeof(g_##name[0]))
#  define DECLARE_MODULE_CONST_ARRAY(structname, name)
#  ifndef CONST_ARRAY_GLOBAL_NAME
#   define CONST_ARRAY_GLOBAL_NAME(modulename, name) g_##name
#  endif 

// local file/function variation, used for short-term arrays.

#  define LOCAL_CONST_ARRAY(structname, name) LOCAL_PREFIX_CONST_ARRAY(CONST_ARRAY_NON_STATIC, structname, name)
#  define LOCAL_PREFIX_CONST_ARRAY(prefix, structname,name) prefix structname const name[] = {

/** Call this from the function initialization */
#  define LOCAL_CONST_ARRAY_INIT_L(name) ((void)0)
#  define LOCAL_CONST_ARRAY_INIT(name, return_value) ((void)0)
#  define LOCAL_CONST_ARRAY_INIT_VOID(name) ((void)0)

/** Call this from the module shutdown */
#  define LOCAL_CONST_ARRAY_SHUTDOWN(name) ((void)0)
/** Call this to get the size of the array */
#  define LOCAL_CONST_ARRAY_SIZE(name) (sizeof( name )/sizeof( name[0]))
#  define LOCAL_DECLARE_CONST_ARRAY(structname, name) ((void)0)


# define CONST_DOUBLE_ARRAY(structname, name, modulename) CONST_ARRAY(structname, name, modulename)
# define PREFIX_CONST_DOUBLE_ARRAY(prefix, structname,name, modulename) PREFIX_CONST_ARRAY(prefix, structname,name, modulename) 
# define CONST_DOUBLE_END(name)            CONST_END(name)

# define CONST_DOUBLE_ARRAY_INIT_L(name) CONST_ARRAY_INIT_L(name)
# define CONST_DOUBLE_ARRAY_SIZE(module_name, name) CONST_ARRAY_SIZE(module_name, name)

#endif
