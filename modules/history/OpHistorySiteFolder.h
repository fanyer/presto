/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#ifndef CORE_HISTORY_MODEL_SITE_FOLDER_H
#define CORE_HISTORY_MODEL_SITE_FOLDER_H

#ifdef HISTORY_SUPPORT

#include "modules/history/OpHistoryFolder.h"
#include "modules/history/OpHistorySiteSubFolder.h"

//___________________________________________________________________________
//                         CORE HISTORY MODEL SITE FOLDER
//___________________________________________________________________________

class HistorySiteFolder
	: public HistoryFolder
{
    friend class HistoryModel;
    friend class HistoryItem;

public:

	/*
	 * Public static Create method - will return 0 if no memory condition
	 *
	 * @param title
	 * @param key
	 *
	 * @return
	 **/
    static HistorySiteFolder* Create(const OpStringC& title,
									 HistoryKey * key);

	/*
	 * @return
	 **/
    virtual BOOL IsEmpty() const { return m_num_children == NUM_TIME_PERIODS; }

	/*
	 * @return
	 **/
    virtual BOOL IsDeletable() const { return IsEmpty(); }

	/*
	 * @return
	 **/
    virtual HistoryFolderType GetFolderType() const { return SITE_FOLDER; }

	/*
	 * @param period
	 *
	 * @return
	 **/
    HistorySiteSubFolder & GetSubFolder(TimePeriod period) { return m_subfolders[period]; }

#ifdef HISTORY_DEBUG
	static INT m_number_of_site_folders;
	static OpVector<HistorySiteFolder> sites;
	static OpVector<HistorySiteFolder> & GetSites() { return sites; }
	virtual int GetSize() { return sizeof(HistorySiteFolder) + GetTitleLength();}
#endif //HISTORY_DEBUG

	virtual ~HistorySiteFolder();

private:

	/*
	  Private constructor - use public static Create method for memory safety
	**/
    HistorySiteFolder(HistoryKey * key);

    void RemoveChild(){ m_num_children--; }
    void AddChild()   { m_num_children++; }

#ifdef HISTORY_DEBUG
	int GetSizeOfSubFolders() {
		int tot_size = 0;
		for(int i = 0; i < NUM_TIME_PERIODS; i++)
			tot_size += m_subfolders[i].GetSize();

		return tot_size;
	}
#endif //HISTORY_DEBUG

	HistorySiteSubFolder m_subfolders[NUM_TIME_PERIODS];

    INT32 m_num_children;
};

typedef HistorySiteFolder CoreHistoryModelSiteFolder;  // Temorary typedef to old class name, so old code continues to compile

#endif // HISTORY_SUPPORT
#endif // CORE_HISTORY_MODEL_SITE_FOLDER_H
