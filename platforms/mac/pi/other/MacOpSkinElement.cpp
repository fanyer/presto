/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
** File : MacOpSkinElement.cpp
**
**
*/

#include "core/pch.h"

#ifdef _MACINTOSH_
#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick_toolkit/widgets/OpTabs.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "platforms/mac/subclasses/MacWidgetPainter.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/pi/other/MacOpSkinElement.h"
#include "platforms/mac/pi/MacVEGAPrinterListener.h"
#include "modules/skin/OpSkin.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/MacIcons.h"
#include "platforms/mac/pi/MacOpView.h"
#include "modules/display/vis_dev.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/ui/OpUiInfo.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/libvega/vegarendertarget.h"
#include "modules/libvega/vegawindow.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"
#include "platforms/mac/CocoaVegaDefines.h"

#define RGB2COLORREF(color) (OP_RGB((color.red & 0xFF00) >> 8, (color.green & 0xFF00) >> 8, (color.blue & 0xFF00) >> 8))

// OBS OBS OBS: This skin name array must be kept in sync with their enumeration in NativeType (see MacOpSkinElement.h)
const char* s_mac_native_types[] =
{
	"Push Button Skin",
	"Push Default Button Skin",
	"Toolbar Button Skin",
	"Selector Button Skin",
	"Menu Button Skin",
	"Header Button Skin",
	"Header Button Sort Ascending Skin",
	"Header Button Sort Descending Skin",
	"Link Button Skin",
	"Tab Button Skin",
	"Pagebar Button Skin",
	"Caption Minimize Button Skin",

	"Caption Close Button Skin",
	"Caption Restore Button Skin",
	"Checkbox Skin",
	"Radio Button Skin",
	"Dropdown Skin",
	"Dropdown Button Skin",
	"Dropdown Left Button Skin",
	"Dropdown Search Skin",
	"Zoom Dropdown Skin",	
	"Edit Skin",
	"MultilineEdit Skin",
	"Browser Skin",
	"Progressbar Skin",

	"Treeview Skin",
	"Panel Treeview Skin",
	"Listbox Skin",
	"Listitem Skin",
	"Checkmark",
	"Bullet",
	"Window Skin",
	"Browser Window Skin",
	"Dialog Skin",
	"Dialog Tab Page Skin",
	"Tabs Skin",
	"Hotlist Skin",
	"Hotlist Selector Skin",
	"Hotlist Splitter Skin",
	"Hotlist Panel Header Skin",
	"Status Skin",
	"Mainbar Skin",

	"Personalbar Skin",
	"Pagebar Skin",
	"Addressbar Skin",
	"Navigationbar Skin",
	"Mailbar Skin",
	"Viewbar Skin",
	"Statusbar Skin",
	"Cycler Skin",
	"Dialog Button Border Skin",

// Icons
	"Window Browser Icon",			// 		Opera icon
	"Window Document Icon",			// 		Opera document icon
	"Folder",						// 		Folder icon (used in hotlist)
	"Folder.open",					// 		Open folder icon (used in window panel and window menu)
	"Trash",						// 		Trash icon (used in hotlist)
	//"Trash Large",					//		Trash icon (Tab bar)
	"Mail Trash",					// 		Trash icon (used in hotlist)
	"Bookmark Visited",				// 		Bookmark icon (hotlist)
	"Bookmark Unvisited",			// 		Visited bookmark icon (hotlist)
	//"New page",					// 		New page icon (used in file menu and main toolbar)
	//"Open document",				// 		Open document icon (used in file menu and main toolbar)
	"New Folder",					// 		New folder icon (used in hotlist)

	//"Go to page",					// 		Go to page icon (used for about menu icon and other links in the menus)
	"Dialog Warning",
	"Dialog Error",
	"Dialog Question",
	"Dialog Info",
	"Dropdown Edit Skin",
	"Chatbar Skin",
	"Start Skin",
//	"Dialog Page Skin",
//	"Dialog Secondary Page Skin",
//	"Pagebar Document Loading Icon",

	"Progress Document Skin",
	"Progress Images Skin",
	"Progress Total Skin",
	"Progress Speed Skin",
	"Progress Elapsed Skin",
	"Progress Status Skin",
	"Progress General Skin",
	"Progress Clock Skin",
	"Progress Empty Skin",
	"Progress Full Skin",
	"Identify As Skin",
//
//	"Scrollbar Horizontal Skin",
//	"Scrollbar Horizontal Knob Skin",
//	"Scrollbar Horizontal Left Skin",
//	"Scrollbar Horizontal Right Skin",
//	"Scrollbar Vertical Skin",
//	"Scrollbar Vertical Knob Skin",
//	"Scrollbar Vertical Up Skin",
//	"Scrollbar Vertical Down Skin",

	"Disclosure Triangle Skin",
	"Tooltip Skin",
	NULL,
};


OpBitmap* MacOpSkinElement::sBrowserIcon = 0;
OpBitmap* MacOpSkinElement::sDocumentIcon = 0;
OpBitmap* MacOpSkinElement::sFolderIcon = 0;
OpBitmap* MacOpSkinElement::sTrashIcon = 0;
OpBitmap* MacOpSkinElement::sLargeTrashIcon = 0;
OpBitmap* MacOpSkinElement::sLargeFullTrashIcon = 0;
OpBitmap* MacOpSkinElement::sBookmarkIcon = 0;
//OpBitmap* MacOpSkinElement::sNewPageIcon = 0;
//OpBitmap* MacOpSkinElement::sOpenDocumentIcon = 0;
//OpBitmap* MacOpSkinElement::sNewFolderIcon = 0;
OpBitmap* MacOpSkinElement::sWarningIcon = 0;
OpBitmap* MacOpSkinElement::sErrorIcon = 0;
OpBitmap* MacOpSkinElement::sQuestionIcon = 0;
OpBitmap* MacOpSkinElement::sInfoIcon = 0;

#define kSkinSmallIconSize 16 		// 16x16 pixels
#define kSkinStandardIconSize 32	// 32x32 pixels
#define kSkinBigIconSize 64			// 64x64 pixels
#define kSkinHugeIconSize 128 		// 128x128 pixels

#ifdef MAC_METAL_SKIN
Boolean g_metal_skin = false;
#endif

enum BorderOmission { BORDER_OMIT_TOP=1, BORDER_OMIT_BOTTOM=2 };
static void Draw3DBorder(VisualDevice *vd, UINT32 lcolor, UINT32 dcolor, OpRect rect,
						 int omissions=0);

static void Draw3DBorder(VisualDevice *vd, UINT32 lcolor, UINT32 dcolor, OpRect rect,
						 int omissions)
{
	if (omissions & BORDER_OMIT_TOP)
	{
		rect.y --;
		rect.height ++;
	}
	if (omissions & BORDER_OMIT_BOTTOM)
		rect.height ++;

	vd->SetColor32(lcolor);
	if (!(omissions & BORDER_OMIT_TOP))
		vd->DrawLine(OpPoint(rect.x, rect.y), rect.width-1, TRUE);
	vd->DrawLine(OpPoint(rect.x, rect.y), rect.height-1, FALSE);
	vd->SetColor32(dcolor);
	if (!(omissions & BORDER_OMIT_BOTTOM))
		vd->DrawLine(OpPoint(rect.x, rect.y + rect.height - 1), rect.width, TRUE);
	vd->DrawLine(OpPoint(rect.x + rect.width - 1, rect.y), rect.height, FALSE);
}


OpSkinElement* OpSkinElement::CreateNativeElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size)
{
	return OP_NEW(MacOpSkinElement, (skin, name, type, size));
}

void OpSkinElement::FlushNativeElements()
{}


/***********************************************************************************
**
**	MacOpSkinElement
**
***********************************************************************************/

