// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/desktop_pi/desktop_notifier.h"

#ifdef M2_SUPPORT
# include "adjunct/m2/src/engine/listeners.h"
#endif // M2_SUPPORT

#define g_notification_manager NotificationManager::GetInstance()

class DesktopNotifier;
class OpInputAction;

class NotificationManager
	: public DesktopManager<NotificationManager>
#ifdef M2_SUPPORT
	, public MailNotificationListener
#endif // M2_SUPPORT
{
public:
	~NotificationManager();
	OP_STATUS Init() { return OpStatus::OK; }

	/** Show a notification
	  * @param text Text to show in notification
	  * @param image Image to show in notification
	  * @param action Action to execute when notification is clicked
	  * @param wrap_text TRUE if the text in the notification should be wrapped if each line doesn't fit
	  */
	void	 ShowNotification(DesktopNotifier::DesktopNotifierType notification_group,
		const OpStringC& text,
		const OpStringC8& image,
		OpInputAction* action,
		BOOL wrap_text = FALSE);

    void     ShowNotification(DesktopNotifier::DesktopNotifierType notification_group,
		const OpStringC& title,
		const OpStringC& text,
		Image& image,
		OpInputAction* action,
		OpInputAction* cancel_action,
		BOOL wrap_text,
		time_t receive_time = 0);

    void     ShowNotification(DesktopNotifier::DesktopNotifierType notification_group,
		const OpStringC& title,
		const OpStringC& text,
		Image& image,
		OpInputAction* action,
		OpInputAction* cancel_action,
		BOOL wrap_text,
		OpWidget* anchor,
		time_t receive_time = 0);

	/** Cancel all notifications.
	  *
	  * This destroys all the notifier objects, i.e. closes all the
	  * notification windows.
	  */
	void	CancelAllNotifications();

	/** Add a notifier to list of notifiers (use this when a notifier is created)
	  * @param notifier Notifier to add
	  */
	void	 AddNotifier(DesktopNotifier *notifier) { m_notifiers.Add(notifier); }

	/** Remove a notifier from list of notifiers (use this when a notifier is destroyed)
	  * @param notifier Notifier to remove
	  */
	void	 RemoveNotifier(DesktopNotifier *notifier);

#ifdef M2_SUPPORT
	// From MailNotificationListener
	void NeedNewMessagesNotification(Account* account, unsigned count);
	void NeedNoMessagesNotification(Account* account);
	void NeedSoundNotification(const OpStringC& sound_path);
	void OnMailViewActivated(MailDesktopWindow* mail_window, BOOL active) {}
#endif // M2_SUPPORT

private:
	OpVector<DesktopNotifier> m_notifiers;
};

#endif // NOTIFICATION_MANAGER_H
