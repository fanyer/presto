/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/skin/UnixOpSkinElement.h"

#include "adjunct/desktop_util/string/hash.h"
#include "modules/display/vis_dev.h"
#include "modules/pi/OpBitmap.h"
#include "modules/widgets/OpButton.h"
#include "platforms/quix/toolkits/x11/X11SkinElement.h"
#include "platforms/quix/toolkits/NativeSkinElement.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/unix/base/x11/x11_colormanager.h"
#include "platforms/unix/base/x11/x11_globals.h"

namespace
{
	struct ElementMapping
	{
		const unsigned name_hash;
		const NativeSkinElement::NativeType native_type;
	};

	const ElementMapping element_mapping[] =
	{
		{ HASH_LITERAL("Push Button Skin"), NativeSkinElement::NATIVE_PUSH_BUTTON },
		{ HASH_LITERAL("Push Default Button Skin"), NativeSkinElement::NATIVE_PUSH_DEFAULT_BUTTON },
		{ HASH_LITERAL("Toolbar Button Skin"), NativeSkinElement::NATIVE_TOOLBAR_BUTTON },
		{ HASH_LITERAL("Selector Button Skin"), NativeSkinElement::NATIVE_SELECTOR_BUTTON },
		{ HASH_LITERAL("Menu Skin"), NativeSkinElement::NATIVE_MENU },
		{ HASH_LITERAL("Menu Button Skin"), NativeSkinElement::NATIVE_MENU_BUTTON },
		{ HASH_LITERAL("Menu Right Arrow"), NativeSkinElement::NATIVE_MENU_RIGHT_ARROW },
		{ HASH_LITERAL("Popup Menu Skin"), NativeSkinElement::NATIVE_POPUP_MENU },
		{ HASH_LITERAL("Popup Menu Button Skin"), NativeSkinElement::NATIVE_POPUP_MENU_BUTTON },
		{ HASH_LITERAL("Header Button Skin"), NativeSkinElement::NATIVE_HEADER_BUTTON },
		{ HASH_LITERAL("Link Button Skin"), NativeSkinElement::NATIVE_LINK_BUTTON },
		{ HASH_LITERAL("Tab Button Skin"), NativeSkinElement::NATIVE_TAB_BUTTON },
		{ HASH_LITERAL("Pagebar Button Skin"), NativeSkinElement::NATIVE_PAGEBAR_BUTTON },
		{ HASH_LITERAL("Caption Minimize Button Skin"), NativeSkinElement::NATIVE_CAPTION_MINIMIZE_BUTTON },
		{ HASH_LITERAL("Caption Close Button Skin"), NativeSkinElement::NATIVE_CAPTION_CLOSE_BUTTON },
		{ HASH_LITERAL("Caption Restore Button Skin"), NativeSkinElement::NATIVE_CAPTION_RESTORE_BUTTON },
		{ HASH_LITERAL("Checkbox Skin"), NativeSkinElement::NATIVE_CHECKBOX },
		{ HASH_LITERAL("Radio Button Skin"), NativeSkinElement::NATIVE_RADIO_BUTTON },
		{ HASH_LITERAL("Dropdown Skin"), NativeSkinElement::NATIVE_DROPDOWN },
		{ HASH_LITERAL("Dropdown Button Skin"), NativeSkinElement::NATIVE_DROPDOWN_BUTTON },	
		{ HASH_LITERAL("Dropdown Left Button Skin"), NativeSkinElement::NATIVE_DROPDOWN_LEFT_BUTTON },
		{ HASH_LITERAL("Dropdown Edit Skin"), NativeSkinElement::NATIVE_DROPDOWN_EDIT },
		{ HASH_LITERAL("Edit Skin"), NativeSkinElement::NATIVE_EDIT },
		{ HASH_LITERAL("MultilineEdit Skin"), NativeSkinElement::NATIVE_MULTILINE_EDIT },
		{ HASH_LITERAL("Browser Skin"), NativeSkinElement::NATIVE_BROWSER },
		{ HASH_LITERAL("Progressbar Skin"), NativeSkinElement::NATIVE_PROGRESSBAR },
		{ HASH_LITERAL("Start Skin"), NativeSkinElement::NATIVE_START },
		{ HASH_LITERAL("Search Skin"), NativeSkinElement::NATIVE_SEARCH },
		{ HASH_LITERAL("Treeview Skin"), NativeSkinElement::NATIVE_TREEVIEW },
		{ HASH_LITERAL("Listbox Skin"), NativeSkinElement::NATIVE_LISTBOX },
		{ HASH_LITERAL("Checkmark"), NativeSkinElement::NATIVE_CHECKMARK },
		{ HASH_LITERAL("Bullet"), NativeSkinElement::NATIVE_BULLET },
		{ HASH_LITERAL("Window Skin"), NativeSkinElement::NATIVE_WINDOW },
		{ HASH_LITERAL("Browser Window Skin"), NativeSkinElement::NATIVE_BROWSER_WINDOW },
		{ HASH_LITERAL("Dialog Skin"), NativeSkinElement::NATIVE_DIALOG },
		{ HASH_LITERAL("Dialog Page Skin"), NativeSkinElement::NATIVE_DIALOG_PAGE },
		{ HASH_LITERAL("Dialog Tab Page Skin"), NativeSkinElement::NATIVE_DIALOG_TAB_PAGE },
		{ HASH_LITERAL("Dialog Button Border Skin"), NativeSkinElement::NATIVE_DIALOG_BUTTON_BORDER },
		{ HASH_LITERAL("Tabs Skin"), NativeSkinElement::NATIVE_TABS },
		{ HASH_LITERAL("Hotlist Skin"), NativeSkinElement::NATIVE_HOTLIST },
		{ HASH_LITERAL("Hotlist Selector Skin"), NativeSkinElement::NATIVE_HOTLIST_SELECTOR },
		{ HASH_LITERAL("Hotlist Splitter Skin"), NativeSkinElement::NATIVE_HOTLIST_SPLITTER },
		{ HASH_LITERAL("Hotlist Panel Header Skin"), NativeSkinElement::NATIVE_HOTLIST_PANEL_HEADER },
		{ HASH_LITERAL("Scrollbar Horizontal Skin"), NativeSkinElement::NATIVE_SCROLLBAR_HORIZONTAL },
		{ HASH_LITERAL("Scrollbar Horizontal Knob Skin"), NativeSkinElement::NATIVE_SCROLLBAR_HORIZONTAL_KNOB },
		{ HASH_LITERAL("Scrollbar Horizontal Left Skin"), NativeSkinElement::NATIVE_SCROLLBAR_HORIZONTAL_LEFT },
		{ HASH_LITERAL("Scrollbar Horizontal Right Skin"), NativeSkinElement::NATIVE_SCROLLBAR_HORIZONTAL_RIGHT },
		{ HASH_LITERAL("Scrollbar Vertical Skin"), NativeSkinElement::NATIVE_SCROLLBAR_VERTICAL },
		{ HASH_LITERAL("Scrollbar Vertical Knob Skin"), NativeSkinElement::NATIVE_SCROLLBAR_VERTICAL_KNOB },
		{ HASH_LITERAL("Scrollbar Vertical Up Skin"), NativeSkinElement::NATIVE_SCROLLBAR_VERTICAL_UP },
		{ HASH_LITERAL("Scrollbar Vertical Down Skin"), NativeSkinElement::NATIVE_SCROLLBAR_VERTICAL_DOWN },
		{ HASH_LITERAL("Status Skin"), NativeSkinElement::NATIVE_STATUS },
		{ HASH_LITERAL("Mainbar Skin"), NativeSkinElement::NATIVE_MAINBAR },
		{ HASH_LITERAL("Mainbar Button Skin"), NativeSkinElement::NATIVE_MAINBAR_BUTTON },
		{ HASH_LITERAL("Personalbar Skin"), NativeSkinElement::NATIVE_PERSONALBAR },
		{ HASH_LITERAL("Personalbar Button Skin"), NativeSkinElement::NATIVE_PERSONALBAR_BUTTON },
		{ HASH_LITERAL("Pagebar Skin"), NativeSkinElement::NATIVE_PAGEBAR },
		{ HASH_LITERAL("Addressbar Skin"), NativeSkinElement::NATIVE_ADDRESSBAR },
		{ HASH_LITERAL("Navigationbar Skin"), NativeSkinElement::NATIVE_NAVIGATIONBAR },
		{ HASH_LITERAL("Viewbar Skin"), NativeSkinElement::NATIVE_VIEWBAR },
		{ HASH_LITERAL("Mailbar Skin"), NativeSkinElement::NATIVE_MAILBAR },
		{ HASH_LITERAL("Chatbar Skin"), NativeSkinElement::NATIVE_CHATBAR },
		{ HASH_LITERAL("Cycler Skin"), NativeSkinElement::NATIVE_CYCLER },
		{ HASH_LITERAL("Progress Document Skin"), NativeSkinElement::NATIVE_PROGRESS_DOCUMENT },
		{ HASH_LITERAL("Progress Images Skin"), NativeSkinElement::NATIVE_PROGRESS_IMAGES },
		{ HASH_LITERAL("Progress Total Skin"), NativeSkinElement::NATIVE_PROGRESS_TOTAL },
		{ HASH_LITERAL("Progress Speed Skin"), NativeSkinElement::NATIVE_PROGRESS_SPEED },
		{ HASH_LITERAL("Progress Elapsed Skin"), NativeSkinElement::NATIVE_PROGRESS_ELAPSED },
		{ HASH_LITERAL("Progress Status Skin"), NativeSkinElement::NATIVE_PROGRESS_STATUS },
		{ HASH_LITERAL("Progress General Skin"), NativeSkinElement::NATIVE_PROGRESS_GENERAL },
		{ HASH_LITERAL("Progress Clock Skin"), NativeSkinElement::NATIVE_PROGRESS_CLOCK },
		{ HASH_LITERAL("Progress Empty Skin"), NativeSkinElement::NATIVE_PROGRESS_EMPTY },
		{ HASH_LITERAL("Progress Full Skin"), NativeSkinElement::NATIVE_PROGRESS_FULL },
		{ HASH_LITERAL("Identify As Skin"), NativeSkinElement::NATIVE_IDENTIFY_AS },
		{ HASH_LITERAL("Dialog Warning"), NativeSkinElement::NATIVE_DIALOG_WARNING },
		{ HASH_LITERAL("Dialog Error"), NativeSkinElement::NATIVE_DIALOG_ERROR },
		{ HASH_LITERAL("Dialog Question"), NativeSkinElement::NATIVE_DIALOG_QUESTION },
		{ HASH_LITERAL("Dialog Info"), NativeSkinElement::NATIVE_DIALOG_INFO },
		{ HASH_LITERAL("Speed Dial Widget Skin"), NativeSkinElement::NATIVE_SPEEDDIAL },
		{ HASH_LITERAL("Disclosure Triangle Skin"), NativeSkinElement::NATIVE_DISCLOSURE_TRIANGLE },
		{ HASH_LITERAL("Notifier Skin"), NativeSkinElement::NATIVE_NOTIFIER },
		{ HASH_LITERAL("Tooltip Skin"), NativeSkinElement::NATIVE_TOOLTIP },
		{ HASH_LITERAL("Listitem Skin"), NativeSkinElement::NATIVE_LISTITEM },
		{ HASH_LITERAL("Slider Horizontal Track"), NativeSkinElement::NATIVE_SLIDER_HORIZONTAL_TRACK },
		{ HASH_LITERAL("Slider Horizontal Knob"), NativeSkinElement::NATIVE_SLIDER_HORIZONTAL_KNOB },
		{ HASH_LITERAL("Slider Vertical Track"), NativeSkinElement::NATIVE_SLIDER_VERTICAL_TRACK },
		{ HASH_LITERAL("Slider Vertical Knob"), NativeSkinElement::NATIVE_SLIDER_VERTICAL_KNOB },
		{ HASH_LITERAL("Menu Separator Skin"), NativeSkinElement::NATIVE_MENU_SEPARATOR },
		{ HASH_LITERAL("DesktopLabel Skin"), NativeSkinElement::NATIVE_LABEL },
	};