MacOpSkinElement::MacOpSkinElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size) : OpSkinElement(skin, name, type, size)
{
	m_is_native = TRUE;
	m_native_type = NATIVE_NOT_SUPPORTED;
	m_icon = NULL;

	if (size != SKINSIZE_DEFAULT)
		return;

	INT32 i;

	for (i = 0; s_mac_native_types[i]; i++)
	{
		if (name.CompareI(s_mac_native_types[i]) == 0)
		{
			NativeType native_type = (NativeType) (i + 1);

#if 1
			switch (native_type)
			{

				case NATIVE_MAINBAR:
				case NATIVE_PERSONALBAR:
				case NATIVE_ADDRESSBAR:
				case NATIVE_NAVIGATIONBAR:
				/*{
					if (type == SKINTYPE_TOP || type == SKINTYPE_LEFT || type == SKINTYPE_RIGHT)
					{
						m_native_type = native_type;
					}
					break;
				}*/

				case NATIVE_LISTBOX:
				case NATIVE_TREEVIEW:
				case NATIVE_PANEL_TREEVIEW:
				case NATIVE_PROGRESSBAR:
				case NATIVE_TAB_BUTTON:
				case NATIVE_PAGEBAR_BUTTON:
				case NATIVE_TABS:
				case NATIVE_PAGEBAR:
				case NATIVE_EDIT:
				case NATIVE_MULTILINE_EDIT:
				case NATIVE_DROPDOWN:
#ifdef WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
				case NATIVE_EDITABLE_DROPDOWN:
#endif // WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
				case NATIVE_DROPDOWN_SEARCH:
				case NATIVE_DROPDOWN_ZOOM:
				case NATIVE_STATUSBAR:
				{
					m_native_type = native_type;
					break;
				}

				default:
				{
					if (type == SKINTYPE_DEFAULT)
					{
						m_native_type = native_type;
					}
					break;
				}
			}
#else
			m_native_type = native_type;
#endif
			break;
		}
	}

	switch (m_native_type)
	{
		case NATIVE_BROWSER_ICON:
			m_icon = sBrowserIcon;
			break;

		case NATIVE_DOCUMENT_ICON:
			m_icon = sDocumentIcon;
			break;

		case NATIVE_NEW_FOLDER_ICON:
		case NATIVE_OPEN_FOLDER_ICON:
		case NATIVE_FOLDER_ICON:
			m_icon = sFolderIcon;
			break;

		case NATIVE_TRASH_ICON:
		case NATIVE_MAIL_TRASH_ICON:
			m_icon = sTrashIcon;
			break;

#if 0
		case NATIVE_LARGE_TRASH_ICON:
			m_icon = sLargeTrashIcon;
			break;
#endif
			
		//case NATIVE_GO_TO_PAGE_ICON:
		case NATIVE_BOOKMARK_VISITED_ICON:
		case NATIVE_BOOKMARK_UNVISITED_ICON:
			m_icon = sBookmarkIcon;
			break;

#if 0
		case NATIVE_NEW_PAGE_ICON:
			m_icon = sNewPageIcon;
			break;

		case NATIVE_OPEN_DOCUMENT_ICON:
			m_icon = sOpenDocumentIcon;
			break;
#endif

		case NATIVE_DIALOG_WARNING:
			m_icon = sWarningIcon;
			break;

		case NATIVE_DIALOG_ERROR:
			m_icon = sErrorIcon;
			break;

		case NATIVE_DIALOG_QUESTION:
			m_icon = sQuestionIcon;
			break;

		case NATIVE_DIALOG_INFO:
			m_icon = sInfoIcon;
			break;
	}

}

MacOpSkinElement::~MacOpSkinElement()
{
}

// static
void MacOpSkinElement::Init()
{
	IconRef 	icon;
	IconRef 	operaIcon = GetOperaApplicationIcon();

	if(operaIcon)
	{
		if(sBrowserIcon)
			delete sBrowserIcon;

		sBrowserIcon = CreateOpBitmapFromIcon(operaIcon, kSkinStandardIconSize, kSkinStandardIconSize);
	}

	if(!sDocumentIcon)
	{
		icon = GetOperaDocumentIcon();
		if(icon)
		{
			sDocumentIcon = CreateOpBitmapFromIcon(icon, kSkinSmallIconSize, kSkinSmallIconSize);
			ReleaseIconRef(icon);
		}
	}
	if(!sFolderIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kGenericFolderIcon, &icon))
		{
			sFolderIcon = CreateOpBitmapFromIcon(icon, kSkinSmallIconSize, kSkinSmallIconSize);
			ReleaseIconRef(icon);
		}
	}
	if(!sTrashIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kTrashIcon, &icon))
		{
			sTrashIcon = CreateOpBitmapFromIcon(icon, kSkinSmallIconSize, kSkinSmallIconSize);
			ReleaseIconRef(icon);
		}
	}
	if(!sLargeTrashIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kTrashIcon, &icon))
		{
			sLargeTrashIcon = CreateOpBitmapFromIcon(icon, kSkinStandardIconSize, kSkinStandardIconSize);
			ReleaseIconRef(icon);
		}
	}
	if(!sLargeFullTrashIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kFullTrashIcon, &icon))
		{
			sLargeFullTrashIcon = CreateOpBitmapFromIcon(icon, kSkinStandardIconSize, kSkinStandardIconSize);
			ReleaseIconRef(icon);
		}
	}
	if(!sBookmarkIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kInternetLocationHTTPIcon, &icon))
		{
			sBookmarkIcon = CreateOpBitmapFromIcon(icon, kSkinSmallIconSize, kSkinSmallIconSize);
			ReleaseIconRef(icon);
		}
	}
#if 0
	if(!sNewPageIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kGenericDocumentIcon, &icon))
		{
			sNewPageIcon = CreateOpBitmapFromIcon(icon, kSkinSmallIconSize, kSkinSmallIconSize);
			ReleaseIconRef(icon);
		}
	}
	if(!sOpenDocumentIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kGenericFolderIcon, &icon))
		{
			sOpenDocumentIcon = CreateOpBitmapFromIcon(icon, kSkinSmallIconSize, kSkinSmallIconSize);
			ReleaseIconRef(icon);
		}
	}
	if(!sNewFolderIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kGenericFolderIcon, &icon))
		{
			sNewFolderIcon = CreateOpBitmapFromIcon(icon, kSkinSmallIconSize, kSkinSmallIconSize);
			ReleaseIconRef(icon);
		}
	}
#endif

	OpRect badgeRect;
	badgeRect.x = kSkinBigIconSize - kSkinStandardIconSize;
	badgeRect.y = kSkinBigIconSize - kSkinStandardIconSize;
	badgeRect.width = kSkinStandardIconSize;
	badgeRect.height = kSkinStandardIconSize;

	if(!sWarningIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kAlertCautionIcon, &icon))
		{
			sWarningIcon = CreateOpBitmapFromIconWithBadge(icon, kSkinBigIconSize, kSkinBigIconSize, sBrowserIcon, badgeRect);
			ReleaseIconRef(icon);
		}
	}
	if(!sErrorIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kAlertStopIcon, &icon))
		{
			sErrorIcon = CreateOpBitmapFromIconWithBadge(icon, kSkinBigIconSize, kSkinBigIconSize, sBrowserIcon, badgeRect);
			ReleaseIconRef(icon);
		}
	}
	if(!sQuestionIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kQuestionMarkIcon, &icon))
		{	
			sQuestionIcon = CreateOpBitmapFromIconWithBadge(icon, kSkinBigIconSize, kSkinBigIconSize, sBrowserIcon, badgeRect);
			ReleaseIconRef(icon);
		}
	}
	if(!sInfoIcon)
	{
		if(noErr == GetIconRef(kOnSystemDisk, kSystemIconsCreator, kAlertNoteIcon, &icon))
		{
			sInfoIcon = CreateOpBitmapFromIconWithBadge(icon, kSkinBigIconSize, kSkinBigIconSize, sBrowserIcon, badgeRect);
			ReleaseIconRef(icon);
		}
	}

	if(operaIcon)
	{	
		if(sBrowserIcon)
			delete sBrowserIcon;

		sBrowserIcon = CreateOpBitmapFromIcon(operaIcon, kSkinSmallIconSize, kSkinSmallIconSize);

		ReleaseIconRef(operaIcon);
	}
}

// static
void MacOpSkinElement::Free()
{
	if(sBrowserIcon)
	{
		delete sBrowserIcon;
		sBrowserIcon = NULL;
	}
	if(sDocumentIcon)
	{
		delete sDocumentIcon;
		sDocumentIcon = NULL;
	}
	if(sFolderIcon)
	{
		delete sFolderIcon;
		sFolderIcon = NULL;
	}
	if(sTrashIcon)
	{
		delete sTrashIcon;
		sTrashIcon = NULL;
	}
	if(sLargeTrashIcon)
	{
		delete sLargeTrashIcon;
		sLargeTrashIcon = NULL;
	}
	if(sLargeFullTrashIcon)
	{
		delete sLargeFullTrashIcon;
		sLargeFullTrashIcon = NULL;
	}	
	if(sBookmarkIcon)
	{
		delete sBookmarkIcon;
		sBookmarkIcon = NULL;
	}
#if 0
	if(sNewPageIcon)
	{
		delete sNewPageIcon;
		sNewPageIcon = NULL;
	}
	if(sOpenDocumentIcon)
	{
		delete sOpenDocumentIcon;
		sOpenDocumentIcon = NULL;
	}
	if(sNewFolderIcon)
	{
		delete sNewFolderIcon;
		sNewFolderIcon = NULL;
	}
#endif
	if(sWarningIcon)
	{
		delete sWarningIcon;
		sWarningIcon = NULL;
	}
	if(sErrorIcon)
	{
		delete sErrorIcon;
		sErrorIcon = NULL;
	}
	if(sQuestionIcon)
	{
		delete sQuestionIcon;
		sQuestionIcon = NULL;
	}
	if(sInfoIcon)
	{
		delete sInfoIcon;
		sInfoIcon = NULL;
	}
}


