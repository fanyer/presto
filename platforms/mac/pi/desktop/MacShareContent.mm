/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Bartek Przybylski (bprzybylski)
 */

#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/usagereport/UsageReport.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"

#include "modules/util/opstring.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkin.h"

#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/quick_support/OperaNSMenuItem.h"

#define BOOL NSBOOL
#import <Foundation/Foundation.h>
#import <Foundation/NSString.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#undef BOOL

#if !defined(MAC_OS_X_VERSION_10_8) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8
@protocol NSSharingServiceDelegate
@end

NSString * const NSSharingServiceNamePostOnTwitter = @"com.apple.share.Twitter.post";
NSString * const NSSharingServiceNamePostOnFacebook = @"com.apple.share.Facebook.post";
NSString * const NSSharingServiceNamePostOnSinaWeibo = @"com.apple.share.SinaWeibo.post";
NSString * const NSSharingServiceNameComposeEmail = @"com.apple.share.Mail.compose";
NSString * const NSSharingServiceNameComposeMessage = @"com.apple.share.Messages.compose";
extern NSString * const NSSharingServiceNameSendViaAirDrop;
extern NSString * const NSSharingServiceNameAddToSafariReadingList;
extern NSString * const NSSharingServiceNameAddToIPhoto;
extern NSString * const NSSharingServiceNameAddToAperture;
extern NSString * const NSSharingServiceNameUseAsTwitterProfileImage;
extern NSString * const NSSharingServiceNameUseAsDesktopPicture;
extern NSString * const NSSharingServiceNamePostImageOnFlickr;
extern NSString * const NSSharingServiceNamePostVideoOnVimeo;
extern NSString * const NSSharingServiceNamePostVideoOnYouku;
extern NSString * const NSSharingServiceNamePostVideoOnTudou;

enum {
	NSSharingContentScopeItem,
	NSSharingContentScopePartial,
	NSSharingContentScopeFull
};
typedef NSInteger NSSharingContentScope;

#endif  // MAC_OS_X_VERSION_10_8

@class NSSharingService;

@protocol OpSharingService<NSObject>
+ (NSSharingService*) sharingServiceNamed:(NSString*)serviceName;
- (id)initWithTitle:(NSString *)title image:(NSImage *)image alternateImage:(NSImage *)alternateImage handler:(void (^)(void))block;
+ (NSArray *)sharingServicesForItems:(NSArray *)items;
- (BOOL)canPerformWithItems:(NSArray *)items;
- (void)performWithItems:(NSArray *)items;
@property(readonly, retain) NSImage *alternateImage;
@property(assign) id<NSSharingServiceDelegate> delegate;
@property(readonly, retain) NSImage *image;
@property(readonly, copy) NSString *title;
@end

namespace {

	enum sharersTypes {
		TYPE_TWITTER,
		TYPE_EMAIL,
		TYPE_FACEBOOK,
		TYPE_CHAT,
		TYPE_SINA_WEIBO
	};

	bool IsDocumentBookmarked() {
		DocumentDesktopWindow* doc = g_application->GetActiveDocumentDesktopWindow();
		if (doc) {
			const uni_char* doc_url = doc->GetWindowCommander()->GetCurrentURL(TRUE);
			if (!doc_url) return false;

			return g_hotlist_manager->GetBookmarksModel()->GetByURL(doc_url, false, true);
		}
		return false;
	}

	bool IsPresentOnSpeeddial() {
		DocumentDesktopWindow* doc = g_application->GetActiveDocumentDesktopWindow();
		if (doc) {

	        const uni_char *doc_url = doc->GetWindowCommander()->GetCurrentURL(TRUE);
	        if (!doc_url)
				return false;

	        return g_speeddial_manager->FindSpeedDialByUrl(doc_url, true) != -1;
		}
		return false;
	}

