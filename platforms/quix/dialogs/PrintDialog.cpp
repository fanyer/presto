/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "platforms/quix/dialogs/PrintDialog.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"
#include "modules/display/prn_info.h" // Defines PRINT_SELECTION_USING_DUMMY_WINDOW
#include "modules/util/handy.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/pi/OpLocale.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"

#include "platforms/posix/posix_native_util.h"
#include "platforms/unix/base/common/unixutils.h"

//#define PRINT_LAST_PAGE_FIRST_SUPPORT


// Only used for hardcoded fallback printer
struct PaperType
{
	Str::LocaleString string_id;
	int width_mm;
	int height_mm;

} PaperTypeArray[30] =
{
	{Str::DI_QT_PRINT_A0, 841, 1189},
	{Str::DI_QT_PRINT_A1, 594, 841},
	{Str::DI_QT_PRINT_A2, 420, 594},
	{Str::DI_QT_PRINT_A3, 297, 420},
	{Str::DI_QT_PRINT_A4, 210, 297},
	{Str::DI_QT_PRINT_A5, 148, 210},
	{Str::DI_QT_PRINT_A6, 105, 148},
	{Str::DI_QT_PRINT_A7, 74,  105},
	{Str::DI_QT_PRINT_A8, 52,  74},
	{Str::DI_QT_PRINT_A9, 37,  52},
	{Str::DI_QT_PRINT_B0, 1030,1456},
	{Str::DI_QT_PRINT_B1, 728, 1030},
	{Str::DI_QT_PRINT_B2, 515, 728},
	{Str::DI_QT_PRINT_B3, 364, 515},
	{Str::DI_QT_PRINT_B4, 257, 364},
	{Str::DI_QT_PRINT_B5, 182, 257},
	{Str::DI_QT_PRINT_B6, 128, 182},
	{Str::DI_QT_PRINT_B7, 91, 128},
	{Str::DI_QT_PRINT_B8, 64, 91},
	{Str::DI_QT_PRINT_B9, 45, 64},
	{Str::DI_QT_PRINT_B10, 32, 45},
	{Str::DI_QT_PRINT_C5E, 163, 229},
	{Str::DI_QT_PRINT_DLE, 110, 220},
	{Str::DI_QT_PRINT_EXECUTIVE, 191, 254},
	{Str::DI_QT_PRINT_FOLIO, 210, 330},
	{Str::DI_QT_PRINT_LEDGER, 432, 279},
	{Str::DI_QT_PRINT_LEGAL, 216, 356},
	{Str::DI_QT_PRINT_LETTER, 216, 279},
	{Str::DI_QT_PRINT_TABLOID, 279, 432},
	{Str::DI_QT_PRINT_USCOMMON, 105, 241}
};



OP_STATUS PrintDialog::PaperItem::Init(const DestinationInfo::PaperItem& item)
{
	m_width_mm  = (UINT32)(0.5 + (item.width / 2.8346456693));
	m_height_mm = (UINT32)(0.5 + (item.length / 2.8346456693));
	return m_name.SetFromUTF8(item.name);
}

OP_STATUS PrintDialog::PaperItem::Init(const OpStringC& name, int width, int height)
{
	RETURN_IF_ERROR(m_name.Set(name));
	m_name_includes_size = TRUE;
	m_width_mm = width;
	m_height_mm = height;
	return OpStatus::OK;
}


OP_STATUS PrintDialog::PaperItem::MakeUIString(OpString& text)
{
	text.Empty();
	if (m_name_includes_size)
		return text.Set(m_name);
	else
		return text.AppendFormat( UNI_L("%s   (%d x %d mm)"), m_name.CStr(), m_width_mm, m_height_mm);
}


BOOL PrintDialog::PaperItem::HasMedia(const OpString& candidate)
{
	return candidate.HasContent() && !m_name.FindI(candidate);
}



