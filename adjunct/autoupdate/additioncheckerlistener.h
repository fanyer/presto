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

#ifndef AUTOUPDATE_ADDITION_CHECKER_LISTENER_H
#define AUTOUPDATE_ADDITION_CHECKER_LISTENER_H

/**
 * The listener interface for the AdditionChecker. Implement this to get notifications about an addition resolve result.
 * In order to receive the notifications, register with the g_autoupdate_manager->AddListener().
 */
class AdditionCheckerListener
{
public:
	virtual ~AdditionCheckerListener() {};

	/**
	 * Called if an addition was able to resolve succesfully. You need to check if this call is for you, since notifications for ALL queued items
	 * are broadcasted to ALL registered listeners.
	 * You need to know what you have requested and ignore calls that are not meant for you.
	 *
	 * @param type - the type of the addition that was just resolved.
	 * @param key - the key of the addition that was just resolved.
	 * @param resource - a pointer to an UpdatableResource object representing the addition that was resolved. You receive the ownership of this
	 *					 resource with this call, so be very careful to filter out the calls that are meant for you.
	 */
	virtual void OnAdditionResolved(UpdatableResource::UpdatableResourceType type, const OpStringC& key, UpdatableResource* resource) = 0;

	/**
	 * Called when an addition finally failed to resolve after a few retries (RESOLVE_MAX_RETRY_COUNT_PER_ITEM). Since this might still be 
	 * a network or the autoupdate server issue it might have sense to ask about this addition later on.
	 * The autoupdate server should be configured in such a way that is sends a meaningful resources for calls that request an addition that
	 * can't be served by it, i.e. a request for a plugin that we don't offer for automatic installation still returns an XML with a plugin
	 * description that has a <has-installer> XML element with a zero value.
	 * This call is meant to inform that there was a problem with receiving any answer about this addition from the server.
	 */
	virtual void OnAdditionResolveFailed(UpdatableResource::UpdatableResourceType type, const OpStringC& key) = 0;
};

#endif // AUTOUPDATE_ADDITION_CHECKER_LISTENER_H
#endif // AUTO_UPDATE_SUPPORT
