/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick/managers/NotificationManager.h"

#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/desktop_util/sound/SoundUtils.h"
#include "adjunct/desktop_util/string/i18n.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpScreenInfo.h"
#include "adjunct/quick/widgets/OpWidgetNotifier.h"

#ifdef M2_SUPPORT
# include "adjunct/m2/src/engine/account.h"
# include "adjunct/m2/src/engine/message.h"
# include "adjunct/m2/src/engine/progressinfo.h"
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/engine/index.h"
# include "adjunct/m2/src/engine/store.h"
# include "modules/util/filefun.h"
#endif // M2_SUPPORT

/***********************************************************************************
 ** Cleanup
 **
 ** NotificationManager::~NotificationManager
 **
 ***********************************************************************************/
NotificationManager::~NotificationManager()
{
	// Notifiers must be deleted after all desktop windows because FindTextManager
	// objects delete them internally as well.
	CancelAllNotifications();

#ifdef M2_SUPPORT
	if (g_m2_engine)
		MessageEngine::GetInstance()->GetMasterProgress().RemoveNotificationListener(this);
#endif // M2_SUPPORT
}


/***********************************************************************************
 ** Show a notification
 **
 ** NotificationManager::ShowNotification
 ** @param text Text to show in notification
 ** @param image Image to show in notification
 ** @param action Action to execute when notification is clicked, ownership is transferred
 **
 ***********************************************************************************/
void NotificationManager::ShowNotification(DesktopNotifier::DesktopNotifierType notification_group, 
										   const OpStringC& text,
										   const OpStringC8& image,
			 							   OpInputAction* action,
										   BOOL wrap_text)
{
	if (!g_application)
		return;
	OpAutoPtr<OpInputAction> action_ptr(action);
	DesktopWindow *adw;

	// don't show notifications if we're in fullscreen mode. Must test BrowserDesktopWindow
	if ((adw = g_application->GetActiveBrowserDesktopWindow()) != NULL && adw->IsFullscreen())
	{
		return;
	}
	DesktopNotifier* notifier = NULL;
	RETURN_VOID_IF_ERROR(DesktopNotifier::Create(&notifier));

	if (notifier)
	{
		RETURN_VOID_IF_ERROR(notifier->Init(notification_group, text, image, action, TRUE, wrap_text));
		action_ptr.release();
		if (m_notifiers.GetCount() == 1 || !notifier->IsManaged())
			notifier->StartShow();
	}
}

/***********************************************************************************
 ** Show a notification
 **
 ** NotificationManager::ShowNotification
 ** @param title Title to show in notification
 ** @param text Text to show in notification
 ** @param image Image to show in notification
 ** @param action Action to execute when notification is clicked, ownership is transferred
 **
 ***********************************************************************************/
void NotificationManager::ShowNotification(DesktopNotifier::DesktopNotifierType notification_group, 
										   const OpStringC& title,const OpStringC& text,
										   Image& image,
			 							   OpInputAction* action,
										   OpInputAction* cancel_action,
										   BOOL wrap_text,
										   time_t receive_time)
{
	if (!g_application)
		return;

	DesktopWindow *adw;
	OpAutoPtr<OpInputAction> action_ptr(action);

	// don't show notifications if we're in fullscreen mode. Must test BrowserDesktopWindow
	if ((adw = g_application->GetActiveBrowserDesktopWindow()) != NULL && adw->IsFullscreen())
	{
		return;
	}
	DesktopNotifier* notifier = NULL;
	RETURN_VOID_IF_ERROR(DesktopNotifier::Create(&notifier));

	if (notifier)
	{
		RETURN_VOID_IF_ERROR(notifier->Init(notification_group, title, image, text, action, cancel_action, TRUE, wrap_text));
		action_ptr.release();
		notifier->SetReceiveTime(receive_time);
		if (m_notifiers.GetCount() == 1 || !notifier->IsManaged())
			notifier->StartShow();
	}
}

/***********************************************************************************
 ** Show a notification next to the specified control. It will never use managed
 ** notification
 **
 ** NotificationManager::ShowNotification
 ** @param title Title to show in notification
 ** @param text Text to show in notification
 ** @param image Image to show in notification
 ** @param action Action to execute when notification is clicked, ownership is transferred
 ** @param anchor A widget to use as a reference point for the notification.
 **
 ***********************************************************************************/
