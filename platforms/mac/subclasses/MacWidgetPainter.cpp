/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/subclasses/MacWidgetPainter.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpResizeCorner.h"
#include "modules/widgets/OpRadioButton.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "adjunct/quick/widgets/OpWidgetBitmap.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "modules/skin/OpSkin.h"
#include "modules/skin/OpSkinManager.h"
#include "platforms/mac/subclasses/MacWidgetInfo.h"
#include "platforms/mac/pi/MacVEGAPrinterListener.h"
#include "modules/style/src/css_values.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/CocoaVegaDefines.h"
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/pi/MacOpView.h"
#include "modules/display/vis_dev.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/libvega/vegarendertarget.h"
#include "modules/libvega/vegawindow.h"
#include "platforms/mac/pi/CocoaVEGAWindow.h"

#define RGB2COLORREF(color) (OP_RGB((color.red & 0xFF00) >> 8, (color.green & 0xFF00) >> 8, (color.blue & 0xFF00) >> 8))

// DSK-316225: This should be enough resolution
#define NUMBER_OF_STEPS 0x10000000L

OpWidgetInfo* g_MacWidgetInfo = NULL;
extern Boolean gDrawScrollDelim;

MacWidgetPainter::MacWidgetPainter()
{
	m_offscreen = NULL;
	m_painter = NULL;
}

MacWidgetPainter::~MacWidgetPainter()
{
#ifndef SIXTY_FOUR_BIT
	if(m_offscreen)
	{
		DisposeGWorld(m_offscreen);
		m_offscreen = NULL;
	}
#endif
}

void MacWidgetPainter::InitPaint(VisualDevice* vd, OpWidget* widget)
{
	IndpWidgetPainter::InitPaint(vd, widget);
	m_painter = (MacOpPainter*)vd->painter;
}

OpWidgetInfo* MacWidgetPainter::GetInfo(OpWidget* widget)
{
	if (!g_MacWidgetInfo)
	{
		IndpWidgetPainter::GetInfo(widget);
		g_MacWidgetInfo = new MacWidgetInfo();
	}
	return g_MacWidgetInfo;
}

template<> MacCachedObjectFactory<MacWidgetBitmap, MacWidgetBitmapTraits>* MacCachedObjectFactory<MacWidgetBitmap, MacWidgetBitmapTraits>::instance_ = NULL;
template<> const uint16_t MacCachedObjectFactory<MacWidgetBitmap, MacWidgetBitmapTraits>::kCacheSize = 64;	///< Change this value to something larger, if you think works better.