void MacOpSkinElement::OverrideDefaults(OpSkinElement::StateElement* se)
{
	switch(m_native_type)
	{
		case NATIVE_EDIT:
		{
			se->m_padding_left = 1;
			se->m_padding_top = 2;
			se->m_padding_right = 1;
			se->m_padding_bottom = 2;
			se->m_margin_left = 8;
			se->m_margin_top = 10;
			se->m_margin_right = 8;
			se->m_margin_bottom = 10;
			break;
		}
		case NATIVE_PUSH_BUTTON:
		case NATIVE_PUSH_DEFAULT_BUTTON:
		{
			se->m_margin_left = 12;
			se->m_margin_right = 12;
			se->m_margin_top = 12;
			se->m_margin_bottom = 12;
			se->m_text_color = OP_RGB(0,0,0);
			break;
		}
		case NATIVE_DROPDOWN:
		{
			if (se->m_state & SKINSTATE_RTL)
			{
				se->m_padding_left = 3;
				se->m_padding_right = 5;
			}
			else
			{
				se->m_padding_left = 5;
				se->m_padding_right = 3;
			}
			se->m_padding_top = 3;
			se->m_padding_bottom = 1;
			se->m_margin_left = 8;
			se->m_margin_right = 8;
			se->m_margin_top = 10;
			se->m_margin_bottom = 10;
			break;
		}
		case NATIVE_DIALOG:
		{
			se->m_padding_left = 20;
			se->m_padding_top = 8;
			se->m_padding_right = 20;
			se->m_padding_bottom = 20;
			break;
		}
/*		
		case NATIVE_DIALOG_PAGE:
		{
			se->m_padding_left = 8;
			se->m_padding_top = 8;
			se->m_padding_right = 8;
			se->m_padding_bottom = 8;
			break;
		}
*/
		case NATIVE_DIALOG_TAB_PAGE:
		{
			se->m_padding_left = 8;
			se->m_padding_top = 20;
			se->m_padding_right = 8;
			se->m_padding_bottom = 12;
			se->m_margin_bottom = 8;
			break;
		}
		case NATIVE_DIALOG_BUTTON:
		{
			se->m_padding_left = 5;
			se->m_padding_top = 2;
			se->m_padding_right = 5;
			se->m_padding_bottom = 2;
			break;
		}
		case NATIVE_TAB_BUTTON:
		{
			se->m_padding_left = 9;
			se->m_padding_top = 1;
			se->m_padding_right = 9;
			se->m_padding_bottom = 1;
			break;
		}
		case NATIVE_TABS:
		{
			se->m_padding_left = 0;
			se->m_padding_top = 0;
			se->m_padding_right = 0;
			se->m_padding_bottom = 0;
			break;
		}
		case NATIVE_CHECKBOX:
		case NATIVE_RADIO_BUTTON:
		{
			se->m_margin_left = 8;
			se->m_margin_right = 8;
			se->m_margin_top = 6;
			se->m_margin_bottom = 6;
		}
	}
}

/***********************************************************************************
**
**	GetTextColor
**
***********************************************************************************/

OP_STATUS MacOpSkinElement::GetTextColor(UINT32* color, INT32 state)
{
	switch (m_native_type)
	{
		case NATIVE_PUSH_DEFAULT_BUTTON:
		case NATIVE_PAGEBAR_BUTTON:
		case NATIVE_TAB_BUTTON:
		case NATIVE_MENU_BUTTON:
		case NATIVE_LINK_BUTTON:
		case NATIVE_HEADER_BUTTON:
		case NATIVE_SELECTOR_BUTTON:
			if (state & SKINSTATE_DISABLED)
				*color = OP_RGB(127, 127, 127);
			else if (state & SKINSTATE_ATTENTION)
				*color = OP_RGB(0, 0, 255);
			else
				*color = OP_RGB(0, 0, 0);
			return OpStatus::OK;
	}

	return OpSkinElement::GetTextColor(color, state);
}

/***********************************************************************************
**
**	GetBorderWidth
**
***********************************************************************************/

