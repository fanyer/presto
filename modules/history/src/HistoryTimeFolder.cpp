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

#include "modules/history/OpHistoryTimeFolder.h"

/*___________________________________________________________________________*/
/*                         CORE HISTORY MODEL TIME FOLDER                    */
/*___________________________________________________________________________*/


/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/
HistoryTimeFolder* HistoryTimeFolder::Create(const OpStringC& title,
											 TimePeriod period,
											 HistoryKey * key)
{
	OP_ASSERT(key);

	OpAutoPtr<HistoryTimeFolder> time_folder(OP_NEW(HistoryTimeFolder, (period, key)));

	if(!time_folder.get())
		return 0;

	RETURN_VALUE_IF_ERROR(time_folder->SetTitle(title), 0);

	return time_folder.release();
}

#endif // HISTORY_SUPPORT
