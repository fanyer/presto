/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * File : WindowsOpSkinElement.cpp
 */
#include "core/pch.h"

#ifdef SKIN_SUPPORT
#ifdef SKIN_NATIVE_ELEMENT

#include "WindowsOpSkinElement.h"

#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkin.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/display/vis_dev.h"
#ifdef VEGA_AERO_INTEGRATION
# include "modules/libvega/src/oppainter/vegaoppainter.h"
# include "modules/libvega/vegarenderer.h"

# include "platforms/windows/pi/windowsopview.h"
# include "platforms/windows/pi/windowsopwindow.h"
#endif // VEGA_AERO_INTEGRATION
#include "platforms/windows/win_handy.h"

extern HINSTANCE hInst;
extern void ConvertRGBToHSV(UINT32 color, double& h, double& s, double& v);
extern UINT32 ConvertHSVToRGB(double h, double s, double v);

#define TMT_TEXTCOLOR 3803

#define ETDT_ENABLE         0x00000002
#define ETDT_USETABTEXTURE  0x00000004
#define ETDT_ENABLETAB      (ETDT_ENABLE  | ETDT_USETABTEXTURE)

#define BP_PUSHBUTTON 1
#define BP_RADIOBUTTON 2
#define BP_CHECKBOX 3

#define HP_HEADERITEM		1
#define HP_HEADERSORTARROW	4

// HEADERSORTARROW
#define HSAS_SORTEDUP			1
#define HSAS_SORTEDDOWN			2

#define CP_DROPDOWNBUTTON 1
#define CP_DROPDOWNBUTTONRIGHT 6
#define CP_DROPDOWNBUTTONLEFT 7
#define CP_BORDER 4

#define TABP_TABITEM 1
#define TABP_TABITEMLEFTEDGE 2
#define TABP_PANE 9
#define TABP_BODY 10

#define WP_MDIMINBUTTON                   16
#define WP_MDICLOSEBUTTON                 20
#define WP_MDIRESTOREBUTTON               22

#define SBP_ARROWBTN 1
#define SBP_THUMBBTNHORZ 2
#define SBP_THUMBBTNVERT 3
#define SBP_LOWERTRACKHORZ 5
#define SBP_LOWERTRACKVERT 6
#define SBP_GRIPPERHORZ 8
#define SBP_GRIPPERVERT 9

#define TKP_TRACK 1
#define TKP_TRACKVERT 2
#define TKP_THUMB 3
#define TKP_THUMBBOTTOM 4
#define TKP_THUMBTOP 5
#define TKP_THUMBVERT 6
#define TKP_THUMBLEFT 7
#define TKP_THUMBRIGHT 8
#define TKP_TICS 9
#define TKP_TICSVERT 10

enum THUMBSTATES 
{
	TUS_NORMAL = 1,
	TUS_HOT = 2,
	TUS_PRESSED = 3,
	TUS_FOCUSED = 4,
	TUS_DISABLED = 5,
};

#define EP_EDITTEXT 1

#define SPNP_UP 1
#define SPNP_DOWN 2

// explorerbar parts
#define EBP_HEADERBACKGROUND				1
#define EBP_HEADERCLOSE						2
#define EBP_HEADERPIN						3
#define EBP_IEBARMENU						4
#define EBP_NORMALGROUPBACKGROUND			5
#define EBP_NORMALGROUPCOLLAPSE				6
#define EBP_NORMALGROUPEXPAND				7
#define EBP_NORMALGROUPHEAD					8
#define EBP_SPECIALGROUPBACKGROUND			9
#define EBP_SPECIALGROUPCOLLAPSE			10
#define EBP_SPECIALGROUPEXPAND				11
#define EBP_SPECIALGROUPHEAD				12

// explorerbar states

// HEADERCLOSE
#define EBHC_NORMAL							1
#define EBHC_HOT							2
#define EBHC_PRESSED						3

// HEADERPIN
#define EBHP_NORMAL							1
#define EBHP_HOT							2
#define EBHP_PRESSED						3
#define EBHP_SELECTEDNORMAL					4
#define EBHP_SELECTEDHOT					5
#define EBHP_SELECTEDPRESSED				6

// NORMALGROUPCOLLAPSE
#define EBNGC_NORMAL						1
#define EBNGC_HOT							2
#define EBNGC_PRESSED						3

// NORMALGROUPEXPAND
#define EBNGE_NORMAL						1
#define EBNGE_HOT							2
#define EBNGE_PRESSED						3

// SPECIALGROUPCOLLAPSE
#define EBSGC_NORMAL						1
#define EBSGC_HOT							2
#define EBSGC_PRESSED						3

// SPECIALGROUPEXPAND
#define EBSGE_NORMAL						1
#define EBSGE_HOT							2
#define EBSGE_PRESSED						3

// treeview parts
#define TVP_TREEITEM	1
#define TVP_GLYPH		2
#define TVP_BRANCH		3

#define PP_BAR 1
#define PP_BARVERT 2
#define PP_CHUNK 3
#define PP_CHUNKVERT 4

// tooltip
#define TTP_STANDARD	1
#define TTP_BALLOON		3
#define TTP_CLOSE		5

// tooltip standard states
#define TTSS_NORMAL		1
#define TTSS_LINK		2

// tooltip balloon states
#define TTBS_NORMAL		1
#define TTBS_LINK		2

// tooltip close states
#define TTCS_NORMAL		1
#define TTCS_HOT		2
#define TTCS_PRESSED	3

#define RP_GRIPPER 1
#define RP_GRIPPERVERT 2
#define RP_BAND 3
#define RP_CHEVRON 4

// toolbar parts
#define TP_BUTTON			1
#define TP_DROPDOWNBUTTON	2
#define TP_SPLITBUTTON		3
#define TP_SPLITBUTTONDROPDOWN	4
#define TP_SEPARATOR		5
#define TP_SEPARATORVERT	6

// toolbar states
#define TS_NORMAL			1

#define TTSS_NORMAL 1

#define CHEVS_NORMAL 1
#define CHEVS_HOT 2
#define CHEVS_PRESSED 3

#define TIS_NORMAL 1
#define TIS_HOT 2
#define TIS_SELECTED 3
#define TIS_DISABLED 4
#define TIS_FOCUSED	5

#define ETS_NORMAL 1
#define ETS_DISABLED 4
#define ETS_FOCUSED 5
#define ETS_READONLY 6

#define SCRBS_NORMAL 1
#define SCRBS_HOT 2
#define SCRBS_PRESSED 3
#define SCRBS_DISABLED 4

#define ABS_UPNORMAL 1
#define ABS_UPHOT 2
#define ABS_UPPRESSED 3
#define ABS_UPDISABLED 4
#define ABS_DOWNNORMAL 5
#define ABS_DOWNHOT 6
#define ABS_DOWNPRESSED 7
#define ABS_DOWNDISABLED 8
#define ABS_LEFTNORMAL 9
#define ABS_LEFTHOT 10
#define ABS_LEFTPRESSED 11
#define ABS_LEFTDISABLED 12
#define ABS_RIGHTNORMAL 13
#define ABS_RIGHTHOT 14
#define ABS_RIGHTPRESSED 15
#define ABS_RIGHTDISABLED 16
#define ABS_UPHOVER 17
#define ABS_DOWNHOVER 18
#define ABS_LEFTHOVER 19
#define ABS_RIGHTHOVER 20

enum CHECKBOXSTATES {
	CBS_UNCHECKEDNORMAL = 1,
	CBS_UNCHECKEDHOT = 2,
	CBS_UNCHECKEDPRESSED = 3,
	CBS_UNCHECKEDDISABLED = 4,
	CBS_CHECKEDNORMAL = 5,
	CBS_CHECKEDHOT = 6,
	CBS_CHECKEDPRESSED = 7,
	CBS_CHECKEDDISABLED = 8,
	CBS_MIXEDNORMAL = 9,
	CBS_MIXEDHOT = 10,
	CBS_MIXEDPRESSED = 11,
	CBS_MIXEDDISABLED = 12,
	CBS_IMPLICITNORMAL = 13,
	CBS_IMPLICITHOT = 14,
	CBS_IMPLICITPRESSED = 15,
	CBS_IMPLICITDISABLED = 16,
	CBS_EXCLUDEDNORMAL = 17,
	CBS_EXCLUDEDHOT = 18,
	CBS_EXCLUDEDPRESSED = 19,
	CBS_EXCLUDEDDISABLED = 20,
};

#define PBS_NORMAL 1
#define PBS_HOT 2
#define PBS_PRESSED 3
#define PBS_DISABLED 4
#define PBS_DEFAULTED 5

#define DNS_NORMAL 1
#define DNS_HOT 2
#define DNS_PRESSED 3
#define DNS_DISABLED 4

#define UPS_NORMAL 1
#define UPS_HOT 2
#define UPS_PRESSED 3
#define UPS_DISABLED 4

#define GLPS_CLOSED 1
#define GLPS_OPENED 2

#define MP_MENUITEM 1
#define MP_SEPARATOR 6
#define MS_NORMAL 1
#define MS_SELECTED 2
#define MS_DEMOTED 3

#define SP_PANE 1
#define SP_GRIPPER 2

#define TMT_CAPTIONFONT 801
#define TMT_MENUFONT 803
#define TMT_STATUSFONT 804
#define TMT_MSGBOXFONT 805

//---- rendering MARGIN properties ----
#define TMT_SIZINGMARGINS	3601		// margins used for 9-grid sizing
#define TMT_CONTENTMARGINS	3602	    // margins that define where content can be placed
#define TMT_CAPTIONMARGINS	3603		// margins that define where caption text can be placed

OpAutoStringHashTable<WindowsOpSkinElement::Theme> WindowsOpSkinElement::m_theme_hash_table;
UINT32 WindowsOpSkinElement::m_dpi = 0;

