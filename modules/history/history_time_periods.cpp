/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#include "core/pch.h"

#ifdef HISTORY_SUPPORT

#include "modules/history/src/HistoryModel.h"
#include "modules/history/history_structs.h"

//___________________________________________________________________________
// 
//___________________________________________________________________________

OP_STATUS HistoryModel::MakeHistoryTimePeriods(HistoryTimePeriod *& history_time_periods, int & size)
{
	HistoryTimePeriod time_periods[] =
		{
			HistoryTimePeriod( Str::S_TODAY_FOLDER_NAME,		TODAY,		0	    ), //Variable relative length to last midnight 
			HistoryTimePeriod( Str::S_YESTERDAY_FOLDER_NAME,	YESTERDAY,	86400	), //Relative to the beginning of today
			HistoryTimePeriod( Str::S_WEEK_FOLDER_NAME,			WEEK,		604800	), //Relative to the beginning of yesterday
			HistoryTimePeriod( Str::S_MONTH_FOLDER_NAME,		MONTH,		2419200	), //Relative to the beginning of last week
			HistoryTimePeriod( Str::S_OLDER_FOLDER_NAME,		OLDER,		0	    )  //Variable relative length to the beginning of last month 
		};

	history_time_periods = OP_NEWA(HistoryTimePeriod, ARRAY_SIZE(time_periods));

	if(!history_time_periods)
		return OpStatus::ERR_NO_MEMORY;

	for(UINT i = 0; i < ARRAY_SIZE(time_periods); i++)
	{
		history_time_periods[i] = time_periods[i]; 
	}

	size = ARRAY_SIZE(time_periods);

	return OpStatus::OK;
}

#endif // HISTORY_SUPPORT