BOOL MacWidgetPainter::DrawScrollbar(const OpRect &drawrect)
{
	HIThemeTrackDrawInfo 	drawInfo;
	ThemeTrackPressState 	pressState = 0;
	OpScrollbar 			*scroll = (OpScrollbar*)widget;
	HIShapeRef				thumbRgn;
	OpRect					opBounds = scroll->GetBounds();
		
	CGRect bounds = {{-drawrect.x, -drawrect.y}, {opBounds.width, opBounds.height}};
	
	pressState = (ThemeTrackPressState)scroll->GetHitPart();
	
	// hack to draw correctly, for some reason DrawThemeTrack needs this.
	if(pressState == kThemeTopInsideArrowPressed)
		pressState = kThemeBottomInsideArrowPressed;
	else if(pressState == kThemeBottomInsideArrowPressed)
		pressState = kThemeTopInsideArrowPressed;

	if(scroll->horizontal)
	{
		bounds.size.width++;
	}
	else
	{
		bounds.size.height++;
	}

	OpWindow *rootWindow;
	BOOL inActiveWindow = FALSE;
	if (vd->GetView())
	{
		rootWindow = vd->GetView()->GetContainer()->GetOpView()->GetRootWindow();
		inActiveWindow = rootWindow->IsActiveTopmostWindow() || rootWindow->GetStyle() == OpWindow::STYLE_POPUP;
	}

	drawInfo.version = 0;
	drawInfo.kind = kThemeMediumScrollBar;
	drawInfo.enableState = (scroll->IsEnabled() && inActiveWindow) ? kThemeTrackActive : kThemeTrackInactive;
	drawInfo.attributes = kThemeTrackShowThumb | (scroll->horizontal ? kThemeTrackHorizontal : 0);
	drawInfo.bounds = bounds;
	drawInfo.min = scroll->limit_min;
	drawInfo.max = scroll->limit_max;
	drawInfo.value = scroll->value;
	drawInfo.trackInfo.scrollbar.viewsize = scroll->limit_visible;
	drawInfo.trackInfo.scrollbar.pressState = pressState;
	
	int minSize = g_op_ui_info->GetHorizontalScrollbarHeight();
	if (GetOSVersion() >= 0x1070)
	{
		minSize = 0;
	}
	else if (GetInfo()->GetScrollbarArrowStyle() == ARROWS_AT_BOTTOM_AND_TOP)
	{
		minSize *= 6;
		minSize -= 4;
	}
	else
	{
		minSize *= 4;
	}
	
	// Bail out if smaller than minSize
	if(scroll->horizontal)
	{
		if((bounds.size.width) < minSize)
		{
			return FALSE;
		}
	}
	else
	{
		if((bounds.size.height) < minSize)
		{
			return FALSE;
		}
	}
	
	// Ok, the thumb(knob) could have been changed, let's update
	if(noErr == HIThemeGetTrackThumbShape(&drawInfo, &thumbRgn))
	{
		HIRect thumbRect;
		OpRect thumbOpRect;
		HIShapeGetBounds(thumbRgn, &thumbRect);
		CFRelease(thumbRgn);
		
		thumbOpRect.x = thumbRect.origin.x - bounds.origin.x;
		thumbOpRect.y = thumbRect.origin.y - bounds.origin.y;
		thumbOpRect.width = thumbRect.size.width;
		thumbOpRect.height = thumbRect.size.height;
		
		scroll->SetKnobRect(thumbOpRect);
	}
	
	if (OpStatus::IsError(MacCachedObjectFactory<MacWidgetBitmap, MacWidgetBitmapTraits>::Init()))
		return FALSE;

	int bmpwidth = drawrect.width;
	int bmpheight = drawrect.height;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	const PixelScaler& scaler = vd->GetVPScale();
	bmpwidth = TO_DEVICE_PIXEL(scaler, bmpwidth);
	bmpheight = TO_DEVICE_PIXEL(scaler, bmpheight);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	MacWidgetBitmap* bitmap = MacCachedObjectFactory<MacWidgetBitmap, MacWidgetBitmapTraits>::CreateObject(MacWidgetBitmapTraits::CreateParam(bmpwidth, bmpheight));

	if (!bitmap)
		return FALSE;

	// Set clip and draw
	widget->SetClipRect(drawrect);
	OpBitmap* bmp = bitmap->GetOpBitmap();
	void* image_data = bmp->GetPointer(OpBitmap::ACCESS_WRITEONLY);
	if (!image_data)
	{
		bitmap->DecRef();
		widget->RemoveClipRect();
		return FALSE;
	}
	bitmap->Lock();
	const int bpl = bmp->GetBytesPerLine();
	memset(image_data, 0, bpl * bmp->Height());

	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
	// create the context at size of drawrect instead of bitmap itself.
	// we cache the bitmap to prevent create-destroy cycle and reallocation.
	// since we need the bitmap's memory only, we can create a much smaller context within that buffer.
	CGContextRef context = CGBitmapContextCreate(image_data, bmpwidth, bmpheight, 8, bpl, colorSpace, alpha);
	CGColorSpaceRelease(colorSpace);
	if (!context)
	{
		bitmap->DecRef();
		bmp->ReleasePointer(FALSE);
		widget->RemoveClipRect();
		bitmap->Unlock();
		return FALSE;
	}

	const int win_height = drawrect.height;

	CGFloat scale = 1.0f;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	scale = CGFloat(scaler.GetScale()) / 100.0f;
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	CGContextScaleCTM(context, scale, -scale);
	CGContextTranslateCTM(context, 0.0f, -win_height);
	
	HIThemeDrawTrack(&drawInfo, NULL, context, kHIThemeOrientationNormal);

	bmp->ReleasePointer();
	vd->BitmapOut(bmp, OpRect(0, 0, bmp->Width(), bmp->Height()), drawrect);
	CGContextRelease(context);

	widget->RemoveClipRect();
	bitmap->Unlock();
	bitmap->DecRef();

	return TRUE;
}