const char* s_windows_native_types[] =
{
	"Push Button Skin",
	"Push Default Button Skin",
	"Toolbar Button Skin",
//	"Selector Button Skin",
	"Header Button Skin",
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
	"Edit Skin",
	"MultilineEdit Skin",
	"Browser Skin",
	"Progressbar Skin",
	"Start Skin",
	"Search Skin",
	"Treeview Skin",
	"Listbox Skin",
	"Checkmark",
	"Bullet",
	"Window Skin",
	"Browser Window Skin",
	"Dialog Skin",
	"Dialog Page Skin",
	"Dialog Tab Page Skin",
	"Dialog Button Border Skin",
	"Tabs Skin",
	"Hotlist Skin",
//	"Hotlist Selector Skin",
	"Hotlist Splitter Skin",
//	"Hotlist Panel Header Skin",
	"Scrollbar Horizontal Skin",
	"Scrollbar Horizontal Knob Skin",
	"Scrollbar Horizontal Left Skin",
	"Scrollbar Horizontal Right Skin",
	"Scrollbar Vertical Skin",
	"Scrollbar Vertical Knob Skin",
	"Scrollbar Vertical Up Skin",
	"Scrollbar Vertical Down Skin",
	"Slider Horizontal Track",
	"Slider Horizontal Knob",
	"Slider Vertical Track",
	"Slider Vertical Knob",
	"Status Skin",
	"Mainbar Skin",
	"Personalbar Skin",
	"Pagebar Skin",
	"Addressbar Skin",
	"Navigationbar Skin",
	"Viewbar Skin",
	"Mailbar Skin",
	"Chatbar Skin",
	"Cycler Skin",
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
	"Dialog Warning",
	"Dialog Error",
	"Dialog Question",
	"Dialog Info",
	"Horizontal Separator",
	"Vertical Separator",
	"Disclosure Triangle Skin",
	"Sort Ascending",
	"Sort Descending",
	"Pagebar Close Button Skin",
	"Notifier Close Button Skin",
	"Tooltip Skin",
	"Notifier Skin",
	"Extender Button Skin",
	"Pagebar Floating Skin",
	"Pagebar Floating Button Skin",
	"Pagebar Thumbnail Floating Button Skin",
	"Drag Scrollbar Skin",
	"Addressbar Button Skin",
	"Left Addressbar Button Skin",
	"Right Addressbar Button Skin",
	"Personalbar Button Skin",
	"Mainbar Button Skin",
	"Pagebar Head Button Skin",
	"Pagebar Thumbnail Head Button Skin",
	"Pagebar Tail Button Skin",
	"Pagebar Thumbnail Tail Button Skin",
	"Listitem Skin",
	"Dropdown Edit Skin",
	NULL,
};

OpSkinElement* OpSkinElement::CreateNativeElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size)
{
	WindowsOpSkinElement* element = OP_NEW(WindowsOpSkinElement, (skin, name, type, size));
	if (element && element->IsSupported())
		return element;
	OP_DELETE(element);
	return NULL;
}

void OpSkinElement::FlushNativeElements()
{
	WindowsOpSkinElement::Flush();
}

/***********************************************************************************
**
**	WindowsWindowsOpSkinElement
**
***********************************************************************************/

WindowsOpSkinElement::WindowsOpSkinElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size) : OpSkinElement(skin, name, type, size)
#ifdef SKIN_CACHED_NATIVE_ELEMENTS
	, m_cached_hdc(NULL)
	, m_cached_bitmap(NULL)
#endif // SKIN_CACHED_NATIVE_ELEMENTS
	, m_close_button_bitmap(NULL)
{
	// we want to support some additional types on Windows
	INT32 i;

	m_windows_native_type = WINDOWS_NOT_SUPPORTED;

	for (int c = 0; c < MAX_CACHED_SKIN_BITMAPS; ++c)
	{
		cachedBitmaps[c] = NULL;
		cachedStates[c] = 0;
	}

	if (size != SKINSIZE_DEFAULT)
		return;

	for (i = 0; s_windows_native_types[i]; i++)
	{
		if (name.CompareI(s_windows_native_types[i]) == 0)
		{
			WindowsNativeType native_type = (WindowsNativeType) (i + 1);

			switch (native_type)
			{
#ifdef VEGA_AERO_INTEGRATION
				case WINDOWS_PAGEBAR:
				// Pagebar needs to know it's state or the glass effect will not work
#endif // VEGA_AERO_INTEGRATION
				case WINDOWS_PAGEBAR_BUTTON:
					{
						if (type == SKINTYPE_DEFAULT || type == SKINTYPE_BOTTOM || type == SKINTYPE_LEFT || type == SKINTYPE_RIGHT)
						{
							m_windows_native_type = native_type;
						}
						break;
					}
				case WINDOWS_MAINBAR:
				case WINDOWS_PERSONALBAR:
				case WINDOWS_ADDRESSBAR:
				case WINDOWS_NAVIGATIONBAR:
				case WINDOWS_VIEWBAR:
					{
						if (type == SKINTYPE_DEFAULT || type == SKINTYPE_BOTTOM)
						{
							m_windows_native_type = native_type;
						}
						break;
					}

				default:
					{
						if (type == SKINTYPE_DEFAULT)
						{
							m_windows_native_type = native_type;
						}
						break;
					}
			}
			break;
		}
	}
}

WindowsOpSkinElement::~WindowsOpSkinElement()
{
	for (int c = 0; c < MAX_CACHED_SKIN_BITMAPS; ++c)
	{
		OP_DELETE(cachedBitmaps[c]);
	}

#ifdef SKIN_CACHED_NATIVE_ELEMENTS
	if(m_cached_bitmap) DeleteObject(m_cached_bitmap);
	if(m_cached_hdc) DeleteDC(m_cached_hdc);
#endif // SKIN_CACHED_NATIVE_ELEMENTS
	if(m_close_button_bitmap) DeleteObject(m_close_button_bitmap);
}

/***********************************************************************************
**
**	GetMargin
**
***********************************************************************************/

OP_STATUS WindowsOpSkinElement::GetMargin(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	OP_STATUS status = OpSkinElement::GetMargin(left, top, right, bottom, state);

	ThemeData theme_data;
	BOOL themed = GetThemeData(state, NULL, theme_data, NULL);

	switch (m_windows_native_type)
	{
		case WINDOWS_THUMBNAIL_PAGEBAR_TAIL_BUTTON:
		case WINDOWS_PAGEBAR_TAIL_BUTTON:
		case WINDOWS_THUMBNAIL_PAGEBAR_HEAD_BUTTON:
		case WINDOWS_PAGEBAR_HEAD_BUTTON:
		case WINDOWS_MAINBAR_BUTTON:
		case WINDOWS_PERSONALBAR_BUTTON:
		case WINDOWS_ADDRESSBAR_BUTTON:
		case WINDOWS_LEFT_ADDRESSBAR_BUTTON:
		case WINDOWS_RIGHT_ADDRESSBAR_BUTTON:
		case WINDOWS_TOOLBAR_BUTTON:
		case WINDOWS_PAGEBAR_FLOATING_BUTTON:
		case WINDOWS_PAGEBAR_THUMBNAIL_FLOATING_BUTTON:
		case WINDOWS_LINK_BUTTON:
		case WINDOWS_DROPDOWN:
		{
			if (state & SKINSTATE_MINI)
			{
				if(*left) *left -= 1;
				if(*top) *top -= 1;
				if(*right) *right -= 1;
				if(*bottom) *bottom -= 1;
			}
			else if(!themed)
			{
				if(*top) *top -= 1;
				if(*bottom) *bottom -= 1;
			}
			break;
		}
		case WINDOWS_DROPDOWN_BUTTON:
		case WINDOWS_DROPDOWN_LEFT_BUTTON:
		{
			if(themed)
			{
				if (IsSystemWinVista())
				{
					*left = *right = *top = *bottom = 0;
				}
				else
				{
					*left = *right = *top = *bottom = 1;
				}
			}
			else
			{
				*left = *right = 2;
				*top = *bottom = 2;
			}
			break;
		}
		case WINDOWS_LISTITEM:
		{
			*left = 1;
			*top = 1;
			*right = 1;
			*bottom = 1;
			break;
		}
		case WINDOWS_PAGEBAR_BUTTON:
		{
			switch(GetType())
			{
				case SKINTYPE_LEFT:
				case SKINTYPE_RIGHT:
				{
					if(!themed)
					{
						*left = -2;
						*top = 0;
						*right = -2;
						*bottom = -2;
					}
					else
					{
						*left = 2;
						*top = 0;
						*right = -2;
						*bottom = 1;
						if(GetType() == SKINTYPE_RIGHT)
						{
							*right = 2;
						}
					}
				}
				break;

				case SKINTYPE_BOTTOM:
				{
					if (state & SKINSTATE_SELECTED)
					{
						if(!themed)
						{
							*left = -2;
							*top = -2;
							*right = -2;
							*bottom = 0;
						}
						else
						{
							*left = -2;
							*top = 0;
							*right = -2;
							*bottom = 0;
						}
					}
					else
					{
						if(!themed)
						{
							*left = 0;
							*top = -2;
							*right = 0;
							*bottom = 1;
						}
						else
						{
							*left = 0;
							*top = 0;
							*right = 0;
							*bottom = 1;
						}
					}
					break;
				}
				case SKINTYPE_TOP:
				case SKINTYPE_DEFAULT:
				{
					if (state & SKINSTATE_SELECTED)
					{
						if(!themed)
						{
							*left = -2;
							*top = 2;
							*right = -2;
							*bottom = 0;
						}
						else
						{
							*left = -2;
							*top = 1;
							*right = -2;
							*bottom = 0;
						}
					}
					else
					{
						if(!themed)
						{
							*left = 0;
							*top = 3;
							*right = 0;
							*bottom = 0;
						}
						else
						{
							*left = 0;
							*top = 2;
							*right = 0;
							*bottom = 0;
						}
					}
					break;
				}
			}
			break;
		}
	}

	return status;
}

/***********************************************************************************
**
**	GetPadding
**
***********************************************************************************/

OP_STATUS WindowsOpSkinElement::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	OP_STATUS status = OpSkinElement::GetPadding(left, top, right, bottom, state);

	ThemeData theme_data;
	BOOL themed = GetThemeData(state, NULL, theme_data, NULL);

	switch (m_windows_native_type)
	{
		case WINDOWS_THUMBNAIL_PAGEBAR_TAIL_BUTTON:
		case WINDOWS_PAGEBAR_TAIL_BUTTON:
		case WINDOWS_THUMBNAIL_PAGEBAR_HEAD_BUTTON:
		case WINDOWS_PAGEBAR_HEAD_BUTTON:
		case WINDOWS_MAINBAR_BUTTON:
		case WINDOWS_PERSONALBAR_BUTTON:
		case WINDOWS_ADDRESSBAR_BUTTON:
		case WINDOWS_LEFT_ADDRESSBAR_BUTTON:
		case WINDOWS_RIGHT_ADDRESSBAR_BUTTON:
		case WINDOWS_TOOLBAR_BUTTON:
		case WINDOWS_PAGEBAR_FLOATING_BUTTON:
		case WINDOWS_PAGEBAR_THUMBNAIL_FLOATING_BUTTON:
		{
			if(!themed)
			{
				if(*top) *top -= 1;
				if(*bottom) *bottom -= 1;
			}
			break;
		}

		case WINDOWS_PAGEBAR_BUTTON:
		{
			switch(GetType())
			{
				case SKINTYPE_LEFT:
				case SKINTYPE_RIGHT:
				{
					if(!themed)
					{
						*left = 2;
						*top = 4;
						*right = 4;
						*bottom = 2;
						if(GetType() == SKINTYPE_LEFT)
						{
							*right -= 2;
						}
					}
					break;
				}

				case SKINTYPE_TOP:
				case SKINTYPE_DEFAULT:
				{
					if (state & SKINSTATE_SELECTED)
					{
						*left = 6;
						*top = 5;
						*right = 6;
						*bottom = 4;
					}
					else
					{
						*left = 4;
						*top = 5;
						*right = 4;
						*bottom = 3;
					}
					break;
				}
				case SKINTYPE_BOTTOM:
				{
					if (state & SKINSTATE_SELECTED)
					{
						*left = 6;
						*top = 4;
						*right = 6;
						*bottom = 5;
					}
					else
					{
						*left = 4;
						*top = 3;
						*right = 4;
						*bottom = 5;
					}
					break;
				}
			}
			break;
		}

	}

	return status;
}

