/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef LCMS_3P_SUPPORT

#include "modules/liblcms/liblcms_constant.h"


void LiblcmsModule::InitL(const OperaInitInfo& info)
{
#ifndef HAS_COMPLEX_GLOBALS
	LIBLCMS_CONST_INIT(MemoryMgr);
	LIBLCMS_CONST_INIT(LogErrorHandler);
	LIBLCMS_CONST_INIT(Interpolators);

	LIBLCMS_CONST_INIT(SupportedMPEtypes);
	LIBLCMS_CONST_INIT(SupportedTagTypes);
	LIBLCMS_CONST_INIT(DefaultCurves);
	LIBLCMS_CONST_INIT(DefaultIntents);
	LIBLCMS_CONST_INIT(DefaultOptimization);
	LIBLCMS_CONST_INIT(SupportedTags);
	LIBLCMS_CONST_INIT(InputFormatters16);
	LIBLCMS_CONST_INIT(InputFormattersFloat);
	LIBLCMS_CONST_INIT(OutputFormatters16);
	LIBLCMS_CONST_INIT(OutputFormattersFloat);
#endif // HAS_COMPLEX_GLOBALS
}

#endif // LCMS_3P_SUPPORT
