/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef __DESKTOPNOTIFIER_H__
# define __DESKTOPNOTIFIER_H__

class DesktopNotifier;
class OpInputAction;

class DesktopNotifierListener
{
public:
	virtual void OnNotifierDeleted(const DesktopNotifier* notifier){}
	virtual ~DesktopNotifierListener(){}
};

class DesktopNotifier
{
public:
	// Type of notification
	enum DesktopNotifierType
	{
		MAIL_NOTIFICATION,
		BLOCKED_POPUP_NOTIFICATION,
		TRANSFER_COMPLETE_NOTIFICATION,
		WIDGET_NOTIFICATION,
		NETWORK_SPEED_NOTIFICATION,
		AUTOUPDATE_NOTIFICATION,
		EXTENSION_NOTIFICATION,
		AUTOUPDATE_NOTIFICATION_EXTENSIONS,
		PLUGIN_DOWNLOADED_NOTIFICATION
	};

	virtual				~DesktopNotifier() {}

	static	OP_STATUS	Create(DesktopNotifier **new_notifier);

	virtual OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& text, const OpStringC8& image, OpInputAction* action, BOOL managed = FALSE, BOOL wrapping = FALSE) = 0;
	virtual OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& title, Image& image, const OpStringC& text, OpInputAction* action, OpInputAction* cancel_action, BOOL managed = FALSE, BOOL wrapping = FALSE) = 0;

	/** Adds a button to the notifier below the title
	  * @param text Text on button
	  * @param action Action to execute when this button is clicked
	  */
	virtual OP_STATUS AddButton(const OpStringC& text, OpInputAction* action) = 0;

	virtual void StartShow() = 0;

	virtual BOOL IsManaged() const = 0;

	virtual void AddListener(DesktopNotifierListener* listener) = 0;
	virtual void RemoveListener(DesktopNotifierListener* listener) = 0;

	virtual void SetReceiveTime(time_t time) = 0;

#ifdef M2_SUPPORT
	static bool UsesCombinedMailAndFeedNotifications();
#endif
};

#endif // __DESKTOPNOTIFIER_H__
