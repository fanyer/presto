/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_BASE_BASEINCLUDES_H
#define MODULES_HARDCORE_BASE_BASEINCLUDES_H

#include "modules/hardcore/hardcore_capabilities.h"

#include "modules/hardcore/base/deprecate.h"

#include "modules/hardcore/base/system.h"

#include "modules/hardcore/features/features.h"

// Include capability definitions
#include "modules/hardcore/base/capabilities.h"

#include "modules/hardcore/base/op_types.h"

#ifdef __cplusplus
#  define OP_EXTERN_C extern "C"
#  define OP_EXTERN_C_BEGIN extern "C" {
#  define OP_EXTERN_C_END }
#else
#  define OP_EXTERN_C
#  define OP_EXTERN_C_BEGIN
#  define OP_EXTERN_C_END
#endif

#ifdef __cplusplus
#include "modules/hardcore/base/opstatus.h"
#endif // __cplusplus

#ifdef HAS_COMPLEX_GLOBALS
# define CONST_ARRAY(name,type) const type const name[] = {
# define CONST_ENTRY(x)         x
# define CONST_END(name)            };
# define CONST_ARRAY_INIT(name) ((void)0)
#else
# define CONST_ARRAY(name,type) void init_##name () { const type *local = name; int i=0;
# define CONST_ENTRY(x)         local[i++] = x
# define CONST_END(name)        ;}
# define CONST_ARRAY_INIT(name) init_##name()
#endif // HAS_COMPLEX_GLOBALS

#include "modules/memory/memory_fundamentals.h"  // new, op_malloc, ...

#ifdef __cplusplus
# ifdef LOC_STR_NAMESPACE
namespace Str { class LocaleString; };
# else
#  include "modules/locale/locale-enum.h" // Strings
# endif // LOC_STR_NAMESPACE
#include "modules/util/excepts.h"	// Error and exception handling
#include "modules/hardcore/unicode/unicode.h" // NOT_A_CHARACTER macro
#include "modules/unicode/unicode.h" // The Unicode:: functions.
#include "modules/stdlib/include/opera_stdlib.h" // After unicode.h, but before opstring.h
#include "modules/debug/debug.h" // OP_ASSERT and other debug macros
#include "modules/util/opstring.h"
#include "modules/hardcore/base/op_point.h"
#include "modules/hardcore/base/op_rect.h"
#include "modules/hardcore/base/op_doublepoint.h"
#include "modules/hardcore/mh/constant.h"
#include "modules/hardcore/opera/opera.h"
#endif // __cplusplus

#endif // !MODULES_HARDCORE_BASE_BASEINCLUDES_H