/***********************************************************************************
**
**	GetSpacing
**
***********************************************************************************/

OP_STATUS WindowsOpSkinElement::GetSpacing(INT32* spacing, INT32 state)
{
	OP_STATUS status = OpSkinElement::GetSpacing(spacing, state);

	ThemeData theme_data;
	if (!GetThemeData(state, NULL, theme_data, NULL))
	{
		return status;
	}

	switch (m_windows_native_type)
	{
		case WINDOWS_PAGEBAR:
		{
			*spacing = 0;
			return OpStatus::OK;
		}
	}

	return status;
}

/***********************************************************************************
**
**	GetTextColor
**
***********************************************************************************/

OP_STATUS WindowsOpSkinElement::GetTextColor(UINT32* color, INT32 state)
{
	OP_STATUS status = OpSkinElement::GetTextColor(color, state);

	switch (m_windows_native_type)
	{
		case WINDOWS_TOOLBAR_BUTTON:
		case WINDOWS_PUSH_BUTTON:
		case WINDOWS_PUSH_DEFAULT_BUTTON:
		{
			if (state&SKINSTATE_DISABLED)
				*color = GetSysColor(COLOR_GRAYTEXT);
			else
				*color = GetSysColor(COLOR_BTNTEXT);
			*color = OP_RGB(OP_GET_R_VALUE(*color), OP_GET_G_VALUE(*color), OP_GET_B_VALUE(*color));
			return OpStatus::OK;
		}
/*		case WINDOWS_HOTLIST_SELECTOR:
		{
			*color = GetSysColor(COLOR_HIGHLIGHTTEXT);
			*color = OP_RGB(OP_GET_R_VALUE(*color), OP_GET_G_VALUE(*color), OP_GET_B_VALUE(*color));
			return OpStatus::OK;
		}
*/		case WINDOWS_TOOLTIP:
		case WINDOWS_NOTIFIER:
		{
			*color = GetSysColor(COLOR_INFOTEXT);
			*color = OP_RGB(OP_GET_R_VALUE(*color), OP_GET_G_VALUE(*color), OP_GET_B_VALUE(*color));
			return OpStatus::OK;
		}

		case WINDOWS_PROGRESS_FULL:
		{
			ThemeData theme_data;
			if (GetThemeData(state, NULL, theme_data, NULL))
			{
				if (state&SKINSTATE_DISABLED)
					*color = GetSysColor(COLOR_GRAYTEXT);
				else
					*color = GetSysColor(COLOR_BTNTEXT);
				*color = OP_RGB(OP_GET_R_VALUE(*color), OP_GET_G_VALUE(*color), OP_GET_B_VALUE(*color));
			}
			else
			{
				*color = GetSysColor(COLOR_HIGHLIGHTTEXT);
				*color = OP_RGB(OP_GET_R_VALUE(*color), OP_GET_G_VALUE(*color), OP_GET_B_VALUE(*color));
				return OpStatus::OK;
			}
		}
	}
	return status;
}

/***********************************************************************************
**
**	GetBorderWidth
**
***********************************************************************************/