BOOL MacWidgetPainter::DrawResizeCorner(const OpRect &drawrect, BOOL active)
{
	//ThemeGrowDirection 	possibleGrowDirections = kThemeGrowDown | kThemeGrowRight;
	//ThemeDrawState 		drawState;
	//Point 				origin = {0,0};
	//Boolean 			isSmall = false;
	OpResizeCorner 		*corner = (OpResizeCorner*)widget;
	OpRect				opBounds = corner->GetBounds();
	float 				scale = ((float)vd->GetScale()) / 100.0;

	if(!g_skin_manager->GetCurrentSkin())
		return IndpWidgetPainter::DrawResizeCorner(drawrect, active);

	//origin.h += vd->GetTranslationX() - vd->GetViewX() + vd->GetOffsetX();
	//origin.v += vd->GetTranslationY() - vd->GetViewY() + vd->GetOffsetY();
	//origin.h *= scale;
	//origin.v *= scale;
	
#ifdef VIEWPORTS_SUPPORT
	opBounds.x += vd->GetTranslationX() - vd->GetRenderingViewX() + vd->ScaleToDoc(vd->GetOffsetX());
	opBounds.y += vd->GetTranslationY() - vd->GetRenderingViewY() + vd->ScaleToDoc(vd->GetOffsetY());
#else
	opBounds.x += vd->GetTranslationX() - vd->GetViewX() + vd->ScaleToDoc(vd->GetOffsetX());
	opBounds.y += vd->GetTranslationY() - vd->GetViewY() + vd->ScaleToDoc(vd->GetOffsetY());
#endif
	
	Rect bounds = 	{
						(scale == 1.0) ? opBounds.y : 0,
						(scale == 1.0) ? opBounds.x : 0,
						(scale == 1.0) ? opBounds.y + opBounds.height : opBounds.height,
						(scale == 1.0) ? opBounds.x + opBounds.width : opBounds.width
					};
					
	OpRect corner_rect(bounds.left, bounds.top,bounds.right-bounds.left, bounds.bottom-bounds.top);

	if (vd->GetView())
	{
		OpPainter * painter = vd->GetView()->GetPainter(corner_rect);
		if (painter)
		{
			// Draw a white square
			painter->SetColor(255, 255, 255);
			painter->FillRect(corner_rect);

			// Draw a grey frame for the top and left sides
			painter->SetColor(217, 217, 217);
			painter->DrawRect(OpRect(corner_rect.x, corner_rect.y, corner_rect.width + 1, corner_rect.height + 1));

            vd->GetView()->ReleasePainter(corner_rect);
		}
	}
	
	return TRUE;
}

