/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SELFTEST
#include "modules/unicode/testsuite/unicode_selftest_utilities.h"

#ifdef USE_UNICODE_LINEBREAK
// Map to textual name to easier catch errors
const char *lbname(LinebreakClass cl)
{
	switch (cl)
	{
	case LB_XX: return "XX";
	case LB_BK: return "BK";
	case LB_CR: return "CR";
	case LB_LF: return "LF";
	case LB_CM: return "CM";
	case LB_NL: return "NL";
	case LB_SG: return "SG";
	case LB_WJ: return "WJ";
	case LB_ZW: return "ZW";
	case LB_GL: return "GL";
	case LB_SP: return "SP";
	case LB_B2: return "B2";
	case LB_BA: return "BA";
	case LB_BB: return "BB";
	case LB_HY: return "HY";
	case LB_CB: return "CB";
	case LB_CL: return "CL";
	case LB_EX: return "EX";
	case LB_IN: return "IN";
	case LB_NS: return "NS";
	case LB_OP: return "OP";
	case LB_QU: return "QU";
	case LB_IS: return "IS";
	case LB_NU: return "NU";
	case LB_PO: return "PO";
	case LB_PR: return "PR";
	case LB_SY: return "SY";
	case LB_AI: return "AI";
	case LB_AL: return "AL";
	case LB_CJ: return "CJ";
	case LB_H2: return "H2";
	case LB_H3: return "H3";
	case LB_HL: return "HL";
	case LB_ID: return "ID";
	case LB_JL: return "JL";
	case LB_JV: return "JV";
	case LB_JT: return "JT";
	case LB_SA: return "SA";
	}
	return "**";
}
#endif

#endif
