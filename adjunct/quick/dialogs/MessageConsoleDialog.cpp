/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file MessageConsoleDialog.cpp
 *
 * Contains function definitions for the classes defined in the corresponding
 * header file.
 */

#include "core/pch.h"

#ifdef OPERA_CONSOLE

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/dialogs/MessageConsoleDialog.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/MessageConsoleManager.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/dochand/winman.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpMultiEdit.h"


/*****************************************************************************
 *                                                                           *
 * Mapping between message source components and strings.                    *
 *                                                                           *
 ****************************************************************************/

struct MessageStringPair
{
	OpConsoleEngine::Source	source;    // Message source. Must have content for all entries.
	const uni_char*			string;    // Dumb hardcoded string describing the source. Will usually work reasonably since these are technical terms that often don't need translation.
	Str::LocaleString		string_id; // Localized string reference for source names that are in the string database. Set NOT_A_STRING if there is no translation.
	MessageStringPair(OpConsoleEngine::Source src, const uni_char* str, Str::LocaleString loc_str) : source(src), string(str), string_id(loc_str) {};
};

// When adding new components in core they need to be added to this mapping
// for the dialog to display them on the dropdown list. This is because
// core doesen't actually give the entries names, they are all enum entries.

static MessageStringPair message_string_mapping[] =
{
	//						Source								Hardcoded string		Localized string ID
	//						Must be set							Must be set				Set Str::NOT_A_STRING if unavailable. Dialog will use hardcoded string in stead.
	MessageStringPair(	OpConsoleEngine::EcmaScript,		UNI_L("JavaScript"),	Str::NOT_A_STRING				),
	MessageStringPair(	OpConsoleEngine::CSS,				UNI_L("CSS"),			Str::NOT_A_STRING				),
	MessageStringPair(	OpConsoleEngine::Network,			UNI_L("Network"),		Str::D_NEW_PREFS_NETWORK		),
#ifdef _BITTORRENT_SUPPORT_
	MessageStringPair(	OpConsoleEngine::BitTorrent,		UNI_L("BitTorrent"),	Str::NOT_A_STRING				),
#endif
	MessageStringPair(	OpConsoleEngine::M2,				UNI_L("M2"),			Str::D_CONSOLE_MAIL_AND_CHAT	),
	MessageStringPair(	OpConsoleEngine::XML,				UNI_L("XML"),			Str::NOT_A_STRING				),
	MessageStringPair(	OpConsoleEngine::HTML,				UNI_L("HTML"),			Str::NOT_A_STRING				),
	MessageStringPair(	OpConsoleEngine::XSLT,				UNI_L("XSLT"),			Str::NOT_A_STRING				),
#ifdef SVG_SUPPORT
	MessageStringPair(	OpConsoleEngine::SVG,				UNI_L("SVG"),			Str::NOT_A_STRING				),
#endif
#ifdef GADGET_SUPPORT
	MessageStringPair(	OpConsoleEngine::Gadget,			UNI_L("Gadget"),		Str::D_WIDGETS					),
#endif
#ifdef SELFTEST
	MessageStringPair(	OpConsoleEngine::SelfTest,			UNI_L("SelfTest"),		Str::NOT_A_STRING				),
#endif
	MessageStringPair(	OpConsoleEngine::Internal,			UNI_L("Internal"),		Str::S_INTERNAL					),
#if defined(SUPPORT_DATA_SYNC) || defined(WEBSERVER_SUPPORT)
	// If we ONLY have link then the name in the console will be Opera Link
	MessageStringPair(	OpConsoleEngine::OperaAccount,		UNI_L("OperaAccount"),	Str::S_OPERA_ACCOUNT			),
#endif // defined(SUPPORT_DATA_SYNC) || defined(WEBSERVER_SUPPORT)
#ifdef CON_CAP_LINK_UNITE
#ifdef SUPPORT_DATA_SYNC
	MessageStringPair(	OpConsoleEngine::OperaLink,			UNI_L("Opera Link"),	Str::S_OPERA_LINK				),
#endif // SUPPORT_DATA_SYNC
#ifdef WEBSERVER_SUPPORT
	MessageStringPair(	OpConsoleEngine::OperaUnite,		UNI_L("Opera Unite"),	Str::D_WEBSERVER_NAME			),
#endif // WEBSERVER_SUPPORT
#endif // CON_CAP_LINK_UNITE

#ifdef AUTO_UPDATE_SUPPORT
	MessageStringPair(	OpConsoleEngine::AutoUpdate,		UNI_L("AutoUpdate"),	Str::D_AUTO_UPDATE_TITLE		),
#endif
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	MessageStringPair(	OpConsoleEngine::PersistentStorage,	UNI_L("Persistent Storage"),	Str::NOT_A_STRING		),
#endif //DATABASE_MODULE_MANAGER_SUPPORT
#ifdef WEBSOCKETS_SUPPORT
	MessageStringPair(	OpConsoleEngine::WebSockets,		UNI_L("WebSockets"),	Str::NOT_A_STRING				),
#endif // WEBSOCKETS_SUPPORT
#ifdef GEOLOCATION_SUPPORT
	MessageStringPair(	OpConsoleEngine::Geolocation,		UNI_L("Geolocation"),	Str::NOT_A_STRING				),
#endif // GEOLOCATION_SUPPORT
#ifdef _PLUGIN_SUPPORT_
	MessageStringPair(	OpConsoleEngine::Plugins,			UNI_L("Plug-in"),		Str::NOT_A_STRING				),
#endif // _PLUGIN_SUPPORT_
	MessageStringPair(	OpConsoleEngine::SourceTypeLast,	UNI_L(""),				Str::NOT_A_STRING				)
};

