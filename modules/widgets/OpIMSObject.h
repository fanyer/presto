/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPIMSINFO_H
#define OPIMSINFO_H

#ifdef WIDGETS_IMS_SUPPORT

#include "modules/windowcommander/src/WindowCommander.h"

class ItemHandler;
class OpIMSUpdateList;

/**
 * OpIMSObject and OpIMSListener are used for communication between a
 * widget which has requested an IMS from the platform, and the
 * WindowCommander.
 *
 * A widget which wants to request an IMS should call the appropriate
 * API in WindowCommander and supply an OpIMSObject used for getting
 * information from the widget, and the widget should also supply an
 * OpIMSListener to receive callbacks from the platform.
 */

/**
 * Listener interface used to receive information about updates after
 * IMS was requested.
 */
class OpIMSListener
{
public:
	virtual ~OpIMSListener() {}

	virtual void OnCommitIMS(OpIMSUpdateList *updates) = 0;
    virtual void OnCancelIMS() = 0;
};

/**
 * Container class for managing a list of updates for a widget.
 *
 * Widget will receive an instance of this class in OpIMSListener::OnCommitIMS()
 * which should be used to get information about which items have been selected.
 */
class OpIMSUpdateList
{
public:
	virtual ~OpIMSUpdateList(){}

	/** Get first modified item.
	*
	* @return Index of first modified item, -1 if no item has been modified.
	*/
	virtual int GetFirstIndex(void) = 0;

	/* Get next modified item.
	*
	* @return Index of next item, -1 if there is no next modified item.
	*/
	virtual int GetNextIndex(int index) = 0;

	/** Get selected value for item at a certain index.
	*
	* @return -1 for not modified, 0 for not selected, 1 for selected
	*/
	virtual int GetValueAt(int index) = 0;

	/** Clear updates and set all items as not modified. */
	virtual void Clear(void) = 0;

	/** Update and item with a selected value. */
	virtual void Update(int index, BOOL selected) = 0;
};

/**
 * Class that sits between the OpWidget and the OpWindowCommander:
 * passes requests from the OpWidget to the OpWindowCommander, and
 * passes callbacks from the platform to the OpIMSListener.
 */
class OpIMSObject : public OpDocumentListener::IMSCallback
{
public:
    OpIMSObject();
    virtual ~OpIMSObject();

	/**
	 * Set OpIMSListener that shall receive information about commit/cancel
	 */
	void SetListener(OpIMSListener* listener);

	/**
	 * Set ItemHandler that is used for retrieving information about entries.
	 */
	void SetItemHandler(ItemHandler* ih);

	/**
	 * Returns TRUE if the widget that uses this OpIMSObject thinks
	 * the IMS is still active.
	 */
	BOOL IsIMSActive();

	/**
	 * Request the platform to open an activated select list.
	 *
	 * If successful, then the widget will either receive OnCommitIMS()
	 * or OnCancelIMS().
	 *
	 * If the widget is updated or cancelled when an IMS is activated,
	 * the widget can use the API:s UpdateIMS() or CancelIMS() (see below).
	 */
    OP_STATUS StartIMS(WindowCommander* windowcommander);

	/**
	 * Inform the platform that the widget was updated during an active IMS.
	 *
	 * Will have no effect if no IMS is currently active.
	 */
	OP_STATUS UpdateIMS(WindowCommander* windowcommander);

	/**
	 * Inform the platform the the widget was destroyed during an active IMS.
	 *
	 * Will have no effect if no IMS is currently active.
	 */
	void DestroyIMS(WindowCommander* windowcommander);

	/** Widget can use this method to set information about e.g. scrollbar or multiselect. */
	void SetIMSAttribute(IMSAttribute attr, int value);

	/** Set the rect for the widget, in screen coordinats. */
	void SetRect(const OpRect& rect);

    /** IMSCallback */
    virtual int GetItemCount();
    virtual int GetFocusedItem();
    virtual int GetIMSAttribute(IMSAttribute attr);
    virtual int GetIMSAttributeAt(int index, IMSItemAttribute attr);
    virtual const uni_char * GetIMSStringAt(int index);
    virtual OpRect GetIMSRect();

    virtual void OnCommitIMS();
    virtual void OnUpdateIMS(int item, BOOL selected);
    virtual void OnCancelIMS();

private:
	/** Helper function to create the m_updates object. */
	OP_STATUS CreateUpdatesObject();

    OpIMSUpdateList* m_updates;
    OpIMSListener* m_listener;
    ItemHandler *m_ih;
    BOOL has_scrollbar;
	BOOL m_dropdown;
    OpRect m_rect;
};

#endif // WIDGETS_IMS_SUPPORT

#endif // OPIMSINFO_H
