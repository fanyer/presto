/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domglobaldata.h"

DOM_GlobalData::DOM_GlobalData()
	: globalCallbacks(NULL)
	, operaCallbacks(NULL)
#ifdef EXTENSION_SUPPORT
	, have_always_enabled_extension_js(FALSE)
#endif // EXTENSION_SUPPORT
{
#ifdef DOM_NO_COMPLEX_GLOBALS
	extern void DOM_Runtime_prototypes_Init(DOM_GlobalData *global_data);
	DOM_Runtime_prototypes_Init(this);

	/* One for each class that has functions and thus a prototype.  The three
	   variants DOM_FUNCTIONS{1,2,3} are for classes that only have functions
	   without data, only functions with data or both, respectively. */

# define DOM_FUNCTIONS(cls) extern void cls##_InitFunctions(DOM_GlobalData *); cls##_InitFunctions(this)
# define DOM_FUNCTIONS_WITH_DATA(cls) extern void cls##_InitFunctionsWithData(DOM_GlobalData *); cls##_InitFunctionsWithData(this)
# define DOM_FUNCTIONS1(cls) DOM_FUNCTIONS(cls);
# define DOM_FUNCTIONS2(cls) DOM_FUNCTIONS_WITH_DATA(cls);
# define DOM_FUNCTIONS3(cls) DOM_FUNCTIONS(cls); DOM_FUNCTIONS_WITH_DATA(cls);

#include "modules/dom/src/domglobaldata.inc"

	extern void DOM_atomNames_Init(DOM_GlobalData *);
	DOM_atomNames_Init(this);

	extern void DOM_featureList_Init(DOM_GlobalData *);
	DOM_featureList_Init(this);

	extern void DOM_configurationParameters_Init(DOM_GlobalData *);
	DOM_configurationParameters_Init(this);

	extern void DOM_htmlProperties_Init(DOM_GlobalData *);
	DOM_htmlProperties_Init(this);

	extern void DOM_htmlClassNames_Init(DOM_GlobalData *);
	DOM_htmlClassNames_Init(this);

	extern void DOM_prototypeClassNames_Init(DOM_GlobalData *);
	DOM_prototypeClassNames_Init(this);

	extern void DOM_constructorNames_Init(DOM_GlobalData *);
	DOM_constructorNames_Init(this);

	extern void DOM_htmlPrototypeClassNames_Init(DOM_GlobalData *);
	DOM_htmlPrototypeClassNames_Init(this);

#ifdef SVG_DOM
	extern void DOM_svgObjectPrototypeClassNames_Init(DOM_GlobalData *);
	DOM_svgObjectPrototypeClassNames_Init(this);

	extern void DOM_svgElementPrototypeClassNames_Init(DOM_GlobalData *);
	DOM_svgElementPrototypeClassNames_Init(this);
#endif // SVG_DOM

	extern void DOM_eventData_Init(DOM_GlobalData *);
	DOM_eventData_Init(this);

#ifdef DOM3_EVENTS
	extern void DOM_eventNamespaceData_Init(DOM_GlobalData *);
	DOM_eventNamespaceData_Init(this);
#endif // DOM3_EVENTS

#ifdef SVG_SUPPORT
	extern void DOM_svg_if_inheritance_table_Init(DOM_GlobalData *);
	DOM_svg_if_inheritance_table_Init(this);

	extern void DOM_svg_element_spec_Init(DOM_GlobalData *);
	DOM_svg_element_spec_Init(this);

	extern void DOM_svg_element_entries_Init(DOM_GlobalData *);
	DOM_svg_element_entries_Init(this);

	extern void DOM_svg_object_entries_Init(DOM_GlobalData *);
	DOM_svg_object_entries_Init(this);
#endif // SVG_SUPPORT

#undef DOM_FUNCTIONS
#undef DOM_FUNCTIONS_WITH_DATA
#undef DOM_FUNCTIONS1
#undef DOM_FUNCTIONS2
#undef DOM_FUNCTIONS3
#endif // DOM_NO_COMPLEX_GLOBALS
}