OP_STATUS PrintDialog::PrinterState::Init(ToolkitPrinterHelper* helper)
{
	// Read from preferences
	RETURN_IF_ERROR(file.Set(g_pcprint->GetStringPref(PrefsCollectionPrint::PrinterFileName)));

	is_file    = g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrinterToFile);
	background = g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrintBackground);
	header     = g_pcprint->GetIntegerPref(PrefsCollectionPrint::ShowPrintHeader);
	scale      = g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrinterScale);
	fit_width  = g_pcprint->GetIntegerPref(PrefsCollectionPrint::FitToWidthPrint);
	left_dmm   = g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginLeft);
	right_dmm  = g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginRight);
	top_dmm    = g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginTop);
	bottom_dmm = g_pcprint->GetIntegerPref(PrefsCollectionPrint::MarginBottom);

	// Read from printer interface

	helper->ReleaseDestinations(destinations);
	destinations = NULL;
	if (helper->GetDestinations(destinations))
	{
		for (int i=0; destinations[i]; i++)
		{
			helper->GetPaperSizes(*destinations[i]);

			if (destinations[i]->is_default)
			{
				int index = destinations[i]->default_paper;

				OpString printer_name;
				if (OpStatus::IsSuccess(printer_name.SetFromUTF8(destinations[i]->name)))
				{
					PrintDialog::SessionItem* item = PrintDialog::FindSessionItem(printer_name);
					if (item)
					{
						index = item->m_active_paper;
						portrait = item->m_portrait;
					}
				}

				if (index >= 0 && index < destinations[i]->paperlist_size)
				{
					media_width_mm = static_cast<UINT32>(0.5 + (destinations[i]->paperlist[index]->width / 2.8346456693));
					media_height_mm = static_cast<UINT32>(0.5 + (destinations[i]->paperlist[index]->length / 2.8346456693));
				}
			}
		}
	}

	return OpStatus::OK;
}


void PrintDialog::PrinterState::WritePrefsL()
{
	g_pcprint->WriteStringL(PrefsCollectionPrint::PrinterFileName, file);
	g_pcprint->WriteIntegerL(PrefsCollectionPrint::PrinterToFile, is_file);
	g_pcprint->WriteIntegerL(PrefsCollectionPrint::PrintBackground, background);
	g_pcprint->WriteIntegerL(PrefsCollectionPrint::ShowPrintHeader, header);
	g_pcprint->WriteIntegerL(PrefsCollectionPrint::PrinterScale, scale);
	g_pcprint->WriteIntegerL(PrefsCollectionPrint::FitToWidthPrint, fit_width);
	g_pcprint->WriteIntegerL(PrefsCollectionPrint::MarginTop, top_dmm);
	g_pcprint->WriteIntegerL(PrefsCollectionPrint::MarginBottom, bottom_dmm);
	g_pcprint->WriteIntegerL(PrefsCollectionPrint::MarginLeft, left_dmm);
	g_pcprint->WriteIntegerL(PrefsCollectionPrint::MarginRight, right_dmm);
}


