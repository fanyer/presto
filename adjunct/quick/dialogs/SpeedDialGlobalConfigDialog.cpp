/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Author: Petter Nilsen
*/

#include "core/pch.h"

#include "SpeedDialGlobalConfigDialog.h"

#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "adjunct/quick/widgets/OpZoomDropDown.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/WidgetContainer.h"

namespace
{
	enum // dropdown resizing modes
	{
		THUMBNAIL_SIZE_AUTO,
		THUMBNAIL_SIZE_FROM_ZOOM
	};
}

/***********************************************************************************
**
**  SpeedDialGlobalConfigDialog - Dialog for global configuration settings for speed dial
**
***********************************************************************************/

SpeedDialGlobalConfigDialog::SpeedDialGlobalConfigDialog()
		: m_extra_watched_window(0)
		, m_placement_relative_widget(NULL)
		, m_zoom_slider(NULL)
		, m_zoom_value(NULL)
{
}

SpeedDialGlobalConfigDialog::~SpeedDialGlobalConfigDialog()
{
	// stop listening to desktopwindow (that is a browserdektopwindow)
	if (m_extra_watched_window)
		m_extra_watched_window->RemoveListener(this);
}

/***********************************************************************************
**
**  OnInit
**
***********************************************************************************/

void SpeedDialGlobalConfigDialog::OnInit()
{
	//don't save position, the dialog will be aligned automatically
	SetSavePlacementOnClose(FALSE);	

	// Listen to parent desktop window to get notified about movements
	if (GetParentDesktopWindow())
		m_extra_watched_window = GetParentDesktopWindow()->GetParentDesktopWindow();
	if (m_extra_watched_window)
		m_extra_watched_window->AddListener(this);

	// get the old filename and store it
	DesktopFileChooserEdit *chooser = (DesktopFileChooserEdit*)GetWidgetByName("Speeddial_image_filename");
	if (chooser && chooser->GetEdit())
	{
		chooser->GetEdit()->SetReadOnly(TRUE);
	}
	g_speeddial_manager->GetBackgroundImageFilename(m_old_background_filename);
	if(m_old_background_filename.HasContent())
	{
		if(chooser)
		{
			chooser->SetText(m_old_background_filename.CStr());
		}
	}
	else
	{
		OpString text;
		OP_STATUS rc = g_languageManager->GetString(Str::D_SPEEDDIAL_CONFIG_CHANGE_DEFAULT_IMAGE, text);
		if (OpStatus::IsSuccess(rc) && text.HasContent())
		{
			chooser->GetEdit()->SetGhostText(text.CStr());
		}
	}
	if(chooser)
	{
		OpString filter;
		OP_STATUS rc = g_languageManager->GetString(Str::SI_OPEN_FIGURE_FILE_TYPES, filter);
		if (OpStatus::IsSuccess(rc) && filter.HasContent())
		{
			chooser->SetFilterString(filter.CStr());
		}		
	}

	OpWidget* disable = GetWidgetByName("Enable_image");
	if(disable)
	{
		disable->SetValue(g_speeddial_manager->IsBackgroundImageEnabled());
	}

	disable = GetWidgetByName("disable_plus");
	if (disable)
	{
		disable->SetValue(!g_speeddial_manager->IsPlusButtonShown());
	}

	SetWidgetEnabled("Image_layout_dropdown", g_speeddial_manager->IsBackgroundImageEnabled());
	SetWidgetEnabled("Speeddial_image_filename", g_speeddial_manager->IsBackgroundImageEnabled());

	OpStatus::Ignore(PopulateImageLayout());
	OpStatus::Ignore(PopulateColumnLayout());
	OpStatus::Ignore(PopulateThumbnailSize());

	m_zoom_value = static_cast<OpLabel*>(GetWidgetByName("zoom_value"));

	m_zoom_slider = static_cast<OpZoomSlider*>(GetWidgetByName("zoom_slider"));
	if (m_zoom_slider)
	{
		m_zoom_slider->SetEnabled(!g_speeddial_manager->IsScaleAutomatic());
		SetWidgetEnabled("zoom_slider_label", m_zoom_slider->IsEnabled());
		SetWidgetEnabled("zoom_value", m_zoom_slider->IsEnabled());
		m_zoom_value->SetJustify(m_zoom_value->GetRTL() ? JUSTIFY_LEFT : JUSTIFY_RIGHT, FALSE);

		// Force zoom value label update:
		OnChange(m_zoom_slider, FALSE);
	}
}