OP_STATUS WindowsOpSkinElement::GetBorderWidth(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	*left = *top = *right = *bottom = 2;
	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetSize
**
***********************************************************************************/

OP_STATUS WindowsOpSkinElement::GetSize(INT32* width, INT32* height, INT32 state)
{
	OP_ASSERT(m_windows_native_type != WINDOWS_EDIT);
	OP_ASSERT(m_windows_native_type != WINDOWS_MULTILINE_EDIT);

	switch (m_windows_native_type)
	{
		case WINDOWS_SLIDER_HORIZONTAL_KNOB:
		{
			*width = 11;
			*height = 16;		
			return OpStatus::OK;
		}
		case WINDOWS_SLIDER_VERTICAL_KNOB:
		{
			*width = 16;
			*height = 11;		
			return OpStatus::OK;
		}
		case WINDOWS_DIALOG_WARNING:
		case WINDOWS_DIALOG_ERROR:
		case WINDOWS_DIALOG_QUESTION:
		case WINDOWS_DIALOG_INFO:
		{
			*width = GetSystemMetrics(SM_CXICON);
			*height = GetSystemMetrics(SM_CYICON);		
			return OpStatus::OK;
		}

		case WINDOWS_CHECKMARK:
		case WINDOWS_BULLET:
		{
			*width = GetSystemMetrics(SM_CXMENUCHECK);
			*height = GetSystemMetrics(SM_CYMENUCHECK);
			return OpStatus::OK;
		}
		case WINDOWS_DISCLOSURE_TRIANGLE:
		case WINDOWS_NOTIFIER_CLOSE_BUTTON:
		case WINDOWS_PAGEBAR_CLOSE_BUTTON:
		case WINDOWS_SORT_ASCENDING:
		case WINDOWS_SORT_DESCENDING:
		case WINDOWS_EDIT:
		case WINDOWS_MULTILINE_EDIT:
		case WINDOWS_DROPDOWN_EDIT_SKIN:
		{
			ThemeData theme_data;

			if (GetThemeData(state, NULL, theme_data, NULL))
			{
				SIZE size;

				HDC hdc = GetDC(g_main_hwnd);
				if(hdc)
				{
					int part = 0;
					int part_state = 0;
					if (state & SKINSTATE_DISABLED)
						part_state = PBS_DISABLED;
					else if (state & SKINSTATE_PRESSED)
						part_state	= PBS_PRESSED;
					else if (state & SKINSTATE_HOVER)
						part_state	= PBS_HOT;
					else
						part_state	= PBS_NORMAL;

					switch (m_windows_native_type)
					{
						case WINDOWS_MULTILINE_EDIT:
						case WINDOWS_EDIT:
						case WINDOWS_DROPDOWN_EDIT_SKIN:
						{
							break;
						}

						case WINDOWS_DISCLOSURE_TRIANGLE:
						{
							part = (state & SKINSTATE_OPEN || state & SKINSTATE_SELECTED) ? EBP_NORMALGROUPEXPAND : EBP_NORMALGROUPCOLLAPSE;
							break;
						}
						case WINDOWS_NOTIFIER_CLOSE_BUTTON:
						case WINDOWS_PAGEBAR_CLOSE_BUTTON:
						{
							part = TTP_CLOSE;
							break;
						}
						case WINDOWS_SORT_ASCENDING:
						case WINDOWS_SORT_DESCENDING:
						{
							part = HP_HEADERSORTARROW;

							if (m_windows_native_type == WINDOWS_SORT_ASCENDING)
							{
								part_state	= HSAS_SORTEDUP;
							}
							else
							{
								part_state	= HSAS_SORTEDDOWN;
							}
							break;
						}
					}
					HRESULT hr = GetThemePartSize(theme_data.theme, hdc, part, part_state, NULL, TS_TRUE, &size);
					if(SUCCEEDED(hr))
					{
						// some defaults..
						*width = size.cx;
						*height = size.cy;
					}
					ReleaseDC(g_main_hwnd, hdc);
				}
			}
			else
			{
				switch (m_windows_native_type)
				{
					case WINDOWS_DISCLOSURE_TRIANGLE:
					case WINDOWS_NOTIFIER_CLOSE_BUTTON:
					case WINDOWS_PAGEBAR_CLOSE_BUTTON:
					{
						*width = 15;
						*height = 16;
						break;
					}
					case WINDOWS_SORT_ASCENDING:
					case WINDOWS_SORT_DESCENDING:
					{
						*width = 13;
						*height = 5;
						break;
					}
				}
			}
			return OpStatus::OK;
		}

		case WINDOWS_PUSH_BUTTON:
		case WINDOWS_PUSH_DEFAULT_BUTTON:
			*width = GetPhysicalPixelFromRelative(75);
			*height = GetPhysicalPixelFromRelative(23);
			return OpStatus::OK;

	}
	return OpSkinElement::GetSize(width, height, state);
}

/***********************************************************************************
**
**	OverrideDefaults
**
***********************************************************************************/

void WindowsOpSkinElement::OverrideDefaults(OpSkinElement::StateElement* se)
{
	ThemeData theme_data;
	const BOOL themed = GetThemeData(se->m_state, NULL, theme_data, NULL);

	switch (m_windows_native_type)
	{
	case WINDOWS_BROWSER:
		se->m_padding_left = se->m_padding_right = se->m_padding_top = se->m_padding_bottom = 0;
		break;

	case WINDOWS_EDIT:
	case WINDOWS_DROPDOWN_EDIT_SKIN:
		if (se->m_state & SKINSTATE_MINI)
		{
			se->m_padding_top = se->m_padding_bottom = 0;
			se->m_margin_top = se->m_margin_bottom = 0;
		}
		else
		{
			se->m_margin_left = se->m_margin_right = se->m_margin_top = 3;
			se->m_margin_bottom = 1;
		}
		break;
	case WINDOWS_DROPDOWN:
		// Same margin as WINDOWS_EDIT to align them and give them same height in toolbars
		if (se->m_state & SKINSTATE_MINI)
		{
			se->m_margin_top = se->m_margin_bottom = 0;
		}
		else
		{
			se->m_margin_left = se->m_margin_right = se->m_margin_top = 3;
			se->m_margin_bottom = 1;
		}
		break;
	case WINDOWS_DROPDOWN_BUTTON:
	case WINDOWS_DROPDOWN_LEFT_BUTTON:
		se->m_padding_left = se->m_padding_right = se->m_padding_top = se->m_padding_bottom = 0;
		break;
	case WINDOWS_RADIO_BUTTON:
	case WINDOWS_CHECKBOX:
		se->m_width = se->m_height = 13;
		break;
	case WINDOWS_DIALOG:
		if (se->m_state&SKINSTATE_DISABLED)
		{
			se->m_text_color = GetSysColor(COLOR_GRAYTEXT);
			se->m_text_color = OP_RGB(OP_GET_R_VALUE(se->m_text_color), OP_GET_G_VALUE(se->m_text_color), OP_GET_B_VALUE(se->m_text_color));
		}
		se->m_padding_left = se->m_padding_right = se->m_padding_top = se->m_padding_bottom = GetPhysicalPixelFromRelative(11);
		break;
	case WINDOWS_DIALOG_PAGE:
		se->m_spacing = 10;
		se->m_padding_left = se->m_padding_right = se->m_padding_top = se->m_padding_bottom = 0;
		se->m_margin_left = se->m_margin_right = se->m_margin_top = se->m_margin_bottom = 0;
		break;
	case WINDOWS_DIALOG_TAB_PAGE:
		se->m_padding_left = se->m_padding_right = se->m_padding_top = se->m_padding_bottom = 10;
		break;
	case WINDOWS_DIALOG_BUTTON_BORDER:
		se->m_padding_left = se->m_padding_right = se->m_padding_top = se->m_padding_bottom = 0;
		se->m_margin_left = se->m_margin_right = se->m_margin_bottom = 0;
		se->m_margin_top = 11;
		break;
	case WINDOWS_TAB_BUTTON:
		if (se->m_state & SKINSTATE_SELECTED)
		{
			se->m_padding_left = se->m_padding_right = 6;
			se->m_padding_top = 5;
			se->m_padding_bottom = 4;
			se->m_margin_left = se->m_margin_right = -2;
			se->m_margin_top = 0;
		}
		else
		{
			se->m_padding_left = se->m_padding_right = se->m_padding_top = 4;
			se->m_margin_top = 1;
		}
		break;
	case WINDOWS_TABS:
		se->m_padding_top = se->m_padding_bottom = 0;
		if (themed)
		{
			if (se->m_state & SKINSTATE_RTL)
				// Otherwise, the tab buttons are not aligned with the right
				// edge of the dialog.
				se->m_padding_left += 2;
		}
		break;
	case WINDOWS_TOOLBAR_BUTTON:
		// Shouldn't override at all
		break;
	default:
		se->m_margin_bottom = se->m_margin_left = se->m_margin_right = se->m_margin_top = GetPhysicalPixelFromRelative(7);
		break;
	}
}

/***********************************************************************************
**
**	Draw
**
***********************************************************************************/
OP_STATUS WindowsOpSkinElement::Draw(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args)
{
	if (rect.IsEmpty())
		return OpStatus::OK;

	if (m_windows_native_type == WINDOWS_NOT_SUPPORTED)
	{
		return OpSkinElement::Draw(vd, rect, state, args);
	}

#ifdef VEGA_AERO_INTEGRATION
	if (vd->GetOpView() && ((WindowsOpView*)vd->GetOpView())->GetNativeWindow() && 
		((WindowsOpView*)vd->GetOpView())->GetNativeWindow()->m_titlebar_height)
	{
		if (m_windows_native_type == WINDOWS_PAGEBAR_BUTTON && !(state & SKINSTATE_SELECTED))
		{
			// Compensate for a 1px border in screen space
			rect.height-=vd->ScaleToDoc(1);
		}
	}
#endif // VEGA_AERO_INTEGRATION
	OpBitmap* curBmp = NULL;
	for (int i = 0; i < MAX_CACHED_SKIN_BITMAPS; ++i)
	{
		if (cachedStates[i] == state && cachedBitmaps[i] && (INT32)cachedBitmaps[i]->Width() == rect.width && (INT32)cachedBitmaps[i]->Height() == rect.height)
		{
			// Move to front
			curBmp = cachedBitmaps[i];
			for (int j = i; j > 0; --j)
			{
				cachedStates[j] = cachedStates[j-1];
				cachedBitmaps[j] = cachedBitmaps[j-1];
			}
			cachedStates[0] = state;
			cachedBitmaps[0] = curBmp;
			break;
		}
	}
	OP_STATUS status = OpStatus::OK;

	if (!curBmp)
	{
		RECT tmpr;
		tmpr.left = 0;
		tmpr.top = 0;
		tmpr.right = rect.width;
		tmpr.bottom = rect.height;

		OpBitmap* bmp;
		RETURN_IF_ERROR(OpBitmap::Create(&bmp, rect.width, rect.height));

		UINT32* tdata = NULL;
		BITMAPINFO bi;
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = rect.width;
		bi.bmiHeader.biHeight = -(int)rect.height;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		bi.bmiHeader.biSizeImage = 0;
		bi.bmiHeader.biXPelsPerMeter = 0;
		bi.bmiHeader.biYPelsPerMeter = 0;
		bi.bmiHeader.biClrUsed = 0;
		bi.bmiHeader.biClrImportant = 0;

		HBITMAP bitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&tdata, NULL, 0);
		if (!bitmap)
		{
			OP_DELETE(bmp);
			return OpStatus::ERR_NO_MEMORY;
		}

		HDC dc = CreateCompatibleDC(NULL);
		if (!dc)
		{
			DeleteObject(bitmap);
			OP_DELETE(bmp);
			return OpStatus::ERR_NO_MEMORY;
		}
		HBITMAP oldb = (HBITMAP)SelectObject(dc, bitmap);

		UINT32* data = (UINT32*)bmp->GetPointer(OpBitmap::ACCESS_WRITEONLY);
		if (!data)
			status = OpStatus::ERR;
		else
		{
			op_memset(tdata, 0, rect.width*rect.height*4);
			if (!DrawThemeStyled(dc, state, tmpr))
			{
				if(!DrawOldStyled(dc, state, tmpr))
				{
					status = OpStatus::ERR;
				}
			}
			op_memcpy(data, tdata, rect.width*rect.height*4);

			op_memset(tdata, 255, rect.width*rect.height*4);
			if (!DrawThemeStyled(dc, state, tmpr))
			{
				if(!DrawOldStyled(dc, state, tmpr))
				{
					status = OpStatus::ERR;
				}
			}
			for (int i = 0; i < rect.width*rect.height; ++i)
			{
				data[i] = data[i]&0xffffff;
				data[i] |= (255-((tdata[i]&0xff)-(data[i]&0xff)))<<24;
			}

			bmp->ReleasePointer();
		}

		SelectObject(dc, oldb);
		DeleteDC(dc);
		DeleteObject(bitmap);

		if (OpStatus::IsSuccess(status))
		{
			OP_DELETE(cachedBitmaps[MAX_CACHED_SKIN_BITMAPS-1]);
			for (int cp = MAX_CACHED_SKIN_BITMAPS-1; cp > 0; --cp)
			{
				cachedStates[cp] = cachedStates[cp-1];
				cachedBitmaps[cp] = cachedBitmaps[cp-1];
			}
			cachedStates[0] = state;
			cachedBitmaps[0] = bmp;
			curBmp = bmp;
		}
		else
		{
			OP_DELETE(bmp);
		}
	}
	if (curBmp)
		vd->BitmapOut(curBmp, OpRect(0, 0, curBmp->Width(), curBmp->Height()), rect);
	else
		status = OpStatus::ERR;
	return status;
}

/***********************************************************************************
**
**	GetIcon
**
***********************************************************************************/

HICON WindowsOpSkinElement::GetIcon(WindowsNativeType type)
{
	uni_char* id = NULL;

	switch (type)
	{
		case WINDOWS_DIALOG_WARNING:		id = IDI_WARNING; break;
		case WINDOWS_DIALOG_ERROR:		id = IDI_ERROR; break;
		case WINDOWS_DIALOG_QUESTION:	id = IDI_QUESTION; break;
		case WINDOWS_DIALOG_INFO:		id = IDI_INFORMATION; break;
	}

	return LoadIcon(NULL, id); 
}

/***********************************************************************************
**
**	GetTheme
**
***********************************************************************************/

HTHEME WindowsOpSkinElement::GetTheme(const uni_char* theme_name)
{
	Theme* theme = NULL;

	m_theme_hash_table.GetData(theme_name, &theme);

	if (!theme)
	{
		HTHEME theme_handle = OpenThemeData(g_main_hwnd, theme_name);
		theme = new Theme(theme_name, theme_handle);
		if(!theme)
		{
			return NULL;
		}
		m_theme_hash_table.Add(theme->GetThemeName(), theme);
	}
	return theme->GetThemeHandle();
}

/***********************************************************************************
**
**	GetThemeData
**
***********************************************************************************/