void NotificationManager::ShowNotification(DesktopNotifier::DesktopNotifierType notification_group, 
										   const OpStringC& title,const OpStringC& text,
										   Image& image,
			 							   OpInputAction* action,
										   OpInputAction* cancel_action,
										   BOOL wrap_text,
										   OpWidget* anchor,
										   time_t receive_time)
{
	if (!g_application)
		return;

	DesktopWindow *adw;
	OpAutoPtr<OpInputAction> action_ptr(action);

	// don't show notifications if we're in fullscreen mode. Must test BrowserDesktopWindow
	if ((adw = g_application->GetActiveBrowserDesktopWindow()) != NULL && adw->IsFullscreen())
	{
		return;
	}
	DesktopNotifier* notifier = OP_NEW(OpWidgetNotifier, (anchor));

	if (notifier)
	{
		RETURN_VOID_IF_ERROR(notifier->Init(notification_group, title, image, text, action, cancel_action, TRUE, wrap_text));
		action_ptr.release();
		notifier->SetReceiveTime(receive_time);
		if (m_notifiers.GetCount() == 1 || !notifier->IsManaged())
			notifier->StartShow();
	}
}


void NotificationManager::CancelAllNotifications()
{
	// Deleting a notifier will remove it from the m_notifiers list. (bug #264565).
	for (UINT32 i = m_notifiers.GetCount(); i > 0; --i)
		OP_DELETE(m_notifiers.Get(i - 1));

	// ...and just in case some DesktopNotification doesn't do it:
	OP_ASSERT(m_notifiers.GetCount() == 0);
	m_notifiers.Remove(0, m_notifiers.GetCount());
}


/***********************************************************************************
 ** Remove a notifier from list of notifiers (use this when a notifier is destroyed)
 **
 ** NotificationManager::RemoveNotifier
 ** @param notifier Notifier to remove
 **
 ***********************************************************************************/
void NotificationManager::RemoveNotifier(DesktopNotifier* notifier)
{
	OpStatus::Ignore(m_notifiers.RemoveByItem(notifier));

	if (notifier->IsManaged())
	{
		for (unsigned i = 0; i < m_notifiers.GetCount(); i++)
		{
			if (m_notifiers.Get(i)->IsManaged())
			{
				m_notifiers.Get(i)->StartShow();
				break;
			}
		}
	}
}

#ifdef M2_SUPPORT
/***********************************************************************************
 ** Show a notification for multiple new messages
 **
 ** NotificationManager::NeedNewMessagesNotification
 ** @param index_id Index to go to when clicked on notification
 ** @param new_messages Number of new messages received
 **
 ***********************************************************************************/
