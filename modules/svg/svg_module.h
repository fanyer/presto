/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_SVG_SVG_MODULE_H
#define MODULES_SVG_SVG_MODULE_H

#ifdef SVG_SUPPORT

#include "modules/hardcore/opera/module.h"
#include "modules/layout/layoutpool.h"

#include "modules/svg/src/SVGInternalEnumTables.h"
#include "modules/svg/src/svgpch.h"

#define SVG_PROPERTIES_POOL_SIZE 30

class SVGManager;

class SvgModule : public OperaModule
{
public:
    SvgModule();

    virtual void InitL(const OperaInitInfo& info);
    virtual void Destroy();

	/**
	 * Tells the module to try to free cached data.
	 * 
	 * @toplevel_context FALSE means we could potentially be
	 *                   inside a new() call or such. TRUE
	 *                   means the stack is "safe".
	 * @return TRUE if an attempt to free data was made.
	 */
	virtual BOOL FreeCachedData(BOOL toplevel_context);

	LayoutPool			svg_properties_pool;

    SVGManager*         manager;            // Public 
	UINT32				attr_serial;        // Public
#ifdef _DEBUG
	BOOL debug_is_in_layout_pass;
#endif // _DEBUG

#if !defined(HAS_COMPLEX_GLOBALS)
#ifdef _DEBUG
	const char*	m_svg_enum_type_strings[SVGENUM_TYPE_COUNT];
	const char*	m_svg_enum_name_strings[SVG_ENUM_ENTRIES_COUNT];
#endif // _DEBUG
	SVGEnumEntry m_svg_enum_entries[SVG_ENUM_ENTRIES_COUNT+1]; // the +1 is the sentinel
#if defined(_DEBUG) || defined(SVG_LOG_ERRORS)
	const uni_char* m_svg_errorstrings[1 + OpSVGStatus::TIMED_OUT - OpSVGStatus::WRONG_NUMBER_OF_ARGUMENTS];
#endif // _DEBUG || SVG_LOG_ERRORS
#endif // !HAS_COMPLEX_GLOBALS
};

#if !defined(HAS_COMPLEX_GLOBALS)
# ifdef _DEBUG
#  define g_svg_enum_type_strings	(g_opera->svg_module.m_svg_enum_type_strings)
#  define g_svg_enum_name_strings	(g_opera->svg_module.m_svg_enum_name_strings)
# endif // _DEBUG
# define g_svg_enum_entries			(g_opera->svg_module.m_svg_enum_entries)
# if defined(_DEBUG) || defined(SVG_LOG_ERRORS)
# define g_svg_errorstrings			(g_opera->svg_module.m_svg_errorstrings)
# endif // _DEBUG || SVG_LOG_ERRORS
#else
extern const char* const g_svg_enum_type_strings[];
extern const char* const g_svg_enum_name_strings[];
#endif // !HAS_COMPLEX_GLOBALS

#define g_svg_manager            g_opera->svg_module.manager
#define g_svg_properties_pool    (&g_opera->svg_module.svg_properties_pool)
#ifdef _DEBUG
#define g_svg_debug_is_in_layout_pass g_opera->svg_module.debug_is_in_layout_pass
#endif // _DEBUG

#define SVG_MODULE_REQUIRED

// Enables svg search highlighting
#if defined(SVG_SUPPORT_TEXTSELECTION) && !defined(HAS_NO_SEARCHTEXT) && defined(SEARCH_MATCHES_ALL)
# define SVG_SUPPORT_SEARCH_HIGHLIGHTING
#endif // SVG_SUPPORT_TEXTSELECTION && !HAS_NO_SEARCHTEXT && SEARCH_MATCHES_ALL

#endif // SVG_SUPPORT
#endif // MODULES_SVG_SVG_MODULE_H
