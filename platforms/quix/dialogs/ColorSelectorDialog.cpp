/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "ColorSelectorDialog.h"

#include "modules/prefs/prefsmanager/collections/pc_unix.h"

#include "platforms/unix/base/x11/x11_colorpickermanager.h"

#include <errno.h>


// Colors in arrays, ColorSelectorDialog::m_color and data read/written to prefs 
// are in all RGB format. Colors in and out of the dialog are in GBR


static UINT32 PredefinedColors[48] = 
{
	0xFF000000,
	0xFFAA0000,
	0xFF005500,
	0xFFAA5500,
	0xFF00AA00,
	0xFFAAAA00,
	0xFF00FF00,
	0xFFAAFF00,

	0xFF00007F,
	0xFFAA007F,
	0xFF00557F,
	0xFFAA557F,
	0xFF00AA7F,
	0xFFAAAA7F,
	0xFF00FF7F,
	0xFFAAFF7F,

	0xFF0000FF,
	0xFFAA00FF,
	0xFF0055FF,
	0xFFAA55FF,
	0xFF00AAFF,
	0xFFAAAAFF,
	0xFF00FFFF,
	0xFFAAFFFF,

	0xFF550000,
	0xFFFF0000,
	0xFF555500,
	0xFFFF5500,
	0xFF55AA00,
	0xFFFFAA00,
	0xFF55FF00,
	0xFFFFFF00,
	
	0xFF55007F,
	0xFF7F007F,
	0xFF55557F,
	0xFFFF557F,
	0xFF55AA7F,
	0xFFFFAA7F,
	0xFF55FF7F,
	0xFFFFFF7F,
	
	0xFF5500FF,
	0xFFFF00FF,
	0xFF5555FF,
	0xFFFF55FF,
	0xFF55AAFF,
	0xFFFFAAFF,
	0xFF55FFFF,
	0xFFffFFFF
};


static UINT32 CustomColors[16] = 
{
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF
};


ColorSelectorDialog::ColorSelectorDialog(UINT32 color)
	:m_color(color)
	,m_color_selector_dialog_listener(0)
	,m_validate(TRUE)
{
	m_color = SwitchRGB(m_color);
}

ColorSelectorDialog::~ColorSelectorDialog()
{
}


void ColorSelectorDialog::OnInit()
{
	// Reuse old translations. We no longer use accels in labels
	RemoveAmpersand("basic_label");
	RemoveAmpersand("custom_label");
	RemoveAmpersand("Red_label");
	RemoveAmpersand("Green_label");
	RemoveAmpersand("Blue_label");
	RemoveAmpersand("Add_custom");


	OpWidget* widget = GetWidgetByName("Current_color");
	if (widget)
		widget->SetEnabled(FALSE);

	OpString colors;
	colors.Set(g_pcunix->GetStringPref(PrefsCollectionUnix::CustomColors));
	if (colors.HasContent())
	{
		UINT32 i=0;
		uni_char* p = uni_strtok(colors.CStr(), UNI_L(","));
		while(p && i<ARRAY_SIZE(CustomColors))
		{
			errno = 0;
			uni_char* endptr = 0;
			UINT32 value = uni_strtol(p, &endptr, 0);
			if(errno == 0)
				CustomColors[i] = 0xFF000000 | value;
			p = uni_strtok(0,UNI_L(","));
			i++;
		}
	}

	for (UINT32 i=0; i< ARRAY_SIZE(PredefinedColors); i++ )
	{
		OpString8 name;
		name.AppendFormat("Color%d", i);

		OpWidget* widget = GetWidgetByName(name.CStr());
		if (widget)
			widget->SetBackgroundColor(SwitchRGB(PredefinedColors[i]));
	}

	for (UINT32 i=0; i< ARRAY_SIZE(CustomColors); i++ )
	{
		OpString8 name;
		name.AppendFormat("Custom%d", i);

		OpWidget* widget = GetWidgetByName(name.CStr());
		if (widget)
			widget->SetBackgroundColor(SwitchRGB(CustomColors[i]));
	}

	SetColor(m_color);
}

void ColorSelectorDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (m_validate) 
	{
		BOOL valid = TRUE;
		INT32 color = m_color;

		if (widget == GetWidgetByName("Red_edit"))
		{
			UINT32 value = GetColorValue("Red_edit", valid);
			if (valid)
				color = color&0xFF00FFFF | value << 16;
			SetColor(color);
		}
		else if (widget == GetWidgetByName("Green_edit"))
		{
			UINT32 value = GetColorValue("Green_edit", valid);
			if (valid)
				color = color&0xFFFF00FF | value << 8;
			SetColor(color);
		}
		else if (widget == GetWidgetByName("Blue_edit"))
		{
			UINT32 value = GetColorValue("Blue_edit", valid);
			if (valid)
				color = color&0xFFFFFF00 | value;
			SetColor(color);
		}
		else
		{
			Dialog::OnChange(widget,changed_by_mouse);
		}
	}
	else
	{
		Dialog::OnChange(widget,changed_by_mouse);
	}
}


