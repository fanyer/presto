/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#ifndef CORE_HISTORY_MODEL_TIME_FOLDER_H
#define CORE_HISTORY_MODEL_TIME_FOLDER_H

#ifdef HISTORY_SUPPORT

#include "modules/history/OpHistoryFolder.h"

//___________________________________________________________________________
//                         CORE HISTORY MODEL TIME FOLDER
//___________________________________________________________________________

class HistoryTimeFolder
	: public HistoryFolder
{
	friend class HistoryModel;

public:

	/*
	 * Public static Create method - will return 0 if no memory condition
	 *
	 * @param title
	 * @param period
	 * @param key
	 *
	 * @return
	 **/
    static HistoryTimeFolder* Create(const OpStringC& title,
									 TimePeriod period,
									 HistoryKey * key);

	/*
	 * @return
	 **/
    virtual BOOL IsDeletable() const { return FALSE; }

	/*
	 * @return
	 **/
    virtual HistoryFolderType GetFolderType() const { return TIME_FOLDER; }

	/*
	 * @return
	 **/
    TimePeriod GetPeriod() const { return m_period; }

#ifdef HISTORY_DEBUG
	virtual int GetSize() { return sizeof(HistoryTimeFolder) + GetTitleLength();}
#endif //HISTORY_DEBUG

	virtual ~HistoryTimeFolder(){}

private:

	/*
	 * @param period
	 * @param key
	 *
	 * Private constructor - use public static Create method for memory safety
	 **/
    HistoryTimeFolder(TimePeriod period,
					  HistoryKey * key):
		HistoryFolder(key)
		{ m_period = period; }

	TimePeriod m_period;
};

typedef HistoryTimeFolder CoreHistoryModelTimeFolder;  // Temorary typedef to old class name, so old code continues to compile

#endif // HISTORY_SUPPORT
#endif // CORE_HISTORY_MODEL_TIME_FOLDER_H
