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

#include "modules/history/OpHistorySiteFolder.h"

#ifdef HISTORY_DEBUG
INT HistorySiteFolder::m_number_of_site_folders = 0;
OpVector<HistorySiteFolder> HistorySiteFolder::sites;
#endif //HISTORY_DEBUG

/*___________________________________________________________________________*/
/*                         CORE HISTORY MODEL SITE FOLDER                    */
/*___________________________________________________________________________*/

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistorySiteFolder* HistorySiteFolder::Create(const OpStringC& title,
											 HistoryKey * key)
{
	OP_ASSERT(key);

	OpAutoPtr<HistorySiteFolder> site_folder(OP_NEW(HistorySiteFolder, (key)));

	if(!site_folder.get())
		return 0;

	RETURN_VALUE_IF_ERROR(site_folder->SetTitle(title), 0);

	return site_folder.release();
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistorySiteFolder::HistorySiteFolder(HistoryKey * key)
	: HistoryFolder(key),
	  m_num_children(0)
{
	for(INT32 i = 0; i < NUM_TIME_PERIODS; i++)
	{
		m_subfolders[i].SetParent(this);
		m_subfolders[i].SetPeriod((TimePeriod) i);
	}

#ifdef HISTORY_DEBUG
	sites.Add(this);
	m_number_of_site_folders++;
#endif //HISTORY_DEBUG
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistorySiteFolder::~HistorySiteFolder()
{
	OP_ASSERT(m_num_children == NUM_TIME_PERIODS);

#ifdef HISTORY_DEBUG
	sites.RemoveByItem(this);
	m_number_of_site_folders--;
#endif //HISTORY_DEBUG
}

#endif // HISTORY_SUPPORT