OP_STATUS PrintDialog::PrinterItem::Init(DestinationInfo* info)
{
	if (!info || !info->name)
	{
		RETURN_IF_ERROR(InitFallbackItem());
	}
	else
	{
		RETURN_IF_ERROR(m_printer_name.SetFromUTF8(info->name));
		RETURN_IF_ERROR(m_printer_location.SetFromUTF8(info->location));

		if (info->paperlist_size == 0)
		{
			RETURN_IF_ERROR(InitFallbackItem());
		}
		else
		{
			m_active_paper = info->default_paper;
			m_copies       = info->copies;

			for (int i=0; i<info->paperlist_size; i++)
			{
				PaperItem* item = OP_NEW(PaperItem,());
				if (!item || OpStatus::IsError(item->Init(*info->paperlist[i])) || OpStatus::IsError(m_paper.Add(item)))
				{
					OP_DELETE(item);
					return OpStatus::ERR_NO_MEMORY;
				}
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS PrintDialog::PrinterItem::InitFallbackItem()
{
	m_copies = 1;

	// Use current locale to determine what paper to set as default

	OpString languages, candidate;
	if (OpStatus::IsSuccess(g_op_system_info->GetUserLanguages(&languages)) && languages.HasContent())
	{
		int pos = languages.Find(",");
		if (pos != KNotFound)
			languages.Delete(pos);
		if (languages.FindI(UNI_L("en_US")) == 0 || languages.FindI(UNI_L("en_CA")) == 0 || languages.FindI(UNI_L("fr_CA")) == 0)
			candidate.Set("letter");
	}
	if (candidate.IsEmpty())
		candidate.Set("a4");

	// Install hardcoded paper values

	for (UINT32 i=0; i < ARRAY_SIZE(PaperTypeArray); i++)
	{
		PaperType& pt = PaperTypeArray[i];
		OpString text;
		TRAPD(err, g_languageManager->GetStringL(pt.string_id, text));
		PaperItem* item = OP_NEW(PaperItem,());
		if (!item || OpStatus::IsError(item->Init(text, pt.width_mm, pt.height_mm)) || OpStatus::IsError(m_paper.Add(item)))
		{
			OP_DELETE(item);
			return OpStatus::ERR_NO_MEMORY;
		}
		if (item->HasMedia(candidate))
			m_active_paper = i;
	}

	return OpStatus::OK;
}


OpAutoVector<PrintDialog::SessionItem> PrintDialog::m_session_data;

PrintDialog::PrintDialog(PrintDialogListener* listener, PrinterState& state)
	: m_state(state)
	, m_printer_model(3)
	, m_print_dialog_listener(listener)
{
}

void PrintDialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	is_enabled = TRUE;

	if (index == 0)
	{
		action = GetOkAction();
		TRAPD(err,g_languageManager->GetStringL(Str::DI_OPERA_QT_BUTTON_PRINT,text));
		is_default = TRUE;
		name.Set(WIDGET_NAME_BUTTON_OK);
	}
	else if (index == 1)
	{
		action = GetCancelAction();
		text.Set(GetCancelText());
		name.Set(WIDGET_NAME_BUTTON_CANCEL);
	}
	else if (index == 2)
	{
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_APPLY));
		TRAPD(err,g_languageManager->GetStringL(Str::DI_IDM_APPLY,text));
		name.Set(WIDGET_NAME_BUTTON_APPLY);
	}
}

void PrintDialog::OnInit()
{
	// 1) Initialize all content that does not depend on configuration

	// List title
	OpTreeView* printer_tree_view = static_cast<OpTreeView*>(GetWidgetByName("Printer_list"));
	if (printer_tree_view)
	{ 
		OpString printer, location, paper;
		TRAPD(err, g_languageManager->GetStringL(Str::DI_QT_PRINT_PRINTERCOLUMN, printer));
		TRAP(err, g_languageManager->GetStringL(Str::D_PRINT_LOCATION_COLUMN, location));
		TRAP(err, g_languageManager->GetStringL(Str::D_PRINT_OPTIONS_DIALOG_PAPER_ORIENTATION, paper));
		m_printer_model.SetColumnData(0, printer);
		m_printer_model.SetColumnData(1, location);
		m_printer_model.SetColumnData(2, paper);
	}

	// Scale
	OpDropDown* scale_dropdown = static_cast<OpDropDown*>(GetWidgetByName("Print_scale_dropdown"));
	if (scale_dropdown)
	{
		UINT16 zoom[] = { 20, 30, 50, 70, 80, 100, 120, 130, 150, 180, 200, 250, 300, 350, 400 };
		for (UINT32 i = 0; i < ARRAY_SIZE(zoom); i++)
		{
			OpString s;
			if (OpStatus::IsSuccess(s.AppendFormat(UNI_L("%d%%"), zoom[i])))
				scale_dropdown->AddItem(s);
		}
		scale_dropdown->SetAlwaysInvoke(TRUE);
		scale_dropdown->SetEditableText(TRUE);
	}

	// Orientation
	OpDropDown* paperorientation_dropdown = static_cast<OpDropDown*>(GetWidgetByName("Orientation_dropdown"));
	if (paperorientation_dropdown)
	{
		OpString portrait, landscape;
		TRAPD(err, g_languageManager->GetStringL(Str::DI_QT_PRINT_PORTRAIT, portrait));
		TRAP(err, g_languageManager->GetStringL(Str::DI_QT_PRINT_LANDSCAPE, landscape));
		paperorientation_dropdown->AddItem(portrait.CStr());
		paperorientation_dropdown->AddItem(landscape.CStr());
	}

	// 2) Initialize content with system data and prefs settings

	int active_printer = 0;

	if (m_state.destinations)
	{
		for (int i=0; m_state.destinations[i]; i++)
		{
			PrinterItem* item = OP_NEW(PrinterItem,());
			if (!item || OpStatus::IsError(item->Init(m_state.destinations[i])) || OpStatus::IsError(m_printer_list.Add(item)))
				OP_DELETE(item);
			else
			{
				if (m_state.destinations[i]->is_default)
					active_printer = m_printer_list.GetCount() - 1;
				GetSessionItem(*item);
			}
		}
	}

	if (m_printer_list.GetCount() == 0)
	{
		// Add a nameless fallback printer in case we have no real. Allows printing to file
		PrinterItem* item = OP_NEW(PrinterItem,());
		if (!item || OpStatus::IsError(item->Init(0)) || OpStatus::IsError(m_printer_list.Add(item)))
			OP_DELETE(item);
		else
			GetSessionItem(*item);
	}

	// Add all printers with a name to the list
	for (UINT32 i=0; i<m_printer_list.GetCount(); i++ )
	{
		PrinterItem* item = m_printer_list.Get(i);
		if (item->m_printer_name.HasContent())
		{
			UINT32 printer_index = m_printer_model.AddItem(item->m_printer_name, 0, 0, -1, item);
			m_printer_model.SetItemData(printer_index, 1, item->m_printer_location);
			PrintPaperAndOrientation(printer_index);
		}
	}

	if (printer_tree_view)
		printer_tree_view->SetTreeModel(&m_printer_model);

	// Select active printer or nameless fallback
	if (printer_tree_view && m_printer_model.GetItemCount() > 0)
		printer_tree_view->SetSelectedItem(active_printer);
	else
		OnPrinterChanged(0);


	SetWidgetValue("Print_to_file_checkbox", m_state.is_file);

	SetWidgetText("File_chooser", m_state.file);

	SetWidgetEnabled("Print_to_file_checkbox", !KioskManager::GetInstance()->GetNoSave());

	DesktopFileChooserEdit* chooser = static_cast<DesktopFileChooserEdit*>(GetWidgetByName("File_chooser"));
	if (chooser)
	{
		chooser->SetSelectorMode(DesktopFileChooserEdit::SaveSelector);
		OpString filter;
		TRAPD(err, g_languageManager->GetStringL(Str::S_POSTSCRIPT_FILE_TYPE, filter));
		chooser->SetFilterString(filter);
	}

#ifndef PRINT_SELECTION_USING_DUMMY_WINDOW
	SetWidgetEnabled("Selection_radio", FALSE);
#endif

	if (1 /*m_printer->printRange() == QPrinter::AllPages*/ )
		SetWidgetValue("All_radio", 1);
	else if (0 /*m_printer->printRange() == QPrinter::PageRange*/)
		SetWidgetValue("Pages_radio", 1);
	else
	{
#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
		SetWidgetValue("Selection_radio", 1);
#else
		SetWidgetValue("All_radio", 1);
#endif
	}

	INT32 from = 1;
	INT32 to   = 9999;
	SelectPageRangeMode(true);
	
	OpString text;
	text.AppendFormat(UNI_L("%d"), from);
	SetWidgetText("From_edit", text);
	text.Empty();
	text.AppendFormat(UNI_L("%d"), to);
	SetWidgetText("To_edit", text);

#if defined(PRINT_LAST_PAGE_FIRST_SUPPORT)
	SetWidgetValue( "First_first_radio", 1);
	SetWidgetValue( "Last_first_radio",  0);
#else
	SetWidgetValue( "First_first_radio", 1 );
	SetWidgetEnabled( "Last_first_radio", FALSE );
#endif
	SetWidgetValue("Print_msr", m_state.fit_width);

	if (scale_dropdown)
	{
		OpString s;
		s.AppendFormat(UNI_L("%d%%"), m_state.scale);
		scale_dropdown->SetText(s);
	}

	SetWidgetValue("Background_check", m_state.background);
	SetWidgetValue("Header_check", m_state.header);
	SetWidgetValue("Color_radio", 0);
	SetWidgetValue("Grayscale_radio", 1);
	SetMarginValue("Print_top", m_state.top_dmm );
	SetMarginValue("Print_bottom", m_state.bottom_dmm );
	SetMarginValue("Print_left", m_state.left_dmm );
	SetMarginValue("Print_right", m_state.right_dmm );
}


void PrintDialog::OnPrintToFileChanged(BOOL state)
{
	SetWidgetEnabled("File_chooser", state && !KioskManager::GetInstance()->GetNoSave());
	g_input_manager->UpdateAllInputStates();
}

void PrintDialog::OnOrientationChanged(UINT32 printer_index, BOOL portrait)
{
	PrinterItem* item = static_cast<PrinterItem*>(m_printer_model.GetItemUserData(printer_index));
	if (item)
	{
		item->m_portrait = portrait;
		PrintPaperAndOrientation(printer_index);
	}
}

void PrintDialog::OnPaperChanged(UINT32 printer_index, int value)
{
	PrinterItem* item = static_cast<PrinterItem*>(m_printer_model.GetItemUserData(printer_index));
	if (item)
	{
		item->m_active_paper = value;
		PrintPaperAndOrientation(printer_index);
	}
}

void PrintDialog::PrintPaperAndOrientation(UINT32 printer_index)
{
	PrinterItem* item = static_cast<PrinterItem*>(m_printer_model.GetItemUserData(printer_index));
	if (item && item->m_active_paper < item->m_paper.GetCount())
	{
		OpString text;
		text.Set(item->m_paper.Get(item->m_active_paper)->m_name);
		OpString tmp;
		TRAPD(rc, g_languageManager->GetStringL(item->m_portrait ? Str::DI_QT_PRINT_PORTRAIT : Str::DI_QT_PRINT_LANDSCAPE, tmp));
		if (tmp.HasContent())
			text.AppendFormat(UNI_L(" / %s"), tmp.CStr());
		m_printer_model.SetItemData(printer_index, 2, text);
	}
}


void PrintDialog::OnPrinterChanged(UINT32 printer_index)
{
	if (printer_index >= m_printer_list.GetCount() )
		return;

	PrinterItem* item = m_printer_list.Get(printer_index);
	if (!item)
		return;

	OpDropDown* papertype_dropdown = static_cast<OpDropDown*>(GetWidgetByName("Papertype_dropdown"));
	if (papertype_dropdown)
	{
		papertype_dropdown->Clear();
		for (UINT32 i=0; i<item->m_paper.GetCount(); i++)
		{
			OpString text;
			PaperItem* paper = item->m_paper.Get(i);
			paper->MakeUIString(text);
			papertype_dropdown->AddItem(text, -1, 0, reinterpret_cast<INTPTR>(paper));
		}
		papertype_dropdown->SelectItem(item->m_active_paper, TRUE);
	}

	OpDropDown* orientation_dropdown = static_cast<OpDropDown*>(GetWidgetByName("Orientation_dropdown"));
	if (orientation_dropdown)
		orientation_dropdown->SelectItem(item->m_portrait ? 0 : 1, TRUE);

	OpString text;
	text.AppendFormat(UNI_L("%d"), item->m_copies);
	SetWidgetText("Number_copies_edit", text);

}

void PrintDialog::OnSetFocus()
{
	Dialog::OnSetFocus();

	// Keeping code and comment for a while. Probably no longer needed [espen 2010-10-19]

	/*
	// Workaround for column width=0 problem in OpTreeView
	// Sometimes (timing issue) the column widths remain zero when
	// we show the dialog. Seems that a layout request is not
	// sent. OnSetFocus() is called after the dialog becomes visible
	// [espen 2004-01-08]

	OpTreeView* printer_tree_view = (OpTreeView*)GetWidgetByName("Printer_list");
	if (printer_tree_view)
		printer_tree_view->GenerateOnLayout(TRUE);

	OpDropDown* scale_dropdown = (OpDropDown*)(GetWidgetByName("Print_scale_dropdown"));
	if (scale_dropdown)
		scale_dropdown->GenerateOnLayout(TRUE);
	*/
}


void PrintDialog::SaveSettings()
{
	// Save data that we want to restore if we reopen the dialog in the same session
	// So that we remember a non-default paper size + more
	for (UINT32 i=0; i<m_printer_list.GetCount(); i++ )
		SetSessionItem(*m_printer_list.Get(i));

	OpTreeView* printer_tree_view = static_cast<OpTreeView*>(GetWidgetByName("Printer_list"));
	if (printer_tree_view)
	{
		INT32 index = printer_tree_view->GetSelectedItemModelPos();
		if (index >= 0 && m_state.destinations)
			m_state.printer_name.SetFromUTF8(m_state.destinations[index]->name);
	}

	OpString text;
	DesktopFileChooserEdit* chooser = static_cast<DesktopFileChooserEdit*>(GetWidgetByName("File_chooser"));
	if (chooser)
	{
		chooser->GetEdit()->GetText(text);
		if (!text.IsEmpty() && text.FindFirstOf('/') == KNotFound)
		{
			char buf[MAX_PATH];
			if (getcwd(buf, MAX_PATH) == 0)
			{
				OpString tmp;
				RETURN_VOID_IF_ERROR(PosixNativeUtil::FromNative(buf, &tmp));
				RETURN_VOID_IF_ERROR(tmp.Append("/"));
				RETURN_VOID_IF_ERROR(tmp.Append(text));
				text.TakeOver(tmp);
			}
		}
	}

	m_state.file.Set(text);
	m_state.is_file = GetWidgetValue("Print_to_file_checkbox") ? TRUE : FALSE;

	OpDropDown* papertype_dropdown = static_cast<OpDropDown*>(GetWidgetByName("Papertype_dropdown"));
	if (papertype_dropdown)
	{
		PaperItem* paper = reinterpret_cast<PaperItem*>(papertype_dropdown->GetItemUserData(papertype_dropdown->GetSelectedItem()));
		if (paper)
		{
			m_state.media_width_mm  = paper->m_width_mm;
			m_state.media_height_mm = paper->m_height_mm;
		}
	}

	m_state.portrait      = GetSelectedDropDownValue( "Orientation_dropdown", 0) == 0;
	m_state.reverse       = GetWidgetValue("First_first_radio") ? FALSE : TRUE;
	m_state.background    = GetWidgetValue("Background_check") ? TRUE : FALSE;
	m_state.header        = GetWidgetValue("Header_check") ? TRUE : FALSE;
	m_state.fit_width     = GetWidgetValue("Print_msr") ? TRUE : FALSE;
	if (GetWidgetValue("All_radio"))
		m_state.range     = PrinterState::PrintAll;
	else if (GetWidgetValue("Selection_radio"))
		m_state.range     = PrinterState::PrintSelection;
	else
		m_state.range     = PrinterState::PrintPages;
	m_state.from          = GetEditValue("From_edit", 1, 1, 9999 );
	m_state.to            = GetEditValue("To_edit", 1, 1, 9999 );
	m_state.copies        = GetEditValue("Number_copies_edit", 1, 1, 100 );
	m_state.scale         = GetEditValue("Print_scale_dropdown", 100, 20, 400 );
	m_state.top_dmm       = GetMarginValue("Print_top", 100, 0, 500);
	m_state.bottom_dmm    = GetMarginValue("Print_bottom", 100, 0, 500);
	m_state.left_dmm      = GetMarginValue("Print_left", 100, 0, 500);
	m_state.right_dmm     = GetMarginValue("Print_right", 100, 0, 500);

	TRAPD(rc, m_state.WritePrefsL());
}


BOOL PrintDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_OK:
				{
					OpTreeView* printer_tree_view = static_cast<OpTreeView*>(GetWidgetByName("Printer_list"));
					if (printer_tree_view)
					{
						BOOL print_to_file = GetWidgetValue("Print_to_file_checkbox");
						child_action->SetEnabled(print_to_file || printer_tree_view->GetSelectedItemModelPos()!= -1);
					}
				}
				return TRUE;
			}
		}
		break;

		case OpInputAction::ACTION_OK:
		{
			BOOL print_to_file = GetWidgetValue("Print_to_file_checkbox");
			if (print_to_file)
			{
				OpString filename;
				GetWidgetText( "File_chooser", filename );
				if (!UnixUtils::CheckFileToWrite(this, filename, TRUE))
					return TRUE;
			}

			SaveSettings();

			if (m_print_dialog_listener)
				m_print_dialog_listener->OnDialogClose(true);
		}
		break;

		case OpInputAction::ACTION_APPLY:
			SaveSettings();
		break;
	}

	return Dialog::OnInputAction(action);
}


