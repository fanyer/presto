/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
**
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/spatial_navigation/sn_debug.h"

#ifdef _SPAT_NAV_SUPPORT_

#ifdef SN_DEBUG_TRACE
SnTrace::SnTrace()
{
	out_file.Construct(UNI_L("spatnav.html"), OPFILE_SERIALIZED_FOLDER);
	out_file.Open(OPFILE_WRITE);

	buf.Append("<!DOCTYPE HTML>\n<style>div {opacity:0.5;position:absolute;}</style>\n");
}

SnTrace::~SnTrace()
{
	out_file.Write(buf.CStr(), buf.Length());
	out_file.Close();
}

void
SnTrace::TraceRect(RECT rect, int32 rank, OpSnAlgorithm::EvaluationResult result)
{
	OpString8 line, title, color;

	const int32 rank_threshold = 20000;
	switch (result)
	{
	case OpSnAlgorithm::NOT_IN_SECTOR:
		color.Append("#444");
		title.Append("Not in sector");
		break;
	case OpSnAlgorithm::NOT_BEST:
		if (rank < rank_threshold)
			color.Append("#FF0");
		else
			color.Append("#CC0");
		title.Append("Not best");
		break;
	case OpSnAlgorithm::NOT_BEST_BUT_CLOSE:
		if (rank < rank_threshold)
			color.Append("#AA0");
		else
			color.Append("#880");			
		title.Append("Not best but close");
		break;
	case OpSnAlgorithm::BEST_ELEMENT:
		color.Append("#0F0");
		title.Append("Best element");
		break;
	case OpSnAlgorithm::BEST_ELEMENT_BUT_CLOSE:
		if (rank < rank_threshold)
			color.Append("#0A0");
		else
			color.Append("#080");
		title.Append("Best element but close");
		break;
	case OpSnAlgorithm::ALTERNATIVE_ELEMENT:
		color.Append("#00F");
		title.Append("Alternative element");
		break;
	case OpSnAlgorithm::START_ELEMENT:
		color.Append("#F00");
		title.Append("Start element");
		break;
	}

	line.AppendFormat("<div onclick='' title='%s (rank %d)' style='left:%dpx;top:%dpx;width:%dpx;height:%dpx;background-color:%s;'></div>\n",
					  title.CStr(), rank, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, color.CStr());

	buf.Append(line);
}
#endif // SN_DEBUG_TRACE

#endif // _SPAT_NAV_SUPPORT_