void MacWidgetPainter::DrawFocusRect(const OpRect &drawrect)
{
	OpRect opBounds = drawrect;
	float scale = ((float)vd->GetScale()) / 100.0f;
	
#ifdef VIEWPORTS_SUPPORT
	opBounds.x += vd->GetTranslationX() - vd->GetRenderingViewX() + vd->ScaleToDoc(vd->GetOffsetX());
	opBounds.y += vd->GetTranslationY() - vd->GetRenderingViewY() + vd->ScaleToDoc(vd->GetOffsetY());
#else
	opBounds.x += vd->GetTranslationX() - vd->GetViewX() + vd->ScaleToDoc(vd->GetOffsetX());
	opBounds.y += vd->GetTranslationY() - vd->GetViewY() + vd->ScaleToDoc(vd->GetOffsetY());
#endif
	
	CGRect scaledBounds = {{opBounds.x*scale, opBounds.y*scale}, {opBounds.width*scale, opBounds.height*scale}};
	OpRect vd_clip = vd->GetClipping(); // Get current cliprect, we need to sync QDclip with that
	vd_clip = vd->ScaleToScreen(vd_clip);
	vd_clip.x += vd->GetOffsetX();
	vd_clip.y += vd->GetOffsetY();
	CGRect clipBounds = {{vd_clip.x, vd_clip.y}, {vd_clip.width, vd_clip.height}};
	clipBounds = CGRectInset(clipBounds, -3, -3); // don't quite know why...
	scaledBounds = CGRectInset(scaledBounds, 4, 4);

	VEGAOpPainter* painter = (VEGAOpPainter*)m_painter;
	VEGARenderTarget* target = painter->GetRenderTarget();
	CocoaVEGAWindow* window = (CocoaVEGAWindow*)target->getTargetWindow();
	if(!window)
	{
		return;
	}
	int win_height = CGImageGetHeight(window->getImage());
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	const PixelScaler& scaler = vd->GetVPScale();
	win_height = FROM_DEVICE_PIXEL(scaler, win_height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	CGContextRef context = window->getPainter();
	
	if (context)
	{
		MacOpView *macview = (MacOpView *)vd->GetOpView();
		OpPoint pt;
		pt = macview->ConvertToScreen(pt);
		OpWindow* root = macview->GetRootWindow();
		INT32 xpos,ypos;
		root->GetInnerPos(&xpos, &ypos);
		
		scaledBounds = CGRectOffset(scaledBounds, pt.x - xpos, pt.y - ypos);
		clipBounds = CGRectOffset(clipBounds, pt.x - xpos, pt.y - ypos);
		
		CGContextSaveGState(context);
		float scale = 1.0f;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		scale = TO_DEVICE_PIXEL(scaler, scale);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		CGContextScaleCTM(context, scale, -scale);
		CGContextTranslateCTM(context, 0.0f, -win_height);
		CGContextClipToRect(context, clipBounds);
		
		HIThemeDrawFocusRect(&scaledBounds, true, context, kHIThemeOrientationNormal);
		
		CGContextRestoreGState(context);
	}
}

void MacWidgetPainter::DrawRoundedFocusRect(const OpRect &drawrect, BOOL drawFocusRect)
{
}

BOOL MacWidgetPainter::DrawEdit(const OpRect &drawrect)
{
	BOOL res = IndpWidgetPainter::DrawEdit(drawrect);
//	if(!((OpEdit*)widget)->IsReadOnly())
//	{
//		if(widget->GetBorderSkin()->GetImage() && (uni_strcmp(widget->GetBorderSkin()->GetImage(), UNI_L("Edit Search Skin")) == 0))
//			DrawRoundedFocusRect(drawrect.InsetBy(-2,-2), widget->IsFocused());
//		else if (widget->IsFocused())
//			DrawFocusRect(drawrect.InsetBy(-2,-2));
//	}
	return res;
}

BOOL MacWidgetPainter::DrawMultiEdit(const OpRect &drawrect)
{
	BOOL res = IndpWidgetPainter::DrawMultiEdit(drawrect);
//	if(!((OpMultilineEdit*)widget)->IsReadOnly() && widget->IsFocused())
//	{
//		DrawFocusRect(drawrect.InsetBy(-2,-2));
//	}
	return res;
}

BOOL MacWidgetPainter::DrawDropdown(const OpRect &drawrect)
{
	OpSkinElement *border_skin = widget->GetBorderSkin()->GetSkinElement();
	if(!g_skin_manager->GetCurrentSkin() || !border_skin || !border_skin->IsNative())
	{
		return IndpWidgetPainter::DrawDropdown(drawrect);
	}

	OpDropDown* dropdown = (OpDropDown*) widget;

	INT32 left = 0;
	INT32 top = 0;
	INT32 right = 0;
	INT32 bottom = 0;

	dropdown->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);

	// Fix for bug 249063, Pop-up menus have wrong padding
/*
	if(dropdown->GetFormObject())
	{
		left += 5;
		right += 3;
	}
*/
	OpRect inner_rect(drawrect.x + left, drawrect.y + top, drawrect.width - left - right, drawrect.height - top - bottom);
	if (inner_rect.width < 0 || inner_rect.height < 0)
		return FALSE;

	if (dropdown->GetForegroundSkin()->HasContent())
	{
		inner_rect.x += 2;
		inner_rect.width -= 2;

		OpRect image_rect = dropdown->GetForegroundSkin()->CalculateScaledRect(inner_rect, FALSE, TRUE);
		if (widget->GetRTL())
			image_rect.x = drawrect.Right() - image_rect.Right() + drawrect.x;
		dropdown->GetForegroundSkin()->Draw(vd, image_rect);

		UINT32 image_width = image_rect.width + 4;

		inner_rect.x += image_width;
		inner_rect.width -= image_width;
	}

	if (widget->GetRTL())
		inner_rect.width -= GetInfo()->GetDropdownLeftButtonWidth(widget);
	else
		inner_rect.width -= GetInfo()->GetDropdownButtonWidth(widget);
	inner_rect.height--;

	OpStringItem* item = dropdown->GetItemToPaint();

	if (!dropdown->edit && (item || dropdown->ghost_string.Get()))
	{
		OpRect item_rect = inner_rect;
		if (widget->GetRTL())
			item_rect.x = drawrect.Right() - item_rect.Right() + drawrect.x;

		dropdown->SetClipRect(item_rect);
		if (item)
		{
			dropdown->GetPainterManager()->DrawItem(item_rect, item, FALSE, FALSE);
		}
		else
		{
			OpRect string_rect = inner_rect;
			dropdown->AddMargin(&string_rect);
			if (widget->GetRTL())
				string_rect.x = drawrect.Right() - string_rect.Right() + drawrect.x;
			dropdown->ghost_string.Draw(string_rect, vd, g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_DISABLED_FONT));
		}
		dropdown->RemoveClipRect();
	}

	OpRect button_rect = drawrect;

	const char* button_image = widget->GetRTL() ? "Dropdown Left Button Skin" : "Dropdown Button Skin";
	widget->GetSkinManager()->GetMargin(button_image, &left, &top, &right, &bottom);
	button_rect.y += top;
	button_rect.height -= top + bottom;
	button_rect.x += button_rect.width - GetInfo()->GetDropdownButtonWidth(widget) - right;
	button_rect.width = GetInfo()->GetDropdownButtonWidth(widget);

	INT32 state = 0;

	if (!widget->IsEnabled())
		state |= SKINSTATE_DISABLED;

	if (dropdown->m_dropdown_window)
		state |= SKINSTATE_PRESSED;

	INT32 hover_value = 0;

	if (dropdown->m_dropdown_packed.is_hovering_button)
	{
		state |= SKINSTATE_HOVER;
		hover_value = 100;
	}

	if(dropdown->edit && !dropdown->edit->IsReadOnly())
	{
		const char *skin_image = widget->GetBorderSkin()->GetImage();

#ifdef WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
		// only draw the dropdown arrow if this is a special editable drop down. This should be placed with a generic
		// check of the skin once arjanl has commited his code
		if(!skin_image || !uni_strcmp(skin_image, UNI_L("Zoom Dropdown Skin")) || !uni_strcmp(skin_image, UNI_L("Dropdown Addressfield Skin")) ||
		   !uni_strcmp(skin_image, UNI_L("Dropdown Search Skin")))
#else
		// only draw the dropdown arrow if this is an editable dropdown
		if(!skin_image || uni_strcmp(skin_image, UNI_L("Dropdown Search Field Skin")))
#endif // WIDGETS_CAP_HAS_BOTH_CUSTOM_AND_DEFAULT_DROPDOWN
		{
			if (widget->GetRTL())
				button_rect.x = drawrect.Right() - button_rect.Right() + drawrect.x;
			widget->GetSkinManager()->DrawElement(vd, button_image, button_rect, state, hover_value);
		}

#if 1
		// Put focusrect in the right place (dropdowns are different from pure editfields)
		OpRect newrect = drawrect.InsetBy(-2, -2);

		if(skin_image && (!uni_strcmp(skin_image, UNI_L("Dropdown Search Skin")) || !uni_strcmp(skin_image, UNI_L("Dropdown Search Field Skin"))))
			DrawRoundedFocusRect(newrect, dropdown->edit->IsFocused() || widget->IsFocused());
		else if (dropdown->edit->IsFocused() || widget->IsFocused())
			DrawFocusRect(newrect);
#endif
	}

	return TRUE;
}