OP_STATUS MacOpSkinElement::GetBorderWidth(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	*left = *top = *right = *bottom = 2;
	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetSize
**
***********************************************************************************/

OP_STATUS MacOpSkinElement::GetSize(INT32* width, INT32* height, INT32 state)
{
	SInt32 s_width;
	SInt32 s_height;

	switch (m_native_type)
	{
#if 0
	// This is not used at the moment, but we might need it if we want to support different header heights
		case NATIVE_HEADER_BUTTON:
		case NATIVE_HEADER_SORT_ASC_BUTTON:
		case NATIVE_HEADER_SORT_DESC_BUTTON:
			if(OpStatus::IsSuccess(OpSkinElement::GetSize(width, height, state)) && (*height > 0))
			{
				// let's use this value
				return OpStatus::OK;
			}
			else
			{
				if(noErr == GetThemeMetric(kThemeMetricListHeaderHeight, &s_height))
				{
					*height = s_height;
					return OpStatus::OK;
				}
			}
			break;
#endif

		case NATIVE_RADIO_BUTTON:
			if((noErr == GetThemeMetric(kThemeMetricSmallRadioButtonHeight, &s_height)) && (noErr == GetThemeMetric(kThemeMetricSmallRadioButtonWidth, &s_width)))
			{
				*width = s_width;
				*height = s_height;
				return OpStatus::OK;
			}
			break;
		case NATIVE_CHECKBOX:
			if((noErr == GetThemeMetric(kThemeMetricSmallCheckBoxHeight, &s_height)) && (noErr == GetThemeMetric(kThemeMetricSmallCheckBoxWidth, &s_width)))
			{
				*width = s_width;
				*height = s_height;
				return OpStatus::OK;
			}
			break;
		case NATIVE_BROWSER_ICON:
		case NATIVE_DOCUMENT_ICON:
		case NATIVE_FOLDER_ICON:
		case NATIVE_TRASH_ICON:
		case NATIVE_MAIL_TRASH_ICON:
		case NATIVE_BOOKMARK_VISITED_ICON:
		case NATIVE_BOOKMARK_UNVISITED_ICON:
		case NATIVE_OPEN_FOLDER_ICON:
		//case NATIVE_NEW_PAGE_ICON:
		//case NATIVE_OPEN_DOCUMENT_ICON:
		case NATIVE_NEW_FOLDER_ICON:
		//case NATIVE_GO_TO_PAGE_ICON:
			*width = kSkinSmallIconSize;
			*height = kSkinSmallIconSize;
			return OpStatus::OK;
		case NATIVE_DIALOG_WARNING:
		case NATIVE_DIALOG_ERROR:
		case NATIVE_DIALOG_QUESTION:
		case NATIVE_DIALOG_INFO:
			*width = kSkinBigIconSize;
			*height = kSkinBigIconSize;
			return OpStatus::OK;
//		case NATIVE_CHECKMARK:
//		case NATIVE_BULLET:
//		{
//			*width = GetSystemMetrics(SM_CXMENUCHECK);
//			*height = GetSystemMetrics(SM_CYMENUCHECK);
//			return OpStatus::OK;
//		}
		case NATIVE_DISCLOSURE_TRIANGLE:
			if((noErr == GetThemeMetric(kThemeMetricDisclosureTriangleHeight, &s_height)) && (noErr == GetThemeMetric(kThemeMetricDisclosureTriangleWidth, &s_width)))
			{
				*width = s_width;
				*height = s_height;
				return OpStatus::OK;
			}
		case NATIVE_DROPDOWN_BUTTON:
		case NATIVE_DROPDOWN_LEFT_BUTTON:
		{
			*width = 19;
			*height = 19;
			return OpStatus::OK;
		}
		case NATIVE_PUSH_BUTTON:
		case NATIVE_PUSH_DEFAULT_BUTTON:
			*height = 22;
			*width = 87;
			return OpStatus::OK;
	}

	return OpSkinElement::GetSize(width, height, state);
}

/***********************************************************************************
**
**	Draw
**
***********************************************************************************/

OP_STATUS MacOpSkinElement::Draw(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args)
{
	if (g_widget_globals)
		state |= g_widget_globals->painted_widget_extra_state; // Add focus information

	if (m_native_type == NATIVE_NOT_SUPPORTED)
		return OpSkinElement::Draw(vd, rect, state, args);

	const INT32 hover_value = args.hover_value;
	const OpRect *clip_rect = args.clip_rect;
	
	VEGAOpPainter* painter = (VEGAOpPainter*)vd->painter;
	
	// Fix for DSK-325822. This should really never have happened, but is caused by faulty date picker widget.
	if (!painter)
		return TRUE;	// Must return TRUE or else we get a core crash instead.
	
	VEGARenderTarget* target = painter->GetRenderTarget();
	CocoaVEGAWindow* window = target ? (CocoaVEGAWindow*)target->getTargetWindow() : NULL;
	int win_height = window?CGImageGetHeight(window->getImage()):0;
	CGContextRef context = NULL;
	MacWidgetBitmap* cached_bitmap = NULL;
	OpBitmap* bitmap = NULL;
	const int bitmap_extra = m_native_type==NATIVE_TABS?0:2;
	unsigned int rect_width = rect.width + 2*bitmap_extra,
	rect_height = rect.height + 2*bitmap_extra;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	const PixelScaler& scaler = vd->GetVPScale();
	rect_width = TO_DEVICE_PIXEL(scaler, rect_width);
	rect_height = TO_DEVICE_PIXEL(scaler, rect_height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	if(!context)
	{
		MacVegaPrinterListener* pl = static_cast<MacVegaPrinterListener*>(painter->getPrinterListener());
		if (pl) {
			context = pl->GetCGContext();
			win_height = pl->GetWindowHeight();
			clip_rect = NULL;
		}
	}
	if(!context)
	{
		RETURN_IF_ERROR((MacCachedObjectFactory<MacWidgetBitmap, MacWidgetBitmapTraits>::Init()));
		cached_bitmap = MacCachedObjectFactory<MacWidgetBitmap, MacWidgetBitmapTraits>::CreateObject(MacWidgetBitmapTraits::CreateParam(rect_width, rect_height));
		RETURN_OOM_IF_NULL(cached_bitmap);
		bitmap = cached_bitmap->GetOpBitmap();

		void *image_data = bitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY);
		if (NULL == image_data) {
			cached_bitmap->DecRef();
			return FALSE;
		}
		cached_bitmap->Lock();
		memset(image_data, 0, bitmap->GetBytesPerLine() * bitmap->Height());
		CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
		CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
		int bpl = bitmap->GetBytesPerLine();
		context = CGBitmapContextCreate(image_data, rect_width, rect_height, 8, bpl, colorSpace, alpha);
		CGColorSpaceRelease(colorSpace);
		win_height = rect_height;
	}
	RETURN_OOM_IF_NULL(context);
	
	float fop = painter->GetAccumulatedOpacity()/255.0f;
	float scale = ((float)vd->GetScale()) / 100.0f;

	OpRect translatedOpRect = rect;
	
#ifdef VIEWPORTS_SUPPORT
	translatedOpRect.x += vd->GetTranslationX() - vd->GetRenderingViewX() + vd->ScaleToDoc(vd->GetOffsetX());
	translatedOpRect.y += vd->GetTranslationY() - vd->GetRenderingViewY() + vd->ScaleToDoc(vd->GetOffsetY());
#else
	translatedOpRect.x += vd->GetTranslationX() - vd->GetViewX() + vd->ScaleToDoc(vd->GetOffsetX());
	translatedOpRect.y += vd->GetTranslationY() - vd->GetViewY() + vd->ScaleToDoc(vd->GetOffsetY());
#endif
	
	MacOpView *macview = (MacOpView *)vd->GetOpView();
	INT32 xpos=0,ypos=0;
	OpPoint pt;
	CGRect view_rect = CGRectMake(0,0,0,0);
	if (macview) {
		pt = macview->ConvertToScreen(pt);
		OpWindow* root = macview->GetRootWindow();
		root->GetInnerPos(&xpos, &ypos);
		UINT32 w=0,h=0;
		macview->GetSize(&w,&h);

		xpos = pt.x - xpos;
		ypos = pt.y - ypos;
		view_rect = CGRectMake(xpos, ypos, w, h);
	}
	
	// vd->GetOffsetX() and vd->GetOffsetY() are in screen coordinates
	// vd_clip.x and vd_clip.y are in doc coordinates
	// thus they cannot be simply added together
	OpRect vd_clip = vd->GetClipping();
	int clip_x = (vd_clip.x * scale + vd->GetOffsetX()) + xpos;
	int clip_y = (vd_clip.y * scale + vd->GetOffsetY()) + ypos;
	CGRect cr = {{clip_x, clip_y}, {vd_clip.width*scale, vd_clip.height*scale}};

	if(clip_rect)
	{
#ifdef VIEWPORTS_SUPPORT
		clip_x = (clip_rect->x + vd->GetTranslationX() - vd->GetRenderingViewX())*scale + vd->GetOffsetX() + xpos;
		clip_y = (clip_rect->y + vd->GetTranslationY() - vd->GetRenderingViewY())*scale + vd->GetOffsetY() + ypos;
#else
		clip_x = (clip_rect->x + vd->GetTranslationX() - vd->GetViewX())*scale + vd->GetOffsetX();
		clip_y = (clip_rect->y + vd->GetTranslationY() - vd->GetViewY())*scale + vd->GetOffsetY();
#endif
		CGRect cr2 = {{clip_x, clip_y}, {clip_rect->width*scale, clip_rect->height*scale}};

		cr = CGRectIntersection(cr, cr2);
	}

	if(!CGRectIsEmpty(view_rect))
	{
		cr = CGRectIntersection(cr, view_rect);
	}

	CGRect r = {{translatedOpRect.x*scale, translatedOpRect.y*scale}, {translatedOpRect.width*scale, translatedOpRect.height*scale}};
	r = CGRectOffset(r, xpos, ypos);

	ThemeTabStyle 			themeTabStyle = kThemeTabNonFront;
	ThemeButtonDrawInfo 	theme;
	theme.state = kThemeStateActive;
	theme.value = kThemeButtonOff;
	theme.adornment = kThemeAdornmentNone;
	
	ThemeButtonKind buttonKind = 0xFFFF;	// invalid

	if (bitmap)
	{
		r.origin.y = bitmap_extra;
		r.origin.x = bitmap_extra;
		r.size.height = translatedOpRect.height;
		r.size.width = translatedOpRect.width;
	}

	CGContextSaveGState(context);
	CGContextSetAlpha(context, fop);
	// Flip everything vertically.  In addition, flip horizontally in special
	// cases.
	float scale_x = 1.0, scale_y = 1.0, offset_x = 0.0, offset_y = 0.0;
	switch (m_native_type)
	{
		case NATIVE_DROPDOWN:
		case NATIVE_EDITABLE_DROPDOWN:
		case NATIVE_HEADER_SORT_ASC_BUTTON:
		case NATIVE_HEADER_SORT_DESC_BUTTON:
		case NATIVE_HEADER_BUTTON:
			if (state & SKINSTATE_RTL)
			{
				scale_x = -1.0;
				offset_x = -(r.origin.x * 2 + r.size.width);
			}
			// fall through
		default:
			scale_y = -1.0;
			offset_y = -win_height;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			// offset_y should be in points
			offset_y = FROM_DEVICE_PIXEL(scaler, offset_y);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	}
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	scale_x = TO_DEVICE_PIXEL(scaler, scale_x);
	scale_y = TO_DEVICE_PIXEL(scaler, scale_y);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	CGContextScaleCTM(context, scale_x, scale_y);
	CGContextTranslateCTM(context, offset_x, offset_y);

	if (state & SKINSTATE_INDETERMINATE)
	{
		theme.value = kThemeButtonMixed;
		if (state & SKINSTATE_DISABLED)
			theme.state = kThemeStateInactive;
		themeTabStyle = kThemeTabFront;
	}
	else if (state & SKINSTATE_SELECTED) 
	{
		theme.value = kThemeButtonOn;
		if (state & SKINSTATE_DISABLED)
			theme.state = kThemeStateInactive;
		//theme.state = kThemeStatePressed;
		themeTabStyle = kThemeTabFront;
	} 
	else 
	{
		if (state & SKINSTATE_DISABLED) 
		{
			theme.state = kThemeStateInactive;
			themeTabStyle = kThemeTabNonFrontInactive;
		} 
		else if (state & SKINSTATE_PRESSED) 
		{
			theme.state = kThemeStatePressed;
			themeTabStyle = kThemeTabNonFrontPressed;
		}
		else
		{
			theme.state = kThemeStateActive;
			themeTabStyle = kThemeTabNonFront;
		}	
	}
	
	if(state & SKINSTATE_FOCUSED)
	{
		theme.adornment |= kThemeAdornmentFocus;
	}
	
	OpRect temp_rect1, temp_rect2, temp_rect3;
	if(clip_rect && !vd->HasTransform())
	{
		painter->GetClipRect(&temp_rect1);
		painter->RemoveClipRect();
		painter->GetClipRect(&temp_rect2);
		painter->RemoveClipRect();
		painter->GetClipRect(&temp_rect3);
		painter->RemoveClipRect();
		OpRect clip_rect2 = temp_rect1;
		if (rect.width <= cr.size.width)
		{
			clip_rect2.width += 2*bitmap_extra;
			clip_rect2.x -= bitmap_extra;
		}
		if (rect.height <= cr.size.height)
		{
			clip_rect2.y -= bitmap_extra;
			clip_rect2.height += 2*bitmap_extra;
		}
		painter->SetClipRect(clip_rect2);
	}
	
	switch (m_native_type)
	{
		case NATIVE_PUSH_DEFAULT_BUTTON:
			// Do not set the state to pushed if it was set to inactive above or else the button will look blue!
			if (theme.state != kThemeStateInactive)
			{
				OpRect pulse(r.origin.x, r.origin.y, r.size.width, r.size.height);
				((MacOpView*)vd->view->GetOpView())->DelayedInvalidate(pulse, 40);
			}
			theme.adornment |= kThemeAdornmentDefault;
			// fall through
		case NATIVE_PUSH_BUTTON: {
			INT32 left, top, right, bottom;
			SInt32 normalh;
			SInt32 smallh;
			SInt32 minih;
			
			// Use bevel button if left or right padding is zero, see bug 323683.
			buttonKind = kThemeBevelButton;
			OpWidget* w = g_widget_globals->skin_painted_widget;
			if (w)
			{
				w->GetPadding(&left, &top, &right, &bottom);
			}
			if (!w || (left && right))
			{
				unsigned button_vertical_inset = (w && w->GetFormObject()) ? 2 : 1;
				r = CGRectInset(r, 1, button_vertical_inset);
				if ((noErr == GetThemeMetric(kThemeMetricPushButtonHeight, &normalh)))
				{
					if (r.size.height < normalh)
					{
						if (noErr == GetThemeMetric(kThemeMetricSmallPushButtonHeight, &smallh))
						{
							if (r.size.height < smallh)
							{
								if (noErr == GetThemeMetric(kThemeMetricMiniPushButtonHeight, &minih))
								{
									if (r.size.height < minih || r.size.width < 12)
									{
										r = CGRectInset(r, -1, -button_vertical_inset);
										buttonKind = kThemeBevelButton;
									}
									else
									{
										buttonKind = kThemePushButtonMini;
									}
								}
							}
							else
							{
								buttonKind = kThemePushButtonSmall;
							}
						}
					}
					else
					{
						buttonKind = kThemePushButton;
					}
				}
			}
			PainterDrawThemeButton(context, r, buttonKind, &theme, NULL, kHIThemeOrientationNormal);
			break;			
		}
			
		case NATIVE_HEADER_SORT_ASC_BUTTON:
			buttonKind = kThemeListHeaderButton;
			theme.value = kThemeButtonOn;
			theme.adornment |= kThemeAdornmentHeaderButtonSortUp;
			r.size.width++;
			PainterDrawThemeButton(context, r, buttonKind, &theme, NULL, kHIThemeOrientationNormal);
			break;
			
		case NATIVE_HEADER_SORT_DESC_BUTTON:
			buttonKind = kThemeListHeaderButton;
			theme.value = kThemeButtonOn;
			r.size.width++;
			PainterDrawThemeButton(context, r, buttonKind, &theme, NULL, kHIThemeOrientationNormal);
			break;
			
		case NATIVE_HEADER_BUTTON:
			buttonKind = kThemeListHeaderButton;
			r.size.width++;
			PainterDrawThemeButton(context, r, buttonKind, &theme, NULL, kHIThemeOrientationNormal);
			break;
			
		case NATIVE_SELECTOR_BUTTON:
			if(state & SKINSTATE_SELECTED)
			{
				theme.adornment |= kThemeAdornmentDefault;
				PainterDrawThemeGenericWell(context, r, &theme);
			}
			break;
			
		case NATIVE_TOOLBAR_BUTTON:
			if(state & SKINSTATE_FOCUSED)
			{
				CGRect rect = r;
				rect = CGRectInset(rect, 4, 4);
				PainterDrawThemeFocusRect(context, rect, true);
				break;
			}
			
		case NATIVE_MENU_BUTTON:
		case NATIVE_LINK_BUTTON:
			if(state & SKINSTATE_SELECTED)
			{
				UINT32 dgray = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_DARK);
				vd->painter->SetColor(0,0,0,16);
				vd->FillRect(rect);
				vd->SetColor(dgray);
				vd->DrawRect(rect);
			}
			break;
			
		case NATIVE_RADIO_BUTTON:
			buttonKind = kThemeSmallRadioButton;
			PainterDrawThemeButton(context, r, buttonKind, &theme, NULL, kHIThemeOrientationNormal);
			break;
			
		case NATIVE_CHECKBOX:
			buttonKind = kThemeSmallCheckBox;
			PainterDrawThemeButton(context, r, buttonKind, &theme, NULL, kHIThemeOrientationNormal);
			break;
			
		case NATIVE_DROPDOWN_BUTTON:
		case NATIVE_DROPDOWN_LEFT_BUTTON:
			//r.origin.x += r.size.width/2 - 4;
			//r.origin.y += r.size.height/2 - 3;
			//PainterDrawThemePopupArrow(context, r, kThemeArrowDown, kThemeArrow9pt, theme.state);
			break;
			
#ifdef WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
		case NATIVE_EDITABLE_DROPDOWN:
			buttonKind = kThemeComboBox;
			theme.adornment |= kThemeAdornmentArrowDownArrow;
			//InsetRect(&r, 1, 1);
			r.size.height -= 2;
			PainterDrawThemeButton(context, r, buttonKind, &theme, NULL, kHIThemeOrientationNormal);
			break;
#endif // WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
			
		case NATIVE_DROPDOWN:
		{
			OpWidget* w = g_widget_globals->skin_painted_widget;
			if(!w || w->GetType() != OpTypedObject::WIDGET_TYPE_QUICK_FIND && w->GetType() != OpTypedObject::WIDGET_TYPE_ZOOM_DROPDOWN)
			{
				buttonKind = DetermineDropdownButtonKind(w, r);
				theme.adornment |= kThemeAdornmentArrowDownArrow;
				r.size.height -= (w && w->GetFormObject())?1:2;
				PainterDrawThemeButton(context, r, buttonKind, &theme, NULL, kHIThemeOrientationNormal);
				break;
			}
			// Fall through
		}	
		case NATIVE_START:
#ifndef WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
		case NATIVE_EDITABLE_DROPDOWN:
#endif // WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
		case NATIVE_DROPDOWN_SEARCH:
		case NATIVE_DROPDOWN_ZOOM:
		case NATIVE_MULTILINE_EDIT:
		case NATIVE_EDIT:
		{
			r = CGRectInset(r, 1, 1);
			vd->SetColor(g_op_ui_info->GetSystemColor(state & SKINSTATE_DISABLED ? OP_SYSTEM_COLOR_BACKGROUND_DISABLED : OP_SYSTEM_COLOR_BACKGROUND));
			vd->FillRect(rect);
			
			PainterDrawThemeEditTextFrame(context, r, theme.state);
			PainterDrawThemeFocusRect(context, r, state & SKINSTATE_FOCUSED);
			break;
		}
		case NATIVE_LIST_ITEM:
		{
			vd->SetColor(g_op_ui_info->GetSystemColor(state & SKINSTATE_SELECTED ? OP_SYSTEM_COLOR_BACKGROUND_SELECTED : OP_SYSTEM_COLOR_BACKGROUND));
			vd->FillRect(rect);
			break;
		}
		case NATIVE_LISTBOX:
		case NATIVE_TREEVIEW:
		case NATIVE_PANEL_TREEVIEW:
		{
			r = CGRectInset(r, 1, 1);
			vd->SetColor(g_op_ui_info->GetSystemColor(state & SKINSTATE_DISABLED ? OP_SYSTEM_COLOR_BACKGROUND_DISABLED : OP_SYSTEM_COLOR_BACKGROUND));
			vd->FillRect(rect);
			
			PainterDrawThemeListBoxFrame(context, r, theme.state);
			break;
		}
		case NATIVE_PAGEBAR:
		{
			int inset = -4;
			r = CGRectInset(r, inset, inset);
			PainterDrawThemeWindowHeader(context, r, theme.state);
			
			vd->painter->SetColor(0, 0, 0, 40);
			vd->FillRect(OpRect(rect.x, rect.y, rect.width, rect.height));
			
			vd->painter->SetColor(0xa0, 0xa0, 0xa0, 0xff);
			
			if(GetType() == SKINTYPE_TOP && translatedOpRect.y > 0)
			{
				vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y), translatedOpRect.width, TRUE, 1);
			}
			if(GetType() == SKINTYPE_LEFT)
			{
				vd->painter->DrawLine(OpPoint(translatedOpRect.x + translatedOpRect.width - 1, translatedOpRect.y + 1), translatedOpRect.height - 3, FALSE, 1);
			}
			if(GetType() == SKINTYPE_RIGHT)
			{
				vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y + 1), translatedOpRect.height - 3, FALSE, 1);
			}
			if(GetType() == SKINTYPE_TOP)
			{
				vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y + translatedOpRect.height - 1), translatedOpRect.width, TRUE, 1);
			}
			break;
		}
		case NATIVE_TABS:
		{
			vd->SetColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_DARK));
			
			switch(GetKey()->m_type)
			{
				case SKINTYPE_BOTTOM:
					if(m_native_type == NATIVE_PAGEBAR)
					{
						vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y), OpPoint(translatedOpRect.x+translatedOpRect.width, translatedOpRect.y), (INT)(1*scale));
					}
					r.size.height = 7;
					break;
					
				case SKINTYPE_TOP:
				case SKINTYPE_DEFAULT:
					if(m_native_type == NATIVE_PAGEBAR)
					{
						vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y/*+translatedOpRect.height*/), OpPoint(translatedOpRect.x+translatedOpRect.width, translatedOpRect.y/*+translatedOpRect.height*/), (INT)(1*scale));
					}
					
					if(m_native_type == NATIVE_PAGEBAR)
					{
						r.origin.y += r.size.height - 7;
						
						PainterDrawThemeTabPane(context, r, theme.state);
					}
					else
					{
						HIRect rect;
						rect.origin.x = r.origin.x;
						rect.origin.y = r.origin.y + 12;
						rect.size.width = r.size.width;
						rect.size.height = r.size.height;
						
						HIThemeTabPaneDrawInfo drawInfo;
						drawInfo.version = 1;
						drawInfo.state = kThemeStateActive;
						drawInfo.direction = kThemeTabNorth;
						drawInfo.size = kHIThemeTabSizeNormal;
						drawInfo.kind = kHIThemeTabKindNormal;
						drawInfo.adornment = kHIThemeTabPaneAdornmentNormal;
						
						PainterHIThemeDrawTabPane(context, &rect, (HIThemeTabPaneDrawInfo*)&drawInfo, kHIThemeOrientationNormal);
					}
					break;
					
				case SKINTYPE_LEFT:	
				case SKINTYPE_RIGHT:
				default:
					break;
			}
		}
			break;
			
		case NATIVE_DIALOG_TAB_PAGE:
		{
			HIRect rect;
			rect.origin.x = r.origin.x;
			rect.origin.y = r.origin.y - 10;
			rect.size.width = r.size.width;
			rect.size.height = r.size.height;
			
			HIThemeTabPaneDrawInfo drawInfo;
			drawInfo.version = 1;
			drawInfo.state = kThemeStateActive;
			drawInfo.direction = kThemeTabNorth;
			drawInfo.size = kHIThemeTabSizeNormal;
			drawInfo.kind = kHIThemeTabKindNormal;
			drawInfo.adornment = kHIThemeTabPaneAdornmentNormal;
			
			PainterHIThemeDrawTabPane(context, &rect, (HIThemeTabPaneDrawInfo*)&drawInfo, kHIThemeOrientationNormal);
			break;
		}
			
		case NATIVE_PAGEBAR_BUTTON:	
		{
			INT32 ntabs = 0;
			INT32 indx = -1;
			INT32 selected_indx = -1;
			
			MacOpView *view = (MacOpView *)vd->GetOpView();
			DesktopWindow *w = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(view->GetRootWindow());
			
			if(w)
			{
				OpWidget *widg = w->GetWidgetByType(OpTypedObject::WIDGET_TYPE_PAGEBAR);
				if(widg)
				{
					OpPagebar *bar = (OpPagebar *)widg;
					ntabs = bar->GetWidgetCount();
					for(int i=0; i<ntabs; i++)
					{
						OpWidget *child = bar->GetWidget(i);
						if(child->GetType() == OpWidget::WIDGET_TYPE_BUTTON)
						{
							OpButton *button = (OpButton *)child;
							if(button->GetValue())
							{
								selected_indx = i;
							}
						}
						if(child == g_widget_globals->skin_painted_widget)
						{
							indx = i;
						}
					}
					
				}			
			}
			
			if(!(state & SKINSTATE_SELECTED))
			{
				if(state & SKINSTATE_HOVER)
				{
					vd->painter->SetColor(0, 0, 0, 30);
					vd->FillRect(OpRect(rect.x, rect.y, rect.width, rect.height - 1));
				}
				
				if(GetType() == SKINTYPE_TOP || GetType() == SKINTYPE_BOTTOM)
				{
					if(indx != selected_indx + 1)
					{
						if(g_skin_manager->GetCurrentSkin()->GetOptionValue("Metal", 0))
						{
							vd->painter->SetColor(0, 0, 0, 100);
							vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y + 1), translatedOpRect.height - 3, FALSE, 1);
						}
						else
						{
							vd->painter->SetColor(0xa0, 0xa0, 0xa0, 0xff);
							vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y), translatedOpRect.height - 1, FALSE, 1);
						}
					}
					
					if(indx == ntabs - 1)
					{
						if(g_skin_manager->GetCurrentSkin()->GetOptionValue("Metal", 0))
						{
							vd->painter->SetColor(0, 0, 0, 100);
							vd->painter->DrawLine(OpPoint(translatedOpRect.x + translatedOpRect.width - 1, translatedOpRect.y + 1), translatedOpRect.height - 3, FALSE, 1);
						}
						else
						{
							vd->painter->SetColor(0xa0, 0xa0, 0xa0, 0xff);
							vd->painter->DrawLine(OpPoint(translatedOpRect.x + translatedOpRect.width - 1, translatedOpRect.y), translatedOpRect.height - 1, FALSE, 1);
						}
					}
				}
			}
			else
			{
				{
					float win_h = window?window->height():0;
					
					CGPoint points[8];
					
					if(GetType() != SKINTYPE_BOTTOM)
					{
						points[0].x = translatedOpRect.x;
						points[0].y = win_h - translatedOpRect.y - translatedOpRect.height;
						points[1].x = translatedOpRect.x;
						points[1].y = win_h - translatedOpRect.y - 5;
						points[2].x = translatedOpRect.x + 1;
						points[2].y = win_h - translatedOpRect.y - 2;
						points[3].x = translatedOpRect.x + 3;
						points[3].y = win_h - translatedOpRect.y - 1;
						points[4].x = translatedOpRect.x + translatedOpRect.width - 3;
						points[4].y = win_h - translatedOpRect.y - 1;
						points[5].x = translatedOpRect.x + translatedOpRect.width - 1;
						points[5].y = win_h - translatedOpRect.y - 2;
						points[6].x = translatedOpRect.x + translatedOpRect.width;
						points[6].y = win_h - translatedOpRect.y - 5;
						points[7].x = translatedOpRect.x + translatedOpRect.width;
						points[7].y = win_h - translatedOpRect.y - translatedOpRect.height;
					}
					else
					{
						points[0].x = translatedOpRect.x;
						points[0].y = win_h - translatedOpRect.y;
						points[1].x = translatedOpRect.x;
						points[1].y = win_h - translatedOpRect.y - translatedOpRect.height + 5;
						points[2].x = translatedOpRect.x + 1;
						points[2].y = win_h - translatedOpRect.y - translatedOpRect.height + 2;
						points[3].x = translatedOpRect.x + 3;
						points[3].y = win_h - translatedOpRect.y - translatedOpRect.height + 1;
						points[4].x = translatedOpRect.x + translatedOpRect.width - 3;
						points[4].y = win_h - translatedOpRect.y - translatedOpRect.height + 1;
						points[5].x = translatedOpRect.x + translatedOpRect.width - 1;
						points[5].y = win_h - translatedOpRect.y - translatedOpRect.height + 2;
						points[6].x = translatedOpRect.x + translatedOpRect.width;
						points[6].y = win_h - translatedOpRect.y - translatedOpRect.height + 5;
						points[7].x = translatedOpRect.x + translatedOpRect.width;
						points[7].y = win_h - translatedOpRect.y;
					}
					
					CGRect rr = r;
					rr.size.height -= 1;
					
					//PainterDrawAxialShading(context, &rr, 0xd5d5d5ff, 0xf5f5f5ff, points, sizeof(points)/sizeof(CGPoint));
				}					
			}
			break;
		}
		case NATIVE_TAB_BUTTON:
		{
			ThemeTabDirection tabDirection;
			vd->SetColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_DARK));
			
			switch(GetKey()->m_type)
			{
				case SKINTYPE_BOTTOM:
					tabDirection = kThemeTabSouth;
					break;
				case SKINTYPE_LEFT:
					vd->painter->DrawLine(OpPoint(translatedOpRect.x+translatedOpRect.width, translatedOpRect.y), OpPoint(translatedOpRect.x+translatedOpRect.width, translatedOpRect.y+translatedOpRect.height), (INT)(1*scale));
					tabDirection = kThemeTabNorth;
					break;
				case SKINTYPE_RIGHT:
					vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y), OpPoint(translatedOpRect.x, translatedOpRect.y+translatedOpRect.height), (INT)(1*scale));
					tabDirection = kThemeTabNorth;
					break;
				case SKINTYPE_TOP:
				default:
					tabDirection = kThemeTabNorth;
					break;
					
			}
			
			INT32 ntabs = 0;
			INT32 indx = -1;
			
			OpWidget *widg = NULL;
			if (g_widget_globals->skin_painted_widget)
				widg = g_widget_globals->skin_painted_widget->GetParent();
			
			if (widg)
			{
				if (widg->GetType() == OpTypedObject::WIDGET_TYPE_TABS)
				{
					OpTabs *tabs = (OpTabs *)widg;
					ntabs = tabs->GetTabCount();
					for(int i=0; i<ntabs; i++)
					{
						OpWidget *child = tabs->GetWidget(i);
						if(child == g_widget_globals->skin_painted_widget)
						{
							indx = i;
							break;
						}
					}
				}
				else if (widg->GetType() == OpTypedObject::WIDGET_TYPE_PAGEBAR)
				{						
					OpPagebar *bar = (OpPagebar *)widg;
					ntabs = bar->GetWidgetCount();
					for(int i=0; i<ntabs; i++)
					{
						OpWidget *child = bar->GetWidget(i);
						if(child == g_widget_globals->skin_painted_widget)
						{
							indx = i;
							break;
						}
					}
				}
			}
			PainterDrawThemeTab(context, r, themeTabStyle, tabDirection, indx, ntabs, state);
			break;
		}
			
		case NATIVE_MAINBAR:
		case NATIVE_STATUSBAR:
		case NATIVE_ADDRESSBAR:
		case NATIVE_VIEWBAR:
		case NATIVE_CHATBAR:
		case NATIVE_PERSONALBAR:
		case NATIVE_NAVIGATIONBAR:
		case NATIVE_PROGRESSBAR:
		case NATIVE_MAILBAR:
		{
			if(m_native_type == NATIVE_PROGRESSBAR)
			{
				int inset = -4;
				r = CGRectInset(r, inset, inset);
				PainterDrawThemeWindowHeader(context, r, theme.state);
			}
#if 0
			vd->SetColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_DARK));
			switch(GetKey()->m_type)
			{
				case SKINTYPE_BOTTOM:
					vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y), OpPoint(translatedOpRect.x+translatedOpRect.width, translatedOpRect.y), (INT)(1*scale));
					break;
				case SKINTYPE_RIGHT:
					vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y), OpPoint(translatedOpRect.x, translatedOpRect.y+translatedOpRect.height), (INT)(1*scale));
					break;
				case SKINTYPE_LEFT:
					vd->painter->DrawLine(OpPoint(translatedOpRect.x+translatedOpRect.width, translatedOpRect.y), OpPoint(translatedOpRect.x+translatedOpRect.width, translatedOpRect.y+translatedOpRect.height), (INT)(1*scale));
					break;
				case SKINTYPE_TOP:
					vd->painter->DrawLine(OpPoint(translatedOpRect.x, translatedOpRect.y/*+translatedOpRect.height*/), OpPoint(translatedOpRect.x+translatedOpRect.width, translatedOpRect.y/*+translatedOpRect.height*/), (INT)(1*scale));
					break;
			}
