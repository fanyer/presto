/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_LAYOUT_LAYOUT_MODULE_H
#define MODULES_LAYOUT_LAYOUT_MODULE_H

#include "modules/hardcore/opera/module.h"
#include "modules/layout/layoutpool.h"
#include "modules/style/css_decl.h"

#define ROMAN_SIZE	14
#define DECIMAL_LEADING_SIZE	9
#define WORD_INFO_MAX_SIZE 4000

#define LAZY_LOADPROPS
#define IMG_SUPPORTS_SVG_DIRECTLY
#define CSS3_BACKGROUND

class WordInfo;
class HTML_Element;
class CSS_gen_array_decl;
class CssPropertyItem;
class SharedCssManager;
class ContentGenerator;

class LayoutModule : public OperaModule
{
private:
	void InitMemoryPoolsL();
	void DestroyMemoryPools();

	ContentGenerator*	content_generator;

public:
	LayoutModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	ContentGenerator*	GetContentGenerator() const { return content_generator; }

	HTML_Element*		first_line_elm;
	CSS_gen_array_decl*	m_img_alt_content;
	CSS_gen_array_decl*	m_space_content;
	long*				m_support_attr_array;
	BOOL				m_blink_on;
	LayoutPool			layout_properties_pool;
	LayoutPool			htm_layout_properties_pool;
	LayoutPool			container_reflow_state_pool;
	LayoutPool			verticalbox_reflow_state_pool;
	LayoutPool			inlinebox_reflow_state_pool;
	LayoutPool			absolutebox_reflow_state_pool;
	LayoutPool			tablecontent_reflow_state_pool;
	LayoutPool			tablerowgroup_reflow_state_pool;
	LayoutPool			tablerow_reflow_state_pool;
	LayoutPool			tablecell_reflow_state_pool;
	LayoutPool			spacemanager_reflow_state_pool;
	LayoutPool			float_reflow_cache_pool;
	LayoutPool			reflowelem_pool;
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	LayoutPool			misspelling_paint_info_pool;
#endif // INTERNAL_SPELLCHECK_SUPPORT
	CSS_generic_value	quotes[1];
	CSS_stack_gen_array_decl
						array_decl;
#ifndef HAS_COMPLEX_GLOBALS
	const uni_char*		RomanStr[ROMAN_SIZE];
	const uni_char*		DecimalLeadingZeroStr[DECIMAL_LEADING_SIZE];
#endif // HAS_COMPLEX_GLOBALS
	SharedCssManager*	m_shared_css_manager;
	CssPropertyItem*	props_array;
	WordInfo*			tmp_word_info_array;

#ifdef PLATFORM_FONTSWITCHING
	uni_char			ellipsis_str[2]; /* ARRAY OK 2010-09-30 mg */
#else
	const uni_char*		ellipsis_str;
#endif

	/** Length of ellipsis text. */
	short				ellipsis_str_len;
};

#define g_anonymous_first_line_elm g_opera->layout_module.first_line_elm
#define g_support_attr_array g_opera->layout_module.m_support_attr_array
#define g_layout_properties_pool (&(g_opera->layout_module.layout_properties_pool))
#define g_container_reflow_state_pool (&(g_opera->layout_module.container_reflow_state_pool))
#define g_verticalbox_reflow_state_pool (&(g_opera->layout_module.verticalbox_reflow_state_pool))
#define g_inlinebox_reflow_state_pool (&(g_opera->layout_module.inlinebox_reflow_state_pool))
#define g_absolutebox_reflow_state_pool (&(g_opera->layout_module.absolutebox_reflow_state_pool))
#define g_tablecontent_reflow_state_pool (&(g_opera->layout_module.tablecontent_reflow_state_pool))
#define g_tablerowgroup_reflow_state_pool (&(g_opera->layout_module.tablerowgroup_reflow_state_pool))
#define g_tablerow_reflow_state_pool (&(g_opera->layout_module.tablerow_reflow_state_pool))
#define g_tablecell_reflow_state_pool (&(g_opera->layout_module.tablecell_reflow_state_pool))
#define g_spacemanager_reflow_state_pool (&(g_opera->layout_module.spacemanager_reflow_state_pool))
#define g_float_reflow_cache_pool (&(g_opera->layout_module.float_reflow_cache_pool))
#define g_reflowelem_pool (&(g_opera->layout_module.reflowelem_pool))

#define g_quotes g_opera->layout_module.quotes
#define g_array_decl g_opera->layout_module.array_decl

#ifdef INTERNAL_SPELLCHECK_SUPPORT
# define g_misspelling_paint_info_pool (&(g_opera->layout_module.misspelling_paint_info_pool))
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifndef HAS_COMPLEX_GLOBALS
#define RomanStr g_opera->layout_module.RomanStr
#define DecimalLeadingZeroStr g_opera->layout_module.DecimalLeadingZeroStr
#endif // HAS_COMPLEX_GLOBALS

#define g_sharedCssManager (g_opera->layout_module.m_shared_css_manager)
#define g_props_array g_opera->layout_module.props_array

#define g_ellipsis_str g_opera->layout_module.ellipsis_str
#define g_ellipsis_str_len g_opera->layout_module.ellipsis_str_len

#define LAYOUT_MODULE_REQUIRED

#endif // !MODULES_LAYOUT_LAYOUT_MODULE_H