void PrintDialog::OnChange(OpWidget* widget, BOOL changed_by_mouse)
{
	if (widget == GetWidgetByName("Print_to_file_checkbox"))
	{
		OnPrintToFileChanged(GetWidgetValue("Print_to_file_checkbox"));
	}
	else if (widget == static_cast<OpTreeView*>(GetWidgetByName("Printer_list")))
	{
		OpTreeView* printer_tree_view = static_cast<OpTreeView*>(GetWidgetByName("Printer_list"));
		OnPrinterChanged(printer_tree_view->GetSelectedItemModelPos());
	}
	else if(widget == static_cast<OpDropDown*>(GetWidgetByName("Orientation_dropdown")))
	{
		int value = GetSelectedDropDownValue("Orientation_dropdown", -1);
		if (value >= 0)
		{
			OpTreeView* printer_tree_view = static_cast<OpTreeView*>(GetWidgetByName("Printer_list"));
			if (printer_tree_view)
			{
				int pos = printer_tree_view->GetSelectedItemModelPos();
				if (pos >= 0)
				{
					OnOrientationChanged(pos, value == 0);
				}
			}
		}
	}
	else if(widget == static_cast<OpDropDown*>(GetWidgetByName("Papertype_dropdown")))
	{
		int value = GetSelectedDropDownValue("Papertype_dropdown", -1);
		if (value >= 0)
		{
			OpTreeView* printer_tree_view = static_cast<OpTreeView*>(GetWidgetByName("Printer_list"));
			if (printer_tree_view)
			{
				int pos = printer_tree_view->GetSelectedItemModelPos();
				if (pos >= 0)
				{
					OnPaperChanged(pos, value);
				}
			}
		}
	}

	Dialog::OnChange(widget,changed_by_mouse);
}