#endif
		}
			break;
			
		case NATIVE_CYCLER:
		{
#if 0
			COLORREF dgrayAlpha = dgray2 | 0xFF000000;
			((MacOpPainter*)vd->painter)->ClearRect();
			int radius = 12;
			
			vd->painter->SetColor(((UINT8*)&dgrayAlpha)[3], ((UINT8*)&dgrayAlpha)[2], ((UINT8*)&dgrayAlpha)[1], ((UINT8*)&dgrayAlpha)[0]);
			if((rect.height < radius*2) || (rect.width < radius*2))
			{
				vd->FillEllipse(OpRect(0, 0, rect.height, rect.height));
				vd->FillEllipse(OpRect(rect.width-rect.height, 0, rect.height, rect.height));
				vd->FillRect(OpRect(rect.height/2, 0, rect.width-rect.height, rect.height));
			}
			else
			{
				vd->FillEllipse(OpRect(0, 0, radius, radius));
				vd->FillEllipse(OpRect(rect.width-radius, 0, radius, radius));
				vd->FillEllipse(OpRect(rect.width-radius, rect.height-radius, radius, radius));
				vd->FillEllipse(OpRect(0, rect.height-radius, radius, radius));
				
				vd->FillRect(OpRect(radius/2, 0, rect.width-radius/2, rect.height));
				vd->FillRect(OpRect(0, radius/2, rect.width, rect.height-radius/2));
			}
			
#else
			UINT32 gray = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON);
			UINT32 dgray2 = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_VERYDARK);
			Draw3DBorder(vd, gray, dgray2, rect);
