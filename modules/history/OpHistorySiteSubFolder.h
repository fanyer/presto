/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#ifndef CORE_HISTORY_MODEL_SITE_SUB_FOLDER_H
#define CORE_HISTORY_MODEL_SITE_SUB_FOLDER_H

#ifdef HISTORY_SUPPORT

#include "modules/history/OpHistoryFolder.h"

//___________________________________________________________________________
//                         CORE HISTORY MODEL SITE SUB FOLDER
//___________________________________________________________________________

class HistorySiteSubFolder
	: public HistoryFolder
{
	friend class HistorySiteFolder;

public:

	/*
	  @return
	**/
    virtual HistoryFolderType GetFolderType() const { return SITE_SUB_FOLDER; }

	/* Use the title and key of the site folder they belong to
	 * @return
	 **/
	virtual OP_STATUS GetTitle(OpString & title) const;
	virtual const OpStringC & GetTitle() const;
	virtual const LexSplayKey * GetKey() const;

	/*
	  @return
	**/
    virtual BOOL IsDeletable() const { return FALSE; }

	/*
	  @return
	**/
    TimePeriod GetPeriod() const { return m_period; }

	/*
	  @param parent
	**/
	void SetParent(HistorySiteFolder* parent) { SetSiteFolder(parent); }

#ifdef HISTORY_DEBUG
	virtual int GetSize() { return sizeof(HistorySiteSubFolder);}
#endif //HISTORY_DEBUG

private:

	/*
	  Private constructor - use public static Create method for memory safety
	**/
    HistorySiteSubFolder() {}

	/*
	  Private destructor - access allowed only by subclasses and friend class HistoryModel
	**/
	virtual ~HistorySiteSubFolder(){}

	void SetPeriod(TimePeriod period) { m_period = period; }

    TimePeriod m_period;
};

typedef HistorySiteSubFolder CoreHistoryModelSiteSubFolder;  // Temorary typedef to old class name, so old code continues to compile

#endif // HISTORY_SUPPORT
#endif // CORE_HISTORY_MODEL_SITE_SUB_FOLDER_H