void PrintDialog::SelectPageRangeMode( BOOL use_page_range )
{
	SetWidgetEnabled("From_edit", use_page_range );
	SetWidgetEnabled("To_edit", use_page_range );
}

INT32 PrintDialog::GetSelectedDropDownValue( const char* name, int default_value )
{
	OpDropDown* dropdown = static_cast<OpDropDown*>(GetWidgetByName(name));
	if (dropdown)
	{
		return dropdown->GetSelectedItem();
	}
	else
	{
		return default_value;
	}
}

INT32 PrintDialog::GetEditValue( const char* name, INT32 default_value, INT32 min, INT32 max )
{
	OpString text;

	GetWidgetText(name, text);

	if (text.HasContent())
	{
		int value = uni_atoi(text.CStr());
		if (value < min)
			value = min;
		else if (value > max)
			value = max;
		return value;
	}
	else
	{
		return default_value;
	}
}

INT32 PrintDialog::GetMarginValue( const char* name, INT32 default_value, INT32 min, INT32 max )
{
	OpString text;
	GetWidgetText(name, text);

	OpString8 tmp;
	tmp.Set(text);

	if (tmp.HasContent())
	{
		INT32 value = MetricStringToInt(tmp.CStr());
		if (value < min)
			value = min;
		else if (value > max)
			value = max;
		return value;
	}
	else
	{
		return default_value;
	}
}