	NativeSkinElement::NativeType GetNativeType(const OpStringC8& name)
	{
		unsigned name_hash = Hash::String(name.CStr());
		for (size_t i = 0; i < ARRAY_SIZE(element_mapping); i++)
		{
			if (element_mapping[i].name_hash == name_hash)
				return element_mapping[i].native_type;
		}
#ifdef _DEBUG
		fprintf(stderr, "'%s' skin element not supported!\n", name.CStr() ? name.CStr() : "");
#endif
		return NativeSkinElement::NATIVE_NOT_SUPPORTED;
	}

	int ToNativeState(INT32 state)
	{
		int native_state = 0;

		if (state & SKINSTATE_DISABLED)
			native_state |= NativeSkinElement::STATE_DISABLED;
		if (state & SKINSTATE_SELECTED)
			native_state |= NativeSkinElement::STATE_SELECTED;
		if (state & SKINSTATE_PRESSED)
			native_state |= NativeSkinElement::STATE_PRESSED;
		if (state & SKINSTATE_HOVER)
			native_state |= NativeSkinElement::STATE_HOVER;
		if (state & SKINSTATE_FOCUSED)
			native_state |= NativeSkinElement::STATE_FOCUSED;
		if (state & SKINSTATE_INDETERMINATE)
			native_state |= NativeSkinElement::STATE_INDETERMINATE;
		if (state & SKINSTATE_RTL)
			native_state |= NativeSkinElement::STATE_RTL;

		return native_state;
	}

