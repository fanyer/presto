/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/subclasses/MacWidgetInfo.h"
#include "platforms/mac/bundledefines.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "adjunct/embrowser/EmBrowser_main.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/display/vis_dev.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkin.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpButton.h"
#include "platforms/mac/util/MachOCompatibility.h"

BOOL MacWidgetInfo::GetScrollbarButtonDirection(BOOL first_button, BOOL horizontal, INT32 pos)
{
	return !first_button;
}

SCROLLBAR_ARROWS_TYPE MacWidgetInfo::GetScrollbarArrowStyle()
{
#ifndef SIXTY_FOUR_BIT
	ThemeScrollBarArrowStyle style;

	if(noErr == GetThemeScrollBarArrowStyle(&style))
	{
		switch(style)
		{
			case kThemeScrollBarArrowsSingle:
				return ARROWS_NORMAL;
			case kThemeScrollBarArrowsLowerRight:
				return ARROWS_AT_BOTTOM;
			case kThemeScrollBarArrowsLowerRight+1:
				return ARROWS_AT_BOTTOM_AND_TOP;
			case kThemeScrollBarArrowsLowerRight+2:
				return ARROWS_AT_TOP;
		}
	}

	return ARROWS_NORMAL;
#else
	// (Note: This is just one setting for Opera or Opera Next)
	CFPropertyListRef arrowStyle = CFPreferencesCopyAppValue(CFSTR("AppleScrollBarVariant"), CFSTR(OPERA_BUNDLE_ID_STRING));
	if (arrowStyle && CFGetTypeID(arrowStyle) == CFStringGetTypeID())
	{
		if (CFStringCompare((CFStringRef)arrowStyle, CFSTR("Single"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			return ARROWS_NORMAL;
		else if (CFStringCompare((CFStringRef)arrowStyle, CFSTR("DoubleMax"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			return ARROWS_AT_BOTTOM;
		else if (CFStringCompare((CFStringRef)arrowStyle, CFSTR("DoubleMin"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			return ARROWS_AT_TOP;
		else if (CFStringCompare((CFStringRef)arrowStyle, CFSTR("DoubleBoth"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			return ARROWS_AT_BOTTOM_AND_TOP;
	}
	// Assume the worst
	return ARROWS_AT_BOTTOM_AND_TOP;
#endif
}

INT32 MacWidgetInfo::GetScrollbarFirstButtonSize()
{
	if (GetOSVersion() < 0x1070)
	{
		switch (GetScrollbarArrowStyle())
		{
			case ARROWS_NORMAL:
				return g_op_ui_info->GetHorizontalScrollbarHeight();
			case ARROWS_AT_TOP:
				return g_op_ui_info->GetHorizontalScrollbarHeight()*2;
			case ARROWS_AT_BOTTOM:
				return 0;
			case ARROWS_AT_BOTTOM_AND_TOP:
				return g_op_ui_info->GetHorizontalScrollbarHeight()*2;
		}
	}

	return 0;
}

INT32 MacWidgetInfo::GetScrollbarSecondButtonSize()
{
	if (GetOSVersion() < 0x1070)
	{
		switch (GetScrollbarArrowStyle())
		{
			case ARROWS_NORMAL:
				return g_op_ui_info->GetHorizontalScrollbarHeight();
			case ARROWS_AT_TOP:
				return 0;
			case ARROWS_AT_BOTTOM:
				return g_op_ui_info->GetHorizontalScrollbarHeight()*2;
			case ARROWS_AT_BOTTOM_AND_TOP:
				return g_op_ui_info->GetHorizontalScrollbarHeight()*2;
		}
	}

	return 0;
}

BOOL MacWidgetInfo::GetScrollbarPartHitBy(OpWidget *widget, const OpPoint &oppt, SCROLLBAR_PART_CODE &hit_part)
{
	OpScrollbar 			*scroll = (OpScrollbar*)widget;
	OpRect					opBounds = scroll->GetBounds();
	HIThemeTrackDrawInfo	drawInfo;
	HIScrollBarTrackInfo 	trackInfo;
	ThemeTrackPressState 	pressState = 0;
	HIPoint 				pt = {0,0};
	HIRect 					trackRect;
	ControlPartCode 		cntrlPart;

	pt.x = oppt.x;
	pt.y = oppt.y;

	HIRect bounds = {
						{0, 0},
						{opBounds.width, opBounds.height}
					};
	pressState = (ThemeTrackPressState)scroll->GetHitPart();

	drawInfo.version = 0;
	drawInfo.kind = kThemeMediumScrollBar;
	drawInfo.bounds = bounds;
	drawInfo.min = scroll->limit_min;
	drawInfo.max = scroll->limit_max;
	drawInfo.value = scroll->value;
	drawInfo.attributes = kThemeTrackShowThumb | (scroll->horizontal ? kThemeTrackHorizontal : 0);
	drawInfo.enableState = scroll->IsEnabled() ? kThemeTrackActive : kThemeTrackInactive;
	drawInfo.trackInfo.scrollbar.viewsize = scroll->limit_visible;
	drawInfo.trackInfo.scrollbar.pressState = pressState;
	trackInfo.version = 0;
	trackInfo.enableState = scroll->IsEnabled() ? kThemeTrackActive : kThemeTrackInactive;
	trackInfo.viewsize = scroll->limit_visible;
	trackInfo.pressState = pressState;

	int minSize = g_op_ui_info->GetHorizontalScrollbarHeight();
	if(GetScrollbarArrowStyle() == ARROWS_AT_BOTTOM_AND_TOP)
	{
		minSize *= 6;
		minSize -= 4;
	}
	else
	{
		minSize *= 4;
	}
	
	// Use simplified scrollbar if smaller than minSize
	if(scroll->horizontal)
	{
		if(opBounds.width < minSize)
		{
			return FALSE;
		}
	}
	else
	{
		if(opBounds.height < minSize)
		{
			return FALSE;
		}
	}

	if(HIThemeHitTestScrollBarArrows(&bounds, &trackInfo, scroll->horizontal ? true : false, &pt, &trackRect, &cntrlPart))
	{
		switch(cntrlPart)
		{
			case kControlUpButtonPart:
				hit_part = TOP_OUTSIDE_ARROW;
				break;
			case kControlDownButtonPart:
				hit_part = BOTTOM_OUTSIDE_ARROW;
				break;
			case 28:	// Only happens when double arrows at both ends
				hit_part = BOTTOM_INSIDE_ARROW;
				break;
			case 29:	// Only happens when double arrows at both ends
				hit_part = TOP_INSIDE_ARROW;
				break;
		}

		return TRUE;
	}
	else if(HIThemeHitTestTrack(&drawInfo, &pt, &cntrlPart))
	{
		switch(cntrlPart)
		{
			case kControlPageUpPart:
				hit_part = TOP_TRACK;
				break;
			case kControlPageDownPart:
				hit_part = BOTTOM_TRACK;
				break;
			default:
				hit_part = KNOB;
		}

		return TRUE;
	}

	hit_part = INVALID_PART;
	return TRUE;
}

INT32 MacWidgetInfo::GetDropdownButtonWidth(OpWidget* widget)
{
	OpSkinElement *border_skin = widget->GetBorderSkin()->GetSkinElement();
	if(!g_skin_manager->GetCurrentSkin() || !border_skin || !border_skin->IsNative())
	{
		return IndpWidgetInfo::GetDropdownButtonWidth(widget);
	}

	if(widget && widget->GetBorderSkin())
	{
		const char *skin_image = widget->GetBorderSkin()->GetImage();
		if(skin_image && !uni_strcmp(skin_image, UNI_L("Dropdown Search Field Skin")))
		{
			return 0;
		}
	}

	if(widget->IsOfType(OpTypedObject::WIDGET_TYPE_DROPDOWN) && !((OpDropDown*)widget)->m_dropdown_packed.show_button)
	{
		return 0;
	}

	return 19; // AquaHIG says 15 pixels for a big version, and 13 pixels for a small...
}

void MacWidgetInfo::GetPreferedSize(OpWidget* widget, OpTypedObject::Type type, INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	switch(type)
	{
		case OpTypedObject::WIDGET_TYPE_EDIT:
			IndpWidgetInfo::GetPreferedSize(widget, type, w, h, cols, rows);
			if(!widget->GetFormObject())
			{
				// Apple HIG says 22 px for edit fields
				*h = 22;
			}
			break;
		case OpTypedObject::WIDGET_TYPE_DROPDOWN:
			IndpWidgetInfo::GetPreferedSize(widget, type, w, h, cols, rows);
			if (!widget->GetFormObject())
			{
				*h = 23;
			}
			else
			{
				*h = widget->font_info.size + 7;

				// Fix for bug 249063, Pop-up menus have wrong padding
				*w += 3;

				// Needs a reasonable minimum
				*w = MAX(*w, 35);
			}
			break;
		case OpTypedObject::WIDGET_TYPE_BUTTON:
			IndpWidgetInfo::GetPreferedSize(widget, type, w, h, cols, rows);
			if(!widget->GetFormObject() && ((OpButton*)widget)->GetButtonType() == OpButton::TYPE_TAB)
			{
				*h = 23;
			}
			break;
		default:
			IndpWidgetInfo::GetPreferedSize(widget, type, w, h, cols, rows);
			break;
	};
}

void MacWidgetInfo::AddBorder(OpWidget* widget, OpRect *rect)
{
	if(widget->HasCssBorder())
	{
		IndpWidgetInfo::AddBorder(widget, rect);
		return;
	}

	BOOL mac_native = g_skin_manager->GetCurrentSkin() ? TRUE : FALSE;

	switch(widget->GetType())
	{
		// browserview hack
		case OpTypedObject::WIDGET_TYPE_BROWSERVIEW:
			if(gVendorDataID == 'OPRA')
			{
				// This is to get borders in the right places, we don't want them where
				// the browserview is at the border of the mac window.

				INT32 left, top, right, bottom;
				widget->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
				rect->x -= left;
				rect->y -= top;
				rect->width += left + right;
				rect->height += top + bottom;

				// The code that was insetting the entire window by one pixel here has been removed.
				// This was to fix issue DSK-253746. To our best recollection at the time this code
				// was here to support the old native toolbars which did not draw their own border. 
				// Since we do not use them anymore it should be safe to remove this code.
			}
			break;

		case OpTypedObject::WIDGET_TYPE_ADDRESS_DROPDOWN:
			if(mac_native)
			{
				return;
			}
			IndpWidgetInfo::AddBorder(widget, rect);
			break;
			
		default:
			IndpWidgetInfo::AddBorder(widget, rect);
			break;
	}
}

void MacWidgetInfo::GetItemMargin(INT32 &left, INT32 &top, INT32 &right, INT32 &bottom)
{
#ifdef SKIN_SUPPORT
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Listitem Skin");
	if (skin_elm)
	{
		skin_elm->GetMargin(&left, &top, &right, &bottom, 0);
		return;
	}
#endif
	top = bottom = 1;
	left = right = 3;	// Only line that differed from overridden function at time of copy.
}

UINT32 MacWidgetInfo::GetSystemColor(OP_SYSTEM_COLOR color)
{
	switch (color)
	{
		case OP_SYSTEM_COLOR_BACKGROUND:
			return USE_DEFAULT_COLOR;
		default:
			return g_op_ui_info->GetSystemColor(color);
	}
}

