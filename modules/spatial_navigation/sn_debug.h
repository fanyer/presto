/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
**
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _SN_DEBUG_H_
#define _SN_DEBUG_H_

#ifdef _SPAT_NAV_SUPPORT_

#ifdef _DEBUG

// Define to enable tracing.
//#define SN_DEBUG_TRACE

#include "modules/util/opfile/opfile.h"
#include "modules/spatial_navigation/sn_algorithm.h"

#ifdef SN_DEBUG_TRACE

/**
 * Simple class to output spatnavigatable elements to a html file
 * for further debugging.
 */
class SnTrace
{
public:
	SnTrace();
	~SnTrace();

	/**
	 * Inserts a div representing the rect.
	 *
	 * @param	rect	The rect to trace.
	 * @param	rank	Rank of the rect.
	 * @param	result	Result of EvaluateLink.
	 */
	void TraceRect(RECT rect, int32 rank, OpSnAlgorithm::EvaluationResult result);

private:
	OpFile out_file;
	OpString8 buf;
};

#endif // SN_DEBUG_TRACE

#endif // _DEBUG

#endif // _SPAT_NAV_SUPPORT_

#endif // _SN_DEBUG_H_
