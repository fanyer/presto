/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#ifndef CORE_HISTORY_MODEL_ITEM_H
#define CORE_HISTORY_MODEL_ITEM_H

#ifdef HISTORY_SUPPORT

#include "modules/history/src/OpSplayTree.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/util/OpTypedObject.h"

class HistoryPrefixFolder;
class HistorySiteFolder;

//#define HISTORY_DEBUG

//___________________________________________________________________________
//                         Core HISTORY MODEL ITEM
//___________________________________________________________________________

class HistoryItem
	: public OpTypedObject
{
    friend class HistoryModel;

 public:

	//---------------------------------------------------------------------------
	// Public listener class:
	// Associates one HistoryItem with one (and only one) listener
    //---------------------------------------------------------------------------

    class Listener
	{
	public:

		virtual OpTypedObject::Type GetListenerType() = 0;

		virtual void OnHistoryItemDestroying() = 0;

		virtual void OnHistoryItemAccessed(time_t acc) {}

	protected:
		virtual ~Listener(){}
	};

	/**
	 * @param listener that is to be associated with this item
	 *
	 * @return OpStatus::OK if successful in setting listener
	 **/
	OP_STATUS AddListener(HistoryItem::Listener *listener)
		{
			return m_listeners.Add(listener);
		}

	/**
	 * @param listener that is to be removed from this item
	 *
	 * @return OpStatus::OK if successful in removing listener
	 **/
	OP_STATUS RemoveListener(HistoryItem::Listener *listener)
		{
			return m_listeners.Remove(listener);
		}

	/**
	 * @return the vector with the listeners currently associated with this item
	 **/
	const OpListeners<Listener> & GetListeners() const { return m_listeners; }

	/**
	 * @param type   - the type of listener you want
	 * @param result - where all listeners of type currently associated with this item are added
	 *
	 * @return OpStatus::OK if successful
	 **/
	OP_STATUS GetListenersByType(OpTypedObject::Type type, OpVector<Listener> & result)
		{
			OP_STATUS status = OpStatus::OK;

			for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator) && OpStatus::IsSuccess(status);)
			{
				HistoryItem::Listener* item = m_listeners.GetNext(iterator);

				if(item->GetListenerType() == type)
				{
					status = result.Add(item);
				}
			}

			return status;
		}

	/**
	 * @param type   - the type of listener you want
	 *
	 * @return the first listener found with this type
	 **/
	Listener * GetListenerByType(OpTypedObject::Type type)
		{
			for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
			{
				HistoryItem::Listener* item = m_listeners.GetNext(iterator);

				if(item->GetListenerType() == type)
					return item;
			}

			return 0;
		}

	/**
	 * @return the title of this item
	 **/
	virtual OP_STATUS GetTitle(OpString & title) const { return title.Set(m_title.CStr()); }
	virtual const OpStringC & GetTitle() const { return m_title; }

	/**
	 * @return a pointer to the associated prefix folder for this item
	 **/
	HistoryPrefixFolder* GetPrefixFolder() const {return m_prefixFolder;}

	/**
	 * This is only used in the page and site sub folder subclasses
	 *
	 * @return a pointer to the associated site folder for this item
	 **/
	HistorySiteFolder* GetSiteFolder() const {return m_siteFolder;}

	/**
	 * @return TRUE if item is pinned - meaning it cannot be removed unforced
	 **/
    virtual BOOL IsPinned() const {return FALSE;}

	/**
	 * @return TRUE if item can be deleted
	 **/
	virtual BOOL IsDeletable() const {return TRUE;}

	/**
	 * @return the type of image that should be used for this item
	 **/
	virtual const char* GetImage() const = 0;

	/**
	 * @return the key string
	 **/
	virtual const uni_char* GetKeyString() const { return GetKey() ? GetKey()->GetKey() : 0; }

	// == OpTypedObject ======================

	virtual INT32 GetID() { return m_id; }
	virtual Type  GetType() = 0;

	// == LexSplayItem ======================

 	virtual const LexSplayKey * GetKey() const = 0;

#ifdef HISTORY_DEBUG
	virtual int GetSize() = 0;
#endif // HISTORY_DEBUG

 protected:

	/**
	 * @param title associated with this folder
	 * @param listeners that are to be associated with this item
	 *
	 * Protected constructor - this is an abstract class
	 **/
	HistoryItem();

	/**
	 * Protected destructor - this is an abstract class
	 **/
    virtual ~HistoryItem();

	/*
	 * @param folder the site folder that this item should have associated to it
	 **/
    void  SetSiteFolder(HistorySiteFolder* folder);

#ifdef HISTORY_DEBUG
	virtual void OnTitleSet() {}
	int Size() { return sizeof(HistoryItem) + GetTitleLength(); }
#endif // HISTORY_DEBUG

	int GetTitleLength() const { return m_title.HasContent() ? m_title.Length() * sizeof(uni_char) : 0; }

	OP_STATUS SetTitle(const OpStringC& title);

	// Hook for detecting that site folder has been set
	virtual void OnSiteFolderSet() {}

	/**
	 * Broadcast that the item has been accessed
	 * @param acc - the new accessed time
	 **/
	void BroadcastHistoryItemAccessed(time_t acc)
	{
		for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		{
			HistoryItem::Listener* item = m_listeners.GetNext(iterator);
  			item->OnHistoryItemAccessed(acc);
		}
	}

	/**
	 * Broadcast that the item is being destroyed
	 **/
	void BroadcastHistoryItemDestroying()
	{
		for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		{
			HistoryItem::Listener* item = m_listeners.GetNext(iterator);
  			item->OnHistoryItemDestroying();
		}
	}

 private:

    HistorySiteFolder*    m_siteFolder;
	HistoryPrefixFolder*  m_prefixFolder;
	INT32	              m_id;
	OpListeners<Listener> m_listeners;
	OpString              m_title;

	void SetPrefixFolder(HistoryPrefixFolder* folder) { m_prefixFolder = folder; }
};

typedef HistoryItem CoreHistoryModelItem;    // Temorary typedef to old class name, so old code continues to compile

#endif // HISTORY_SUPPORT
#endif // CORE_HISTORY_MODEL_ITEM_H