/*****************************************************************************
 *                                                                           *
 * MessageConsoleDialog class implementation begins here.                    *
 *                                                                           *
 ****************************************************************************/

MessageConsoleDialog::MessageConsoleDialog(MessageConsoleManager& manager)
{
//	m_using_view = using_view;
	if (!(m_message_tree_model = OP_NEW(SimpleTreeModel, (2))))
		init_status = OpStatus::ERR_NO_MEMORY;

	m_manager = &manager;
}

MessageConsoleDialog::~MessageConsoleDialog()
{
	if (m_message_tree_model)
	{
		OP_DELETE(m_message_tree_model);
	}

	m_manager->ConsoleDialogClosed();
}

void MessageConsoleDialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	is_enabled = TRUE;
	switch (index)
	{
		case 0:
		{
			action = OP_NEW(OpInputAction, (OpInputAction::ACTION_CLEAR));
			g_languageManager->GetString(Str::D_CLEAR_JS_CONSOLE, text);
			name.Set(WIDGET_NAME_BUTTON_CLEAR);
			break;
		}

		case 1:
		{
			action = GetOkAction();
			g_languageManager->GetString(Str::DI_IDBTN_CLOSE, text);
			is_default = TRUE;
			name.Set(WIDGET_NAME_BUTTON_CLOSE);
			break;
		}
	}
}

void MessageConsoleDialog::OnInit()
{
	// Prepare the component dropdown.
	FillComponentDropdown();

	// Prepare the severity dropdown.
	OpDropDown* severityDropdown = static_cast<OpDropDown*>(GetWidgetByName("Severity_selector"));
	if (severityDropdown)
	{
		OpString str;

		g_languageManager->GetString(Str::D_CONSOLE_LEVEL_MESSAGE, str); // This works essentially as "All" in current implementation.
		severityDropdown->AddItem(str.CStr(), -1, 0, OpConsoleEngine::Verbose);

		g_languageManager->GetString(Str::D_CONSOLE_LEVEL_WARNING, str);
		severityDropdown->AddItem(str.CStr(), -1, 0, OpConsoleEngine::Information);

		g_languageManager->GetString(Str::D_CONSOLE_LEVEL_ERROR, str);
		severityDropdown->AddItem(str.CStr(), -1, 0, OpConsoleEngine::Error);
	}

	// Prepare the message treeview.
	OpTreeView* message_treeview   = static_cast<OpTreeView*>(GetWidgetByName("Message_treeview"));
	if (message_treeview)
	{
		message_treeview->SetUserSortable(FALSE);
		message_treeview->SetShowThreadImage(TRUE);
		message_treeview->SetShowColumnHeaders(FALSE);
		message_treeview->SetTreeModel(m_message_tree_model);
		message_treeview->SetMultiselectable(TRUE);
		message_treeview->SetSystemFont(OP_SYSTEM_FONT_FORM_TEXT);
		message_treeview->SetShowHorizontalScrollbar(true);
	}

	// Intialized with dummy values to prevent warnings.
	OpConsoleEngine::Source source = OpConsoleEngine::EcmaScript;
	OpConsoleEngine::Severity severity = OpConsoleEngine::Error;

	m_manager->GetSelectedComponentAndSeverity(source, severity);
	UpdateDropdownsSelectedState(source, severity);
}


