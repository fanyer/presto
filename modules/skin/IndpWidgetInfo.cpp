/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef SKIN_SUPPORT

#include "modules/skin/IndpWidgetInfo.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpResizeCorner.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpFont.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/display/vis_dev.h"
#include "modules/prefs/prefsmanager/prefstypes.h"

void IndpWidgetInfo::GetPreferedSize(OpWidget* widget, OpTypedObject::Type type, INT32* w, INT32* h, INT32 cols, INT32 rows)
{
#ifdef QUICK
	if (widget->IsForm())
	{
		OpWidgetInfo::GetPreferedSize(widget, type, w, h, cols, rows);
		return;
	}
	switch(type)
	{
	case OpTypedObject::WIDGET_TYPE_RADIOBUTTON:
	case OpTypedObject::WIDGET_TYPE_CHECKBOX:
	{
		OpButton* button = (OpButton*) widget;

		const char* skin = type == OpTypedObject::WIDGET_TYPE_RADIOBUTTON ? "Checkbox Skin" : "Radio Button Skin";

		INT32 image_width;
		INT32 image_height;

		widget->GetSkinManager()->GetSize(skin, &image_width, &image_height);

		image_width += 4;
		image_height += 2;

		INT32 text_width = button->string.GetWidth() + 4;
		INT32 text_height = button->string.GetHeight() + (widget->IsMiniSize() ? 0 : 4);

		*w = image_width + text_width;
		*h = image_height;

		if (text_height > *h)
			*h = text_height;

		*h += 1;
		break;
	}

	case OpTypedObject::WIDGET_TYPE_BUTTON:
		{
			OpButton* button = (OpButton*) widget;

			OpButton::ButtonStyle style = button->GetForegroundSkin()->HasContent() ? button->m_button_style : OpButton::STYLE_TEXT;

			OpSkinElement* element = widget->GetSkinManager()->GetSkinElement(button->GetBorderSkin()->GetImage(), button->GetBorderSkin()->GetType());

			BOOL bold = button->packed2.is_bold || (element && element->HasBoldState());

			INT32 old_weight = button->font_info.weight;

			if (bold)
			{
				button->font_info.weight = 7;
				button->string.NeedUpdate();
			}

			INT32 text_width = button->string.GetWidth();
			INT32 text_height = button->string.GetHeight();

			OpInputAction* action = button->GetAction();

			if (action)
			{
				BOOL changed = FALSE;

				OpString original_string;

				original_string.Set(button->string.Get());

				if (original_string.Compare(button->m_text.CStr()))
				{
					changed = TRUE;
					button->string.Set(button->m_text.CStr(), button);
					INT32 new_text_width = button->string.GetWidth();
					INT32 new_text_height = button->string.GetHeight();

					if (new_text_width > text_width)
					{
						text_width = new_text_width;
					}
					if (new_text_height > text_height)
					{
						text_height = new_text_height;
					}
				}

				while (action)
				{
					if (original_string.Compare(action->GetActionText()))
					{
						changed = TRUE;
						button->string.Set(action->GetActionText(), button);
						INT32 new_text_width = button->string.GetWidth();
						INT32 new_text_height = button->string.GetHeight();

						if (new_text_width > text_width)
						{
							text_width = new_text_width;
						}
						if (new_text_height > text_height)
						{
							text_height = new_text_height;
						}
					}


					action = action->GetActionOperator() != OpInputAction::OPERATOR_PLUS ? action->GetNextInputAction() : NULL;
				}

				if (changed)
				{
					button->string.Set(original_string.CStr(), button);
				}
			}

			if (bold)
			{
				button->font_info.weight = old_weight;
				button->string.NeedUpdate();
			}

			if (text_width == 0 && style != OpButton::STYLE_TEXT)
				style = OpButton::STYLE_IMAGE;

			if (button->m_button_type == OpButton::TYPE_PUSH || button->m_button_type == OpButton::TYPE_PUSH_DEFAULT)
			{
				text_height += text_height / 4;
				if (button->font_info.justify == JUSTIFY_CENTER)
					text_width += text_width * 2 / 5;
			}
			else
			{
				INT32 extra = -1;
				
				button->GetBorderSkin()->GetButtonTextPadding(&extra);

				if(extra == -1)
				{
					extra = button->GetSkinManager()->GetOptionValue("Button Text Padding", 2) * 2;
				}
				else
				{
					extra *= 2;
				}

				text_width += extra;
				text_height += extra;
			}

			INT32 image_width;
			INT32 image_height;


			if( button->GetForegroundSkin()->GetRestrictImageSize() )
			{
				OpRect r = button->GetForegroundSkin()->CalculateScaledRect( OpRect(0,0,100,100), 0,0 );
				image_width = r.width;
				image_height = r.height;
			}
			else
			{
				button->GetForegroundSkin()->GetSize(&image_width, &image_height);
			}

			INT32 left, top, right, bottom;

			button->GetForegroundSkin()->GetMargin(&left, &top, &right, &bottom);

			if (right < 0 && style != OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT)
			{
				right = 0;
			}

			if (bottom < 0 && style != OpButton::STYLE_IMAGE_AND_TEXT_BELOW)
			{
				bottom = 0;
			}

			image_width += left + right;
			image_height += top + bottom;

			INT32 spacing = 0;
			
			button->GetSpacing(&spacing);

			switch (style)
			{
				case OpButton::STYLE_IMAGE_AND_TEXT_ON_LEFT:
				case OpButton::STYLE_IMAGE_AND_TEXT_CENTER:
				case OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT:
				{
					*w = image_width + text_width + spacing;
					*h = max(image_height, text_height);
					break;
				}

				case OpButton::STYLE_IMAGE:
				{
					*w = image_width;
					*h = image_height;
					break;
				}

				case OpButton::STYLE_IMAGE_AND_TEXT_BELOW:
				{
					*w = max(text_width, image_width);
					*h = image_height + text_height + spacing;
					break;
				}

				case OpButton::STYLE_TEXT:
				{
					*w = text_width;
					*h = text_height;
					break;
				}
			}

			button->GetPadding(&left, &top, &right, &bottom);

			if (*w < button->GetMinWidth())
				*w = button->GetMinWidth();

			if (button->GetMaxWidth() && *w > button->GetMaxWidth())
				*w = button->GetMaxWidth();

			*w += left + right;
			*h += top + bottom;

			if (*w < *h && (style != OpButton::STYLE_IMAGE && (style != OpButton::STYLE_IMAGE && button->GetButtonType() != OpButton::TYPE_CUSTOM)))
				*w = *h;

#ifdef ACTION_SHOW_MENU_SECTION_ENABLED
			if (button->GetButtonExtra() == OpButton::EXTRA_SUB_MENU)
			{
				INT32 sw = 0, sh = 0;
				if (OpSkinElement* element = widget->GetSkinManager()->GetSkinElement("Menu Right Arrow", SKINTYPE_DEFAULT, SKINSIZE_DEFAULT, TRUE))
					element->GetSize(&sw, &sh, 0);
				*w += sw;
			}
#endif

			if (button->m_dropdown_image.HasContent())
			{
				INT32 image_width;
				INT32 image_height;

				button->m_dropdown_image.GetSize(&image_width, &image_height);

				INT32 dropdown_left, dropdown_top, dropdown_right, dropdown_bottom;

				button->m_dropdown_image.GetMargin(&dropdown_left, &dropdown_top, &dropdown_right, &dropdown_bottom);

				INT32 auto_dropdown_padding = (left + right) / 2;

				*w += image_width + auto_dropdown_padding + dropdown_left + dropdown_right;
			}
		}
		break;
	case OpTypedObject::WIDGET_TYPE_DROPDOWN:
		{
			OpDropDown* dr = (OpDropDown*) widget;

			{
				if (dr->edit || dr->CountItems() == 0)
				{
					*w = 150;
				}
				else
				{
					*w = dr->ih.widest_item + GetDropdownButtonWidth(widget);

					INT32 left = 0;
					INT32 top = 0;
					INT32 right = 0;
					INT32 bottom = 0;

					dr->GetPadding(&left, &top, &right, &bottom);

					*w += left + right;
				}

#if 1 
				// Part of fix for bug #199381 (UI label text is cut off due to widget padding changes)
				if (dr->edit)
				{
					OP_ASSERT(dr->edit->GetVisualDevice()); // might not get height if vis_dev is not set
					int text_height = dr->edit->string.GetHeight();
					*h = text_height + dr->edit->GetPaddingTop() + dr->edit->GetPaddingBottom() + dr->GetPaddingTop() + dr->GetPaddingBottom();
					// The '4' is because of OpWidgetInfo::AddBorder()
					if (!dr->edit->HasCssBorder())
						*h += 4; 
				}
				else
#endif
				{
					*h = widget->font_info.size + (widget->IsMiniSize() ? 8 : 12);
				}

				if (widget->GetType() == OpTypedObject::WIDGET_TYPE_ZOOM_DROPDOWN)
				{
					// Can not use a hardcoded width here. Must take padding, icon and font size 
					// into the equation. We assume "100%" to be the default string we want to show.

					// save current zoom string
					OpString org_string;
					org_string.Set(dr->edit->string.Get());

					// set default string, to measure default size
					dr->edit->string.Set(UNI_L("100%"), dr->edit);
					// get the require size for the default text
					INT32 edit_width = dr->edit->string.GetWidth();

					// restore original string
					dr->edit->string.Set(org_string.CStr(), dr->edit);

					INT32 icon_width = 0;

					// Compute the width as is done in OpDropDown::OnLayout()
					if (dr->GetForegroundSkin()->HasContent())
					{
						// We have to fake a little bit here. CalculateScaledRect() will return 
						// an empty rectangle when inner_rect is too small. That will happen when we
						// use dr->GetBounds() when IndpWidgetInfo::GetPreferedSize() gets called 
						// before layout has been set.
						OpRect inner_rect(0,0,100,100);

						OpRect rect = dr->GetForegroundSkin()->CalculateScaledRect(inner_rect, FALSE, TRUE);
						// 4 is added to the image width in OpDropDown::OnLayout()
						icon_width = rect.width + 4;
					}

					*w = icon_width + edit_width + GetDropdownButtonWidth(widget) + dr->edit->GetPaddingLeft() + dr->edit->GetPaddingRight() + dr->GetPaddingLeft() + dr->GetPaddingRight();
					// the '4' is because of OpWidgetInfo::AddBorder()
					if (!dr->HasCssBorder())
						*w += 4;

				}
			}
		}
		break;
	case OpTypedObject::WIDGET_TYPE_EDIT:
		{
			OP_ASSERT(widget->GetVisualDevice());
			INT32 font_height = ((OpEdit*)widget)->string.GetHeight();
			*w = 150;
			*h = font_height + (widget->IsMiniSize() ? 8 : 12);
			break;
		}
		break;
/*
	case OpTypedObject::WIDGET_TYPE_RADIOBUTTON:
		{
			OpSkinElement* skin_elm = g_skin_manager->GetSkinElement(UNI_L("Radio Button Skin"));
			if (skin_elm)
			{
				*w = 0;
				*h = 0;
				skin_elm->GetSize(w, h, 0);
				if (*w && *h)
					break;
			}
			OpWidgetInfo::GetPreferedSize(widget, type, w, h, cols, rows);
		}
		break;
	case OpTypedObject::WIDGET_TYPE_CHECKBOX:
		{
			OpSkinElement* skin_elm = g_skin_manager->GetSkinElement(UNI_L("Radio Button Skin"));
			if (skin_elm)
			{
				*w = 0;
				*h = 0;
				skin_elm->GetSize(w, h, 0);
				if (*w && *h)
					break;
			}
			OpWidgetInfo::GetPreferedSize(widget, type, w, h, cols, rows);
		}
		break;
*/
	default:
		OpWidgetInfo::GetPreferedSize(widget, type, w, h, cols, rows);
		break;
	};
#else
	OpWidgetInfo::GetPreferedSize(widget, type, w, h, cols, rows);
#endif
}

