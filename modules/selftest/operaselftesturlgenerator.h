/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Anders Oredsson
*/

#if !defined MODULES_SELFTEST_OPERASELFTESTURLGENERATOR_H && defined SELFTEST
#define MODULES_SELFTEST_OPERASELFTESTURLGENERATOR_H

#include "modules/url/url_lop_api.h"

/** 
 * Simple URL hook used to register opera:selftest to the URL api.
 * This hook simply instanciates a OperaSelftest generated page, and
 * returns this to the browser user.
 *
 * The hook is initialized, remembered, registered and de-registered 
 * by the selftest_module class.
 */
class OperaSelftestURLGenerator 
	: public OperaURL_Generator
{
public:
	virtual OperaURL_Generator::GeneratorMode GetMode() const { return OperaURL_Generator::KQuickGenerate; }
	virtual OP_STATUS QuickGenerate(URL &url, OpWindowCommander*);
};

#endif