	int ToNativeExtraState(INT32 state)
	{
		int native_state = 0;
#if defined(HAS_TAB_BUTTON_POSITION)
		if (state & (OpButton::TAB_FIRST << OpButton::TAB_STATE_OFFSET))
			native_state |= NativeSkinElement::STATE_TAB_FIRST;
		if (state & (OpButton::TAB_LAST << OpButton::TAB_STATE_OFFSET))
			native_state |= NativeSkinElement::STATE_TAB_LAST;
		if (state & (OpButton::TAB_SELECTED << OpButton::TAB_STATE_OFFSET))
			native_state |= NativeSkinElement::STATE_TAB_SELECTED;
		if (state & (OpButton::TAB_PREV_SELECTED << OpButton::TAB_STATE_OFFSET))
			native_state |= NativeSkinElement::STATE_TAB_PREV_SELECTED;
		if (state & (OpButton::TAB_NEXT_SELECTED << OpButton::TAB_STATE_OFFSET))
			native_state |= NativeSkinElement::STATE_TAB_NEXT_SELECTED;
#endif
		return native_state;
	}

};

OpSkinElement* OpSkinElement::CreateNativeElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size)
{
	NativeSkinElement::NativeType native_type = GetNativeType(name);
	NativeSkinElement* native_element = g_toolkit_library ? g_toolkit_library->GetNativeSkinElement(native_type) : 0;
	if (native_element)
		return OP_NEW(UnixOpSkinElement, (native_element, skin, name, type, size));

	// X11 skin code does not have to paint via a bitmap. So we avoid the UnixOpSkinElement layer
	return OP_NEW(X11SkinElement, (skin, name, type, size));
}