#endif
		}
			break;
			
		case NATIVE_DIALOG:
		{
			int inset = -4;			
			r = CGRectInset(r, inset, inset);
			PainterDrawThemeWindowHeader(context, r, theme.state);
			
			break;
		}
		case NATIVE_BROWSER_WINDOW:
		case NATIVE_WINDOW:
		case NATIVE_HOTLIST:
		{
			MacOpView *view = (MacOpView *)vd->GetOpView();
			if (view)
			{
				OpWindow* opwin = view->GetRootWindow();
				INT32 x,y;
				UINT32 w,h;
				opwin->GetOuterPos(&x,&y);
				opwin->GetOuterSize(&w,&h);
				
				HIRect rect;
				rect.origin.x = x;
				rect.origin.y = 0;
				rect.size.width = w;
				rect.size.height = window?window->height() - y:y;
				
				HIThemeBackgroundDrawInfo bgDrawInfo;
				bgDrawInfo.version = 0;
				bgDrawInfo.state = kThemeStateActive;
				bgDrawInfo.kind = kThemeBackgroundMetal;
				
				PainterHIThemeDrawBackground(context, &rect, &bgDrawInfo, kHIThemeOrientationNormal);
			}
			break;
		}
			
		case NATIVE_BROWSER:
		{
			UINT32 dgray = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_DARK);
			vd->SetColor(dgray);
			vd->painter->DrawRect(translatedOpRect);
			break;
		}
		case NATIVE_STATUS:
		case NATIVE_PROGRESS_DOCUMENT:
		case NATIVE_PROGRESS_TOTAL:
		case NATIVE_PROGRESS_IMAGES:
		case NATIVE_PROGRESS_SPEED:
		case NATIVE_PROGRESS_ELAPSED:
		case NATIVE_PROGRESS_STATUS:
		case NATIVE_PROGRESS_GENERAL:
		case NATIVE_PROGRESS_CLOCK:
		case NATIVE_IDENTIFY_AS:
		{
			//painter->PainterDrawThemePrimaryGroup(&r, theme.state);
			break;
		}
			
		case NATIVE_PROGRESS_EMPTY:
		case NATIVE_PROGRESS_FULL:
		case NATIVE_CHECKMARK:
		case NATIVE_BULLET:
		case NATIVE_CAPTION_MINIMIZE_BUTTON:
		case NATIVE_CAPTION_CLOSE_BUTTON:
		case NATIVE_CAPTION_RESTORE_BUTTON:
			
			
			/*
			 case NATIVE_SCROLLBAR_HORIZONTAL:
			 case NATIVE_SCROLLBAR_VERTICAL:
			 case NATIVE_SCROLLBAR_HORIZONTAL_KNOB:
			 case NATIVE_SCROLLBAR_VERTICAL_KNOB:
			 case NATIVE_SCROLLBAR_HORIZONTAL_LEFT:
			 case NATIVE_SCROLLBAR_HORIZONTAL_RIGHT:
			 case NATIVE_SCROLLBAR_VERTICAL_UP:
			 case NATIVE_SCROLLBAR_VERTICAL_DOWN:
			 */
			break;
			
		case NATIVE_DISCLOSURE_TRIANGLE:
		{
			buttonKind = kThemeDisclosureTriangle;
			
			// The triangle is actually the foreground skin of the button so we need to 
			// check if it is focused and then used the correct state on the button that the
			// triangle is on
			if (g_widget_globals->skin_painted_widget && g_widget_globals->skin_painted_widget->IsFocused())
				theme.adornment |= kThemeAdornmentFocus;
			
			break;
		}
			