BOOL MacWidgetPainter::DrawProgressbar(const OpRect &drawrect, double percent, INT32 progress_when_total_unknown, OpWidgetString* string, const char *skin_empty, const char *skin_full)
{
	const char *full_skin  = skin_full && *skin_full ? skin_full : "Progress Full Skin";

	OpSkinElement *border_skin = g_skin_manager->GetSkinElement(full_skin);
	if(!g_skin_manager->GetCurrentSkin() || !border_skin || !border_skin->IsNative())
	{
		return IndpWidgetPainter::DrawProgressbar(drawrect, percent, progress_when_total_unknown, string, skin_empty, skin_full);
	}

	UINT32 full_color = g_op_ui_info->GetUICSSColor(CSS_VALUE_HighlightText);

	g_skin_manager->GetTextColor(full_skin, &full_color);
		
	CGRect r = {{0, 0}, {drawrect.width, drawrect.height}};
	
	CGContextRef context;
	OpBitmap* bitmap = NULL;

	int bmpwidth = drawrect.width;
	int bmpheight = drawrect.height;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	const PixelScaler& scaler = vd->GetVPScale();
	bmpwidth = TO_DEVICE_PIXEL(scaler, bmpwidth);
	bmpheight = TO_DEVICE_PIXEL(scaler, bmpheight);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	if(OpStatus::IsSuccess(OpBitmap::Create(&bitmap, bmpwidth, bmpheight, FALSE, TRUE, 0, 0, TRUE)))
	{
		int w = bitmap->Width();
		int h = bitmap->Height();
		int bpl = bitmap->GetBytesPerLine();
		void *image_data = bitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY);
		if (!image_data)
		{
			delete bitmap;
			return FALSE;
		}
		memset(image_data, 0, bpl*h);
		CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
		CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
		context = CGBitmapContextCreate(image_data, w, h, 8, bpl, colorSpace, alpha);
		CGColorSpaceRelease(colorSpace);
		int win_height = drawrect.height;

		float scale = 1.0f;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		scale = TO_DEVICE_PIXEL(scaler, scale);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		CGContextScaleCTM(context, scale, -scale);
		CGContextTranslateCTM(context, 0.0, -win_height);

		if (percent == 0 && progress_when_total_unknown)
		{
			HIThemeTrackDrawInfo drawInfo;
			
			SInt32 thickness = 0;
			SInt32 shadow = 0;
			if	(noErr == GetThemeMetric(kThemeMetricLargeProgressBarThickness, &thickness) &&
				(noErr == GetThemeMetric(kThemeMetricProgressBarShadowOutset, &shadow)))
			{
				SInt32 progressHeight = thickness + shadow;
				
				if((r.size.height) > progressHeight)
				{
					float f = (r.size.height - progressHeight); // / 2;
					r.origin.y += f;
					r.size.height -= f;
				}
			}
			else
			{
				float f = (r.size.height / 4) - 1;
				r.origin.y += f;
				r.size.height -= f;
			}

			drawInfo.version = 0;
			drawInfo.kind = kThemeIndeterminateBarLarge;
			drawInfo.bounds = r;
			drawInfo.min = 0;
			drawInfo.max = 100;
			drawInfo.value = 0;
			drawInfo.attributes = kThemeTrackHorizontal;
			drawInfo.enableState = widget->IsEnabled() ? kThemeTrackActive : kThemeTrackInactive;
			drawInfo.trackInfo.progress.phase = progress_when_total_unknown;
			
			HIThemeDrawTrack(&drawInfo, NULL, context, kHIThemeOrientationNormal);		
		}
		else
		{
			HIThemeTrackDrawInfo drawInfo;
			
			SInt32 thickness = 0;
			SInt32 shadow = 0;
			if	(noErr == GetThemeMetric(kThemeMetricLargeProgressBarThickness, &thickness) &&
				(noErr == GetThemeMetric(kThemeMetricProgressBarShadowOutset, &shadow)))
			{
				SInt32 progressHeight = thickness + shadow;
				
				if((r.size.height) > progressHeight)
				{
					float f = (r.size.height - progressHeight); // / 2;
					r.origin.y += f;
					r.size.height -= f;
				}
			}
			else
			{
				float f = (r.size.height / 4) - 1;
				r.origin.y += f;
				r.size.height -= f;
			}

			drawInfo.version = 0;
			drawInfo.kind = kThemeProgressBarLarge;
			drawInfo.bounds = r;
			drawInfo.min = 0;
			drawInfo.max = 100;
			drawInfo.value = (SInt32)(percent);
			drawInfo.attributes = kThemeTrackHorizontal;
			drawInfo.enableState = widget->IsEnabled() ? kThemeTrackActive : kThemeTrackInactive;
			drawInfo.trackInfo.progress.phase = floorf(GetCurrentEventTime()*16);
			
			HIThemeDrawTrack(&drawInfo, NULL, context, kHIThemeOrientationNormal);
		}
		
		CGContextRelease(context);
		bitmap->ReleasePointer();
		vd->BitmapOut(bitmap, OpRect(0, 0, bitmap->Width(), bitmap->Height()), drawrect);
		delete bitmap;
	}
	
	if (string)
	{
		widget->SetClipRect(drawrect);
		OpRect textRect = drawrect;
		textRect.y -= 1;
		string->Draw(textRect, vd, full_color);
		widget->RemoveClipRect();
	}
	
	return TRUE;
}