BOOL WindowsOpSkinElement::GetThemeData(const INT32 state, RECT* draw_rect, ThemeData& theme_data, HDC hdc)
{
	RECT rect = {0, 0, 0, 0};

	if (draw_rect)
		rect = *draw_rect;

	theme_data.theme		= NULL;
	theme_data.part		= 0;
	theme_data.part_state	= 0;
	theme_data.edge_type	= 0;
	theme_data.border_type	= 0;
	theme_data.empty		= FALSE;

	if (state & SKINSTATE_DISABLED)
		theme_data.part_state	= PBS_DISABLED;
	else if (state & SKINSTATE_PRESSED)
		theme_data.part_state	= PBS_PRESSED;
	else if (state & SKINSTATE_HOVER)
		theme_data.part_state	= PBS_HOT;
	else
		theme_data.part_state	= PBS_NORMAL;

	switch (m_windows_native_type)
	{
		case WINDOWS_PUSH_DEFAULT_BUTTON:
		case WINDOWS_PUSH_BUTTON:
//		case WINDOWS_SELECTOR_BUTTON:
		{
			theme_data.theme		= GetTheme(UNI_L("button"));
			theme_data.part		= BP_PUSHBUTTON;

			if (theme_data.part_state == PBS_NORMAL)
			{
				if (state & SKINSTATE_SELECTED)
					theme_data.part_state	= PBS_PRESSED;
				else if (m_windows_native_type == WINDOWS_PUSH_DEFAULT_BUTTON)
					theme_data.part_state	= PBS_DEFAULTED;
//				else if (m_windows_native_type == WINDOWS_SELECTOR_BUTTON)
//					theme_data.empty = TRUE;
			}
			break;
		}

		case WINDOWS_CAPTION_MINIMIZE_BUTTON:
		{
			theme_data.theme	= GetTheme(UNI_L("window"));
			theme_data.part		= WP_MDIMINBUTTON;
			break;
		}
		case WINDOWS_CAPTION_CLOSE_BUTTON:
		{
			theme_data.theme	= GetTheme(UNI_L("window"));
			theme_data.part		= WP_MDICLOSEBUTTON;
			break;
		}
		case WINDOWS_CAPTION_RESTORE_BUTTON:
		{
			theme_data.theme	= GetTheme(UNI_L("window"));
			theme_data.part		= WP_MDIRESTOREBUTTON;
			break;
		}

		case WINDOWS_RADIO_BUTTON:
		case WINDOWS_CHECKBOX:
		{
			theme_data.theme		= GetTheme(UNI_L("button"));
			theme_data.part		= m_windows_native_type == WINDOWS_RADIO_BUTTON ? BP_RADIOBUTTON : BP_CHECKBOX;

			if (state & SKINSTATE_INDETERMINATE) 
				theme_data.part_state += 8; // Use the CBS_MIXED* states 
			else if (state & SKINSTATE_SELECTED) 
				theme_data.part_state += 4; // Use the CBS_CHECKED* states

			break;
		}

		case WINDOWS_HEADER_BUTTON:
		{
			theme_data.theme		= GetTheme(UNI_L("header"));
			theme_data.part		= HP_HEADERITEM;
			break;
		}

		case WINDOWS_WINDOW:
		case WINDOWS_BROWSER_WINDOW:
		case WINDOWS_HOTLIST:
		case WINDOWS_DRAG_SCROLLBAR:
		{
			theme_data.theme		= GetTheme(UNI_L("rebar"));
			theme_data.part		= 0;  // should really be RP_BAND, but there seems to be a bug in theme code.. using 0 works as expected
			theme_data.part_state	= 6;
			break;
		}
//		case WINDOWS_HOTLIST_PANEL_HEADER:
		case WINDOWS_HOTLIST_SPLITTER:
		case WINDOWS_MAINBAR:
		case WINDOWS_PERSONALBAR:
		case WINDOWS_PAGEBAR:
		case WINDOWS_ADDRESSBAR:
		case WINDOWS_NAVIGATIONBAR:
		case WINDOWS_MAILBAR:
		case WINDOWS_CHATBAR:
		case WINDOWS_VIEWBAR:
		{
			//If we do not return a theme for the pagebar at the bottom, the spacing and padding of the
			//skin.ini file is used instead of the spacing and padding hard-coded above
			//(which are used for themes only). 
			if (GetType() == SKINTYPE_BOTTOM && m_windows_native_type != WINDOWS_PAGEBAR)
				break;

			theme_data.theme		= GetTheme(UNI_L("rebar"));
			theme_data.part		= RP_BAND;
			theme_data.edge_type	= EDGE_ETCHED;
			theme_data.border_type = BF_TOP;

			switch(m_windows_native_type)
			{
				// we don't want a top line on these toolbars as they would interfere with the seamless gradient from the tabs
//				case WINDOWS_HOTLIST_PANEL_HEADER:
				case WINDOWS_ADDRESSBAR:
				case WINDOWS_MAILBAR:
				case WINDOWS_CHATBAR:
				case WINDOWS_HOTLIST_SPLITTER:
					{
						theme_data.edge_type	= 0;
						theme_data.border_type	= BF_TOP;
					}
					break;

			}
			break;
		}

		case WINDOWS_THUMBNAIL_PAGEBAR_TAIL_BUTTON:
		case WINDOWS_PAGEBAR_TAIL_BUTTON:
		case WINDOWS_THUMBNAIL_PAGEBAR_HEAD_BUTTON:
		case WINDOWS_PAGEBAR_HEAD_BUTTON:
		case WINDOWS_MAINBAR_BUTTON:
		case WINDOWS_PERSONALBAR_BUTTON:
		case WINDOWS_ADDRESSBAR_BUTTON:
		case WINDOWS_LEFT_ADDRESSBAR_BUTTON:
		case WINDOWS_RIGHT_ADDRESSBAR_BUTTON:
		case WINDOWS_LINK_BUTTON:
		case WINDOWS_TOOLBAR_BUTTON:
		case WINDOWS_EXTENDER_BUTTON:
		case WINDOWS_PAGEBAR_FLOATING_BUTTON:
		case WINDOWS_PAGEBAR_THUMBNAIL_FLOATING_BUTTON:
		{
			theme_data.theme		= GetTheme(UNI_L("toolbar"));
			theme_data.part		= TP_BUTTON;

			if ((state & SKINSTATE_SELECTED) && theme_data.part_state != PBS_DISABLED && theme_data.part_state != PBS_PRESSED)
				theme_data.part_state += 4;

			break;
		}

		case WINDOWS_TREEVIEW:
		{
			theme_data.theme		= GetTheme(UNI_L("treeview"));
			break;
		}

		case WINDOWS_LISTBOX:
		case WINDOWS_START:
		case WINDOWS_SEARCH:
		{
			theme_data.theme		= GetTheme(UNI_L("listview"));
			break;
		}
		case WINDOWS_MULTILINE_EDIT:
		case WINDOWS_EDIT:
		case WINDOWS_DROPDOWN_EDIT_SKIN:
		{
			theme_data.theme		= GetTheme(UNI_L("edit"));
			break;
		}

		case WINDOWS_DROPDOWN:
		{
			theme_data.theme		= GetTheme(UNI_L("combobox"));
			// Todo: Update the hover state on the dropdown skin to get the blue border which match the rounded button edge.
			if (IsSystemWinVista())
				theme_data.part		= CP_BORDER;
			break;
		}

		case WINDOWS_DROPDOWN_BUTTON:
		{
			theme_data.theme		= GetTheme(UNI_L("combobox"));
			if (IsSystemWinVista())
				theme_data.part		= CP_DROPDOWNBUTTONRIGHT;
			else
				theme_data.part		= CP_DROPDOWNBUTTON;
			break;
		}
		case WINDOWS_DROPDOWN_LEFT_BUTTON:
		{
			theme_data.theme		= GetTheme(UNI_L("combobox"));
			if (IsSystemWinVista())
				theme_data.part		= CP_DROPDOWNBUTTONLEFT;
			else
				theme_data.part		= CP_DROPDOWNBUTTON;
			break;
		}

		case WINDOWS_BROWSER:
		{
			theme_data.theme		= GetTheme(UNI_L("rebar"));
			theme_data.part		= RP_BAND;
			theme_data.edge_type	= EDGE_ETCHED;
			theme_data.border_type = BF_LEFT | BF_TOP | BF_BOTTOM;
			break;
		}
/*
		case WINDOWS_HOTLIST_SELECTOR:
		{
			theme_data.theme		= GetTheme(UNI_L("rebar"));
			theme_data.part		= RP_BAND;
			theme_data.edge_type	= EDGE_ETCHED;
			theme_data.border_type = BF_RECT;
			break;

		}
*/
		case WINDOWS_DIALOG_TAB_PAGE:
		{
			theme_data.theme		= GetTheme(UNI_L("tab"));
			theme_data.part		= TABP_PANE;
			rect.top	-= 1;
			break;
		}

		case WINDOWS_TABS:
		{
			theme_data.theme		= GetTheme(UNI_L("tab"));
			theme_data.part		= TABP_PANE;
			rect.top	= rect.bottom - 1;
			rect.bottom = rect.top + 10;
			break;
		}

		case WINDOWS_PAGEBAR_BUTTON:
		case WINDOWS_TAB_BUTTON:
		{
			theme_data.theme		= GetTheme(UNI_L("tab"));
			theme_data.part			= TABP_TABITEM;

			if (state & SKINSTATE_SELECTED)
			{
				theme_data.part_state = TIS_SELECTED;
			}
			else if(state & SKINSTATE_HOVER)
			{
				theme_data.part_state = TIS_HOT;
			}
			rect.bottom += 1;
			break;
		}

		case WINDOWS_SCROLLBAR_HORIZONTAL:
		{
			theme_data.theme		= GetTheme(UNI_L("scrollbar"));
			theme_data.part		= SBP_LOWERTRACKHORZ;
			break;
		}

		case WINDOWS_SCROLLBAR_VERTICAL:
		{
			theme_data.theme		= GetTheme(UNI_L("scrollbar"));
			theme_data.part		= SBP_LOWERTRACKVERT;
			break;
		}

		case WINDOWS_SCROLLBAR_HORIZONTAL_KNOB:
		{
			theme_data.theme		= GetTheme(UNI_L("scrollbar"));
			theme_data.part		= SBP_THUMBBTNHORZ;
			break;
		}
		case WINDOWS_SCROLLBAR_VERTICAL_KNOB:
		{
			theme_data.theme		= GetTheme(UNI_L("scrollbar"));
			theme_data.part		= SBP_THUMBBTNVERT;
			break;
		}
		case WINDOWS_SCROLLBAR_HORIZONTAL_LEFT:
		case WINDOWS_SCROLLBAR_HORIZONTAL_RIGHT:
		case WINDOWS_SCROLLBAR_VERTICAL_UP:
		case WINDOWS_SCROLLBAR_VERTICAL_DOWN:
		{
			theme_data.theme		= GetTheme(UNI_L("scrollbar"));
			theme_data.part		= SBP_ARROWBTN;

			if (theme_data.part_state == PBS_NORMAL && (state & SKINSTATE_FOCUSED) && IsSystemWinVista())
			{
				switch (m_windows_native_type)
				{
				case WINDOWS_SCROLLBAR_VERTICAL_DOWN:
					theme_data.part_state = ABS_DOWNHOVER;
					break;
				case WINDOWS_SCROLLBAR_HORIZONTAL_LEFT:
					theme_data.part_state = ABS_LEFTHOVER;
					break;
				case WINDOWS_SCROLLBAR_HORIZONTAL_RIGHT:
					theme_data.part_state = ABS_RIGHTHOVER;
					break;
				default:
					theme_data.part_state = ABS_UPHOVER;
					break;
				};
			}
			else
			{
				if (m_windows_native_type == WINDOWS_SCROLLBAR_VERTICAL_DOWN)
					theme_data.part_state += 4;
				else if (m_windows_native_type == WINDOWS_SCROLLBAR_HORIZONTAL_LEFT)
					theme_data.part_state += 8;
				else if (m_windows_native_type == WINDOWS_SCROLLBAR_HORIZONTAL_RIGHT)
					theme_data.part_state += 12;
			}

			break;
		}
		case WINDOWS_SLIDER_HORIZONTAL_TRACK:
			theme_data.theme		= GetTheme(UNI_L("trackbar"));
			theme_data.part		= TKP_TRACK;
			break;

		case WINDOWS_SLIDER_HORIZONTAL_KNOB:
			theme_data.theme		= GetTheme(UNI_L("trackbar"));
			theme_data.part		= TKP_THUMB;

			// for some reason, this element is using non-standard state flags
			if (state & SKINSTATE_DISABLED)
				theme_data.part_state	= TUS_DISABLED;
			else if (state & SKINSTATE_PRESSED)
				theme_data.part_state	= TUS_PRESSED;
			else if (state & SKINSTATE_HOVER)
				theme_data.part_state	= TUS_HOT;
			else
				theme_data.part_state	= TUS_NORMAL;
			break;

		case WINDOWS_SLIDER_VERTICAL_TRACK:
			theme_data.theme		= GetTheme(UNI_L("trackbar"));
			theme_data.part		= TKP_TRACKVERT;
			break;

		case WINDOWS_SLIDER_VERTICAL_KNOB:
			theme_data.theme		= GetTheme(UNI_L("trackbar"));
			theme_data.part		= TKP_THUMBVERT;
			// for some reason, this element is using non-standard state flags
			if (state & SKINSTATE_DISABLED)
				theme_data.part_state	= TUS_DISABLED;
			else if (state & SKINSTATE_PRESSED)
				theme_data.part_state	= TUS_PRESSED;
			else if (state & SKINSTATE_HOVER)
				theme_data.part_state	= TUS_HOT;
			else
				theme_data.part_state	= TUS_NORMAL;
			break;

		case WINDOWS_STATUS:
		{
			theme_data.theme		= GetTheme(UNI_L("status"));
			theme_data.part		= SP_PANE;
			theme_data.edge_type	= 0;
			theme_data.border_type = 0;
			break;
		}

		case WINDOWS_PROGRESS_IMAGES:
		case WINDOWS_PROGRESS_SPEED:
		case WINDOWS_PROGRESS_ELAPSED:
		case WINDOWS_PROGRESS_STATUS:
		case WINDOWS_PROGRESS_GENERAL:
		case WINDOWS_PROGRESS_CLOCK:
		case WINDOWS_IDENTIFY_AS:
		{
			theme_data.theme		= GetTheme(UNI_L("rebar"));
			theme_data.part		= RP_BAND;
			theme_data.edge_type	= BDR_SUNKENOUTER;
			theme_data.border_type = BF_RECT;
			break;
		}

		case WINDOWS_PROGRESS_EMPTY:
		case WINDOWS_PROGRESS_FULL:
		{
			theme_data.theme		= GetTheme(UNI_L("progress"));
			theme_data.part			= PP_BAR;
			break;
		}
		case WINDOWS_DISCLOSURE_TRIANGLE:
		{
			theme_data.theme	= GetTheme(UNI_L("explorerbar"));

			if (state & SKINSTATE_OPEN || state & SKINSTATE_SELECTED)
			{
				theme_data.part		= EBP_NORMALGROUPCOLLAPSE;
			}
			else
			{
				theme_data.part		= EBP_NORMALGROUPEXPAND;
			}
			break;
		}
		case WINDOWS_NOTIFIER_CLOSE_BUTTON:
		case WINDOWS_PAGEBAR_CLOSE_BUTTON:
		{
			theme_data.theme	= GetTheme(UNI_L("tooltip"));
			theme_data.part		= TTP_CLOSE;
			break;
		}
		case WINDOWS_SORT_ASCENDING:
		case WINDOWS_SORT_DESCENDING:
		{
			if(!IsSystemWinVista())
			{
				// XP has a buggy skin on this
				return FALSE;
			}
			theme_data.theme	= GetTheme(UNI_L("header"));
			theme_data.part		= HP_HEADERSORTARROW;

			if (m_windows_native_type == WINDOWS_SORT_ASCENDING)
			{
				theme_data.part_state	= HSAS_SORTEDUP;
			}
			else
			{
				theme_data.part_state	= HSAS_SORTEDDOWN;
			}
			break;
		}
		case WINDOWS_NOTIFIER:
		case WINDOWS_TOOLTIP:
		{
			// tooltip
			theme_data.theme	= GetTheme(UNI_L("tooltip"));
			theme_data.part		= TTP_STANDARD;
			theme_data.part_state	= TTSS_NORMAL;
			break;
		}
		case WINDOWS_HORIZONTAL_SEPARATOR:
		case WINDOWS_VERTICAL_SEPARATOR:
		{
			theme_data.theme		= GetTheme(UNI_L("toolbar"));
			theme_data.part			= (m_windows_native_type == WINDOWS_VERTICAL_SEPARATOR) ? TP_SEPARATOR : TP_SEPARATORVERT;
			theme_data.part_state	= TS_NORMAL;
			if(m_windows_native_type != WINDOWS_VERTICAL_SEPARATOR)
			{
				rect.top += (rect.bottom - rect.top) / 2;
			}
			break;
		}
	}

	if (draw_rect)
		*draw_rect = rect;

	return theme_data.theme ? TRUE : FALSE;
}