//		case NATIVE_PAGEBAR_DOCUMENT_LOADING_ICON:
//		{
//			static CFAbsoluteTime startTime = 0;
//			if (!startTime)
//			{
//				startTime = CFAbsoluteTimeGetCurrent();
//			}
//			CFAbsoluteTime currentTime = CFAbsoluteTimeGetCurrent();
//			
//			if(r.size.width > 15)
//			{
//				HIThemeChasingArrowsDrawInfo info = {0, kThemeStateInactive, (int)((currentTime-startTime)*16)};
//				HIThemeDrawChasingArrows(&r, &info, context, kHIThemeOrientationNormal);
//				
//				OpRect rect(r.origin.x, r.origin.y, r.size.width, r.size.height);
//				((MacOpView*)vd->view->GetOpView())->DelayedInvalidate(rect, 150);
//			}
//			break;
//		}
			
		case NATIVE_TOOLTIP:
		{
			vd->SetColor32(g_op_ui_info->GetUICSSColor(CSS_VALUE_InfoBackground));
			vd->FillRect(rect);
			break;
		}
	}
	
	if (m_icon != 0)
	{
		extern OpBitmap* GetEffectBitmap(OpBitmap* bitmap, INT32 effect, INT32 effect_value);
		INT32 effect = 0;
		INT32 effect_value = 0;
		
		effect = state & SKINSTATE_DISABLED ? Image::EFFECT_DISABLED : hover_value ? Image::EFFECT_GLOW : 0;
		effect_value = effect & Image::EFFECT_GLOW ? hover_value : 0;
		
		OpBitmap* draw_icon = NULL;
//		if (m_native_type == NATIVE_LARGE_TRASH_ICON)
//			draw_icon = ((state&SKINSTATE_DISABLED) ? sLargeTrashIcon : sLargeFullTrashIcon);
//		else
			draw_icon = GetEffectBitmap(m_icon, effect, effect_value);
		
		if(draw_icon)
		{
			vd->painter->DrawBitmapScaledTransparent(draw_icon, OpRect(0,0,draw_icon->Width(),draw_icon->Height()), translatedOpRect);
			
			if(draw_icon != m_icon) // && m_native_type != NATIVE_LARGE_TRASH_ICON)
			{
				delete draw_icon;
			}	
		}		
	}
	
	if (bitmap)
	{
		bitmap->ReleasePointer();

		rect.x -= bitmap_extra;
		rect.y -= bitmap_extra;
		rect.width += 2*bitmap_extra;
		rect.height += 2*bitmap_extra;

		vd->BitmapOut(bitmap, OpRect(0, 0, rect_width, rect_height), rect);
		CGContextRelease(context);
	}
	
	if (clip_rect && !vd->HasTransform())
	{
		painter->RemoveClipRect();
		painter->SetClipRect(temp_rect3);
		painter->SetClipRect(temp_rect2);
		painter->SetClipRect(temp_rect1);
	}

	if (!bitmap)
	{
		CGContextRestoreGState(context);
	}
	else
	{
		cached_bitmap->Unlock();
		cached_bitmap->DecRef();
	}

	return OpStatus::OK;
}

OP_STATUS MacOpSkinElement::GetMargin(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	SInt32 value1;

	switch (m_native_type)
	{
		case NATIVE_MULTILINE_EDIT:
		case NATIVE_EDIT:
			RETURN_IF_ERROR(OpSkinElement::GetMargin(left, top, right, bottom, state));
			if(noErr == GetThemeMetric(kThemeMetricEditTextWhitespace, &value1))
			{
				*left += value1;
				*top += value1;
				*right += value1;
				*bottom += value1;

				return OpStatus::OK;
			}
			break;
	}

	return OpSkinElement::GetMargin(left, top, right, bottom, state);
}

OP_STATUS MacOpSkinElement::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	SInt32 value1;

	switch(m_native_type)
	{
		case NATIVE_EDIT:
			{
				RETURN_IF_ERROR(OpSkinElement::GetPadding(left, top, right, bottom, state));
				if(noErr == GetThemeMetric(kThemeMetricEditTextFrameOutset, &value1))
				{
					*left += value1;
					*top += value1;
					*right += value1;
					*bottom += value1;

					return OpStatus::OK;
				}
			}
			break;

		case NATIVE_TREEVIEW:
			{
				*left = 1;
				*top = 1;
				*right = 1;
				*bottom = 1;

				return OpStatus::OK;
			}
			break;

		case NATIVE_LISTBOX:
		case NATIVE_PANEL_TREEVIEW:
			{
				RETURN_IF_ERROR(OpSkinElement::GetPadding(left, top, right, bottom, state));
				if(noErr == GetThemeMetric(kThemeMetricListBoxFrameOutset, &value1))
				{
					*left += value1;
					*top += value1;
					*right += value1;
					*bottom += value1;

					return OpStatus::OK;
				}
			}
			break;
	}

	return OpSkinElement::GetPadding(left, top, right, bottom, state);
}

