// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

//The macro LIBRARY_CALL should be used in this file to forward-declare anything defined in proc_loader.cpp.
//For each LIBRARY_CALL entry found in proc_loader.cpp, an identical one should be placed in this file.
//Also, any type needed by the declaration of procedures made here should also be declared in this file.
//
//See proc_loader.h for more details

//This file MUST be included by the platform's system.h

#define LIBRARY_CALL(library, return_type, calling_convention, function_name, args_list, unqualified_args, error_return) \
return_type calling_convention function_name##STUB args_list; \
BOOL HAS##function_name (); \
static return_type (calling_convention * OP##function_name) args_list = function_name##STUB; \

//Declaration of types used by LIBRARY_CALL entries below.
//Please, keep track of what uses what. There is no particular order here.


//Please, keep this list ordered:
//The libraries appear in alphabetical order and procedures loaded from them appear under the corresponding library, also sorted alphabetically

//dwmapi
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmIsCompositionEnabled, (BOOL *pfEnabled), (pfEnabled), E_NOTIMPL);