INT32 WindowsOpSkinElement::GetPhysicalPixelFromRelative(INT32 relative_pixels)
{
	if (!m_dpi)
	{
		UINT32 vdpi;
		RETURN_VALUE_IF_ERROR(g_op_screen_info->GetDPI(&m_dpi, &vdpi, NULL), relative_pixels);
	}

	return (relative_pixels * m_dpi) / 96;
}

/***********************************************************************************
**
**	DrawThemeStyled
**
***********************************************************************************/

BOOL WindowsOpSkinElement::DrawThemeStyled(HDC hdc, INT32 state, RECT rect)
{
	ThemeData theme_data;

	if (!GetThemeData(state, &rect, theme_data, hdc))
	{
		return FALSE;
	}

	if (theme_data.empty)
		return TRUE;

	if (m_windows_native_type == WINDOWS_PAGEBAR_BUTTON && GetType() == SKINTYPE_BOTTOM)
	{
		RECT r;
		r.top = r.left = 0;
		r.right = rect.right - rect.left;
		r.bottom = rect.bottom - rect.top;
		HDC non_mirrored_tab = CreateCompatibleDC(hdc);
		if (!non_mirrored_tab)
			return FALSE;
		HBITMAP non_mirrored_tab_bm = CreateCompatibleBitmap(hdc, r.right, r.bottom);
		if (!non_mirrored_tab_bm)
		{
			DeleteDC(non_mirrored_tab);
			return FALSE;
		}

		if (!SelectObject(non_mirrored_tab, non_mirrored_tab_bm))
		{
			DeleteDC(non_mirrored_tab);
			DeleteObject(non_mirrored_tab_bm);
			return FALSE;
		}

		DrawThemeBackground(theme_data.theme, non_mirrored_tab, theme_data.part, theme_data.part_state, &r, NULL);

		SetGraphicsMode(hdc, GM_ADVANCED);

		
		XFORM xForm;
		xForm.eM11 = (FLOAT) 1.0; 
		xForm.eM12 = (FLOAT) 0.0; 
		xForm.eM21 = (FLOAT) 0.0; 
		xForm.eM22 = (FLOAT) -1.0; 
		xForm.eDx  = (FLOAT) 0.0; 
		xForm.eDy  = (FLOAT) (rect.top+rect.bottom); 
		SetWorldTransform(hdc, &xForm);

		BitBlt(hdc, rect.left, rect.top+1, r.right, r.bottom+1, non_mirrored_tab, 0, 0, SRCCOPY);

		ModifyWorldTransform(hdc, NULL, MWT_IDENTITY);
		SetGraphicsMode(hdc, GM_COMPATIBLE);
		
		DeleteDC(non_mirrored_tab);
		DeleteObject(non_mirrored_tab_bm);
		return TRUE;
	}
	
	if (theme_data.edge_type)
	{
		DrawThemeEdge(theme_data.theme, hdc, theme_data.part, theme_data.part_state, &rect, theme_data.edge_type, theme_data.border_type, NULL);
	}
	else
	{
		DrawThemeBackground(theme_data.theme, hdc, theme_data.part, theme_data.part_state, &rect, NULL);
	}

	// special 2 pass for some elements

	switch (m_windows_native_type)
	{
		case WINDOWS_SCROLLBAR_HORIZONTAL_KNOB:
		{
			SIZE s;
			MARGINS marg;
			GetThemePartSize(theme_data.theme, hdc, theme_data.part, theme_data.part_state, NULL, TS_TRUE, &s);
			GetThemeMargins(theme_data.theme, hdc, theme_data.part, theme_data.part_state, TMT_CONTENTMARGINS, NULL, &marg);
			if (s.cx < rect.right-rect.left-marg.cxLeftWidth-marg.cxRightWidth)
				DrawThemeBackground(theme_data.theme, hdc, SBP_GRIPPERHORZ, theme_data.part_state, &rect, NULL);
			break;
		}
		case WINDOWS_SCROLLBAR_VERTICAL_KNOB:
		{
			SIZE s;
			MARGINS marg;
			GetThemePartSize(theme_data.theme, hdc, theme_data.part, theme_data.part_state, NULL, TS_TRUE, &s);
			GetThemeMargins(theme_data.theme, hdc, theme_data.part, theme_data.part_state, TMT_CONTENTMARGINS, NULL, &marg);
			if (s.cy < rect.bottom-rect.top-marg.cyBottomHeight-marg.cyTopHeight)
				DrawThemeBackground(theme_data.theme, hdc, SBP_GRIPPERVERT, theme_data.part_state, &rect, NULL);
			break;
		}

		case WINDOWS_WINDOW:
		{
			// comment code back in to get the line below active tabs again!
//			DrawThemeEdge(theme_data.theme, hdc, RP_BAND, theme_data.part_state, &rect, EDGE_ETCHED, BF_TOP, NULL);
			break;
		}

		case WINDOWS_PROGRESS_FULL:
		{
			RECT content;
			GetThemeBackgroundContentRect(theme_data.theme, hdc, PP_BAR, theme_data.part_state, &rect, &content);
			DrawThemeBackground(theme_data.theme, hdc, PP_CHUNK, theme_data.part_state, &content, NULL);
			break;
		}
	}

	return TRUE;
}

enum ArrowDirection { ARROWUP, ARROWDOWN, ARROWLEFT, ARROWRIGHT };

