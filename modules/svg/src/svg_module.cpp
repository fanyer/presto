/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGCache.h"

#if !defined(HAS_COMPLEX_GLOBALS)
#ifdef _DEBUG
extern void init_g_svg_enum_type_strings();
extern void init_g_svg_enum_name_strings();
#endif // _DEBUG
extern void init_g_svg_enum_entries();
#if defined(_DEBUG) || defined(SVG_LOG_ERRORS)
extern void init_g_svg_errorstrings();
#endif // _DEBUG || SVG_LOG_ERRORS
#endif // !HAS_COMPLEX_GLOBALS

SvgModule::SvgModule()
		: svg_properties_pool(SVG_PROPERTIES_POOL_SIZE),
		  manager(NULL),
		  attr_serial(1) // 0 is reserved for serials for non-existing attributes
#ifdef _DEBUG
		, debug_is_in_layout_pass(FALSE)
#endif // _DEBUG
{
}

void
SvgModule::InitL(const OperaInitInfo& info)
{
#ifdef _DEBUG
	CONST_ARRAY_INIT(g_svg_enum_type_strings);
	CONST_ARRAY_INIT(g_svg_enum_name_strings);
#endif // _DEBUG
	CONST_ARRAY_INIT(g_svg_enum_entries);
#if defined(_DEBUG) || defined(SVG_LOG_ERRORS)
	CONST_ARRAY_INIT(g_svg_errorstrings);
#endif // _DEBUG || SVG_LOG_ERRORS

	manager = OP_NEW_L(SVGManagerImpl, ());
	OP_STATUS status = ((SVGManagerImpl*)manager)->Create();
	if(OpStatus::IsError(status))
	{
		OP_DELETE(manager);
		manager = NULL;
		LEAVE(status);
	}

	svg_properties_pool.ConstructL();
}

void
SvgModule::Destroy()
{
#if 0 // Disabled for now, since the svg module is destroyed before
	  // the ecmascript module.
	OP_ASSERT(SVGObject::reference_counter == 0);
#endif // 0

	svg_properties_pool.Clean();
    OP_DELETE(manager);
    manager = NULL;
}

BOOL SvgModule::FreeCachedData(BOOL toplevel_context)
{
	if(toplevel_context)
	{
		((SVGManagerImpl*)manager)->GetCache()->Clear();
		return TRUE;
	}
	return FALSE;
}

#if defined(_DEBUG) || defined(SVG_LOG_ERRORS)
const uni_char* OpSVGStatus::GetErrorString(OP_STATUS error)
{
        if(error >= WRONG_NUMBER_OF_ARGUMENTS
# ifdef _DEBUG
           && error <= SKIP_CHILDREN
# else
           && error <= INTERNAL_ERROR
# endif
           )
                return g_svg_errorstrings[error - WRONG_NUMBER_OF_ARGUMENTS];
        return NULL;
}
#endif // _DEBUG || SVG_LOG_ERRORS

#endif // SVG_SUPPORT