OP_STATUS SpeedDialGlobalConfigDialog::PopulateImageLayout()
{
	OpDropDown *dropdown = (OpDropDown *)GetWidgetByName("Image_layout_dropdown");
	if(dropdown)
	{
		OpString str;

		RETURN_IF_ERROR(g_languageManager->GetString(Str::M_SPEEDDIAL_CONFIG_PICTURE_BEST_FIT, str));
		RETURN_IF_ERROR(dropdown->AddItem(str.CStr(), -1, NULL, OpImageBackground::BEST_FIT));

		RETURN_IF_ERROR(g_languageManager->GetString(Str::M_SPEEDDIAL_CONFIG_PICTURE_CENTER, str));
		RETURN_IF_ERROR(dropdown->AddItem(str.CStr(), -1, NULL, OpImageBackground::CENTER));

		RETURN_IF_ERROR(g_languageManager->GetString(Str::M_SPEEDDIAL_CONFIG_PICTURE_STRETCH, str));
		RETURN_IF_ERROR(dropdown->AddItem(str.CStr(), -1, NULL, OpImageBackground::STRETCH));

		RETURN_IF_ERROR(g_languageManager->GetString(Str::M_SPEEDDIAL_CONFIG_PICTURE_TILE, str));
		RETURN_IF_ERROR(dropdown->AddItem(str.CStr(), -1, NULL, OpImageBackground::TILE));

		dropdown->SelectItem((INT32)g_speeddial_manager->GetImageLayout(), TRUE);
	}
	return OpStatus::OK;
}

OP_STATUS SpeedDialGlobalConfigDialog::PopulateColumnLayout()
{
	OpDropDown *dropdown = (OpDropDown *)GetWidgetByName("column_layout_dropdown");
	if(dropdown)
	{
		OpString str;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::M_SPEEDDIAL_CONFIG_COLUMNS_AUTOMATIC, str));
		RETURN_IF_ERROR(dropdown->AddItem(str.CStr(), -1, NULL));

		RETURN_IF_ERROR(g_languageManager->GetString(Str::M_SPEEDDIAL_CONFIG_COLUMNS_FIXED, str));

		const int FIXED_COLUMNS_MIN = 2;
		const int FIXED_COLUMNS_MAX = 7;
		for (int i = FIXED_COLUMNS_MIN; i <= FIXED_COLUMNS_MAX; i++)
		{
			OpString columns_str;
			RETURN_IF_ERROR(columns_str.AppendFormat(str.CStr(), i));
			RETURN_IF_ERROR(dropdown->AddItem(columns_str.CStr(), -1, NULL, i));
		};

		int number_of_columns = g_pcui->GetIntegerPref(PrefsCollectionUI::NumberOfSpeedDialColumns);

		if (number_of_columns == 0)
		{
			dropdown->SelectItem(0, TRUE);
		}
		else if (FIXED_COLUMNS_MIN <= number_of_columns && number_of_columns <= FIXED_COLUMNS_MAX)
		{
			dropdown->SelectItem(number_of_columns - FIXED_COLUMNS_MIN + 1, TRUE);
		}
	}
	return OpStatus::OK;
}

OP_STATUS SpeedDialGlobalConfigDialog::PopulateThumbnailSize()
{
	OpDropDown* drop_down = static_cast<OpDropDown*>(GetWidgetByName("thumbnail_size_dropdown"));
	if (!drop_down)
		return OpStatus::ERR;

	OpString str_automatic;
	OpString str_manual;

	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_SPEEDDIAL_CONFIG_ZOOM_CONTROL_AUTOMATIC, str_automatic));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_SPEEDDIAL_CONFIG_ZOOM_CONTROL_MANUAL, str_manual));

	RETURN_IF_ERROR(drop_down->AddItem(str_automatic.CStr(), -1, NULL, THUMBNAIL_SIZE_AUTO));
	RETURN_IF_ERROR(drop_down->AddItem(str_manual.CStr(), -1, NULL, THUMBNAIL_SIZE_FROM_ZOOM));

	if (g_speeddial_manager->IsScaleAutomatic())
		drop_down->SelectItem(0, TRUE);
	else
		drop_down->SelectItem(1, TRUE);

	return OpStatus::OK;
}

void SpeedDialGlobalConfigDialog::OnThumbnailScaleChanged(double scale)
{
	if (m_zoom_slider != NULL)
	{
		m_zoom_slider->SetValue(scale, FALSE);
	}
}

void SpeedDialGlobalConfigDialog::GetOverlayPlacement(OpRect& initial_placement, OpWidget* overlay_layout_widget)
{
	if (!m_placement_relative_widget)
		return Dialog::GetOverlayPlacement(initial_placement, overlay_layout_widget);

	const OpRect parent_rect = overlay_layout_widget->GetParent()->GetRect();
	const OpRect placement_rect = m_placement_relative_widget->GetRect();
	INT32 ignore;
	INT32 placement_bottom_padding = 0;
	m_placement_relative_widget->GetPadding(&ignore, &ignore, &ignore, &placement_bottom_padding);
	if (m_placement_relative_widget->GetRTL())
		initial_placement.x = parent_rect.x + placement_rect.x;
	else
		initial_placement.x = parent_rect.x + placement_rect.x + placement_rect.width - initial_placement.width;
	initial_placement.y = parent_rect.y + placement_rect.y + placement_rect.height - placement_bottom_padding;

	overlay_layout_widget->SetXResizeEffect(RESIZE_MOVE);
	overlay_layout_widget->SetYResizeEffect(RESIZE_FIXED);
	overlay_layout_widget->SetRect(initial_placement);

	GetSkinImage()->PointArrow(initial_placement, m_placement_relative_widget->GetRect());
}