BOOL MacWidgetPainter::DrawSlider(const OpRect& rect, BOOL horizontal, double min, double max, double pos, BOOL highlighted, BOOL pressed_knob, OpRect& out_knob_position, OpPoint& out_start_track, OpPoint& out_end_track)
{
	// Opera 11.50: We are going to use special skinned (non-toolkit) sliders for zoom sliders
	// so we are not drawing with our painter in that case
	if (!widget || widget->GetType() == OpTypedObject::WIDGET_TYPE_ZOOM_SLIDER)
		return FALSE;

	OpSkinElement *border_skin = widget->GetBorderSkin()->GetSkinElement();
	if(!g_skin_manager->GetCurrentSkin() || (border_skin && !border_skin->IsNative()))
	{
		return IndpWidgetPainter::DrawSlider(rect, horizontal, min, max, pos, highlighted, pressed_knob, out_knob_position, out_start_track, out_end_track);
	}
	
	CGRect r = {{0, 0}, {rect.width, rect.height}};
	
	CGContextRef context;
	OpBitmap* bitmap = NULL;

	int bmpwidth = rect.width;
	int bmpheight = rect.height;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	const PixelScaler& scaler = vd->GetVPScale();
	bmpwidth = TO_DEVICE_PIXEL(scaler, bmpwidth);
	bmpheight = TO_DEVICE_PIXEL(scaler, bmpheight);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	if(OpStatus::IsSuccess(OpBitmap::Create(&bitmap, bmpwidth, bmpheight, FALSE, TRUE, 0, 0, TRUE)))
	{
		int w = bitmap->Width();
		int h = bitmap->Height();
		int bpl = bitmap->GetBytesPerLine();
		void *image_data = bitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY);
		if (!image_data)
		{
			delete bitmap;
			return FALSE;
		}
		memset(image_data, 0, bpl*h);
		CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
		CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
		context = CGBitmapContextCreate(image_data, w, h, 8, bpl, colorSpace, alpha);
		CGColorSpaceRelease(colorSpace);
		int win_height = rect.height;
		
		float scale = 1.0f;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		scale = TO_DEVICE_PIXEL(scaler, scale);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		CGContextScaleCTM(context, scale, -scale);
		CGContextTranslateCTM(context, 0.0, -win_height);
		
		HIThemeTrackDrawInfo drawInfo;
		
		SInt32 thickness = 0;
		SInt32 shadow = 0;
		if	(noErr == GetThemeMetric(kThemeMetricLargeProgressBarThickness, &thickness) &&
			 (noErr == GetThemeMetric(kThemeMetricProgressBarShadowOutset, &shadow)))
		{
			SInt32 progressHeight = thickness + shadow;
			
			if (horizontal)
			{
				if((r.size.height) > progressHeight)
				{
					float f = (r.size.height - progressHeight);
					r.origin.y += f / 2;
					r.size.height -= f;
				}
			}
			else 
			{
				if((r.size.width) > progressHeight)
				{
					float f = (r.size.width - progressHeight);
					r.origin.x += f / 2;
					r.size.width -= f;
				}			
			}
		}
		else
		{
			if (horizontal)
			{
				float f = (r.size.height / 4) - 1;
				r.origin.y += f / 2;
				r.size.height -= f;
			}
			else
			{
				float f = (r.size.width / 4) - 1;
				r.origin.x += f / 2;
				r.size.width -= f;			
			}
		}
		
		drawInfo.version = 0;
		drawInfo.kind = kThemeSliderSmall;
		drawInfo.bounds = r;
		drawInfo.min = 0;
		drawInfo.max = NUMBER_OF_STEPS;
		if (widget->GetRTL())
			drawInfo.value = (max - pos) / (max - min) * NUMBER_OF_STEPS;
		else
			drawInfo.value = (pos - min) / (max - min) * NUMBER_OF_STEPS;
		drawInfo.attributes = kThemeTrackShowThumb;
		if (horizontal)
		{
			drawInfo.attributes |= kThemeTrackHorizontal;
		}
		else
		{
			drawInfo.attributes |= kThemeTrackRightToLeft;
		}
		drawInfo.enableState = widget->IsEnabled() ? kThemeTrackActive : kThemeTrackInactive;
		drawInfo.trackInfo.slider.thumbDir = kThemeThumbPlain;
		drawInfo.trackInfo.slider.pressState = 0;
		
		if (highlighted)
			drawInfo.attributes |= kThemeTrackHasFocus;

		HIThemeDrawTrack(&drawInfo, NULL, context, kHIThemeOrientationNormal);
		
		HIShapeRef thumb_shape;
		HIRect thumb_bounds, track_bounds;
		HIThemeGetTrackThumbShape(&drawInfo, &thumb_shape);
		HIShapeGetBounds(thumb_shape, &thumb_bounds);
		HIThemeGetTrackBounds(&drawInfo, &track_bounds);
		CFRelease(thumb_shape);
		
		out_knob_position.Set(thumb_bounds.origin.x, thumb_bounds.origin.y, thumb_bounds.size.width, thumb_bounds.size.height);
		out_start_track.Set(track_bounds.origin.x, track_bounds.origin.y);
		out_end_track.Set(track_bounds.origin.x+track_bounds.size.width, track_bounds.origin.y+track_bounds.size.height);
		
		CGContextRelease(context);
		bitmap->ReleasePointer();
		vd->BitmapOut(bitmap, OpRect(0, 0, bitmap->Width(), bitmap->Height()), rect);
		delete bitmap;
	}
	
	return TRUE;
}