void OpSkinElement::FlushNativeElements()
{}

UnixOpSkinElement::UnixOpSkinElement(NativeSkinElement* native_element, OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size)
  : OpSkinElement(skin, name, type, size)
  , m_native_element(native_element)
{
	OP_ASSERT(m_native_element);

	op_memset(m_bitmaps, 0, sizeof(m_bitmaps));
}

UnixOpSkinElement::~UnixOpSkinElement()
{
	for (size_t i = 0; i < ARRAY_SIZE(m_bitmaps); i++)
		OP_DELETE(m_bitmaps[i]);

	OP_DELETE(m_native_element);
}

OP_STATUS UnixOpSkinElement::Draw(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args)
{
	OpBitmap* bitmap = GetBitmap(rect.width, rect.height, state);
	if (!bitmap)
		return OpStatus::ERR_NO_MEMORY;

	vd->BitmapOut(bitmap, OpRect(0, 0, bitmap->Width(), bitmap->Height()), rect);
	return OpStatus::OK;
}

void UnixOpSkinElement::OverrideDefaults(OpSkinElement::StateElement* se)
{
	int native_state = ToNativeState(se->m_state) | ToNativeExtraState(se->m_state);

	m_native_element->ChangeDefaultPadding(se->m_padding_left, se->m_padding_top, se->m_padding_right, se->m_padding_bottom, native_state);
	m_native_element->ChangeDefaultMargin(se->m_margin_left, se->m_margin_top, se->m_margin_right, se->m_margin_bottom, native_state);
	m_native_element->ChangeDefaultSize(se->m_width, se->m_height, native_state);
	m_native_element->ChangeDefaultSpacing(se->m_spacing, native_state);

	uint8_t red = 0, green = 0, blue = 0, alpha = 0;
	m_native_element->ChangeDefaultTextColor(red, green, blue, alpha, native_state);
	COLORREF color = OP_RGBA(red, green, blue, alpha);
	if (color)
		se->m_text_color = color;
}

OP_STATUS UnixOpSkinElement::GetBorderWidth(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	*left = *top = *right = *bottom = 2;
	return OpStatus::OK;
}

OpBitmap* UnixOpSkinElement::GetBitmap(UINT32 width, UINT32 height, INT32 state)
{
	int native_state = ToNativeState(state);

	OpBitmap*& cached_bitmap = m_bitmaps[native_state];
	if (!m_native_element->IgnoreCache())
	{
		if (cached_bitmap && cached_bitmap->Width() == width && cached_bitmap->Height() == height)
			return cached_bitmap;
	}

	OP_DELETE(cached_bitmap);
	cached_bitmap = 0;

	if (UINT64(width) * UINT64(height) > UINT32_MAX / sizeof(UINT32))
		return 0;

	UINT32* bitmap_data = OP_NEWA(UINT32, width * height);
	if (!bitmap_data)
		return 0;

	NativeSkinElement::NativeRect native_rect (0, 0, width, height);

	m_native_element->Draw(bitmap_data, width, height, native_rect, native_state | (state & 0xFF000000));

	if (OpStatus::IsSuccess(OpBitmap::Create(&cached_bitmap, width, height)))
	{
		for (unsigned y = 0; y < height; y++)
		{
			cached_bitmap->AddLine(bitmap_data + y * width, y);
		}
	}

	OP_DELETEA(bitmap_data);

	return cached_bitmap;
}

