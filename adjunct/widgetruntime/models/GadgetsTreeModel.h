/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGETS_TREE_MODEL_H
#define GADGETS_TREE_MODEL_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetList.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/util/adt/opvector.h"

class GadgetsTreeModelItem;
class GadgetsTreeModelItemImpl;
class OpGadgetClass;


/**
 * An OpTreeModel of all the gadgets installed in the OS.  The model can be
 * extended with additional gadget sources.
 *
 * The strategy for keeping the model in sync with the OS is based on a polling
 * mechanism backed by a PlatformGadgetList:
 *
 * 		every 3 seconds // or so
 *			if platform_gadget_list.HasChangedSince(timestamp)
 *				UpdateModel(platform_gadget_list.FetchGadgetList())
 *				update timestamp
 *
 * The idea is that, while FetchGadgetList() is potentially time and/or resource
 * expensive, HasChangedSince()'s implementation can be very cheap.  E.g., on
 * Windows, HasChangedSince() checks the modification time of a single registry
 * key. 
 *
 * FIXME: A disadvantage of this strategy is the necessity for polling.  No
 * matter how efficient HasChangedSince() is, it's still a waste, especially
 * because the gadget list will usually change very rarely.
 *
 * A better strategy would be to use the pertinent notification mechanisms
 * available on the platform and respond to them (something like
 * RegNotifyChangeKeyValue() on Windows perhaps?).  The platform would implement
 * a lightweight thread only waiting on this notification and using the
 * g_thread_tools functionality to notify Quick via an Opera message when the
 * list changes.  The async file implementaion in WindowsOpLowlevelFile.cpp may
 * serve as an example of g_thread_tools usage.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetsTreeModel
	: public TreeModel<GadgetsTreeModelItem>,
	  public OpTimerListener
{
public:

	/**
	 * A source of gadgets to be merged into a GadgetsTreeModel instance.
	 *
	 * By default, a GadgetsTreeModel is configured with one source representing
	 * the gadgets installed in the OS.  Additional sources can be configured
	 * with GadgetsTreeModel::AddSource().
	 * 
	 * @author Wojciech Dzierzanowski (wdzierzanowski)
	 */
	class Source
	{
	public:
		virtual ~Source()  {}

		/**
		 * Tells the source to refresh itself, so that when the other methods
		 * are called subsequently, the results are up-to-date.
		 *
		 * @return status
		 */
		virtual OP_STATUS Refresh() = 0;

		/**
		 * @return the number of items in the source
		 */
		virtual UINT32 GetItemCount() const = 0;

		/**
		 * @param i the item index
		 * @return an OpGadgetClass instance describing the item at index @a i
		 */
		virtual const OpGadgetClass& GetItemInfo(UINT32 i) const = 0;

		/**
		 * @param i the item index
		 * @return a new GadgetsTreeModelItemImpl representing the item at index
		 * 		@a i, for use in the panel
		 */
		virtual GadgetsTreeModelItemImpl* CreateItemImpl(UINT32 i) const = 0;
	};


	GadgetsTreeModel();
	virtual ~GadgetsTreeModel();

	OP_STATUS Init();

	/**
	 * Returns the default source of gadgets.  These are the gadgets installed
	 * in the OS.
	 *
	 * @return the default source of gadgets
	 */
	const Source& GetDefaultSource() const;

	/**
	 * Adds a source of gadgets.  This tells the model to fetch gadget
	 * information from this source.  The results obtained from all the sources
	 * attached to the model are merged to produce one list of gadgets.
	 *
	 * @param source a source of gadgets
	 * @return status
	 */
	OP_STATUS AddSource(OpAutoPtr<Source> source);

	/**
	 * (De)activates the model.  An active model keeps itself in sync with the
	 * list of gadgets installed in the OS.
	 *
	 * @param active whether the model should be active or not
	 */
	void SetActive(BOOL active);

	//
	// OpTreeModel
	//
	virtual INT32 GetColumnCount()
		{ return 3; }

	virtual OP_STATUS GetColumnData(ColumnData* column_data)
		{ return OpStatus::OK; }

	//
	// OpTimerListener
	//	
	virtual void OnTimeOut(OpTimer* timer);

private:
	class PlatformSource;
	typedef OpAutoVector<Source> Sources;

	void OnPlatformPollingTick();
	void RefreshModel();

	OpAutoPtr<PlatformGadgetList> m_platform_gadget_list;
	OpTimer m_platform_polling_clock;
	time_t m_platform_timestamp;

	Sources m_sources;

	BOOL m_active;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGETS_TREE_MODEL_H
