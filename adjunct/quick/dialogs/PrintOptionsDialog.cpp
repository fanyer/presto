/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _PRINT_SUPPORT_

#include "adjunct/quick/dialogs/PrintOptionsDialog.h"

#include "modules/locale/locale-enum.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/util/handy.h" //ARRAY_SIZE
#include "modules/util/str.h" //MetricStringToInt
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/pi/OpLocale.h"

/***********************************************************************************
**
**	OnInitVisibility
**
***********************************************************************************/
void PrintOptionsDialog::OnInitVisibility()
{
	OpDropDown* scalebox = static_cast<OpDropDown*>(GetWidgetByName("Print_scale_to"));
	if (scalebox)
	{
		OpString curr;
		curr.AppendFormat(UNI_L("%d%%"), g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrinterScale));

		static const uni_char* zoom[] = {	UNI_L("20%"),
											UNI_L("30%"),
											UNI_L("50%"),
											UNI_L("70%"),
											UNI_L("80%"),
											UNI_L("100%"),
											UNI_L("120%"),
											UNI_L("130%"),
											UNI_L("150%"),
											UNI_L("180%"),
											UNI_L("200%"),
											UNI_L("250%"),
											UNI_L("300%"),
											UNI_L("350%"),
											UNI_L("400%")	};

		for (size_t i = 0; i < ARRAY_SIZE(zoom); i++)
		{
			scalebox->AddItem(zoom[i]);
		}

		scalebox->SetAlwaysInvoke(TRUE);
		scalebox->SetEditableText();

		scalebox->SetText(curr.CStr());
	}

	double left  = g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginLeft);
	double right = g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginRight);
	double top   = g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginTop);
	double bottom= g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginBottom);

	if (GetWidgetContainer()->GetRoot()->GetRTL())
	{
		op_swap(left, right);
		SetWidgetText("label_for_Print_left", Str::DI_IDM_RIGHT);
		SetWidgetText("label_for_Print_right", Str::DI_IDM_LEFT);
	}

	OpString tmp;
	if (tmp.Reserve(10))
	{
		g_oplocale->NumberFormat(tmp.CStr(), tmp.Capacity(), top/100, 2, FALSE);
		SetWidgetText("Print_top", tmp.CStr());
		g_oplocale->NumberFormat(tmp.CStr(), tmp.Capacity(), bottom/100, 2, FALSE);
		SetWidgetText("Print_bottom", tmp.CStr());
		g_oplocale->NumberFormat(tmp.CStr(), tmp.Capacity(), left/100, 2, FALSE);
		SetWidgetText("Print_left", tmp.CStr());
		g_oplocale->NumberFormat(tmp.CStr(), tmp.Capacity(), right/100, 2, FALSE);
		SetWidgetText("Print_right", tmp.CStr());
	}

	OpCheckBox* check = static_cast<OpCheckBox*>(GetWidgetByName("Print_page_background"));
	if (check)
	{
		if (g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrintBackground))
		{
			check->SetValue(1);
		}
		else
		{
			check->SetValue(0);
		}
	}

	check = static_cast<OpCheckBox*>(GetWidgetByName("Print_headers_footers"));
	if (check)
	{
		if (g_pcprint->GetIntegerPref(PrefsCollectionPrint::ShowPrintHeader))
		{
			check->SetValue(1);
		}
		else
		{
			check->SetValue(0);
		}
	}
	SetWidgetValue("Print_msr", g_pcprint->GetIntegerPref(PrefsCollectionPrint::FitToWidthPrint));
}


/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/
UINT32 PrintOptionsDialog::OnOk()
{
	SavePrintOptions();
	return 0;
}


/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/
void PrintOptionsDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("Print_scale_to"))
	{
		OpDropDown* scalebox = static_cast<OpDropDown*>(widget);
		INT32 sel_item = scalebox->GetSelectedItem();
		if (sel_item > -1)
		{
			const uni_char* sel_text = scalebox->GetItemText(sel_item);
			if (sel_text)
			{
				scalebox->SetText(sel_text);
			}
		}
	}
}


/***********************************************************************************
**
**	SavePrintOptions
**
***********************************************************************************/
void PrintOptionsDialog::SavePrintOptions()
{
	OP_MEMORY_VAR int left, right, top, bottom;

	OpString text;
	OP_STATUS err;

	GetWidgetText("Print_scale_to", text);
	if (text.HasContent())
	{
		OP_MEMORY_VAR short scale = uni_atoi(text.CStr());
		if (scale < 20)
		{
			scale = 20;
		}
		else if (scale > 400)
		{
			scale = 400;
		}
		TRAP(err, g_pcprint->WriteIntegerL(PrefsCollectionPrint::PrinterScale, scale));
	}

	GetWidgetText("Print_top", text);
	if (text.HasContent())
	{
		char* tmp = reinterpret_cast<char*>(text.CStr());
		make_singlebyte_in_place(tmp);

		top = MetricStringToInt(tmp);
		if (top > 500)
		{
			top = 500;
		}
		TRAP(err, g_pcprint->WriteIntegerL(PrefsCollectionPrint::MarginTop, top));
	}

	GetWidgetText("Print_bottom", text);
	if (text.HasContent())
	{
		char* tmp = reinterpret_cast<char*>(text.CStr());
		make_singlebyte_in_place(tmp);

		bottom = MetricStringToInt(tmp);
		if (bottom > 500)
		{
			bottom = 500;
		}
		TRAP(err, g_pcprint->WriteIntegerL(PrefsCollectionPrint::MarginBottom, bottom));
	}

	const char* left_name = "Print_left";
	const char* right_name = "Print_right";
	if (GetWidgetContainer()->GetRoot()->GetRTL())
		op_swap(left_name, right_name);

	GetWidgetText(left_name, text);
	if (text.HasContent())
	{
		char* tmp = reinterpret_cast<char*>(text.CStr());
		make_singlebyte_in_place(tmp);

		left = MetricStringToInt(tmp);
		if (left > 500)
		{
			left = 500;
		}
		TRAP(err, g_pcprint->WriteIntegerL(PrefsCollectionPrint::MarginLeft, left));
	}

	GetWidgetText(right_name, text);
	if (text.HasContent())
	{
		char* tmp = reinterpret_cast<char*>(text.CStr());
		make_singlebyte_in_place(tmp);

		right = MetricStringToInt(tmp);
		if (right > 500)
		{
			right = 500;
		}
		TRAP(err, g_pcprint->WriteIntegerL(PrefsCollectionPrint::MarginRight, right));
	}

	OpCheckBox* check = static_cast<OpCheckBox*>(GetWidgetByName("Print_page_background"));
	if (check)
	{
		TRAP(err, g_pcprint->WriteIntegerL(PrefsCollectionPrint::PrintBackground, (BOOL)check->GetValue()));
	}

	check = static_cast<OpCheckBox*>(GetWidgetByName("Print_headers_footers"));
	if (check)
	{
		TRAP(err, g_pcprint->WriteIntegerL(PrefsCollectionPrint::ShowPrintHeader, (BOOL)check->GetValue()));
	}

	TRAP(err, g_pcprint->WriteIntegerL(PrefsCollectionPrint::FitToWidthPrint, (BOOL)GetWidgetValue("Print_msr")));

	g_prefsManager->CommitL();
}

#endif // _PRINT_SUPPORT_