void MessageConsoleDialog::Clear()
{
	m_message_tree_model->DeleteAll();
	m_lookup_list.DeleteAll();
}

void MessageConsoleDialog::OnChange(OpWidget* widget, BOOL changed_by_mouse)
{
	OpDropDown* component_dropdown = static_cast<OpDropDown*>(GetWidgetByName("Component_selector"));
	OpDropDown* severity_dropdown  = static_cast<OpDropDown*>(GetWidgetByName("Severity_selector"));
	if (component_dropdown && severity_dropdown && ( widget == component_dropdown || widget == severity_dropdown))
	{
		// Get the severity level that is selected.
		INTPTR item_user_data = severity_dropdown->GetItemUserData(severity_dropdown->GetSelectedItem());
		OpConsoleEngine::Severity severity = static_cast<OpConsoleEngine::Severity>(item_user_data);

		if (component_dropdown->GetSelectedItem() == 0)
		{
			// First item is special, that's the "All" item.
			// In this case we can't reuse the SelectSpecificSourceAndSeverity function,
			// which we can for all other components.

			// Clear the content from the console to prepare it for new content.
			m_message_tree_model->DeleteAll();
			m_lookup_list.DeleteAll();

			// Request resend of all components with the found severity.
			m_manager->SetSelectedAllComponentsAndSeverity(severity);
		}
		else
		{
			// Get the component selected.
			OpConsoleEngine::Source component = OpConsoleEngine::EcmaScript; // Just initalize to something to avoid compile warning.
			RETURN_VOID_IF_ERROR(GetComponentFromString(component_dropdown->GetItemText(component_dropdown->GetSelectedItem()), component));
			SelectSpecificSourceAndSeverity(component, severity);
		}
	}
	Dialog::OnChange(widget, changed_by_mouse);
}

OP_STATUS MessageConsoleDialog::UpdateDropdownsSelectedState(OpConsoleEngine::Source source, OpConsoleEngine::Severity severity)
{
	OpDropDown* component_dropdown = (OpDropDown*)GetWidgetByName("Component_selector");
	OpDropDown* severity_dropdown  = (OpDropDown*)GetWidgetByName("Severity_selector");
	if (!component_dropdown || !severity_dropdown)
	{
		return OpStatus::ERR;
	}

	// Search for the index of the supplied source.
	OpConsoleEngine::Source trav_comp;
	int compidx = 0;
	do
	{
		compidx++; // Starting from entry 1, as 0 is the special "All" item.
		RETURN_IF_ERROR(GetComponentFromString(component_dropdown->GetItemText(compidx), trav_comp));
	}
	while ( (compidx < component_dropdown->CountItems()) &&	(trav_comp != source) );
	if (compidx >= component_dropdown->CountItems())
	{
		// Set "All" source.
		compidx = 0;
	}
	// Set selected component.
	component_dropdown->SelectItem(compidx, TRUE);
	// Set selected severity
	for (INT32 i = 0; i < severity_dropdown->CountItems(); i++)
	{
		if (severity == severity_dropdown->GetItemUserData(i))
		{
			severity_dropdown->SelectItem(i, TRUE);
			break;
		}
	}

	return OpStatus::OK;
}

void MessageConsoleDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	OpTreeView* tree_view = static_cast<OpTreeView*>(GetWidgetByName("Message_treeview"));
	if (tree_view && (tree_view == widget) )
	{
		// Check if this is a right click
		if (!down && nclicks == 1 && button == MOUSE_BUTTON_2)
		{
			const OpPoint p = OpPoint(x, y) + widget->GetScreenRect().TopLeft();
			g_application->GetMenuHandler()->ShowPopupMenu("Opera Console Menu", PopupPlacement::AnchorAt(p), 0, TRUE); // Open a right click menu.

		}
		// Check if this is an applicable mouseclick
		if (down && nclicks == 1 && button == MOUSE_BUTTON_1)
		{
			OpRect rect;
			// Check if the mouseclick is within the second column of the row.
			if (pos != -1 && tree_view->GetCellRect(pos, 1, rect) && rect.Contains(OpPoint(x,y)))
			{
				// Now check if this row has a link in it.
				LookupElem* le = m_lookup_list.Get(pos);
				if (le)
				{ 
					if (!(le->url_lookup.IsEmpty()))
					{
						ShowURLInSourceViewer(le->url_lookup.CStr(), le->window_id);
					}
					else if (le->message_id_set)
					{
						// todo: launch error dialog?
					}
				}
			}
		}
	}
}

BOOL MessageConsoleDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CLEAR:
		{
			// Clear the content from the console.
			m_message_tree_model->DeleteAll();
			m_lookup_list.DeleteAll();
			m_manager->UserClearedConsole();
			return TRUE;
		}
		case OpInputAction::ACTION_COPY:
		{
			// User is copying text to the clipboard
			OpTreeView* treeview = static_cast<OpTreeView*>(GetWidgetByName("Message_treeview"));
			if (treeview)
			{
				OpString marked_text;
				marked_text.Empty();

				OpINT32Vector list;
				treeview->GetSelectedItems(list, FALSE); // Get index of all selected items.
				for (UINT32 i = 0; i < list.GetCount(); i++)
				{
					marked_text.Append(m_message_tree_model->GetItemString(list.Get(i), 0)); // Append selected items to the list.
					marked_text.Append(UNI_L("\n")); // Insert linebreak.
				}
				if (!marked_text.IsEmpty())
				{
					g_desktop_clipboard_manager->PlaceText(marked_text.CStr()); // Place all selected text on clipboard.
				}

			}
			return TRUE;
		}
		case OpInputAction::ACTION_OPEN_ALL_ITEMS:
		case OpInputAction::ACTION_CLOSE_ALL_ITEMS:
		{
			OpTreeView* treeview = static_cast<OpTreeView*>(GetWidgetByName("Message_treeview"));
			if (treeview)
			{
				return treeview->OnInputAction(action);
			}
			else
			{
				break;
			}
		}
	}
	return Dialog::OnInputAction(action);
}

OP_STATUS MessageConsoleDialog::SelectSpecificSourceAndSeverity(OpConsoleEngine::Source source, OpConsoleEngine::Severity severity)
{
	RETURN_IF_ERROR(UpdateDropdownsSelectedState(source, severity));

	// Clear the content from the console to prepare it for new content.
	m_message_tree_model->DeleteAll();
	m_lookup_list.DeleteAll();

	// Call for resend of messages.
	m_manager->SetSelectedComponentAndSeverity(source, severity);
	return OpStatus::OK;
}

