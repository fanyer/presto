/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Michal Zajaczkowski
 */

#ifdef AUTO_UPDATE_SUPPORT

#ifndef AUTOUPDATE_SERVER_URL_H
#define AUTOUPDATE_SERVER_URL_H

/**
 * This was a part of the AutoUpdateURL class.
 * Meant to manage the autoupdate server adresses. The opera:config#AutoUpdate|AutoupdateServer preference may hold a number of autoupdate server addresses
 * separated by a space character. This class parses the preference value and holds the list of available servers, making it possible to iterate trough the
 * list.
 * This comes in handy when a call to the autoupdate server fails and we want to try another server.
 * This is useless since there is no other server available at the moment.
 *
 * This class hold the number of "current" server, which is 0 - the first one on the list - initially. Calls to GetCurrentURL() return the current server
 * address. Use IncrementURLNo(), ResetURLNo(), GetURLCount() and SetURLNo() to set the current server number.
 * Remember that this class is meant primarily to exist in one copy hold by the AutoUpdateManager, so whenever the current server is changed, it has an
 * influence on every autoupdate-related functionality.
 *
 */
class AutoUpdateServerURL:
	public OpPrefsListener
{
public:
	/**
	 * Defines how to wrap around the server list when calling IncrementURLNo.
	 */
	enum WrapType
	{
		/**
		 * If the last server on the list has been reached, wrap the current server to the first server on the list.
		 */
		Wrap,
		/**
		 * If the last server on the list has been reached, don't wrap to the first one.
		 */
		NoWrap
	};

	AutoUpdateServerURL();
	~AutoUpdateServerURL();

	/**
	 * Read the server list from the opera:config#AutoUpdate|AutoupdateServer preference and parse it, putting the result into the m_url_list vector.
	 * This may safely be called multiple times and this is called whenever the preference changes, so changing the autoupdate server address has now
	 * an instant effect, i.e. no browser restart is required.
	 */
	OP_STATUS	Init();

	/**
	 * Get the current server address.
	 *
	 * @param url - the URL string of the current server.
	 *
	 * @return - OpStatus::OK when the URL string could be retrieved succesfully, ERR otherwise.
	 */
	OP_STATUS	GetCurrentURL(OpString& url);

	/**
	 * Return the total URL server addresses count as found in the opera:config#AutoUpdate|AutoupdateServer preference value.
	 */
	UINT32		GetURLCount();

	/**
	 * Increment the current server number by one. Wrap through the end of the list back to the first server in case the wrap type is set to Wrap.
	 *
	 * @param wrap - Set to Wrap to have the list wrap over silently, set to NoWrap to receive an error if the current server is already the last one.
	 *
	 * @return - OpStatus::ERR if Init() did not succeed or the current server is already the last one and NoWrap was used with this call. OpStatus::OK otherwise.
	 */
	OP_STATUS	IncrementURLNo(WrapType wrap = NoWrap);

	/**
	 * Set the current URL number directly.
	 *
	 * @param url_number - the url number that is to be current from now on. URL numbers start with 0.
	 *
	 * @return - OpStatus::ERR in case the number is negative or greater than the total URL count, or Init() did not succeed. OpStatus::OK otherwise.
	 */
	OP_STATUS	SetURLNo(int url_number);

	/**
	 * Set the URL number back to 0.
	 *
	 * @return - OpStatus::ERR in case Init() did not succeed, OK otherwise.
	 */
	OP_STATUS	ResetURLNo();

	/**
	 * OpPrefsListener implementation
	 */
	void PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC & newvalue);
private:
	OP_STATUS ReadServers();

	/**
	 * Current URL number, starting from 0 up to (m_url_count - 1).
	 */
	int		m_current_url_no;

	/**
	 * URLs resulting from parsing the preference value are held here.
	 */
	OpAutoVector<OpString> m_url_list;
};

#endif // AUTOUPDATE_SERVER_URL_H
#endif // AUTO_UPDATE_SUPPORT
