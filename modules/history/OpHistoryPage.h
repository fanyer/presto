/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#ifndef CORE_HISTORY_MODEL_PAGE_H
#define CORE_HISTORY_MODEL_PAGE_H

#ifdef HISTORY_SUPPORT

#include "modules/history/OpHistoryItem.h"
#include "modules/history/src/HistoryKey.h"

//___________________________________________________________________________
//                         Core HISTORY MODEL PAGE
//___________________________________________________________________________

class HistoryPage :
	public HistoryItem
{
    friend class HistoryModel;

 public:

	/**
	 * Public static Create method - will return 0 if page could not be created.
	 *
	 * @param acc				- time of last visit
	 * @param average_interval	- the psudo-average time between visits
	 * @param title				- the title of the page
	 * @param key				- the key that will be used for the item
	 *
	 * @return the item created or 0 if out of memory
	 **/
    static HistoryPage* Create(time_t acc,
							   time_t average_interval,
							   const OpStringC& title,
							   HistoryKey* key);

	/**
	 * @param address
	 *
	 * @return the address of this item
	 **/
	virtual OP_STATUS GetAddress(OpString& address) const;
	
	/**
	 * @return the average interval between visits (psudo-average)
	 **/
    time_t GetAverageInterval() const { return m_average_interval; }

	/**
	 * @return the time of the last visit
	 **/
    time_t GetAccessed() const { return m_accessed; }

	/**
	 * @return
	 **/
    time_t GetPopularity();

	/**
	 * @return the items type
	 **/
    virtual Type GetType() { return HISTORY_ELEMENT_TYPE; }

	/**
	 * @return the image string
	 **/
	virtual const char* GetImage() const;

	/**
	 * @return TRUE if item is in history
	 **/
	BOOL IsInHistory() const { return (m_flags & IN_HISTORY) != 0;}

	/**
	 * @return TRUE if item is bookmarked
	 **/
	BOOL IsBookmarked() const { return (m_flags & BOOKMARKED) != 0;}

	/**
	 * @return TRUE if item is a nick
	 **/
	BOOL IsNick() const { return (m_flags & IS_NICK) != 0;}

	/**
	 * @return TRUE if item is an internal page
	 **/
	BOOL IsOperaPage() const { return (m_flags & IS_OPERA_PAGE) != 0;}

	/**
	 * @return TRUE if item has been typed
	 **/
	BOOL IsTyped() const { return (m_flags & IS_TYPED) != 0;}

	/**
	 * @return TRUE if item can be considered "visited" (for example for marking a link as visited)
	 **/
	BOOL IsVisited() const;

	/**
	 * Set that item is a bookmark
	 **/
	void SetBookmarked() { m_flags |= BOOKMARKED;}

	/**
	 * Set that item is a nick
	 **/
	void SetIsNick() { m_flags |= IS_NICK;}

	/**
	 * Set that item is an internal page
	 **/
	void SetIsOperaPage() { m_flags |= IS_OPERA_PAGE;}

	/**
	 * Set that item is in history
	 **/
	void SetInHistory() { m_flags |= IN_HISTORY;}

	/**
	 * Set that item was typed
	 **/
	void SetIsTyped() { m_flags |= IS_TYPED;}

	/**
	 * Set that item was selected manually
	 **/
	void SetIsSelected() { m_flags |= IS_SELECTED;}

	/**
	 * Clear that item is a bookmark
	 **/
	void ClearBookmarked() { m_flags &= ~BOOKMARKED;}

	/**
	 * Clear that item is a nick
	 **/
	void ClearNick() { m_flags &= ~IS_NICK;}

	/**
	 * Clear that item is an internal page
	 **/
	void ClearIsOperaPage() { m_flags &= ~IS_OPERA_PAGE;}

	/**
	 * Clear that item is in history
	 **/
	void ClearInHistory() { m_flags &= ~IN_HISTORY;}

	/**
	 * Clear that item was typed
	 **/
	void ClearIsTyped() { m_flags &= ~IS_TYPED;}

	/**
	 * Clear that item was selected manually
	 **/
	void ClearIsSelected() { m_flags &= ~IS_SELECTED;}

	/**
	 * @return the key of the item
	 **/
	virtual const LexSplayKey * GetKey() const { return m_key; }

	/**
	 * @return the address stripped of the prefix
	 **/
	const uni_char* GetStrippedAddress() const;

	/**
	 * @return the prefix string
	 **/
	const uni_char* GetPrefix() const;

	/**
	 * @return the prefix index
	 **/
	INT32 GetPrefixNum() const;

#ifdef HISTORY_DEBUG
	virtual void OnTitleSet();
	virtual int GetSize() { return sizeof(HistoryPage) + GetTitleLength();}
	static INT m_number_of_pages;
	static INT s_number_of_addresses;
	static INT s_total_address_length;
	static INT s_number_of_titles;
	static INT s_total_title_length;
#endif //HISTORY_DEBUG

    virtual ~HistoryPage();

private:

	/*
	 * @param acc
	 * @param average_interval
	 * @param key
	 *
	 * Private constructor - use public static Create method for memory safety
	 **/
    HistoryPage(time_t acc,
				time_t average_interval,
				HistoryKey* key);

    void UpdateAccessed(time_t acc);

	void SetKey(HistoryKey* key);

	OP_STATUS SetAssociatedItem(HistoryPage* associated_item);

	void ClearAssociatedItem();

	HistoryPage * GetAssociatedItem() { return m_associated_item; }

	void SetIsRemoved() { m_flags |= IS_REMOVED;}
	BOOL IsRemoved() const { return (m_flags & IS_REMOVED) != 0;}

	virtual void OnSiteFolderSet();

	void SetIsDomain() { m_flags |= IS_DOMAIN;}
	void ClearIsDomain() { m_flags &= ~IS_DOMAIN;}
	BOOL IsDomain() const { return (m_flags & IS_DOMAIN) != 0;}

    enum PageFlags
	{
		IN_HISTORY    = 1,
		BOOKMARKED    = 2,
		IS_NICK       = 4,
		IS_OPERA_PAGE = 8,
		IS_TYPED      = 16,
		IS_DOMAIN     = 32,
		IS_REMOVED    = 64,
		IS_SELECTED   = 128
    };

    time_t       m_accessed;
    time_t       m_average_interval;
	int          m_flags;
	HistoryKey*  m_key;
	HistoryPage* m_associated_item;
};

typedef HistoryPage CoreHistoryModelPage;      // Temorary typedef to old class name, so old code continues to compile

#endif // HISTORY_SUPPORT
#endif // CORE_HISTORY_MODEL_PAGE_H