void IndpWidgetInfo::GetMinimumSize(OpWidget* widget, OpTypedObject::Type type, INT32* minw, INT32* minh)
{
	switch(type)
	{
	case OpTypedObject::WIDGET_TYPE_BUTTON:
		{
		OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Push Button Skin");
		if (skin_elm)
			skin_elm->GetMinSize(minw, minh, 0);
		}
		break;
	default:
		OpWidgetInfo::GetMinimumSize(widget, type, minw, minh);
		break;
	}
}

INT32 IndpWidgetInfo::GetDropdownButtonWidth(OpWidget* widget)
{
	if (widget->IsOfType(OpTypedObject::WIDGET_TYPE_DROPDOWN) && !((OpDropDown*)widget)->m_dropdown_packed.show_button)
		return 0;

	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Dropdown Button Skin");
	if (skin_elm)
	{
		INT32 w = 0, h = 0;
		skin_elm->GetSize(&w, &h, 0);
		return w;
	}
	return OpWidgetInfo::GetDropdownButtonWidth(widget);
}

INT32 IndpWidgetInfo::GetDropdownLeftButtonWidth(OpWidget* widget)
{
	if (widget->IsOfType(OpTypedObject::WIDGET_TYPE_DROPDOWN) && !((OpDropDown*)widget)->m_dropdown_packed.show_button)
		return 0;

	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Dropdown Left Button Skin");
	if (skin_elm)
	{
		INT32 w = 0, h = 0;
		skin_elm->GetSize(&w, &h, 0);
		return w;
	}
	return OpWidgetInfo::GetDropdownLeftButtonWidth(widget);
}

void IndpWidgetInfo::GetBorders(OpWidget* widget, INT32& left, INT32& top, INT32& right, INT32& bottom)
{
	if (widget->HasCssBorder() || widget->IsForm() || !widget->GetBorderSkin()->GetImage())
	{
		OpWidgetInfo::GetBorders(widget, left, top, right, bottom);
		return;
	}

	widget->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
}
#endif // SKIN_SUPPORT