void NotificationManager::NeedNewMessagesNotification(Account* account, unsigned count)
{
	OpString    notification, count_string;
	DesktopNotifier* notifier;

	if (!account || !g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationForNewMessages))
		return;

	Index* account_index = g_m2_engine->GetIndexById(IndexTypes::FIRST_ACCOUNT + account->GetAccountId());

    if (DesktopNotifier::UsesCombinedMailAndFeedNotifications())
	{
		// Create notification string
		RETURN_VOID_IF_ERROR(count_string.AppendFormat(UNI_L("%u"), count));
		RETURN_VOID_IF_ERROR(I18n::Format(notification,
		                                  Str::S_NEW_MESSAGES_RECEIVED,
		                                  count_string,
		                                  account->GetAccountName()));

		// Do notification
		OpInputAction* notification_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_READ_NEWSFEED));
		if (!notification_action)
			return;
		notification_action->SetActionData(IndexTypes::FIRST_ACCOUNT + account->GetAccountId());

		ShowNotification(DesktopNotifier::MAIL_NOTIFICATION,
		                 notification,
		                 account->GetIcon(FALSE),
		                 notification_action);

		// Add buttons
		if (m_notifiers.GetCount() == 0)
			return;

		notifier = m_notifiers.Get(m_notifiers.GetCount() - 1);

		// Loop through recent messages
		for (unsigned i = 0; i < min(count, Index::RECENT_MESSAGES); i++)
		{
			Message  message;
			Header::HeaderValue subject;
			OpString button;

			// Get message
			RETURN_VOID_IF_ERROR(g_m2_engine->GetStore()->GetMessageMinimalHeaders(message, account_index->GetNewMessageByIndex(i)));

			// Get message subject and from
			RETURN_VOID_IF_ERROR(message.GetHeaderValue(Header::SUBJECT, subject));
			if(subject.IsEmpty())
			{
				OpString subjectReplacement;
				g_languageManager->GetString(Str::S_NO_SUBJECT, subjectReplacement);
				RETURN_VOID_IF_ERROR(button.AppendFormat(UNI_L("%s - %s"), subjectReplacement.CStr(), message.GetNiceFrom()));
			}
			else
				RETURN_VOID_IF_ERROR(button.AppendFormat(UNI_L("%s - %s"), subject.CStr(), message.GetNiceFrom()));

			// Add the buttons
			OpAutoPtr<OpInputAction> click_action(OP_NEW(OpInputAction, (OpInputAction::ACTION_GOTO_MESSAGE)));
			if (!click_action.get())
				return;
			click_action->SetActionData(message.GetId());

			RETURN_VOID_IF_ERROR(notifier->AddButton(button, click_action.get()));
			click_action.release();
		}

		if (count > Index::RECENT_MESSAGES)
		{
			OpAutoPtr<OpInputAction> more_action(OP_NEW(OpInputAction, (OpInputAction::ACTION_READ_NEWSFEED)));
			if (!more_action.get())
				return;
			more_action->SetActionData(IndexTypes::FIRST_ACCOUNT + account->GetAccountId());

			OpString more;
			g_languageManager->GetString(Str::S_MAIL_MORE, more);

			RETURN_VOID_IF_ERROR(notifier->AddButton(more.CStr(), more_action.get()));
			more_action.release();
		}
	}
	else
	{
		for (unsigned i = 0; i < min(count, Index::RECENT_MESSAGES); ++i)
		{
			Message message;
			Header::HeaderValue subject;

			RETURN_VOID_IF_ERROR(g_m2_engine->GetStore()->GetMessageMinimalHeaders(message, account_index->GetNewMessageByIndex(i)));

			// Get message subject and from
			RETURN_VOID_IF_ERROR(message.GetHeaderValue(Header::SUBJECT, subject));

			if(subject.IsEmpty())
				RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_NO_SUBJECT, notification));
			else
				RETURN_VOID_IF_ERROR(notification.Set(subject.CStr()));

			OpStringC from(message.GetNiceFrom());

			OpSkinElement* skin_element = g_skin_manager->GetSkinElement(account->GetIcon(FALSE));
			Image img;
			if (skin_element)
				img = skin_element->GetImage(0);

			time_t receive_time;
			Header* header = message.GetHeader(Header::DATE);
			header->GetValue(receive_time);

			OpInputAction* notification_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_GOTO_MESSAGE));
			if (!notification_action)
				return;
			notification_action->SetActionData(message.GetId());

			ShowNotification(DesktopNotifier::MAIL_NOTIFICATION,
							 from,
							 notification,
							 img,
							 notification_action,
							 NULL,
							 false,
							 receive_time);
		}
	}
}


/***********************************************************************************
 ** Show a notification that no new messages were received
 **
 ** NotificationManager::NeedNoMessagesNotification
 ** @param account Account for which no messages were received
 **
 ***********************************************************************************/
void NotificationManager::NeedNoMessagesNotification(Account* account)
{
	if (!account || !g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationForNoNewMessages))
		return;

	OpString    notification;

	// Create notification string
	RETURN_VOID_IF_ERROR(StringUtils::GetFormattedLanguageString(notification, Str::S_NO_NEW_MESSAGES_RECEIVED, account->GetAccountName().CStr()));

	// Do notification
	OpInputAction* notification_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_READ_NEWSFEED));
	if (!notification_action)
		return;
	notification_action->SetActionData(IndexTypes::FIRST_ACCOUNT + account->GetAccountId());

	ShowNotification(DesktopNotifier::MAIL_NOTIFICATION,
						notification,
						account->GetIcon(FALSE),
						notification_action);
}


/***********************************************************************************
 ** Play a sound
 **
 ** NotificationManager::NeedSoundNotification
 ** @param sound_path Sound to play
 **
 ***********************************************************************************/
void NotificationManager::NeedSoundNotification(const OpStringC & sound_path)
{
	// Audio notification
	OpStatus::Ignore(SoundUtils::SoundIt(sound_path, TRUE));
}

#endif // M2_SUPPORT
