/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILEXCEPTIONS_H
#define DOM_DOMJILEXCEPTIONS_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domobj.h"

class DOM_Runtime;

/** Constructor for JIL exception objects.
 */
class DOM_JILException_Constructor : public DOM_BuiltInConstructor
{
public:
	DOM_JILException_Constructor();
	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILEXCEPTIONS_H