void MacAutoString8HashTable::DeleteFunc(const void* key, void *data, OpHashTable* table)
{
	static_cast<MacWidgetBitmap*>(data)->DecRef();
	free((char*)key);
}

void MacAutoString8HashTable::UnrefAndDeleteAll()
{
	ForEach(DeleteFunc);
	RemoveAll();
}

void MacAutoString8HashTable::UnrefAll()
{
	OpHashIterator* it = GetIterator();
	if (OpStatus::IsError(it->First()))
		return;
	do {
		MacWidgetBitmap* o = static_cast<MacWidgetBitmap*>(it->GetData());
		o->DecRef();
	} while (OpStatus::IsSuccess(it->Next()));
}

char* MacWidgetBitmapTraits::CreateParamHashKey(const MacWidgetBitmapTraits::CreateParam& cp) {
	char* tmp = (char*)malloc(32);
	if (!tmp)
		return NULL;
	// generate our key (ophashtable cludge)
	// note that the key must be deleted:
	// * if this function returns NULL (i.e. error condition)
	// * when the key-value pair must be removed from the hashtable.
	//   that is, key must not be free'ed as long as the item is stored in the hashtable.
	snprintf(tmp, 31, "%u-%u", cp.w, cp.h);
	return tmp;
}

OP_STATUS MacWidgetBitmapTraits::CreateResource(pointer* p, const CreateParam& cp)
{
	if (!p)
		return OpStatus::ERR_NULL_POINTER;
	return OpBitmap::Create(p, cp.w, cp.h, FALSE, TRUE, 0, 0, TRUE);
}

bool MacWidgetBitmapTraits::Matches(const MacWidgetBitmap* wbmp, const MacWidgetBitmapTraits::CreateParam& cp) {
	const OpBitmap* bmp = wbmp->GetOpBitmap();
	return (bmp->Width() == cp.w && bmp->Height() == cp.h);
}
