/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/desktop_notifier.h"
#include "adjunct/quick_toolkit/widgets/OpNotifier.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/notifications/CocoaNotifications.h"
#include "platforms/mac/util/systemcapabilities.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick/managers/DesktopTransferManager.h"
#include "modules/pi/system/OpFolderLister.h"

// Last due to YES/NO type redefining.
#include "platforms/mac/pi/MacOpNotifier.h"
#include "platforms/mac/notifications/Headers/Growl.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#import <Foundation/NSDate.h>
#undef BOOL

#ifdef M2_SUPPORT
#include "platforms/mac/prefs/PrefsCollectionMacOS.h"
#endif

namespace {

BOOL PerformDownloadAction(OpInputAction* action)
{
	OpString location;
	if (action) {
		INTPTR data = action->GetActionData();
		INT32 i, count = g_desktop_transfer_manager->GetItemCount();
		for (i=0; i<count; i++) {
			TransferItemContainer* transfer = static_cast<TransferItemContainer*>(g_desktop_transfer_manager->GetItemByPosition(i));
			if (transfer && transfer->GetID() == data) {
				TransferItem* ti = transfer->GetAssociatedItem();
				if (ti) {
					location.Set(ti->GetStorageFilename()->CStr());
					break;
				}
			}
		}
	}
	if (!location.Length()) {
		OpFolderLister *fl = NULL;
		if (OpStatus::IsSuccess(OpFolderLister::Create(&fl))) {
			OpString tmp_storage;
			if (OpStatus::IsSuccess(fl->Construct(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_DOWNLOAD_FOLDER, tmp_storage), UNI_L("*")))) {
				while (fl->Next()) {
					if (fl->GetFileName()
						&& uni_strcmp(fl->GetFileName(), UNI_L(".")) != 0
						&& uni_strcmp(fl->GetFileName(), UNI_L("..")) != 0) {
						location.Set(fl->GetFullPath());
						break;
					}
				}
			}
		}
	}
	if (location.Length()) {
		NSString* nsstr = [[NSString alloc] initWithCharacters:(const unichar*)location.CStr() length:location.Length()];
		[[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"com.apple.DownloadFileFinished" object:nsstr];
		[nsstr release];
		return YES;
	}
	return NO;
}

class MacSimpleOpNotifier : public OpNotifier
{
	virtual OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& text, const OpStringC8& image, OpInputAction* action, BOOL managed = FALSE, BOOL wrapping = FALSE)
	{
		m_notification_group = notification_group;
		m_transfer_action = action;
		return OpNotifier::Init(notification_group, text, image, action, managed, wrapping);
	}
	virtual OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& title, Image& image, const OpStringC& text, OpInputAction* action, OpInputAction* cancel_action, BOOL managed = FALSE, BOOL wrapping = FALSE)
	{
		m_notification_group = notification_group;
		m_transfer_action = action;
		return OpNotifier::Init(notification_group, title, image, text, action, cancel_action, managed, wrapping);
	}
	virtual BOOL IsManaged() const {
		if (m_notification_group == DesktopNotifier::TRANSFER_COMPLETE_NOTIFICATION)
			return FALSE;
		return OpNotifier::IsManaged();
	}
	virtual OP_STATUS AddButton(const OpStringC& text, OpInputAction* action) 
	{
		if (m_notification_group == DesktopNotifier::TRANSFER_COMPLETE_NOTIFICATION) {
			m_transfer_action = action;
			return OpStatus::OK;
		}
		return OpNotifier::AddButton(text, action);
	}
	virtual void StartShow() {
		if (m_notification_group == DesktopNotifier::TRANSFER_COMPLETE_NOTIFICATION) {
			if (PerformDownloadAction(m_transfer_action))
				return;
		}
		OpNotifier::StartShow();
	}
	DesktopNotifier::DesktopNotifierType m_notification_group;
	OpInputAction* m_transfer_action;
};

} // namespace
	
