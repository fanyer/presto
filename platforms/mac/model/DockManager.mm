// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//
#include "core/pch.h"

#include "platforms/mac/model/DockManager.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/util/CocoaPlatformUtils.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"

#include "adjunct/quick/Application.h"

#ifdef WEBSERVER_SUPPORT
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#endif

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"

#define BOOL NSBOOL
#import <AppKit/NSApplication.h>
#import <AppKit/NSImageView.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSGraphicsContext.h>
#import <Foundation/NSString.h>
#undef BOOL

#define kBadgeIconSize 	128	// 128x128 pixels
#define kBadgeHeight	50

// This is a yuk hack, since code has some strangeness witn OP_RGB that has the top bit of the alpha
// set to 0. This breaks mac code so just hardcoding it like this without the macro works great :)
#define kBadgeBlue		0xFFFF914B // OP_RGB(0x4B, 0x91, 0xFF)
#define kBadgeOrange	0xFF1180FE // OP_RGB(0xFE, 0x80, 0x11)
#define kBadgeUnite		0xFFCA672A // OP_RGB(0xFE, 0x80, 0x11)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DockManager &DockManager::GetInstance()
{
	static DockManager s_dock_manager;
	return s_dock_manager;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DockManager::DockManager() :
	m_inited(FALSE),
	m_unread_mails(0),
	m_unread_chats(0)
#ifdef WEBSERVER_SUPPORT
   ,m_show_unite_notification(0)
#endif
{

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DockManager::~DockManager()
{
	if (MessageEngine::GetInstance())
		MessageEngine::GetInstance()->RemoveEngineListener(this);
#ifdef IRC_SUPPORT
	if (MessageEngine::GetInstance())
		MessageEngine::GetInstance()->RemoveChatListener(this);
#endif // IRC_SUPPORT
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DockManager::Init()
{
	m_inited = TRUE;

	if (g_application->ShowM2())
	{
		MessageEngine::GetInstance()->AddEngineListener(this);
#ifdef IRC_SUPPORT
		MessageEngine::GetInstance()->AddChatListener(this);
#endif // IRC_SUPPORT

		// Set the mail count before initing
		OnIndexChanged(0);
	}

#ifdef WEBSERVER_SUPPORT
	if (g_hotlist_manager)
	{
		HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();
		if(model)
		{
			model->AddListener(this);
		}
	}
#endif

	return OpStatus::OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void DockManager::CleanUp()
{
	// Add anything that needs to be done early in the shutdown
#ifdef WEBSERVER_SUPPORT
	HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();
	if(model)
	{
		model->RemoveListener(this);
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void DockManager::BounceDockIcon()
{
	// Bounce the dock icon
/*  Solution used here is fairly old and very dangerous. I'm leaving the code
	for time being, but we should use proper Cocoa solution
 
	NMRecPtr notificationRequest = (NMRecPtr) NewPtr(sizeof(NMRec));
	memset(notificationRequest, 0, sizeof(*notificationRequest));
	notificationRequest->qType = nmType;
	notificationRequest->nmMark = 1;
	notificationRequest->nmIcon = 0;
	notificationRequest->nmSound = 0;
	notificationRequest->nmStr = NULL;
	notificationRequest->nmResp = NULL;
	NMInstall(notificationRequest); 
 */
	CocoaPlatformUtils::BounceDockIcon();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void DockManager::OnIndexChanged(UINT32 index_id)
{
	if (index_id != IndexTypes::UNREAD_UI)
		return;

	// Grab the current count
	int new_unread_mails = g_m2_engine ? g_m2_engine->GetUnreadCount() : 0;

	// Don't waste time if the the number of unread mails hasn't changed
	if (m_unread_mails == new_unread_mails)
		return;

	// Set the new number of unread mails
	m_unread_mails = new_unread_mails;

	// Update the badges
	UpdateBadges();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef IRC_SUPPORT
void DockManager::OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread)
{
	// Don't waste time if the the number of unread chats hasn't changed
	// Removed: This will cause blinking, but should be fixed in combination with the
	// mail badge so it's all smoother
//	if (m_unread_chats == (int)unread)
//		return OpStatus::OK;

	// Set the new number of unread mails
	m_unread_chats = unread;

	// Update the badges
	UpdateBadges();
}
#endif // IRC_SUPPORT
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WEBSERVER_SUPPORT
void DockManager::OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort)
{
	OpTreeModelItem* it = tree_model->GetItemByPosition(pos);
	if(it && it->GetType() == OpTypedObject::UNITE_SERVICE_TYPE)
	{
		UniteServiceModelItem* item = static_cast<UniteServiceModelItem*>(it);
		if(item)
		{
			if(item->GetAttentionStateChanged())
			{
				if(item->GetNeedsAttention())
				{
					m_show_unite_notification++;
				}
				else
				{
					m_show_unite_notification--;
				}
				UpdateBadges();
			}
		}
	}	
}
#endif

//////////////////////////////////////////////////////////////////////
//	Draws all badges for the dock icon (mail & chat)
//
//	This function will reset the icon then draw the badges everytime
//	since you can only get in here if there has been a change
//
OP_STATUS DockManager::UpdateBadges()
{
	// First restore the dock icon to it's former glory
	[[NSApplication sharedApplication] setApplicationIconImage: nil];
	NSImage *main_icon = [[NSApplication sharedApplication] applicationIconImage];

	// FIXME: VegaOpPainter
	// Make sure that we actually have some mail and/or chat badges to draw
	if (m_unread_chats 
		|| m_unread_mails
#ifdef WEBSERVER_SUPPORT
		|| m_show_unite_notification > 0
#endif
		)
	{
		OpBitmap* bm = NULL;
		if(OpStatus::IsSuccess(OpBitmap::Create(&bm, kBadgeIconSize, kBadgeIconSize, FALSE, TRUE, 0, 0, TRUE)))
		{
			// Clear the bitmap first
			OpRect clear_rect(0,0,bm->Width(),bm->Height());
			((VEGAOpPainter*)bm->GetPainter())->ClearRect(clear_rect);

			uni_char uni_unread[6];

			// Setup the mail text
			uni_unread[0] = 0;
			uni_snprintf(uni_unread, 6, UNI_L("%ld"), m_unread_mails);
			uni_unread[5] = 0;

			// Over 99999, write the word "Lots"
			if (m_unread_mails > 99999)
				uni_strcpy(uni_unread, UNI_L("Lots"));

			// Draw the mail badge
			if (!m_unread_mails || DrawBadge(bm, uni_unread, kBadgeBlue, 0))
			{
				// Setup the chat text
				uni_strcpy(uni_unread, UNI_L("!"));

				// Draw the chat badge
				if (!m_unread_chats || DrawBadge(bm, uni_unread, kBadgeOrange, kBadgeIconSize - kBadgeHeight))
				{
					BOOL draw_icon = TRUE;
#ifdef WEBSERVER_SUPPORT
					draw_icon = FALSE;

					// From 1-9 put the number of services that need attention
					if (m_show_unite_notification > 0 && m_show_unite_notification < 10)
					{
						uni_unread[0] = 0;
						uni_snprintf(uni_unread, 6, UNI_L("%ld"), m_show_unite_notification);
						uni_unread[5] = 0;
					}
					else
					{
						// If there are more than 10 services then put a !
						uni_strcpy(uni_unread, UNI_L("!"));
					}

					// Draw the Unite notification
					if (!m_show_unite_notification || DrawBadge(bm, uni_unread, kBadgeUnite, kBadgeIconSize - kBadgeHeight, FALSE))
						draw_icon = TRUE;
#endif // WEBSERVER_SUPPORT
						
					// Get an NSImage from the OpBitmap
					NSImage *image = (NSImage *)GetImageFromBuffer((UINT32*)bm->GetPointer(OpBitmap::ACCESS_READONLY), bm->Width(), bm->Height());
					
					// Combine the images
					[image lockFocus];
					[main_icon compositeToPoint:NSZeroPoint operation:NSCompositeDestinationOver];
					[image unlockFocus];

					// Set the icon
					[[NSApplication sharedApplication] setApplicationIconImage: image];

					bm->ReleasePointer(FALSE);
				}
			}

			delete bm;
		}
	}

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
//	Draws a single badge with the text and background colour
//	passed in
//
BOOL DockManager::DrawBadge(OpBitmap* bm, uni_char *string, UINT32 colour, int badgeTop, BOOL align_right)
{
	BOOL success = FALSE;
	int  badgeWidth = 50;
	int  badgeWidthInc = 0;
	int  badgeLeft = 0;
	int  textX = 0;
	int  textY = 0;

	OpPainter *painter = bm->GetPainter();

	if (painter)
	{
		OpFontManager* fontManager = styleManager->GetFontManager();

		if (fontManager)
		{
			OpFont* font = fontManager->CreateFont(UNI_L("Helvetica"), 30, 10, FALSE, FALSE, 0);

			if(font)
			{
				painter->SetFont(font);
				painter->SetColor(colour);

				switch(uni_strlen(string))
				{
					case 1:
						badgeWidthInc = 0;
					break;

					case 2:
						badgeWidthInc = 20;
					break;

					case 3:
						badgeWidthInc = 32;
					break;

					case 4:
						badgeWidthInc = 44;
					break;

					case 5:
						badgeWidthInc = 55;
					break;
				}

				badgeWidth += badgeWidthInc;
				if (align_right)
					badgeLeft = kBadgeIconSize - badgeWidth;
				painter->FillEllipse(OpRect(badgeLeft, badgeTop, kBadgeHeight, kBadgeHeight));
				if (badgeWidthInc > 0)
				{
					painter->FillRect(OpRect(badgeLeft + kBadgeHeight/2, badgeTop, badgeWidthInc, kBadgeHeight));
					painter->FillEllipse(OpRect(badgeLeft + badgeWidthInc, badgeTop, kBadgeHeight, kBadgeHeight));
				}

				// white
				painter->SetColor(255,255,255);

				// calculate string placement
				UINT32 stringWidth = font->StringWidth(string, uni_strlen(string));
				textX = badgeLeft + (badgeWidth - stringWidth)/2;
				textY = (kBadgeHeight / 2) - 15 + badgeTop;	// -15 looks nice :)
				painter->DrawString(OpPoint(textX, textY), string, uni_strlen(string));

#ifdef UNIT_TEXT_OUT
				// Flush out the text so it paints
				if (painter->Supports(OpPainter::SUPPORTS_PLATFORM))
					((MacOpPainter *)painter)->FlushText();
#endif // UNIT_TEXT_OUT

				success = true;

				delete font;
			}
		}

		bm->ReleasePainter();
	}

	return success;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