	NSMenuItem* CommonCreateItem(OpInputAction* action, Str::LocaleString locale_string_id) {
		NSMenuItem* ret = [[OperaNSMenuItem alloc] initWithAction: action];

		Image skin_image;
		OpSkinElement* skin_element = g_skin_manager->GetSkinElement(action->GetActionImage());

		if (skin_element)
		{
			skin_image = skin_element->GetImage(0);
			[ret setImage: (NSImage*)GetNSImageFromImage(&skin_image)];
		}
		OpString locale_str;
		g_languageManager->GetString(locale_string_id, locale_str);
		[ret setTitle: [NSString stringWithCharacters:(const unichar*)locale_str.CStr() length: locale_str.Length()]];

		[ret setAction:@selector(menuClicked:)];

		return [ret autorelease];
	}

	NSMenuItem* CreateSpeedialTriggerMenuItem() {
		OpInputAction* action;
		Str::LocaleString locale_str_id = Str::S_URL_ADD_TO_STARTPAGE;

		if (IsPresentOnSpeeddial()) {
			RETURN_VALUE_IF_ERROR(OpInputAction::CreateInputActionsFromString("Remove from Start Page", action), NULL);
			locale_str_id = Str::S_URL_REMOVE_FROM_STARTPAGE;
		} else {
			RETURN_VALUE_IF_ERROR(OpInputAction::CreateInputActionsFromString("Add to Start Page", action), NULL);
		}
		action->SetActionData((UINTPTR)g_application->GetActiveDocumentDesktopWindow());
		action->SetActionImage("Trigger Start Page");

		return CommonCreateItem(action, locale_str_id);
	}

	NSMenuItem* CreateBookmarksTriggerMenuItem() {
		OpInputAction* action;
		Str::LocaleString locale_str_id = Str::S_URL_ADD_TO_FAVORITES;;

		if (IsDocumentBookmarked()) {
			RETURN_VALUE_IF_ERROR(OpInputAction::CreateInputActionsFromString("Remove from Favorites", action), NULL);
			locale_str_id = Str::S_URL_REMOVE_FROM_FAVORITES;
		} else {
			RETURN_VALUE_IF_ERROR(OpInputAction::CreateInputActionsFromString("Add to Favorites", action), NULL);
		}
		action->SetActionData((UINTPTR)g_application->GetActiveDocumentDesktopWindow());
		action->SetActionImage("Trigger Bookmark");

		return CommonCreateItem(action, locale_str_id);
	}

	struct RegionSharers {
		NSMutableArray* array;
		sharersTypes* sharers_types;

		~RegionSharers() {
			OP_DELETEA(sharers_types);
			[array release];
		}
	};

}  // namepace

@interface SharingContentDelegate : NSObject<NSSharingServiceDelegate> {
	RegionSharers* sharers_;
}

// NSObject methods

- (void) dealloc;
- (id) init;

// NSSharingServiceDelegate protocol methods

- (NSWindow *)sharingService:(NSSharingService *)sharingService sourceWindowForShareItems:(NSArray *)items sharingContentScope:(NSSharingContentScope *)sharingContentScope;
- (NSRect)sharingService:(NSSharingService *)sharingService sourceFrameOnScreenForShareItem:(id <NSPasteboardWriting>)item;
- (NSImage *)sharingService:(NSSharingService *)sharingService transitionImageForShareItem:(id < NSPasteboardWriting >)item contentRect:(NSRect *)contentRect;

- (void)sharingService:(NSSharingService *)sharingService willShareItems:(NSArray *)items;
- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items;
- (void)sharingService:(NSSharingService *)sharingService didFailToShareItems:(NSArray *)items error:(NSError *)error;


// SharingContentDelegate own methods

- (void)shareUsingSharer:(NSString*) sharerName;

// Return elements which should be shared via sharing services.
// Returned array contains 1 or two elements.
// One of elements is selected text (if available), given as NSString.
// Second one is URL of current website, given as NSURL.
- (NSArray*) getURLAndSelectedText;

// Selector for handling clicking on sharing menu
// it handles bookmarks and speedial adding/removing
// and also triggering sharing services on system level.
- (void) shareClicked:(id) sender;

// Fill menu with bookmarks and speedial menu
// and with sharing services available on system.
- (bool) fillMenuWithSharers:(NSMenu*) menu;

// Returns sharers depended on country stored by
// region info in opera:prefs. But ownership is not
// transfered to caller.
- (RegionSharers*) regionDependentSharers;