BOOL ColorSelectorDialog::OnInputAction(OpInputAction* action)
{
	switch(action->GetAction())
	{
		case OpInputAction::ACTION_CHOOSE_LINK_COLOR:
		{
			UINT32 index = action->GetActionData();
			if (index < ARRAY_SIZE(PredefinedColors))
			{
				SetColor(PredefinedColors[index]);
			}
			else if (index >= 100 && index <= ARRAY_SIZE(CustomColors)+100)
			{
				SetColor(CustomColors[index-100]);
			}
			else if (index == 200)
			{
				OpPoint point;
				X11Widget* target;
				if (OpStatus::IsSuccess(ColorPickerManager::GetWidget(target, point, SwitchRGB(m_color))))
				{
					// Not using the X11Widget yet. I need a way to determine what OpWidget
					// the X11Widget and the position represents. Code below works just fine
					for (UINT32 i=0; i< ARRAY_SIZE(CustomColors); i++ )
					{
						OpString8 name;
						name.AppendFormat("Custom%d", i);
						
						OpWidget* widget = (OpWidget*)GetWidgetByName(name.CStr());
						if (widget && widget->GetBounds().Contains(widget->GetMousePos()))
						{
							CustomColors[i] = m_color;
							widget->SetBackgroundColor(SwitchRGB(CustomColors[i]));
							widget->InvalidateAll();
							break;
						}
					}
				}
			}
			else if (index == 300)
			{
				UINT32 color = 0xFF000000;
				if (OpStatus::IsSuccess(ColorPickerManager::GetColor(color)))
					SetColor(0xFF000000|color);
			}


			return TRUE;
		}
		break;

		case OpInputAction::ACTION_OK:
		{
			OpString colors;
			for( UINT32 i=0; i<ARRAY_SIZE(CustomColors); i++)
			{
				if (colors.HasContent())
					colors.Append(UNI_L(","));
				colors.AppendFormat(UNI_L("0x%06x"),0xFFFFFF & CustomColors[i]);
			}
			TRAPD(rc,g_pcunix->WriteStringL(PrefsCollectionUnix::CustomColors, colors));

			if (m_color_selector_dialog_listener)
				m_color_selector_dialog_listener->OnDialogClose(true, SwitchRGB(m_color));
		}
		break;
	}

	return Dialog::OnInputAction(action);
}


void ColorSelectorDialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	is_enabled = TRUE;

	if (index == 0)
	{
		action = GetOkAction();
		text.Set(GetOkText());
		is_default = TRUE;
		name.Set(WIDGET_NAME_BUTTON_OK);
	}
	else if (index == 1)
	{
		action = GetCancelAction();
		text.Set(GetCancelText());
		name.Set(WIDGET_NAME_BUTTON_CANCEL);
	}
}



void ColorSelectorDialog::SetColor(UINT32 color)
{
	m_validate = FALSE;
	m_color = 0xFF000000 | color;

	OpWidget* widget = GetWidgetByName("Current_color");
	if (widget)
	{
		widget->SetBackgroundColor(SwitchRGB(m_color));
		widget->InvalidateAll();
	}

	// Red
	OpString text;
	text.AppendFormat(UNI_L("%d"), (m_color&0xFF0000)>>16);
	SetWidgetText("Red_edit", text);
	
	// Green
	text.Empty();
	text.AppendFormat(UNI_L("%d"), (m_color&0x00FF00)>>8);
	SetWidgetText("Green_edit", text);

	// Blue
	text.Empty();
	text.AppendFormat(UNI_L("%d"), m_color&0xFF);
	SetWidgetText("Blue_edit", text);

	m_validate = TRUE;
}



UINT32 ColorSelectorDialog::GetColorValue(const char* name, BOOL& valid)
{
	OpString text;
	GetWidgetText(name, text);
	
	if (text.IsEmpty())
	{
		valid = TRUE;
		return 0;
	}

	errno = 0;
	uni_char* endptr = 0;
	long value = uni_strtol(text.CStr(), &endptr, 0);

	valid = errno == 0 && (!endptr || !*endptr) && value >= 0 && value <= 255;
	return valid ? (INT32)value : 0;
}


INT32 ColorSelectorDialog::SwitchRGB(UINT32 color)
{
	INT32 r = (color&0xFF0000)>>16;
	INT32 g = (color&0xFF00)>>8;
	INT32 b = (color&0xFF);	
	return 0xFF000000 | b<<16 | g<<8 | r;
}


void ColorSelectorDialog::RemoveAmpersand(const char* name)
{
	OpString text;
	GetWidgetText(name,text);
	text.ReplaceAll(UNI_L("&"),0);
	SetWidgetText(name,text);
}