void WindowsDrawArrow(HDC hdc, const OpRect &rect, ArrowDirection direction)
{
	static INT32 arrow_offset[4] = { 3, 2, 1, 0 };	///< Offset of the arrow scanline
	static INT32 arrow_length[4] = { 1, 3, 5, 7 };	///< Length of the arrow scanline

	int minsize = MIN(rect.width, rect.height);
	int xofs = rect.x + rect.width/2;
	int yofs = rect.y + rect.height/2;

	int levels = 4;
	if (minsize < 5)
		levels = 2;
	else if (minsize < 8)
		levels = 3;

	int move = 0;
	if (levels < 3)
		move = 2;
	else if (levels < 4)
		move = 1;

	HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));

	if (direction == ARROWUP)
	{
		xofs -= 3;
		yofs -= 2 - move;
		for (int i=0; i<levels; i++)
		{
			RECT r = {xofs+arrow_offset[i], yofs+i, xofs+arrow_offset[i] + arrow_length[i], yofs+i+1};
			::FillRect(hdc, &r, brush);
		}
	}
	else if (direction == ARROWDOWN)
	{
		xofs -= 3;
		yofs -= 2 + move;
		for (int i=0; i<levels; i++)
		{
			RECT r = {xofs+arrow_offset[i], yofs+4-i, xofs+arrow_offset[i] + arrow_length[i], yofs+4-i+1};
			::FillRect(hdc, &r, brush);
		}
	}
	else if (direction == ARROWLEFT)
	{
		xofs -= 2 - move;
		yofs -= 3;
		for (int i=0; i<levels; i++)
		{
			RECT r = {xofs+i, yofs+arrow_offset[i], yofs+arrow_offset[i] + arrow_length[i], xofs+i+1};
			::FillRect(hdc, &r, brush);
		}
	}
	else if (direction == ARROWRIGHT)
	{
		xofs -= 2 + move;
		yofs -= 3;
		for (int i=0; i<levels; i++)
		{
			RECT r = {xofs+4-i, yofs+arrow_offset[i], yofs+arrow_offset[i] + arrow_length[i], xofs+4-i+1};
			::FillRect(hdc, &r, brush);
		}
	}
	DeleteObject(brush);
}