+ (SharingContentDelegate*) getInstance;

@end

@implementation SharingContentDelegate

- (void) dealloc {
	OP_DELETE(sharers_);
	[super dealloc];
}

- (id) init {
	[super init];
	sharers_ = NULL;
	return self;
}

- (RegionSharers*) regionDependentSharers {
	if (sharers_) return sharers_;

	// We can fill this struct only once, becasue region location
	// might by changed only after app restart
	sharers_ = OP_NEW(RegionSharers,());
	if (!sharers_) return NULL;

	sharers_->array = [NSMutableArray arrayWithCapacity: 5];
	sharers_->sharers_types = OP_NEWA(sharersTypes, 5);
	sharersTypes* types = sharers_->sharers_types;

	// if allocation won't work give it a try next time
	if (!sharers_->array || !sharers_->sharers_types) {
		if (sharers_->array)
			[sharers_->array release];
		else
			OP_DELETEA(sharers_->sharers_types);
		OP_DELETE(sharers_);
		sharers_ = NULL;
		return NULL;
	}

	// email and ichat are two default elements
	[sharers_->array addObject: NSSharingServiceNameComposeEmail];
	*types++ = TYPE_EMAIL;
	[sharers_->array addObject: NSSharingServiceNameComposeMessage];
	*types++ = TYPE_CHAT;

	if (g_region_info->m_country.CompareI("cn") == 0) {
		[sharers_->array addObject: NSSharingServiceNamePostOnSinaWeibo];
		*types++ = TYPE_SINA_WEIBO;
	} else if (g_region_info->m_country.CompareI("undefined") == 0) {
		// dont include anything in this case, because we cannot be sure
		// about county "outside world" connectivity
	} else {
		[sharers_->array addObject: NSSharingServiceNamePostOnFacebook];
		*types++ = TYPE_FACEBOOK;
		[sharers_->array addObject: NSSharingServiceNamePostOnTwitter];
		*types++ = TYPE_TWITTER;
	}
	[sharers_->array retain];

	return sharers_;
}

- (void)sharingService:(NSSharingService *)sharingService willShareItems:(NSArray *)items {}

- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items {}

- (void)sharingService:(NSSharingService *)sharingService didFailToShareItems:(NSArray *)items error:(NSError *)error {}

- (NSWindow *)sharingService:(NSSharingService *)sharingService sourceWindowForShareItems:(NSArray *)items sharingContentScope:(NSSharingContentScope *)sharingContentScope {
	return [NSApp keyWindow];
}

+ (SharingContentDelegate*) getInstance {
	static SharingContentDelegate* instance = [[SharingContentDelegate alloc] init];
	return instance;
}

- (NSImage *)sharingService:(NSSharingService *)sharingService transitionImageForShareItem:(id < NSPasteboardWriting >)item contentRect:(NSRect *)contentRect
{
	if ([item isKindOfClass:[NSImage class]]) {
		return (NSImage*) item;
	} else if ([item isKindOfClass:[NSURL class]]) {
		BrowserDesktopWindow* dw = (BrowserDesktopWindow*)g_application->GetActiveBrowserDesktopWindow();
		if (dw) {
			Image img = dw->GetActiveDesktopWindow()->GetThumbnailImage();
			return [(NSImage*)GetNSImageFromImage(&img) autorelease];
		}
	}
	return nil;
}

- (NSRect)sharingService:(NSSharingService *)sharingService sourceFrameOnScreenForShareItem:(id <NSPasteboardWriting>)item {
	return NSZeroRect;
}

