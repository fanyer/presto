// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#ifndef PROC_LOADER_H
#define PROC_LOADER_H

#ifdef OPERA_DLL
# include PLATFORM_HC_PROFILE_INCLUDE
#else
# define OP_PROFILE_METHOD(x)
#endif

/*
READY_LIBRARY and LIBRARY_CALL are two macros used to prepare functions that take care of loading
procedures from external libraries on demand.

READY_LIBRARY is used to indicate we want to load functions from a specific library. LIBRARY_CALL makes a given procedure
from a readied library available to Opera code.
The functions are made available through the proc_loader_headers.h files where a declaration is generated while the actual
implementations are generated in files that include this one.

For a loaded procedure named MyProcedure(), this will generate:
- An OPMyProcedure() function that can be called as the original procedure would be. It will handle loading of the
  dll if necessary and then call the actual loaded procedure if it was found. Otherwise it will return an error
  defined with the LIBRARY_CALL macro.
- A HASMyProcedure() function that can be called to determine if a given procedure was loaded or can be loaded.
  If the procedure wasn't loaded already, it will load it and make it available, returning TRUE if it succeeded and
  FALSE otherwise. This is provided for convenience, to allow skipping blocks of code that would anyway be useless
  if OPMyProcedure calls in them are going to fail anyway. It is not necessary to call this before using OPMyProcedure.

Full example:


For a procedure:

DWORD WINAPI MyProcedure(ULONG a, PVOID b);

Which is documented to return -1 if it fails and contained in mydll.dll; you would use:

READY_LIBRARY(mydll, "mydll.dll");
LIBRARY_CALL(nydll, DWORD, WINAPI, MyProcedure, (ULONG a, PVOID b), (a, b), -1);

Which will make the following functions available
BOOL HASMyProcedure();
DWORD WINAPI OPMyProcedure(ULONG a, PVOID b); which will return -1 if it fails loading the procedure to start with.

*/

/** Prepares a function that loads a library
  *
  * @param library			A symbolic name for the library, to be used in the corresponding LIBRARY_CALL instances
  * @param library_file		The actual name of the library to load, as a uni_char string
  */
#define READY_LIBRARY(library, library_file) \
static HMODULE LoadLibrary##library () \
{ \
	OP_PROFILE_METHOD("Loaded " #library); \
	static HMODULE s_loaded_lib = NULL; \
	if (s_loaded_lib == (HMODULE)-1) \
		return NULL; \
	if (s_loaded_lib) \
		return s_loaded_lib; \
	s_loaded_lib = WindowsUtils::SafeLoadLibrary(library_file); \
	if (!s_loaded_lib) \
	{ \
		s_loaded_lib = (HMODULE)-1; \
		return NULL; \
	} \
	return s_loaded_lib; \
}

/** Prepares functions to access a procedure loaded from a dll
  *
  * @param library				A library symbolic name, as used in a previous READY_LIBRARY instance
  * @param return_type			The return type of the loaded procedure
  * @param calling_convention	The calling convention of the loaded procedure
  * @param function_name		The name of the procedure as provided in the dll. It will also be used to generate the functions names
  *								provided by the macros.
  * @param args_list			The full argument list to be passed to the loaded procedure. It should be provided in parentheses
  * @param unqualified_args		The same list as args_list, without the types specified. The argument names must match the ones given
  *								in args_list
  * @param error_return			A value to return if the procedure fails to be loaded. It should be of type return_type
  */
#undef LIBRARY_CALL
#define LIBRARY_CALL(library, return_type, calling_convention, function_name, args_list, unqualified_args, error_return) \
return_type calling_convention function_name##FAIL args_list \
{ \
	return error_return; \
} \
void function_name##LOAD () \
{ \
	OP_PROFILE_METHOD("Loaded " #function_name); \
	HMODULE module = LoadLibrary##library(); \
	if (!module) \
	{ \
		OP##function_name = function_name##FAIL; \
	} \
	else \
	{ \
		OP##function_name = (return_type (calling_convention *)args_list) GetProcAddress(module, #function_name); \
	} \
	if (!OP##function_name) \
	{ \
		OP##function_name = function_name##FAIL; \
	} \
} \
return_type calling_convention function_name##STUB args_list \
{ \
	function_name##LOAD(); \
	return OP##function_name unqualified_args; \
} \
BOOL HAS##function_name() \
{ \
	if (OP##function_name == function_name##STUB) \
		function_name##LOAD(); \
	return (OP##function_name != function_name##FAIL); \
}

#endif //PROC_LOADER_H
