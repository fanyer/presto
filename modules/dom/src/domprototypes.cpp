/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/domexception.h"
#include "modules/dom/src/domcss/cssrule.h"
#include "modules/dom/src/domload/lsexception.h"
#include "modules/dom/src/domsvg/domsvgexception.h"
#include "modules/dom/src/domtraversal/nodefilter.h"
#include "modules/dom/src/domrange/range.h"
#include "modules/dom/src/domxpath/xpathexception.h"
#include "modules/dom/src/media/dommediaerror.h"

#ifdef DOM_LIBRARY_FUNCTIONS
# ifdef DOM_NO_COMPLEX_GLOBALS
#  define DOM_PROTOTYPES_START() \
	void DOM_Runtime_prototypes_Init(DOM_GlobalData *global_data) \
	{ \
		DOM_PrototypeDesc *prototypes = global_data->DOM_Runtime_prototypes;
#  define DOM_PROTOTYPES_ITEM0(name, prototype_) \
		prototypes->functions = 0; \
		prototypes->functions_with_data = 0; \
		prototypes->library_functions = 0; \
		prototypes->prototype = prototype_; \
		++prototypes;
#  define DOM_PROTOTYPES_ITEM1(name, functions_, prototype_) \
		prototypes->functions = global_data->functions_; \
		prototypes->functions_with_data = 0; \
		prototypes->library_functions = 0; \
		prototypes->prototype = prototype_; \
		++prototypes;
#  define DOM_PROTOTYPES_ITEM2(name, functions_with_data_, prototype_) \
		prototypes->functions = 0; \
		prototypes->functions_with_data = global_data->functions_with_data_; \
		prototypes->library_functions = 0; \
		prototypes->prototype = prototype_; \
		++prototypes;
#  define DOM_PROTOTYPES_ITEM3(name, functions_, functions_with_data_, prototype_) \
		prototypes->functions = global_data->functions_; \
		prototypes->functions_with_data = global_data->functions_with_data_; \
		prototypes->library_functions = 0; \
		prototypes->prototype = prototype_; \
		++prototypes;
#  define DOM_PROTOTYPES_ITEM4(name, functions_, functions_with_data_, library_functions_, prototype_) \
		prototypes->functions = global_data->functions_; \
		prototypes->functions_with_data = global_data->functions_with_data_; \
		prototypes->library_functions = library_functions_; \
		prototypes->prototype = prototype_; \
		++prototypes;
#  define DOM_PROTOTYPES_END() \
	}
# else // DOM_NO_COMPLEX_GLOBALS
#  define DOM_PROTOTYPES_START() const DOM_PrototypeDesc DOM_Runtime_prototypes[DOM_Runtime::PROTOTYPES_COUNT + 1] = {
#  define DOM_PROTOTYPES_ITEM0(name, prototype_) { NULL, NULL, NULL, prototype_ },
#  define DOM_PROTOTYPES_ITEM1(name, functions_, prototype_) { functions_, NULL, NULL, prototype_ },
#  define DOM_PROTOTYPES_ITEM2(name, functions_with_data_, prototype_) { NULL, functions_with_data_, NULL, prototype_ },
#  define DOM_PROTOTYPES_ITEM3(name, functions_, functions_with_data_, prototype_) { functions_, functions_with_data_, NULL, prototype_ },
#  define DOM_PROTOTYPES_ITEM4(name, functions_, functions_with_data_, library_functions_, prototype_) { functions_, functions_with_data_, library_functions_, prototype_ },
#  define DOM_PROTOTYPES_END() { 0, 0, 0 } };
# endif // DOM_NO_COMPLEX_GLOBALS
#else // DOM_LIBRARY_FUNCTIONS
# ifdef DOM_NO_COMPLEX_GLOBALS
#  define DOM_PROTOTYPES_START() \
	void DOM_Runtime_prototypes_Init(DOM_GlobalData *global_data) \
	{ \
		DOM_PrototypeDesc *prototypes = global_data->DOM_Runtime_prototypes;
#  define DOM_PROTOTYPES_ITEM0(name, prototype_) \
		prototypes->functions = 0; \
		prototypes->functions_with_data = 0; \
		prototypes->prototype = prototype_; \
		++prototypes;
#  define DOM_PROTOTYPES_ITEM1(name, functions_, prototype_) \
		prototypes->functions = global_data->functions_; \
		prototypes->functions_with_data = 0; \
		prototypes->prototype = prototype_; \
		++prototypes;
#  define DOM_PROTOTYPES_ITEM2(name, functions_with_data_, prototype_) \
		prototypes->functions = 0; \
		prototypes->functions_with_data = global_data->functions_with_data_; \
		prototypes->prototype = prototype_; \
		++prototypes;
#  define DOM_PROTOTYPES_ITEM3(name, functions_, functions_with_data_, prototype_) \
		prototypes->functions = global_data->functions_; \
		prototypes->functions_with_data = global_data->functions_with_data_; \
		prototypes->prototype = prototype_; \
		++prototypes;
#  define DOM_PROTOTYPES_ITEM4(name, functions_, functions_with_data_, library_functions_, prototype_) \
		prototypes->functions = global_data->functions_; \
		prototypes->functions_with_data = global_data->functions_with_data_; \
		prototypes->prototype = prototype_; \
		++prototypes;
#  define DOM_PROTOTYPES_END() \
	}
# else // DOM_NO_COMPLEX_GLOBALS
#  define DOM_PROTOTYPES_START() const DOM_PrototypeDesc DOM_Runtime_prototypes[DOM_Runtime::PROTOTYPES_COUNT + 1] = {
#  define DOM_PROTOTYPES_ITEM0(name, prototype_) { NULL, NULL, prototype_ },
#  define DOM_PROTOTYPES_ITEM1(name, functions_, prototype_) { functions_, NULL, prototype_ },
#  define DOM_PROTOTYPES_ITEM2(name, functions_with_data_, prototype_) { NULL, functions_with_data_, prototype_ },
#  define DOM_PROTOTYPES_ITEM3(name, functions_, functions_with_data_, prototype_) { functions_, functions_with_data_, prototype_ },
#  define DOM_PROTOTYPES_ITEM4(name, functions_, functions_with_data_, library_functions_, prototype_) { functions_, functions_with_data_, prototype_ },
#  define DOM_PROTOTYPES_END() { 0, 0, 0 } };
# endif // DOM_NO_COMPLEX_GLOBALS
#endif // DOM_LIBRARY_FUNCTIONS

#include "modules/dom/src/domprototypes.cpp.inc"
