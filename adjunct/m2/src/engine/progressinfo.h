// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef PROGRESSINFO_H
#define PROGRESSINFO_H

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/listeners.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/util/adt/oplisteners.h"

class ProgressInfo;
class Index;

/** @brief A way to keep tabs on the progress of a mail backend
  * @author Arjan van Leeuwen
  *
  * This class saves the progress of a mail backend, and takes appropriate
  * actions when the progress changes. Can be queried by UI to display progress.
  */
class ProgressInfo
	: public ProgressInfoListener
	, public MessageObject
{
public:
	enum ProgressAction
	{
		NONE,
		CONNECTING,
		AUTHENTICATING,
		CONTACTING,
		EMPTYING_TRASH,
		SENDING_MESSAGES,
		UPDATING_FEEDS,
		CHECKING_FOLDER,
		FETCHING_FOLDER_LIST,
		CREATING_FOLDER,
		DELETING_FOLDER,
		RENAMING_FOLDER,
		FOLDER_SUBSCRIBE,
		FOLDER_UNSUBSCRIBE,
		DELETING_MESSAGE,
		APPENDING_MESSAGE,
		COPYING_MESSAGE,
		STORING_FLAG,
		SYNCHRONIZING,
		FETCHING_HEADERS,
		FETCHING_MESSAGES,
		FETCHING_GROUPS,
		LOADING_DATABASE,
		_PROGRESS_ACTION_COUNT
	};

	/** Constructor
	  */
	ProgressInfo();

	/** Destructor
	  */
	~ProgressInfo();

	/** Reset all data
	  * @param reset_connected Whether to reset the connection status as well
	  */
	void			Reset(BOOL reset_connected = TRUE);

	/** Set the current action for this progress keeper
	  * @param current_action The action to set
	  * @param total_count How many times this action will have to be performed
	  */
	OP_STATUS		SetCurrentAction(ProgressAction current_action, unsigned total_count = 1);

	/** Update the current action (tell how many of them have been performed)
	  * @param amount The amount of actions that have been performed
	  */
	void			UpdateCurrentAction(int amount = 1) { m_count += amount; AlertListeners(); }

	/** Set the absolute progress of current action
	  * @param count How many actions have already been performed
	  * @param total How many actions should be performed in total
	  */
	void			SetCurrentActionProgress(unsigned count, unsigned total_count);

	/** Add additional actions to the current action
	  * @param count How many to add
	  */
	void			AddToCurrentAction(int count) { m_total_count += count; AlertListeners(); }

	/** End the current action
	  * @param end_connection Ends the connection as well
	  */
	OP_STATUS		EndCurrentAction(BOOL end_connection);

	/** Sets the 'sub progress' for an action (e.g. percentage of a message fetched)
	  * @param current_count Bits done
	  * @param total_count Total amount of bits to do
	  */
	void			SetSubProgress(unsigned current_count, unsigned total_count);

	/** Notify when new messages have been received
	  * @param account Account that received new messages
	  */
	OP_STATUS		NotifyReceived(Account* account);

	/** Sets whether we are connected to a server
	  */
	void			SetConnected(BOOL connected);

	/** Gets whether we are connected to a server
	  */
	BOOL			IsConnected() const { return m_connected; }

	/** Gets the current action in progress
	  * @return Current action in progress
	  */
	ProgressAction	GetCurrentAction() const { return m_current_action; }

	/** @return Whether current action should be displayed as a percentage
	  */
	BOOL			DisplayAsPercentage() const { return m_current_action == LOADING_DATABASE; }

	/** Get amount done for current action
	  * @return amount done (out of total)
	  */
	unsigned		GetCount() const { return m_count; }

	/** Get total amount to do for current action
	  * @return Total amount to do
	  */
	unsigned		GetTotalCount() const { return m_total_count; }

	/** Get amount done for sub progress
	  * @return Amount done (out of total) for sub progress
	  */
	unsigned		GetSubCount() const { return m_sub_count; }

	/** Get amount to do for sub progress
	  * @return Total amount to do for sub progress
	  */
	unsigned		GetSubTotalCount() const { return m_sub_total_count; }

	/** Get a string description of the current progress status
	  * @param progress_string Where to save the string
	  */
	OP_STATUS		GetProgressString(OpString& progress_string) const;

	/** Merge a progress info, keep 'most important' progress
	  * @param to_merge What to merge
	  */
	void			Merge(const ProgressInfo& to_merge);

	/** Add a listener
	  * @param listener Listener to add
	  */
	OP_STATUS		AddListener(ProgressInfoListener* listener) { return m_listeners.Add(listener); }

	/** Remove a listener
	  * @param listener Listener to remove
	  */
	OP_STATUS		RemoveListener(ProgressInfoListener* listener) { return m_listeners.Remove(listener); }

	/** Alert listeners something has changed
	  */
	void			AlertListeners();

	/** Add a notification listener
	  * @param listener Listener to add
	  */
	OP_STATUS		AddNotificationListener(MailNotificationListener* listener) { return m_notification_listeners.Add(listener); }

	/** Remove a notification listener
	  * @param listener Listener to remove
	  */
	OP_STATUS		RemoveNotificationListener(MailNotificationListener* listener) { return m_notification_listeners.Remove(listener); }

	//To forward events from mail views being activated/deactivated to listeners
	void			OnMailViewActivated(MailDesktopWindow* mail_window, BOOL active);

	// From ProgressInfoListener:
	void			OnProgressChanged(const ProgressInfo& progress);
	void			OnSubProgressChanged(const ProgressInfo& progress);
	void			OnStatusChanged(const ProgressInfo& progress);

	// From MessageObject:
	void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	ProgressAction	m_current_action;
	unsigned		m_count;
	unsigned		m_total_count;
	unsigned		m_sub_count;
	unsigned		m_sub_total_count;
	BOOL			m_connected;
	BOOL			m_progress_changed;
	BOOL			m_subprogress_changed;

	OpListeners<ProgressInfoListener> m_listeners;
	OpListeners<MailNotificationListener> m_notification_listeners;

	static const unsigned long ProgressDelay = 100; ///< Delay for posting progress messages (ms)
};


#endif // PROGRESSINFO_H