- (NSArray*) getURLAndSelectedText {
	BrowserDesktopWindow* dw = (BrowserDesktopWindow*)g_application->GetActiveBrowserDesktopWindow();

	NSMutableArray * shareItems = [[NSMutableArray alloc] init];
	if (dw && dw->GetActiveDocumentDesktopWindow() && dw->GetActiveDocumentDesktopWindow()->GetWindowCommander()) {
		OpWindowCommander* commander = dw->GetActiveDocumentDesktopWindow()->GetWindowCommander();

		if (commander->HasSelectedText()) {
			NSString* selected_text = [NSString stringWithCharacters:(const unichar*) commander->GetSelectedText() length: uni_strlen(commander->GetSelectedText())];
			[shareItems addObject:[NSString stringWithFormat:@"\"%@\"", selected_text]];
		}
		NSString* str = [NSString stringWithCharacters:(const unichar*) commander->GetCurrentURL(FALSE) length: uni_strlen(commander->GetCurrentURL(FALSE))];
		NSString* str2 = [str stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
		NSURL* url = [NSURL URLWithString:str2];
		[shareItems addObject: url];
	}
	return [shareItems autorelease];
}

- (void)shareUsingSharer:(NSString *)sharerName {
	id<OpSharingService> service = [NSClassFromString(@"NSSharingService") performSelector:@selector(sharingServiceNamed:) withObject: sharerName];

	service.delegate = self;
	[service performWithItems:[self getURLAndSelectedText]];
}

- (void) shareClicked:(id) sender {
	NSMenuItem* item = (NSMenuItem*)sender;
	NSString* sharer_name = nil;
	
	switch ([item tag])
	{
		case TYPE_TWITTER:
			sharer_name = NSSharingServiceNamePostOnTwitter;
			if (g_usage_report_manager && g_usage_report_manager->GetContentSharingReport())
				g_usage_report_manager->GetContentSharingReport()->ReportSharedOnTwitter();
			break;
		case TYPE_CHAT:
			sharer_name = NSSharingServiceNameComposeMessage;
			if (g_usage_report_manager && g_usage_report_manager->GetContentSharingReport())
				g_usage_report_manager->GetContentSharingReport()->ReportSharedOnChat();
			break;
		case TYPE_EMAIL:
			sharer_name = NSSharingServiceNameComposeEmail;
			if (g_usage_report_manager && g_usage_report_manager->GetContentSharingReport())
				g_usage_report_manager->GetContentSharingReport()->ReportSharedViaEmail();
			break;
		case TYPE_FACEBOOK:
			sharer_name = NSSharingServiceNamePostOnFacebook;
			if (g_usage_report_manager && g_usage_report_manager->GetContentSharingReport())
				g_usage_report_manager->GetContentSharingReport()->ReportSharedOnFacebook();
			break;
		case TYPE_SINA_WEIBO:
			sharer_name = NSSharingServiceNamePostOnSinaWeibo;
			if (g_usage_report_manager && g_usage_report_manager->GetContentSharingReport())
				g_usage_report_manager->GetContentSharingReport()->ReportSharedOnShinaWeibo();
			break;
	}
	if (sharer_name != nil)
		[self shareUsingSharer:sharer_name];
}

- (bool) fillMenuWithSharers:(NSMenu*) menu {
	if (menu) {
		// filling entire menu programmatically is not most elegant solution
		// but .ini files for menu failed to switch input action to next input action

		bool draw_separator = false;

		NSMenuItem* item = CreateBookmarksTriggerMenuItem();
		if (item) {
			[item setTarget:[menu delegate]];
			[menu addItem: item];
			draw_separator = true;
		}

		item = CreateSpeedialTriggerMenuItem();
		if (item) {
			[item setTarget:[menu delegate]];
			[menu addItem: item];
			draw_separator = true;
		}

		if (draw_separator)
			[menu addItem:[NSMenuItem separatorItem]];

		RegionSharers* sharers = [self regionDependentSharers];
		if (sharers) {
			for (UINT i = 0; i < [sharers->array count]; ++i) {
				NSString* sharerid = [sharers->array objectAtIndex:i];
				NSSharingService* sharer = [NSClassFromString(@"NSSharingService") 	performSelector:@selector(sharingServiceNamed:) withObject: sharerid];
				if (sharer && [sharer canPerformWithItems:nil]) {
					item = [[[OperaNSMenuItem alloc] initWithAction:NULL] autorelease];
					[item setTitle: [sharer title]];
					[item setAction:@selector(shareClicked:)];
					[item setImage:[sharer image]];
					[item setTarget:self];
					[item setTag:sharers->sharers_types[i]];
					[menu addItem:item];
				}
			}
		}
	}
	return OpStatus::OK;
}

@end