OP_STATUS DesktopNotifier::Create(DesktopNotifier** new_notifier)
{
	// Create the right type of notifier if growl is installed
	static bool growl_init = false;
	if (!growl_init) {
		InitNotifications();
		growl_init = true;
	}
	if (GetOSVersion() < 0x1060)
		*new_notifier =  OP_NEW(MacSimpleOpNotifier,());
	else
		*new_notifier = OP_NEW(MacOpNotifier, ());

	return *new_notifier ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#ifdef M2_SUPPORT
bool DesktopNotifier::UsesCombinedMailAndFeedNotifications()
{
	return GetOSVersion() < 0x1080 || g_pcmacos->GetIntegerPref(PrefsCollectionMacOS::CombinedMailAndFeedNotifications);
}
#endif // M2_SUPPORT

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Notification names. This must match the DesktopNotifierType and list in the 
// Growl Registration Ticket.growlRegDict file.
//

// Growl notification names.
NSString * const kGrowlNotificationNewMail				= @"New Mail";
NSString * const kGrowlNotificationPopupBlocked			= @"Popup Blocked";
NSString * const kGrowlNotificationTransferComplete		= @"Transfer Complete";
NSString * const kGrowlNotificationWidgetNotifcation	= @"Widget Notifcation";
NSString * const kGrowlNotificationNetworkSpeed			= @"Network Speed";
NSString * const kGrowlNotificationUpdate				= @"Update";
NSString * const kGrowlNotificationExtensions			= @"Extensions";

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

MacOpNotifier::MacOpNotifier() :
	m_action(NULL),
	m_image(NULL),
	m_listener(NULL),
	m_timestamp(0),
	m_timer(NULL)
{
	g_notification_manager->AddNotifier(this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

MacOpNotifier::~MacOpNotifier()
{
	if (m_timer)
	{
		m_timer->Stop();
		OP_DELETE(m_timer);
	}
	g_notification_manager->RemoveNotifier(this);
	
	if (m_listener)
	{
		m_listener->OnNotifierDeleted(this);
	}

	OP_DELETE(m_action);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpNotifier::Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& text, 
							const OpStringC8& image, OpInputAction* action,
							BOOL managed, BOOL wrapping)
{
	m_notification_group = notification_group;
	RETURN_IF_ERROR(m_text.Set(text));
	m_action = action;

	if (!managed)
		StartShow();

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpNotifier::Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& title, 
							Image& image, const OpStringC& text, OpInputAction* action, OpInputAction* cancel_action, 
							BOOL managed, BOOL wrapping)
{
	m_notification_group = notification_group;
	
	// Super hack, because Mac is least important platform...
	if (notification_group == DesktopNotifier::EXTENSION_NOTIFICATION)
	{
		m_text.Empty();
		RETURN_IF_ERROR(m_text.AppendFormat(UNI_L("%s\n\n%s"), title.CStr(), text.CStr()));
	}
	else
		RETURN_IF_ERROR(m_text.Set(text));
	m_action = action;
	m_image = GetNSImageFromImage(&image);
	m_title.Set(title);

	if (!managed)
		StartShow();

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS MacOpNotifier::AddButton(const OpStringC& text, OpInputAction* action) 
{ 
	// For Growl we can't add a button so we are going to throw away the text and swap
	// the action to be the button's action
	OP_DELETE(m_action);
	m_action = action;
	
	return OpStatus::OK; 
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacOpNotifier::StartShow() 
{
	OpString title;
	NSString *name;

	if (SetNotificationTitleAndName(title, name))
	{
	
		m_notifier_id = (NSInteger)[[OperaNotifierDelegate getNotifierDelegate] setNotifier:this];

		NSData* data = [NSData dataWithBytes:&m_notifier_id length: sizeof(NSInteger)];
    	NSString* ns_title = [NSString stringWithCharacters:(const unichar *)title.CStr() length:title.Length()];
	    NSString* ns_desc = [NSString stringWithCharacters:(const unichar *)m_text.CStr() length:m_text.Length()];

    	if (GetOSVersion() >= 0x1080)
		{
			if (m_notification_group == DesktopNotifier::BLOCKED_POPUP_NOTIFICATION ||
				m_notification_group == DesktopNotifier::NETWORK_SPEED_NOTIFICATION)
			{
				m_timer = OP_NEW(OpTimer, ());
				// we continue to show notification even if timeout wont be reached
				if (m_timer) {
					m_timer->SetTimerListener(this);
					m_timer->Start(60000);
				}
			}
	        NSDictionary* user_data = [NSDictionary dictionaryWithObject:data forKey:KEY_NOTIFIER_ID];

	        NSUserNotificationCenter* center = [NSClassFromString(@"NSUserNotificationCenter") defaultUserNotificationCenter];
    	    id<OpUserNotification> notification = [[NSClassFromString(@"NSUserNotification") alloc] init];
        	notification.title = ns_title;
	        notification.informativeText = ns_desc;
    	    notification.userInfo = user_data;
			if (m_timestamp)
				notification.deliveryDate = [NSDate dateWithTimeIntervalSince1970:(double)m_timestamp];
	        [center deliverNotification:notification];
    	}
		else
		{
        	[GrowlApplicationBridge notifyWithTitle:ns_title
            	                        description:ns_desc
                	               notificationName:name
										   iconData:(m_image ? [NSData dataWithData:[(NSImage*)m_image TIFFRepresentation]] : nil)
										   priority:0
                                       	   isSticky:FALSE
                                   	   clickContext:data];
    	}
	}
	else
		fprintf(stderr, "Failed to set notifiaction title and name, notification won't be shown\n");
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MacOpNotifier::OnTimeOut(OpTimer* timer)
{
	NSUserNotificationCenter* center = [NSClassFromString(@"NSUserNotificationCenter") defaultUserNotificationCenter];
	NSArray* delivered_notifications =[center deliveredNotifications];
	for (unsigned i = 0; i < [delivered_notifications count]; ++i) {
		id<OpUserNotification> notification = [delivered_notifications objectAtIndex: i];
		NSDictionary* dict = notification.userInfo;
		int notifier_id = *(int*)[(NSData*)[dict objectForKey:KEY_NOTIFIER_ID] bytes];
		if (notifier_id == m_notifier_id) {
			[center removeDeliveredNotification: notification];
			[[OperaNotifierDelegate getNotifierDelegate] deleteNotifier:m_notifier_id];
			return;
		}
	}
}

void MacOpNotifier::InvokeAction()
{
	if (m_action)
		g_input_manager->InvokeAction(m_action);
}

bool MacOpNotifier::SetNotificationTitleAndName(OpString& title, NSString*& name) {

#define RETURN_FALSE_IF_ERROR(x) RETURN_VALUE_IF_ERROR(x, false)

	// Set the name and load the title based on the notification type
	switch (m_notification_group)
	{
		case DesktopNotifier::MAIL_NOTIFICATION:
			if (!m_title.IsEmpty())
				title.Set(m_title.CStr());
			else
				RETURN_FALSE_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_NOTIFICATION, title));
			name = kGrowlNotificationNewMail;
		break;
		
		case DesktopNotifier::BLOCKED_POPUP_NOTIFICATION:
			RETURN_FALSE_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_POPUP_NOTIFICATION, title));
			name = kGrowlNotificationPopupBlocked;
		break;
		
		case DesktopNotifier::TRANSFER_COMPLETE_NOTIFICATION:
			PerformDownloadAction(m_action);
			RETURN_FALSE_IF_ERROR(g_languageManager->GetString(Str::S_TRANSFER_COMPLETE_NOTIFICATION, title));
			name = kGrowlNotificationTransferComplete;
		break;
		
		case DesktopNotifier::WIDGET_NOTIFICATION:
			RETURN_FALSE_IF_ERROR(g_languageManager->GetString(Str::S_WIDGET_NOTIFICATION, title));
			name = kGrowlNotificationWidgetNotifcation;
		break;
		
		case DesktopNotifier::NETWORK_SPEED_NOTIFICATION:
		RETURN_FALSE_IF_ERROR(g_languageManager->GetString(Str::S_NETWORK_SPEED_NOTIFICATION, title));
		name = kGrowlNotificationNetworkSpeed;
		break;
		
		case DesktopNotifier::AUTOUPDATE_NOTIFICATION:
			RETURN_FALSE_IF_ERROR(g_languageManager->GetString(Str::D_UNITE_UPDATE_NOTIFICATION_TITILE, title));
			name = kGrowlNotificationUpdate;
		break;
		
		case DesktopNotifier::AUTOUPDATE_NOTIFICATION_EXTENSIONS:
			RETURN_FALSE_IF_ERROR(g_languageManager->GetString(Str::S_EXTENSIONS_NOTIFICATION, title));
			name = kGrowlNotificationUpdate;
		break;
		
		case DesktopNotifier::EXTENSION_NOTIFICATION:
			RETURN_FALSE_IF_ERROR(g_languageManager->GetString(Str::S_EXTENSIONS_NOTIFICATION, title));
			name = kGrowlNotificationExtensions;
		break;
		
		default:
			OP_ASSERT(!"Missing type you need to add");
		return false;
	}
	return true;

#undef RETURN_FALSE_IF_ERROR

}