/***********************************************************************************
**
**	DrawOldStyled - 
**
** Return TRUE on success, otherwise FALSE which will fall back to the regular 
** skin rendering
**
***********************************************************************************/
BOOL WindowsOpSkinElement::DrawOldStyled(HDC hdc, INT32 state, RECT r)
{
	INT32 width = r.right - r.left;
	INT32 height = r.bottom - r.top;

	INT32 flags = 0;

	if (state & SKINSTATE_DISABLED)
	{
		flags |= DFCS_INACTIVE;
	}
	if (state & SKINSTATE_PRESSED)
	{
		flags |= DFCS_PUSHED;
	}
	if (state & SKINSTATE_SELECTED)
	{
		flags |= DFCS_CHECKED;
	}
	if(state & SKINSTATE_HOVER)
	{
		flags |= DFCS_HOT;
	}
	switch (m_windows_native_type)
	{
		case WINDOWS_PUSH_DEFAULT_BUTTON:
		{
			FillRect(hdc, &r, (HBRUSH) GetStockObject(BLACK_BRUSH));
			InflateRect(&r, -1, -1);
			DrawFrameControl(hdc, &r, DFC_BUTTON, DFCS_BUTTONPUSH | flags);
			break;
		}
		case WINDOWS_PUSH_BUTTON:
		case WINDOWS_HEADER_BUTTON:
		{
			DrawFrameControl(hdc, &r, DFC_BUTTON, DFCS_BUTTONPUSH | flags);
			break;
		}
/*		case WINDOWS_SELECTOR_BUTTON:
		{
			if (state & (SKINSTATE_PRESSED | SKINSTATE_SELECTED | SKINSTATE_HOVER))
			{
				DrawEdge(hdc, &r, state & (SKINSTATE_PRESSED | SKINSTATE_SELECTED) ? BDR_SUNKENINNER : BDR_RAISEDOUTER, BF_RECT | BF_ADJUST);
			}
			break;
		}
*/
		case WINDOWS_THUMBNAIL_PAGEBAR_TAIL_BUTTON:
		case WINDOWS_PAGEBAR_TAIL_BUTTON:
		case WINDOWS_THUMBNAIL_PAGEBAR_HEAD_BUTTON:
		case WINDOWS_PAGEBAR_HEAD_BUTTON:
		case WINDOWS_MAINBAR_BUTTON:
		case WINDOWS_PERSONALBAR_BUTTON:
		case WINDOWS_ADDRESSBAR_BUTTON:
		case WINDOWS_LEFT_ADDRESSBAR_BUTTON:
		case WINDOWS_RIGHT_ADDRESSBAR_BUTTON:
		case WINDOWS_TOOLBAR_BUTTON:
		case WINDOWS_LINK_BUTTON:
		case WINDOWS_EXTENDER_BUTTON:
		case WINDOWS_PAGEBAR_FLOATING_BUTTON:
		case WINDOWS_PAGEBAR_THUMBNAIL_FLOATING_BUTTON:
		{
			if (state & (SKINSTATE_PRESSED | SKINSTATE_SELECTED | SKINSTATE_HOVER))
			{
				DrawEdge(hdc, &r, state & (SKINSTATE_PRESSED | SKINSTATE_SELECTED) ? BDR_SUNKENOUTER : BDR_RAISEDINNER, BF_RECT | BF_ADJUST);
				FillRect(hdc, &r, GetSysColorBrush(COLOR_BTNFACE));
			}
			break;
		}
		case WINDOWS_CHECKMARK:
		case WINDOWS_BULLET:
		{
			HBITMAP bitmap = CreateBitmap(width, height, 1, 1, NULL);
			HDC bitmap_hdc = CreateCompatibleDC(hdc);

			bitmap = (HBITMAP) SelectObject(bitmap_hdc, bitmap);

			flags |= m_windows_native_type == WINDOWS_BULLET ?  DFCS_MENUBULLET :  DFCS_MENUCHECK;

			RECT bitmap_rect = {0, 0, width, height};

			DrawFrameControl(bitmap_hdc, &bitmap_rect, DFC_MENU, flags);

			COLORREF old_text_color = SetTextColor(hdc, 0);
			COLORREF old_bk_color = SetBkColor(hdc, 0xffffff);

			BitBlt(hdc, r.left, r.top, width, height, bitmap_hdc, 0, 0, SRCAND);

			SetTextColor(hdc, GetSysColor(state & SKINSTATE_HOVER ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT));
			SetBkColor(hdc, 0);

			BitBlt(hdc, r.left, r.top, width, height, bitmap_hdc, 0, 0, SRCPAINT);

			bitmap = (HBITMAP) SelectObject(bitmap_hdc, bitmap);

			SetTextColor(hdc, old_text_color);
			SetBkColor(hdc, old_bk_color);

			DeleteDC(bitmap_hdc);
			DeleteObject(bitmap);

			break;
		}
		case WINDOWS_RADIO_BUTTON:
		case WINDOWS_CHECKBOX:
		{
			flags |= m_windows_native_type == WINDOWS_RADIO_BUTTON ? DFCS_BUTTONRADIO : (state & SKINSTATE_INDETERMINATE ? DFCS_BUTTON3STATE : DFCS_BUTTONCHECK);

			DrawFrameControl(hdc, &r, DFC_BUTTON, flags);
			break;
		}
		case WINDOWS_EDIT:
		case WINDOWS_MULTILINE_EDIT:
		case WINDOWS_LISTBOX:
		case WINDOWS_START:
		case WINDOWS_SEARCH:
		case WINDOWS_TREEVIEW:
		case WINDOWS_DROPDOWN:
		case WINDOWS_DROPDOWN_EDIT_SKIN:
		{
			DrawEdge(hdc, &r,  EDGE_SUNKEN, BF_RECT | BF_ADJUST);
			FillRect(hdc, &r, GetSysColorBrush(state & SKINSTATE_DISABLED ? COLOR_BTNFACE : COLOR_WINDOW));
			break;
		}
/*		case WINDOWS_HOTLIST_SELECTOR:
		{
			DWORD color = GetSysColor(COLOR_3DSHADOW);

			INT R = GetRValue(color);
			INT G = GetGValue(color);
			INT B = GetBValue(color);
			INT R2 = R;
			INT G2 = G;
			INT B2 = B;

			R2 += (255 - R)/2;
			G2 += (255 - G)/2;
			B2 += (255 - B)/2;

			R2 = R2 > 255 ? 255 : R2;
			G2 = G2 > 255 ? 255 : G2;
			B2 = B2 > 255 ? 255 : B2;

			HBRUSH brush = CreateSolidBrush(RGB(MAX(R, R2), MAX(G, G2), MAX(B, B2)));

			if(brush)
			{
				FillRect(hdc, &r, brush);
				DeleteObject(brush);
			}
			DrawEdge(hdc, &r,  EDGE_SUNKEN, BF_RECT | BF_ADJUST);
			break;
		}
*/		case WINDOWS_BROWSER:
		{
			DrawEdge(hdc, &r,  EDGE_SUNKEN, BF_RECT);
			break;
		}
		case WINDOWS_DIALOG:
		case WINDOWS_HOTLIST:
		case WINDOWS_BROWSER_WINDOW:
		{
			FillRect(hdc, &r, GetSysColorBrush(COLOR_3DFACE));
			break;
		}
		case WINDOWS_WINDOW:
		{
			FillRect(hdc, &r, GetSysColorBrush(COLOR_3DFACE));
			DrawEdge(hdc, &r,  EDGE_ETCHED, BF_TOP);
			break;
		}
		case WINDOWS_DIALOG_TAB_PAGE:
		{
			DrawEdge(hdc, &r,  EDGE_RAISED, BF_LEFT | BF_BOTTOM | BF_RIGHT);
			break;
		}
		case WINDOWS_TABS:
		{
			r.top = r.bottom - 2;
			DrawEdge(hdc, &r,  EDGE_RAISED, BF_TOP  | BF_LEFT | BF_RIGHT);
			break;
		}
		case WINDOWS_TAB_BUTTON:
		{
			FillRect(hdc, &r, GetSysColorBrush(COLOR_3DFACE));
			DrawEdge(hdc, &r,  EDGE_RAISED, BF_TOP  | BF_LEFT | BF_RIGHT);
			RECT corner1 = {r.right - 2, r.top, r.right, r.top + 2};
			FillRect(hdc, &corner1, GetSysColorBrush(COLOR_3DFACE));
			RECT corner2 = {r.right - 1, r.top + 2, r.right, r.top + 3};
			FillRect(hdc, &corner2, GetSysColorBrush(COLOR_3DFACE));
			RECT corner3 = {r.left, r.top, r.left + 2, r.top + 2};
			FillRect(hdc, &corner3, GetSysColorBrush(COLOR_3DFACE));
			RECT corner4 = {r.left, r.top + 2, r.left + 1, r.top + 3};
			FillRect(hdc, &corner4, GetSysColorBrush(COLOR_3DFACE));
			break;
		}
		case WINDOWS_PAGEBAR_BUTTON:	// used to be the same as WINDOWS_HEADER_BUTTON
		{
			DWORD color = GetSysColor(COLOR_3DFACE);

			if(state & SKINSTATE_SELECTED)
			{
				INT R = GetRValue(color);
				INT G = GetGValue(color);
				INT B = GetBValue(color);
				INT R2 = R;
				INT G2 = G;
				INT B2 = B;

				R2 += (255 - R)/1.5;
				G2 += (255 - G)/1.5;
				B2 += (255 - B)/1.5;

				HBRUSH brush = CreateSolidBrush(RGB(MAX(R, R2), MAX(G, G2), MAX(B, B2)));

				if(brush)
				{
					FillRect(hdc, &r, brush);
					DeleteObject(brush);
				}
			}
			else if(state & SKINSTATE_HOVER)
			{
				INT R = GetRValue(color);
				INT G = GetGValue(color);
				INT B = GetBValue(color);
				INT R2 = R;
				INT G2 = G;
				INT B2 = B;

				R2 += (255 - R)/2.5;
				G2 += (255 - G)/2.5;
				B2 += (255 - B)/2.5;

				HBRUSH brush = CreateSolidBrush(RGB(MAX(R, R2), MAX(G, G2), MAX(B, B2)));
				if(brush)
				{
					FillRect(hdc, &r, brush);
					DeleteObject(brush);
				}
			}
			else
			{
				FillRect(hdc, &r, GetSysColorBrush(COLOR_3DFACE));
			}
			UINT edgeflags = 0;
			switch(GetType())
			{
				case SKINTYPE_DEFAULT:
				case SKINTYPE_TOP:
					edgeflags |= BF_TOP | BF_LEFT | BF_RIGHT;
					break;
				case SKINTYPE_LEFT:
				case SKINTYPE_RIGHT:
					edgeflags |= BF_TOP | BF_RIGHT | BF_BOTTOM | BF_LEFT;
					break;
				case SKINTYPE_BOTTOM:
					edgeflags |= BF_LEFT | BF_RIGHT | BF_BOTTOM;
					break;
			}
			DrawEdge(hdc, &r,  EDGE_RAISED, edgeflags | BF_SOFT);
			break;
		}
		case WINDOWS_CAPTION_MINIMIZE_BUTTON:
		{
			r.left += 2;
			r.top += 2;
			r.bottom -= 2;
			DrawFrameControl(hdc, &r, DFC_CAPTION, DFCS_CAPTIONMIN | flags);
			break;
		}
		case WINDOWS_NOTIFIER_CLOSE_BUTTON:
		case WINDOWS_PAGEBAR_CLOSE_BUTTON:
		{
#define CLOSEBUTTON_SIZE 7
			if(!m_close_button_bitmap)
			{
				m_close_button_bitmap = (HBITMAP)LoadImage(hInst, UNI_L("ZCLOSEBUTTON"), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_MONOCHROME);
			}
			HBITMAP bitmap = NULL;
			HDC bitmap_hdc = CreateCompatibleDC(hdc);

			bitmap = (HBITMAP) SelectObject(bitmap_hdc, m_close_button_bitmap);

			COLORREF old_text_color = SetTextColor(hdc, 0);
			COLORREF old_bk_color = SetBkColor(hdc, 0xffffff);

			int left = r.left + (width/2) - (CLOSEBUTTON_SIZE/2);
			int top = r.top + (height/2) - (CLOSEBUTTON_SIZE/2);

			BitBlt(hdc, left, top, width, height, bitmap_hdc, 0, 0, SRCAND);

			if(state & SKINSTATE_HOVER)
			{
				DrawEdge(hdc, &r,  EDGE_RAISED, BF_RECT | BF_FLAT);
			}
			if(state & SKINSTATE_PRESSED)
			{
				DrawEdge(hdc, &r, BDR_SUNKENOUTER, BF_RECT);
			}
			SetTextColor(hdc, old_text_color);
			SetBkColor(hdc, old_bk_color);
			
			SelectObject(bitmap_hdc, bitmap);
			DeleteDC(bitmap_hdc);
			break;
		}
		case WINDOWS_CAPTION_CLOSE_BUTTON:
		{
			r.top += 2;
			r.right -= 2;
			r.bottom -= 2;
			DrawFrameControl(hdc, &r, DFC_CAPTION, DFCS_CAPTIONCLOSE | flags);
			break;
		}
		case WINDOWS_CAPTION_RESTORE_BUTTON:
		{
			r.top += 2;
			r.right -= 2;
			r.bottom -= 2;
			DrawFrameControl(hdc, &r, DFC_CAPTION, DFCS_CAPTIONRESTORE | flags);
			break;
		}

		case WINDOWS_SCROLLBAR_HORIZONTAL:
		case WINDOWS_SCROLLBAR_VERTICAL:
		{
			LONG bits[] = {0x005500aa, 0x005500aa, 0x005500aa, 0x005500aa};

			HBITMAP bitmap = CreateBitmap(8, 8, 1, 1, &bits);
			HBRUSH brush = CreatePatternBrush(bitmap);

			COLORREF old_color = SetTextColor(hdc, GetSysColor(COLOR_3DHIGHLIGHT));

			SetBkColor(hdc, GetSysColor(COLOR_SCROLLBAR));

			FillRect(hdc, &r, brush);

			SetTextColor(hdc, old_color);

			DeleteObject(brush);
			DeleteObject(bitmap);
			break;
		}

		case WINDOWS_SCROLLBAR_HORIZONTAL_KNOB:
		case WINDOWS_SCROLLBAR_VERTICAL_KNOB:
		{
			FillRect(hdc, &r, GetSysColorBrush(COLOR_3DFACE));
			DrawEdge(hdc, &r,  EDGE_RAISED, BF_RECT);
			break;
		}
		case WINDOWS_SCROLLBAR_HORIZONTAL_LEFT:
		{
			DrawFrameControl(hdc, &r, DFC_SCROLL, DFCS_SCROLLLEFT | flags);
			break;
		}
		case WINDOWS_SCROLLBAR_HORIZONTAL_RIGHT:
		{
			DrawFrameControl(hdc, &r, DFC_SCROLL, DFCS_SCROLLRIGHT | flags);
			break;
		}
		case WINDOWS_SCROLLBAR_VERTICAL_UP:
		{
			DrawFrameControl(hdc, &r, DFC_SCROLL, DFCS_SCROLLUP | flags);
			break;
		}
		case WINDOWS_SCROLLBAR_VERTICAL_DOWN:
		{
			DrawFrameControl(hdc, &r, DFC_SCROLL, DFCS_SCROLLDOWN | flags);
			break;
		}
		case WINDOWS_DROPDOWN_BUTTON:
		case WINDOWS_DROPDOWN_LEFT_BUTTON:
		{
			DrawFrameControl(hdc, &r, DFC_SCROLL, DFCS_SCROLLCOMBOBOX | flags);
			break;
		}
//		case WINDOWS_HOTLIST_PANEL_HEADER:
		case WINDOWS_ADDRESSBAR:
		case WINDOWS_MAILBAR:
		case WINDOWS_CHATBAR:
		case WINDOWS_HOTLIST_SPLITTER:
//			break;
		case WINDOWS_MAINBAR:
		case WINDOWS_PERSONALBAR:
		case WINDOWS_PAGEBAR:
		case WINDOWS_NAVIGATIONBAR:
		case WINDOWS_VIEWBAR:
		{
			if (GetType() == SKINTYPE_BOTTOM)
				break;

			DrawEdge(hdc, &r,  EDGE_ETCHED, BF_TOP);
			break;
		}

		case WINDOWS_DRAG_SCROLLBAR:
		{
			FillRect(hdc, &r, GetSysColorBrush(COLOR_3DFACE));
			DrawEdge(hdc, &r,  EDGE_ETCHED, BF_TOP);
			break;
		}

		case WINDOWS_CYCLER:
		{
			DrawEdge(hdc, &r,  EDGE_RAISED, BF_RECT);
			break;
		}

		case WINDOWS_STATUS:
		case WINDOWS_PROGRESS_IMAGES:
		case WINDOWS_PROGRESS_SPEED:
		case WINDOWS_PROGRESS_ELAPSED:
		case WINDOWS_PROGRESS_STATUS:
		case WINDOWS_PROGRESS_GENERAL:
		case WINDOWS_PROGRESS_CLOCK:
		case WINDOWS_IDENTIFY_AS:
		{
			DrawEdge(hdc, &r,  BDR_SUNKENOUTER, BF_RECT);
			break;
		}

		case WINDOWS_PROGRESS_EMPTY:
		case WINDOWS_PROGRESS_FULL:
		{
			DrawEdge(hdc, &r,  BDR_SUNKENOUTER, BF_RECT | BF_ADJUST);
			FillRect(hdc, &r, GetSysColorBrush(m_windows_native_type == WINDOWS_PROGRESS_FULL ? COLOR_HIGHLIGHT : COLOR_3DFACE));
			break;
		}

		case WINDOWS_DIALOG_WARNING:
		case WINDOWS_DIALOG_ERROR:
		case WINDOWS_DIALOG_QUESTION:
		case WINDOWS_DIALOG_INFO:
		{
			HICON icon = GetIcon(m_windows_native_type);

			if (icon)
			{
				DrawIconEx(hdc, r.left, r.top, icon, width, height, 0, NULL, DI_NORMAL | DI_COMPAT);
				DestroyIcon(icon);
			}
			break;
		}
		case WINDOWS_HORIZONTAL_SEPARATOR:
		{
			r.top += (r.bottom - r.top) / 2;
			DrawEdge(hdc, &r, EDGE_ETCHED, BF_TOP);
			break;
		}
		case WINDOWS_VERTICAL_SEPARATOR:
		{
			DrawEdge(hdc, &r, EDGE_ETCHED, BF_LEFT);
			break;
		}

		case WINDOWS_SORT_ASCENDING:
		{
			// arrow up
			OpRect rect(&r);

			WindowsDrawArrow(hdc, rect, ARROWUP);

			break;
		}
		case WINDOWS_SORT_DESCENDING:
		{
			// arrow down
			OpRect rect(&r);

			WindowsDrawArrow(hdc, rect, ARROWDOWN);

			break;
		}
		case WINDOWS_NOTIFIER:
		case WINDOWS_TOOLTIP:
		{
			FillRect(hdc, &r, GetSysColorBrush(COLOR_INFOBK));

			HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
			if(brush)
			{
				FrameRect(hdc, &r, brush);
				DeleteObject(brush);
			}
			break;
		}
		case WINDOWS_DISCLOSURE_TRIANGLE:
			{
				// arrow down
				OpRect rect(&r);

				if(state & SKINSTATE_HOVER)
				{
					DrawEdge(hdc, &r, EDGE_SUNKEN, BF_RECT | BF_FLAT);
				}
				if (state & SKINSTATE_OPEN || state & SKINSTATE_SELECTED)
				{
					WindowsDrawArrow(hdc, rect, ARROWUP);
				}
				else
				{
					WindowsDrawArrow(hdc, rect, ARROWDOWN);
				}
				break;
			}

		default:
			return FALSE;
	}
	return TRUE;
}

#endif // SKIN_NATIVE_ELEMENT
#endif // SKIN_SUPPORT

