/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Anders Oredsson
*/

#include "core/pch.h"

#ifdef SELFTEST
#include "modules/selftest/operaselftesturlgenerator.h"
#include "modules/selftest/operaselftest.h"

/** opera:selftest URL hook callback method */
OP_STATUS OperaSelftestURLGenerator::QuickGenerate(URL &url, OpWindowCommander*)
{
	OperaSelftest document(url);
	return document.GenerateData();
}

#endif
