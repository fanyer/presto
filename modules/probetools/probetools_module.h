/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_PROBETOOLS_PROBETOOLS_MODULE_H
#define MODULES_PROBETOOLS_PROBETOOLS_MODULE_H

#ifdef SUPPORT_PROBETOOLS

#include "modules/hardcore/opera/module.h"

class  OpProbeHelper;

class ProbetoolsModule : public OperaModule
{
public:
	ProbetoolsModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	OpProbeHelper* GetProbetoolsHelper(){return m_probehelper;}

private:
	OpProbeHelper* m_probehelper;
	OpString m_probereport_filename;
	OpString m_probelog_filename;
};

#define PROBETOOLS_MODULE_REQUIRED

//Probes access probetools-graph trough this link.
#define g_probetools_helper (g_opera->probetools_module.GetProbetoolsHelper())

#endif // SUPPORT_PROBETOOLS
#endif // !MODULES_PROBETOOLS_PROBETOOLS_MODULE_H