OSStatus MacOpSkinElement::PainterHIThemeDrawTabPane(CGContextRef context, const HIRect * inRect, const HIThemeTabPaneDrawInfo * inDrawInfo, HIThemeOrientation inOrientation)
{
	return HIThemeDrawTabPane(inRect, inDrawInfo, context, inOrientation);
}

OSStatus MacOpSkinElement::PainterHIThemeDrawBackground(CGContextRef context, const HIRect * inBounds, const HIThemeBackgroundDrawInfo * inDrawInfo, HIThemeOrientation inOrientation)
{
	return HIThemeDrawBackground(inBounds, inDrawInfo, context, inOrientation);
}

OSStatus MacOpSkinElement::PainterHIThemeDrawTab(CGContextRef context, const HIRect * inBounds, const HIThemeTabDrawInfo * inDrawInfo, HIThemeOrientation inOrientation)
{
	HIRect dummy_rect;
	return HIThemeDrawTab(inBounds, inDrawInfo, context, inOrientation, &dummy_rect);
}

OSStatus MacOpSkinElement::PainterDrawThemePopupArrow(CGContextRef context, const CGRect& rect, ThemeArrowOrientation orientation, ThemePopupArrowSize size, ThemeDrawState state)
{
	HIThemePopupArrowDrawInfo info = {0, state, orientation, size};
	return HIThemeDrawPopupArrow(&rect, &info, context, kHIThemeOrientationNormal);
}

OSStatus MacOpSkinElement::PainterDrawThemeEditTextFrame(CGContextRef context, const CGRect& rect, ThemeDrawState inState)
{
	HIThemeFrameDrawInfo info = {0, kHIThemeFrameTextFieldSquare, inState, false};
	return HIThemeDrawFrame(&rect, &info, context, kHIThemeOrientationNormal);
}

OSStatus MacOpSkinElement::PainterDrawThemeListBoxFrame(CGContextRef context, const CGRect& rect, ThemeDrawState inState)
{
	HIThemeFrameDrawInfo info = {0, kHIThemeFrameListBox, inState, false};
	return HIThemeDrawFrame(&rect, &info, context, kHIThemeOrientationNormal);
}

OSStatus MacOpSkinElement::PainterDrawThemeTabPane(CGContextRef context, const CGRect& rect, ThemeDrawState inState)
{
	HIThemeTabPaneDrawInfo info = {0, inState, kThemeTabNorth, kHIThemeTabSizeNormal, kHIThemeTabKindNormal, kHIThemeTabAdornmentNone};
	return HIThemeDrawTabPane(&rect, (HIThemeTabPaneDrawInfo*)&info, context, kHIThemeOrientationNormal);
}

OSStatus MacOpSkinElement::PainterDrawThemeTab(CGContextRef context, const CGRect& rect, ThemeTabStyle inStyle, ThemeTabDirection inDirection, int index, int ntabs, INT32 skin_state)
{
	HIThemeTabDrawInfo info = {1, inStyle, inDirection, kHIThemeTabSizeNormal, kHIThemeTabAdornmentTrailingSeparator, kHIThemeTabKindNormal, kHIThemeTabPositionMiddle};
	
	if(index == 0)
	{
		if(ntabs > 1)
		{
			info.position = kHIThemeTabPositionFirst;
		}
		else
		{
			info.position = kHIThemeTabPositionOnly;
		}
	}
	else
	{
		if(index < ntabs - 1 || index < 0)
		{
			info.position = kHIThemeTabPositionMiddle;
		}
		else
		{
			info.position = kHIThemeTabPositionLast;
		}
	}

	if (skin_state & SKINSTATE_RTL)
	{
		switch (info.position)
		{
			case kHIThemeTabPositionFirst:
				info.position = kHIThemeTabPositionLast; break;
			case kHIThemeTabPositionLast:
				info.position = kHIThemeTabPositionFirst; break;
		}
	}

	return HIThemeDrawTab(&rect, (HIThemeTabDrawInfo*)&info, context, kHIThemeOrientationNormal, NULL);
}

OSStatus MacOpSkinElement::PainterDrawThemeWindowHeader(CGContextRef context, const CGRect& rect, ThemeDrawState inState)
{
	HIThemeHeaderDrawInfo info = {0, inState, kHIThemeHeaderKindWindow};
	return HIThemeDrawHeader(&rect, &info, context, kHIThemeOrientationNormal);
}

OSStatus MacOpSkinElement::PainterDrawThemePlacard(CGContextRef context, const CGRect& rect, ThemeDrawState inState)
{
	HIThemePlacardDrawInfo info = {0, inState};
	return HIThemeDrawPlacard(&rect, &info, context, kHIThemeOrientationNormal);
}

OSStatus MacOpSkinElement::PainterDrawThemePrimaryGroup(CGContextRef context, const CGRect& rect, ThemeDrawState inState)
{
	HIThemeGroupBoxDrawInfo info = {0, inState, kHIThemeGroupBoxKindPrimary};
	return HIThemeDrawGroupBox(&rect, &info, context, kHIThemeOrientationNormal);
}

OSStatus MacOpSkinElement::PainterDrawThemeButton(CGContextRef context, const CGRect& rect, ThemeButtonKind inKind, const ThemeButtonDrawInfo * inNewDrawInfo, const ThemeButtonDrawInfo * inPrevInfo, HIThemeOrientation orientation)
{
	HIThemeButtonDrawInfo info = {0, inNewDrawInfo->state, inKind, inNewDrawInfo->value, inNewDrawInfo->adornment, {{0,0}}};
	static MacOpSkinElement *pressed = NULL;

	// DSK-300561: remove default adornment while any button is still pressed
	if (inNewDrawInfo->state == kThemeStatePressed)
		pressed = this;
	else if (this == pressed)
		pressed = NULL;
	if (pressed)
		info.adornment &= ~kThemeAdornmentDefault;

	if ((inKind == kThemePushButton || inKind == kThemePushButtonSmall) && (inNewDrawInfo->adornment & kThemeAdornmentDefault))
	{
		static CFAbsoluteTime startTime = 0;
		if (!startTime)
			startTime = CFAbsoluteTimeGetCurrent();
		info.animation.time.start = startTime;
		info.animation.time.current = CFAbsoluteTimeGetCurrent();
	}

	return HIThemeDrawButton(&rect, &info, context, orientation, NULL);
}

OSStatus MacOpSkinElement::PainterDrawThemeTrack(CGContextRef context, const ThemeTrackDrawInfo * drawInfo)
{
	HIThemeTrackDrawInfo info = {0, drawInfo->kind, {{drawInfo->bounds.left, drawInfo->bounds.top}, {drawInfo->bounds.right-drawInfo->bounds.left, drawInfo->bounds.bottom-drawInfo->bounds.top}}, drawInfo->min, drawInfo->max, drawInfo->value, 0, drawInfo->attributes, drawInfo->enableState, 0};
	info.trackInfo.scrollbar = drawInfo->trackInfo.scrollbar;
	return HIThemeDrawTrack(&info, NULL, context, kHIThemeOrientationNormal);
}

OSStatus MacOpSkinElement::PainterDrawThemeFocusRect(CGContextRef context, const CGRect& rect, Boolean inHasFocus)
{
	return HIThemeDrawFocusRect(&rect, inHasFocus, context, kHIThemeOrientationNormal);
}

OSStatus MacOpSkinElement::PainterDrawThemeGenericWell(CGContextRef context, const CGRect& rect, const ThemeButtonDrawInfo *inNewDrawInfo)
{
	HIThemeButtonDrawInfo info = {0, inNewDrawInfo->state, 0, inNewDrawInfo->value, inNewDrawInfo->adornment, {{0,0}}};
	
	return HIThemeDrawGenericWell(&rect, &info, context, kHIThemeOrientationNormal);
}

ThemeButtonKind MacOpSkinElement::DetermineDropdownButtonKind(OpWidget* w, CGRect& r)
{
	ThemeButtonKind buttonKind = kThemePopupButtonNormal;
	SInt32 normalh, smallh, minih;
	r = CGRectInset(r, 1, 1);
	if ((noErr == GetThemeMetric(kThemeMetricPopupButtonHeight, &normalh)))
	{
		if (r.size.height < normalh)
		{
			if (noErr == GetThemeMetric(kThemeMetricSmallPopupButtonHeight, &smallh))
			{
				if (r.size.height < smallh)
				{
					if (noErr == GetThemeMetric(kThemeMetricMiniPopupButtonHeight, &minih))
					{
						if (r.size.height < minih || r.size.width < 12)
						{
							r = CGRectInset(r, -1, -1);
							buttonKind = kThemeBevelButton;
						}
						else
						{
							buttonKind = kThemePopupButtonMini;
						}
					}
				}
				else
				{
					buttonKind = kThemePopupButtonSmall;
				}
			}
		}
	}
	return buttonKind;
}

#endif // _MACINTOSH_