void PrintDialog::SetMarginValue(const char* name, INT32 value)
{
	OpString t_value;
	t_value.Empty();
	if (t_value.Reserve(10))
	{
		g_oplocale->NumberFormat(t_value.CStr(), t_value.Capacity(), (double)value/100, 2, FALSE);
	}
	SetWidgetText (name, t_value);
}




PrintDialog::SessionItem* PrintDialog::FindSessionItem(const OpString& name)
{
	for (UINT32 i=0; i<m_session_data.GetCount(); i++)
	{
		SessionItem* si = m_session_data.Get(i);
		if (si->m_printer_name.IsEmpty() && name.IsEmpty())
			return si;
		else if (!si->m_printer_name.Compare(name))
			return si;
	}
	return 0;
}

OP_STATUS PrintDialog::SetSessionItem(const PrinterItem& item)
{
	SessionItem* si = FindSessionItem(item.m_printer_name);
	if (!si)
	{
		si = OP_NEW(SessionItem,());
		if (!si)
			return OpStatus::ERR_NO_MEMORY;;
		if (item.m_printer_name.HasContent() && OpStatus::IsError(si->m_printer_name.Set(item.m_printer_name)))
		{
			OP_DELETE(si);
			return OpStatus::ERR_NO_MEMORY;
		}
		if (OpStatus::IsError(m_session_data.Add(si)))
		{
			OP_DELETE(si);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	si->m_portrait     = item.m_portrait;
	si->m_active_paper = item.m_active_paper;

	return OpStatus::OK;
}


void PrintDialog::GetSessionItem(PrinterItem& item)
{
	SessionItem* si = FindSessionItem(item.m_printer_name);
	if (si)
	{
		item.m_portrait     = si->m_portrait;
		item.m_active_paper = si->m_active_paper;
	}
}