void SpeedDialGlobalConfigDialog::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (desktop_window == m_extra_watched_window)
		m_extra_watched_window = 0;
};

void SpeedDialGlobalConfigDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	bool speed_dial_file_needs_save = false;

	if (widget->IsNamed("Enable_image"))
	{
		OpButton* disable_button = static_cast<OpButton*>(widget);
		SetWidgetEnabled("Image_layout_dropdown", disable_button->GetValue());
		SetWidgetEnabled("Speeddial_image_filename", disable_button->GetValue());
		if(!disable_button->GetValue() == g_speeddial_manager->IsBackgroundImageEnabled())
		{
			if (disable_button->GetValue())
			{
				OpString filename;
				OpFileChooserEdit* chooser = static_cast<OpFileChooserEdit*>(GetWidgetByName("Speeddial_image_filename"));
				if(chooser && OpStatus::IsSuccess(chooser->GetText(filename)))
				{
					g_speeddial_manager->LoadBackgroundImage(filename);
					OpStatus::Ignore(m_old_background_filename.Set(filename));
				}
			}
			g_speeddial_manager->SetBackgroundImageEnabled(disable_button->GetValue() ? true : false);
			speed_dial_file_needs_save = true;
		}
	}
	else if (widget->IsNamed("disable_plus"))
	{
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowAddSpeedDialButton, !widget->GetValue()));
	}
	else if (widget->IsNamed("thumbnail_size_dropdown"))
	{
		OpDropDown* drop_down = static_cast<OpDropDown*>(widget);
		const INT32 item = drop_down->GetSelectedItem();
		if (item >= 0)
		{
			const INT32 mode = drop_down->GetItemUserData(item);
			if (mode == THUMBNAIL_SIZE_AUTO)
			{
				g_speeddial_manager->SetAutomaticScale(true);
			}
			else if (m_zoom_slider != NULL)
			{
				g_speeddial_manager->SetAutomaticScale(false);
				g_speeddial_manager->SetThumbnailScale(m_zoom_slider->GetNumberValue());
			}
			SetWidgetEnabled("zoom_slider"      , mode != THUMBNAIL_SIZE_AUTO);
			SetWidgetEnabled("zoom_slider_label", mode != THUMBNAIL_SIZE_AUTO);
			SetWidgetEnabled("zoom_value"       , mode != THUMBNAIL_SIZE_AUTO);
			g_input_manager->UpdateAllInputStates();
		}
	}
	else if (widget->IsNamed("column_layout_dropdown"))
	{
		OpDropDown* dropdown = static_cast<OpDropDown*>(widget);
		INT32 sel_item = dropdown->GetSelectedItem();
		if(sel_item > -1)
		{
			/* Some compilers refuse to do a narrowing cast from a
			 * pointer to an int.  So a two-stage cast is required:
			 * One non-narrowing cast from a pointer to an integral
			 * type, and another narrowing cast between integral
			 * types.
			 *
			 * (This is safe, because the user data of these items is
			 * always set to a "small" integer anyway).
			 */
			int val = (INTPTR)(dropdown->GetItemUserData(sel_item));
			TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::NumberOfSpeedDialColumns, val));
			g_speeddial_manager->OnColumnLayoutChanged();
		}
	}
	else if (widget->IsNamed("Image_layout_dropdown"))
	{
		OpDropDown* dropdown = static_cast<OpDropDown*>(widget);
		INT32 sel_item = dropdown->GetSelectedItem();
		if(sel_item > -1)
		{
			int val = (INTPTR)(dropdown->GetItemUserData(sel_item));
			OpImageBackground::Layout layout = static_cast<OpImageBackground::Layout>(val);
			if(layout != g_speeddial_manager->GetImageLayout())
			{
				g_speeddial_manager->SetImageLayout(layout);
				speed_dial_file_needs_save = true;
			}
		}
	}
	else if (widget->IsNamed("Speeddial_image_filename"))
	{
		OpString filename;
		OpFileChooserEdit* chooser = static_cast<OpFileChooserEdit*>(widget);
		if(OpStatus::IsSuccess(chooser->GetText(filename)) && filename.Compare(m_old_background_filename))
		{
			g_speeddial_manager->LoadBackgroundImage(filename);
			OpStatus::Ignore(m_old_background_filename.Set(filename));
			speed_dial_file_needs_save = true;
		}
	}
	else if (widget == m_zoom_slider)
	{
		if (m_zoom_value != NULL)
		{
			OpString value;
			value.AppendFormat(UNI_L("%d%%"), (int)(m_zoom_slider->GetNumberValue() * 100 + 0.5));
			m_zoom_value->SetText(value.CStr());
		}
	}

	if (speed_dial_file_needs_save)
	{
		OpStatus::Ignore(g_speeddial_manager->Save());
	}
}

void SpeedDialGlobalConfigDialog::QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values)
{
	if (GetParentDesktopWindow())
		GetParentDesktopWindow()->QueryZoomSettings(min, max, step, tick_values, num_tick_values);
}