OP_STATUS MessageConsoleDialog::PostConsoleMessage(const OpConsoleEngine::Message* message)
{
	// Check source before processing the message and ignore it if the currently selected compoment is different
	if (message)
	{
		OpDropDown* component_dropdown = static_cast<OpDropDown*>(GetWidgetByName("Component_selector"));
		OpDropDown* severity_dropdown  = static_cast<OpDropDown*>(GetWidgetByName("Severity_selector"));

		// 0 is the special "All" entry which we don't check against
		if (component_dropdown && component_dropdown->GetSelectedItem() != 0)
		{
			OpConsoleEngine::Source component = OpConsoleEngine::EcmaScript; // Just initalize to something to avoid compile warning.
			RETURN_IF_ERROR(GetComponentFromString(component_dropdown->GetItemText(component_dropdown->GetSelectedItem()), component));

			if (message->source != component)
				return OpStatus::OK;
		}

		// Check if severity matches. Item 0 is equivalent to "All".
		if (severity_dropdown && severity_dropdown->GetSelectedItem() != 0)
		{
			INTPTR item_user_data = severity_dropdown->GetItemUserData(severity_dropdown->GetSelectedItem());

			if (message->severity != item_user_data)
				return OpStatus::OK;
		}
	}

	// TODO: Add something to determine if the message is expanded or not.
	OpTreeView* tw = static_cast<OpTreeView*>(GetWidgetByName("Message_treeview"));
	if (tw)
	{
		// Extract the contents from the message.
		OpString source, msg, url, context;
		const char *icon;
		int window_id = -1;
		RETURN_IF_ERROR(msg.Set(message->message));
		RETURN_IF_ERROR(url.Set(message->url));
		RETURN_IF_ERROR(context.Set(message->context));
		window_id = message->window;
		if (OpStatus::IsError(GetStringFromComponent(message->source, source)) || source.IsEmpty())
		{
			OP_ASSERT(0); // The engine tried to post a message that this dialog don't know about.
			return OpStatus::ERR;
		}

		// Set the correct icon.
		switch (message->severity)
		{
		case OpConsoleEngine::Error:
		case OpConsoleEngine::Critical:
			icon = "Dialog Error";
			break;
		case OpConsoleEngine::Information:
			icon = "Dialog Warning";
			break;
		case OpConsoleEngine::Verbose:
#ifdef _DEBUG
		case OpConsoleEngine::Debugging:
#endif // DEBUG
		default:
			icon = "Dialog Info";
			break;
		};

		tw->SetColumnFixedWidth(1, 22);

		OpString source_and_url_string;
		OpString time;

		FormatTime(time, UNI_L("%x %X"), message->time);
		RETURN_IF_ERROR(source_and_url_string.AppendFormat("[%s] %s", time.CStr(), source.CStr()));

		if (url.HasContent())
		{
			RETURN_IF_ERROR(source_and_url_string.Append(" - "));
			RETURN_IF_ERROR(source_and_url_string.Append(url));
		}

		int position = m_message_tree_model->AddItem(source_and_url_string.CStr(), icon, 0, -1);     // Set the main title of the message.

		if(url.HasContent() && OpStatus::IsSuccess(StoreURLInLookupVector(url.CStr(), window_id, position)))
			m_message_tree_model->SetItemData(position, 1, NULL, "Note");  // insert the icon

#ifdef WEBSERVER_SUPPORT
		int context_pos = 
#endif // WEBSERVER_SUPPORT
			m_message_tree_model->AddItem(context.CStr(), NULL, 0, position);          // Set the context line of the message.

#ifdef WEBSERVER_SUPPORT
		// If this an Opera Account error add the icon to goto the UI friendly error dialog
		if (message->source == OpConsoleEngine::OperaAccount)
		{
			m_message_tree_model->SetItemData(context_pos, 1, NULL, "Note");  // insert the icon
			StoreMessageIdInLookupVector(message->id, context_pos);
		}
#endif // WEBSERVER_SUPPORT

		const OpStringC line_break_character = UNI_L("\n");
		const OpStringC carriage_return_character = UNI_L("\r");

		// Because anything after a \r seems to be ignored,
		// let's replace them with \n's, so they're interpreted as linebreaks.
		// But make sure that \r\n doesen't cause two linebreaks in the console.
		// For bug 217961.
		int car_ret_pos = msg.FindFirstOf(carriage_return_character);
		while (car_ret_pos != KNotFound)
		{
			msg[car_ret_pos] = line_break_character[0];
			car_ret_pos = msg.FindFirstOf(carriage_return_character, car_ret_pos);
		}

		// Fill in the body of the message line by line.
		int start = 0;
		int stop  = -1;
		do
		{
			stop = msg.FindFirstOf(line_break_character, stop + 1);
			if (stop == KNotFound)
			{
				stop = msg.Length();
			}
			OpString tmpmsg;
			RETURN_IF_ERROR(tmpmsg.Set(msg.SubString(start).CStr(), stop-start));
			if ((tmpmsg.Length() > 1) || ((tmpmsg.Length() == 1) && (tmpmsg[0] != '\r')))                     // Check if the line has content, that is not just a linebreak
			{
				int text_pos = m_message_tree_model->AddItem(tmpmsg.CStr(), NULL, 0, position);                      // Insert line into treeview
				int link_pos = tmpmsg.Find(UNI_L("http://"));
				if (link_pos != KNotFound)                                                                    // If the line contains a link, insert an icon
				{
					m_message_tree_model->SetItemData(text_pos, 1, NULL, "Window Mail Compose Icon");  // insert the icon
					int end_of_link = tmpmsg.FindFirstOf(UNI_L(" "), link_pos);     // Find where the link part of the line ends.
					if (end_of_link != KNotFound)                                                             // Store the url in hte lookuptable
					{
						OpString url;
						RETURN_IF_ERROR(url.Set(tmpmsg.SubString(link_pos).CStr(), end_of_link-link_pos));
						StoreURLInLookupVector(url, window_id, text_pos);
					}
					else
					{
						StoreURLInLookupVector(tmpmsg.SubString(link_pos), window_id, text_pos);
					}
				}
			}
			start = stop + 1;
		}
		while (stop < msg.Length());

		if (tw->GetItemCount() > 0)
		{
			tw->OpenItem(position, TRUE);
			tw->ScrollToItem(position, TRUE); // Otherwise only first line of expanded message will be visible
		}
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

void MessageConsoleDialog::BeginUpdate()
{
	m_message_tree_model->BeginChange();
}

void MessageConsoleDialog::EndUpdate()
{
	m_message_tree_model->EndChange();
}

OP_STATUS MessageConsoleDialog::FillComponentDropdown()
{
	// This function constructs the component dropdown. It controls:
	//  - The localizable strings that go into the ui part of the dropdown.
	OpDropDown* comp_dropdown = static_cast<OpDropDown*>(GetWidgetByName("Component_selector"));
	if (comp_dropdown)
	{
		comp_dropdown->Clear();  // Clear the dropdown, so we know that we are starting with a clean slate.
		UINT32 position = 0;     // These two variables are reused for inserting elements
		OpString component_name; // into the the dropdown

		// Insert the 'All' component into the drowpdown
		g_languageManager->GetString(Str::D_CONSOLE_ALL_COMPONENTS, component_name);
		comp_dropdown->AddItem(component_name.CStr(), position++, NULL);

		// Thrawl throug all possible components and add those that have a translation text.
		for (int i = 0; i < (int)OpConsoleEngine::SourceTypeLast; i++)
		{
			if (OpStatus::IsSuccess(GetStringFromComponent((OpConsoleEngine::Source)i, component_name))) // Check if the component has a translation.
			{
				comp_dropdown->AddItem(component_name.CStr(), position++, NULL); // Insert the element into the dropdown
			}
		}

		// Done inserting components into the dropdown.
		return OpStatus::OK;

	}
	return OpStatus::ERR;
}

OP_STATUS MessageConsoleDialog::GetComponentFromString(const OpStringC &string, OpConsoleEngine::Source &component)
{
	// Loop through all message sources searching for the specified one.
	OP_MEMORY_VAR int i = -1;
	OpString loc_str;
	do
	{
		i++;
		g_languageManager->GetString(message_string_mapping[i].string_id, loc_str);
	}
	while ( (message_string_mapping[i].source != OpConsoleEngine::SourceTypeLast) &&    // All mappings have been visited.
			(string.Compare(message_string_mapping[i].string) )                   &&    // The hardcoded string matches the given string.
			(string.Compare(loc_str.CStr()))                                         ); // The localized string matches the given string.
	if (string.Compare(message_string_mapping[i].string) && string.Compare(loc_str) )
	{
		// The given string wasn't found.
		return OpStatus::ERR;

	}
	// The given string was found.
	component = message_string_mapping[i].source;
	return OpStatus::OK;
}

OP_STATUS MessageConsoleDialog::GetStringFromComponent(OpConsoleEngine::Source component, OpString &string)
{
	// Loop through all message sources searching for the specified one.
	UINT i = 0;
	while ( (message_string_mapping[i].source != OpConsoleEngine::SourceTypeLast) &&   // All mappings have been visited.
			(component != message_string_mapping[i].source)                          ) // The source matches the given component.
	{
		i++;
	}
	if (component != message_string_mapping[i].source)
	{
		// The component was not found
		return OpStatus::ERR;
	}
	if (message_string_mapping[i].string_id != Str::NOT_A_STRING)
	{
		// The component has a translated name
		RETURN_IF_ERROR(g_languageManager->GetString(message_string_mapping[i].string_id, string));
	}
	else
	{
		// The component has no translated name, use the hardcoded one.
		RETURN_IF_ERROR(string.Set(message_string_mapping[i].string));
	}
	return OpStatus::OK;
}

OP_STATUS  MessageConsoleDialog::StoreMessageIdInLookupVector(unsigned int message_id, UINT32 pos)
{
	if (pos < m_lookup_list.GetCount())
	{
		return OpStatus::ERR;
	}

	while (m_lookup_list.GetCount() < pos)
	{
		// Insert blank dummies for rows where there is no reference.
		OP_STATUS add_status = m_lookup_list.Add(NULL);
		if (OpStatus::IsError(add_status))
		{
			return add_status;
		}
	}
	LookupElem* new_elem = OP_NEW(LookupElem, ());

	if (!new_elem)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	new_elem->message_id = message_id;
	new_elem->message_id_set = TRUE;
	OP_STATUS insert_status = m_lookup_list.Insert(pos, new_elem);
	if (OpStatus::IsError(insert_status))
	{
		return insert_status;
	}
	else
	{
		return OpStatus::OK;
	}

}

OP_STATUS  MessageConsoleDialog::StoreURLInLookupVector(const OpStringC &url, UINT32 window_id, UINT32 pos)
{
	if (pos < m_lookup_list.GetCount())
	{
		return OpStatus::ERR;
	}

	while (m_lookup_list.GetCount() < pos)
	{
		// Insert blank dummies for rows where there is no reference.
		OP_STATUS add_status = m_lookup_list.Add(NULL);
		if (OpStatus::IsError(add_status))
		{
			return add_status;
		}
	}
	LookupElem* new_elem = OP_NEW(LookupElem, ());

	if (!new_elem)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	new_elem->url_lookup.Set(url);
	new_elem->window_id = window_id;
	OP_STATUS insert_status = m_lookup_list.Insert(pos, new_elem);
	if (OpStatus::IsError(insert_status))
	{
		return insert_status;
	}
	else
	{
		return OpStatus::OK;
	}
}

#if defined HAVE_DISK && !defined NO_EXTERNAL_APPLICATIONS
OP_STATUS MessageConsoleDialog::ShowURLInSourceViewer(const uni_char *urlname, UINT32 window_id)
{
	if (!urlname || !*urlname)
		return OpStatus::ERR_NULL_POINTER;

	URL url = g_url_api->GetURL(urlname, static_cast<uni_char*>(NULL));

	int dwin_id = 0;
	DesktopWindow* dwin = g_application->GetActiveDesktopWindow();
	if (dwin)
	{
		dwin_id = dwin->GetID();
	}
	g_application->OpenSourceViewer(&url, dwin_id);

	return OpStatus::OK;
}
#endif // HAVE_DISK && !NO_EXTERNAL_APPLICATIONS

/*****************************************************************************
 *                                                                           *
 * MailMessageDialog                                                         *
 *                                                                           *
 ****************************************************************************/

MailMessageDialog::MailMessageDialog(MessageConsoleManager& manager,
		const OpString& message)
	: m_manager(&manager)
{
	m_message.Empty();
	m_message.Set(message);
}

MailMessageDialog::~MailMessageDialog()
{
	m_manager->MailMessageDialogClosed();
}


void MailMessageDialog::OnInit()
{
	OpMultilineEdit* multi_edit = static_cast<OpMultilineEdit*>(GetWidgetByName("Message_edit_box"));
	if (multi_edit)
	{
		multi_edit->SetReadOnly(TRUE);
		multi_edit->SetFlatMode();

	}
	SetMessage(m_message);
}

void MailMessageDialog::OnClose(BOOL user_initiated)
{
	if (IsDoNotShowDialogAgainChecked())
	{
		m_manager->SetM2ConsoleLogging(FALSE);
		TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowMailErrorDialog, FALSE));
	}

	Dialog::OnClose(user_initiated);
}

void MailMessageDialog::SetMessage(const OpString &message)
{
	m_message.Set(message);
	OpMultilineEdit* multi_edit = static_cast<OpMultilineEdit*>(GetWidgetByName("Message_edit_box"));
	if (multi_edit)
	{
		multi_edit->SetText(m_message.CStr());
	}
}

BOOL MailMessageDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OPEN_MESSAGE_CONSOLE_FOR_MAIL:
		{
			m_manager->OpenDialog(OpConsoleEngine::M2, OpConsoleEngine::Error);
			CloseDialog(FALSE, FALSE, FALSE); // Close the dialog later, but very soon.
			return TRUE;
		}
	}
	return Dialog::OnInputAction(action);
}

#endif // OPERA_CONSOLE
