/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "InputManagerDialog.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/url/url_man.h"
#include "modules/util/OpLineParser.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

const char* InputManagerDialog::m_shortcut_sections[] =
{
	"Application",
	"Browser Window",
	"Mail Window",
	"Document Window",
	"Compose Window",
	"Bookmarks Panel",
	"Contacts Panel",
	"History Panel",
	"Links Panel",
//	"Chat Panel",
	"Mail Panel",
	"Transfers Panel",
	"Windows Panel",
	"Notes Panel",
	NULL,
};

const char* InputManagerDialog::m_advanced_shortcut_sections[] =
{
	"Dialog",
	"Form",
	"Widget Container",
	"Browser Widget",
	"Bookmarks Widget",
	"Contacts Widget",
	"Notes Widget",
	"Button Widget",
	"Radiobutton Widget",
	"Checkbox Widget",
	"Dropdown Widget",
	"List Widget",
	"Tree Widget",
	"Edit Widget",
	"Address Dropdown Widget",
	NULL
};

#ifdef _MACINTOSH_
const uni_char* TranslateModifiersVolatile(const uni_char* instr, BOOL forWrite)
{
	static OpString opstr;
	static OpString meta;
	static OpString ctrl;
	static OpString alt;
	static OpString shift;

	if(alt.IsEmpty()) {
		g_languageManager->GetString(Str::M_KEY_ALT, alt);
	}
#ifdef _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
	if(meta.IsEmpty()) {
		g_languageManager->GetString(Str::M_KEY_META, meta);
	}
#endif
	if(ctrl.IsEmpty()) {
		g_languageManager->GetString(Str::M_KEY_CTRL, ctrl);
	}
	if(shift.IsEmpty()) {
		g_languageManager->GetString(Str::M_KEY_SHIFT, shift);
	}

	if (ctrl.IsEmpty() ||
#ifdef _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
		meta.IsEmpty() ||
#endif
		alt.IsEmpty() ||
		shift.IsEmpty())
	{
		return instr;
	}
	opstr.Empty();
	while (instr)
	{
		uni_char* scan = uni_strchr(instr, UNI_L(' '));
		int len = scan ? scan-instr : uni_strlen(instr);
		if(forWrite)
		{
			if (len==ctrl.Length() && !uni_strncmp(instr, ctrl.CStr(), len))
				opstr.Append("ctrl");
			else if (len==alt.Length() && !uni_strncmp(instr, alt.CStr(), len))
				opstr.Append("alt");
			else if (len==meta.Length() && !uni_strncmp(instr, meta.CStr(), len))
				opstr.Append("meta");
			else if (len==shift.Length() && !uni_strncmp(instr, shift.CStr(), len))
				opstr.Append("shift");
			else
				opstr.Append(instr, len);
		}
		else {
			if (len==4 && !uni_strncmp(instr, UNI_L("ctrl"), len))
				opstr.Append(ctrl.CStr());
			else if (len==3 && !uni_strncmp(instr, UNI_L("alt"), len))
				opstr.Append(alt.CStr());
			else if (len==4 && !uni_strncmp(instr, UNI_L("meta"), len))
				opstr.Append(meta.CStr());
			else if (len==5 && !uni_strncmp(instr, UNI_L("shift"), len))
				opstr.Append(shift.CStr());
			else
				opstr.Append(instr, len);
		}
		if (scan)
			opstr.Append(" ");
		instr = scan?scan+1:scan;
	}
	return opstr.CStr();
}
#endif

/***********************************************************************************
**
**	InputManagerDialog
**
***********************************************************************************/

InputManagerDialog::~InputManagerDialog()
{
	OP_DELETE(m_input_prefs);
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void InputManagerDialog::OnInit()
{
	// this is the one we will change and write

	OpString title;

	switch(m_input_type)
	{
	case OPKEYBOARD_SETUP:
		{
			m_old_file_index = g_setup_manager->GetIndexOfKeyboardSetup();
			INT32 index = m_input_index < 0 || m_input_index >= (INT32)g_setup_manager->GetKeyboardConfigurationCount() ? m_old_file_index : m_input_index;
			TRAPD(rc, m_input_prefs = g_setup_manager->GetSetupPrefsFileL(index, m_input_type, NULL, TRUE));
			m_new_file_index = g_setup_manager->GetIndexOfKeyboardSetup();
			g_languageManager->GetString(Str::D_EDIT_KEYBOARD_SETUP, title);
		}
		break;
	case OPMOUSE_SETUP:
		{
			m_old_file_index = g_setup_manager->GetIndexOfMouseSetup();
			INT32 index = m_input_index < 0 || m_input_index >= (INT32)g_setup_manager->GetMouseConfigurationCount() ? m_old_file_index : m_input_index;
			TRAPD(rc, m_input_prefs = g_setup_manager->GetSetupPrefsFileL(index, m_input_type, NULL, TRUE));
			m_new_file_index = g_setup_manager->GetIndexOfMouseSetup();
			g_languageManager->GetString(Str::D_EDIT_MOUSE_SETUP, title);
		}
		break;
	default:
		{
			OP_ASSERT(FALSE);	//unknown type... look into it!
		}
		break;
	}

	SetTitle(title.CStr());

	m_input_treeview = (OpTreeView*) GetWidgetByName("Input_treeview");
	OpQuickFind* quickfind = (OpQuickFind*) GetWidgetByName("Quickfind_edit");

	m_input_treeview->SetAutoMatch(TRUE);
	m_input_treeview->SetShowThreadImage(TRUE);
	quickfind->SetTarget(m_input_treeview);

	OpString str;

	g_languageManager->GetString(Str::S_CONTEXT_AND_SHORTCUT, str);
	m_input_model.SetColumnData(0, str.CStr());
	g_languageManager->GetString(Str::S_ACTIONS, str);
	m_input_model.SetColumnData(1, str.CStr());

	AddShortcutSections(m_shortcut_sections, -1);

	Item *new_item = OP_NEW(Item, (ITEM_CATEGORY, NULL));
	if (new_item)
	{
		INT32 parent_index = m_input_model.AddItem(UNI_L("Advanced"), NULL, 0, -1, new_item);

		AddShortcutSections(m_advanced_shortcut_sections, parent_index);
	}

	m_input_treeview->SetTreeModel(&m_input_model);
}




/***********************************************************************************
**
**	GetHelpAnchor
**
***********************************************************************************/

const char* InputManagerDialog::GetHelpAnchor()
{
	switch (m_input_type)
	{
		case OPKEYBOARD_SETUP:
			// deliberate fall through to OPMOUSE_SETUP
		case OPMOUSE_SETUP:
			return "mouse.html";
			break;
		default:
		{
			OP_ASSERT(FALSE);	//unknown type... look into it!
		}
		break;
	}
	return "";
}

/***********************************************************************************
**
**	OnInitVisibility
**
***********************************************************************************/

void InputManagerDialog::OnInitVisibility()
{
}

/***********************************************************************************
**
**	AddShortcutSections
**
***********************************************************************************/

void InputManagerDialog::AddShortcutSections(const char** shortcut_section, INT32 root)
{
	for (; *shortcut_section; shortcut_section++)
	{
		AddShortcutSection(*shortcut_section, root, -1, FALSE);
	}
}

/***********************************************************************************
**
**	AddShortcutSections
**
***********************************************************************************/

void InputManagerDialog::AddShortcutSection(const char* shortcut_section, INT32 root, INT32 parent_index, BOOL force_default)
{
	if (!m_input_prefs)
		return;

	BOOL was_default = FALSE;

	PrefsSection* section = m_input_prefs->IsSection(shortcut_section) && !force_default ? m_input_prefs->ReadSectionL(shortcut_section) : g_setup_manager->GetSectionL(shortcut_section, m_input_type, &was_default, TRUE);

	if (parent_index == -1)
	{
		Item* item = OP_NEW(Item, (ITEM_SECTION, shortcut_section));
		if (item)
		{
			OpString8 name8;
			item->MakeSectionString(name8, was_default);

			OpString name;
			name.Set(name8);
			parent_index = m_input_model.AddItem(name.CStr(), NULL, 0, root, item);
		}
	}
	else
	{
		OpString8 name8;
		m_input_model.GetItem(parent_index)->MakeSectionString(name8, was_default);

		OpString name;
		name.Set(name8);
		m_input_model.SetItemData(parent_index, 0, name.CStr());

		while (m_input_model.GetChildIndex(parent_index) != -1)
		{
			m_input_model.DeleteItem(m_input_model.GetChildIndex(parent_index));
		}
	}

	if (section)
	{
		for (const PrefsEntry *entry = section->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
		{
			const uni_char* key = entry->Key();

			// translate voice commands, for example
			OpString translated_key;
			Str::LocaleString key_id(Str::NOT_A_STRING);

			if (key && uni_stristr(key, UNI_L(",")) == NULL && m_input_type != OPKEYBOARD_SETUP)
			{
				OpLineParser key_line(key);
				key_line.GetNextLanguageString(translated_key, &key_id);

				if (translated_key.HasContent())
				{
					OpLineParser key_line(key);
					key_line.GetNextLanguageString(translated_key, &key_id);
					
					if (translated_key.HasContent())
					{
						key = translated_key.CStr();
					}
				}
			}
			Item *new_item = OP_NEW(Item, (ITEM_ENTRY, shortcut_section));
			if (new_item)
			{
#ifdef _MACINTOSH_
				INT32 got_index = m_input_model.AddItem(TranslateModifiersVolatile(key, FALSE), NULL, 0, parent_index, new_item);
#else
				INT32 got_index = m_input_model.AddItem(key, NULL, 0, parent_index, new_item);
#endif
				OpLineParser line(entry->Value());
				OpString8 action;

				line.GetNextToken8(action);

				m_input_model.SetItemData(got_index, 1, entry->Value(), action.CStr());
			}
		}

		OP_DELETE(section);
	}
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 InputManagerDialog::OnOk()
{
	if (!m_input_prefs)
		return 0;

	for (OP_MEMORY_VAR INT32 i = 0; i < m_input_model.GetItemCount(); i++)
	{
		Item* item = m_input_model.GetItem(i);

		if (item && item->IsSection() && item->IsChanged())
		{
			m_input_prefs->ClearSectionL(item->GetSection().CStr());

			for (OP_MEMORY_VAR INT32 j = m_input_model.GetChildIndex(i);
				 j != -1; j = m_input_model.GetSiblingIndex(j))
			{
				Item* item = m_input_model.GetItem(j);

				if (item && item->IsEntry())
				{
					const uni_char* trigger = m_input_model.GetItemString(j, 0);
					const uni_char* action = m_input_model.GetItemString(j, 1);

					if (trigger && *trigger && action && *action)
					{
						OpString8 trigger8;
#ifdef _MACINTOSH_
						trigger8.Set(TranslateModifiersVolatile(trigger, TRUE));
#else
						trigger8.Set(trigger);
#endif
						TRAPD(err, m_input_prefs->WriteStringL(item->GetSection().CStr(), trigger8, action));
					}
				}
			}
		}
	}

	TRAPD(rc,m_input_prefs->CommitL());

	//this will update the preferences dialog page for both keyboard and mouse
	g_application->SettingsChanged(SETTINGS_KEYBOARD_SETUP);

	g_setup_manager->ReloadSetupFile(m_input_type);

	return 0;
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void InputManagerDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == m_input_treeview && nclicks == 2)
	{
		Item* item = m_input_model.GetItem(m_input_treeview->GetModelPos(pos));

		if (item && item->IsEntry())
		{
			OpRect rect;

			if (m_input_treeview->GetCellRect(pos, 1, rect) && rect.Contains(OpPoint(x,y)))
			{
				m_input_treeview->EditItem(pos, 1, TRUE, AUTOCOMPLETION_ACTIONS);
			}
			else
			{
				m_input_treeview->EditItem(pos, 0, TRUE);
			}
		}
	}
}

/***********************************************************************************
**
**	OnItemEdited
**
***********************************************************************************/

void InputManagerDialog::OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	if (column == 0)
	{
		m_input_model.SetItemData(m_input_treeview->GetModelPos(pos), column, text.CStr());
	}
	else
	{
		OpLineParser line(text.CStr());
		OpString8 action;

		line.GetNextToken8(action);
		m_input_model.SetItemData(m_input_treeview->GetModelPos(pos), column, text.CStr(), action.CStr());
	}
	OnItemChanged(m_input_treeview->GetModelPos(pos));
}

/***********************************************************************************
**
**	OnItemChanged
**
***********************************************************************************/

void InputManagerDialog::OnItemChanged(INT32 model_pos)
{
	model_pos = m_input_model.GetParentIndex(model_pos);

	Item* item = m_input_model.GetItem(model_pos);

	if (item && item->IsSection())
	{
		item->SetChanged(TRUE);

		OpString8 name8;
		item->MakeSectionString(name8, FALSE);

		OpString name;
		name.Set(name8);
		m_input_model.SetItemData(model_pos, 0, name.CStr());
	}
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void InputManagerDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL InputManagerDialog::OnInputAction(OpInputAction* action)
{
	if (!m_input_treeview)
		return Dialog::OnInputAction(action);

	if (m_input_treeview->OnInputAction(action))
		return TRUE;

	INT32 pos = m_input_treeview->GetSelectedItemPos();
	INT32 model_pos = m_input_treeview->GetSelectedItemModelPos();
	Item* item = m_input_model.GetItem(model_pos);

	if (!item || item->IsCategory())
		return Dialog::OnInputAction(action);

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			switch (action->GetChildAction()->GetAction())
			{
				case OpInputAction::ACTION_FOCUS_NEXT_WIDGET:
				{
					if (m_input_treeview->GetEditColumn() == 0)
					{
						m_input_treeview->EditItem(m_input_treeview->GetEditPos(), 1, TRUE, AUTOCOMPLETION_ACTIONS);
						return TRUE;
					}
					else if (m_input_treeview->GetEditColumn() == 1)
					{
						m_input_treeview->FinishEdit();
						return TRUE;
					}
					break;
				}
			}

			return FALSE;
		}

		case OpInputAction::ACTION_RESET_SHORTCUTS:
		{
			if (item->IsEntry())
			{
				model_pos = m_input_model.GetParentIndex(model_pos);
				item = m_input_model.GetItem(model_pos);
			}

			AddShortcutSection(item->GetSection().CStr(), -1, model_pos, TRUE);
			item->SetChanged(TRUE);

			return TRUE;
		}

		case OpInputAction::ACTION_NEW_SHORTCUT:
		{
			INT32 got_index;

			Item *new_item = OP_NEW(Item, (ITEM_ENTRY, item->GetSection().CStr()));
			if (new_item)
			{
				if (item->IsSection())
				{
					got_index = m_input_model.AddItem(NULL, NULL, 0, model_pos, new_item);
				}
				else
				{
					got_index = m_input_model.InsertBefore(model_pos, NULL, NULL, 0, new_item);
				}

				m_input_treeview->EditItem(m_input_treeview->GetItemByModelPos(got_index), 0, TRUE);
			}

			return TRUE;
		}

		case OpInputAction::ACTION_DELETE:
		{
			if (item->IsEntry())
			{
				OnItemChanged(model_pos); // Do this first, otherwise it will fail
				m_input_model.DeleteItem(model_pos);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			if (item->IsEntry())
			{
				m_input_treeview->EditItem(pos, 0, TRUE);
			}

			return TRUE;
		}
	}

	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**
**	WriteShortcutSections
**
***********************************************************************************/

/*static*/ void InputManagerDialog::WriteShortcutSections(OpSetupType input_type, URL& url)
{
	url.WriteDocumentData(URL::KNormal, "<h1>", 4);
	if (input_type == OPKEYBOARD_SETUP)
	{
		OpString keyboard_setup;
		g_languageManager->GetString(Str::S_KEYBOARD_SETUP, keyboard_setup);

		url.WriteDocumentData(URL::KNormal, keyboard_setup.CStr());
	}
	else
	{
		OpString mouse_setup;
		g_languageManager->GetString(Str::S_MOUSE_SETUP, mouse_setup);

		url.WriteDocumentData(URL::KNormal, mouse_setup.CStr());
	}
	url.WriteDocumentData(URL::KNormal, "</h1>", 5);

	WriteShortcutSections(input_type, m_shortcut_sections, url);
	WriteShortcutSections(input_type, m_advanced_shortcut_sections, url);
}

/***********************************************************************************
**
**	WriteShortcutSections
**
***********************************************************************************/

/*static*/ void InputManagerDialog::WriteShortcutSections(OpSetupType input_type, const char** shortcut_section, URL& url)
{
	for (; *shortcut_section; shortcut_section++)
	{
		PrefsSection* section = g_setup_manager->GetSectionL(*shortcut_section, input_type, NULL, FALSE);

		url.WriteDocumentData(URL::KNormal, "<h2>", 4);
		url.WriteDocumentData(URL::KNormal, *shortcut_section, op_strlen(*shortcut_section));
		url.WriteDocumentData(URL::KNormal, "</h2><table>", 12);

		if (section)
		{
			for (const PrefsEntry *entry = section->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
			{
				url.WriteDocumentData(URL::KNormal, "<tr><td>", 8);
#ifdef _MACINTOSH_
				url.WriteDocumentData(URL::KNormal, TranslateModifiersVolatile(entry->Key(), TRUE));
#else
				url.WriteDocumentData(URL::KNormal, entry->Key());
#endif
				url.WriteDocumentData(URL::KNormal, "</td><td>", 9);

				url.WriteDocumentData(URL::KNormal, entry->Value());

				url.WriteDocumentData(URL::KNormal, "</td></tr>", 10);
			}

			OP_DELETE(section);
		}

		url.WriteDocumentData(URL::KNormal, "</table>", 8);
	}
}

/***********************************************************************************
**
**	OnCancel
**
***********************************************************************************/

void InputManagerDialog::OnCancel()
{
	//revert to old file, and delete the copied file, if any
	if(m_new_file_index != m_old_file_index)
	{
		//delete the copied file
		TRAPD(err, g_setup_manager->DeleteSetupL(m_new_file_index, m_input_type));
		g_setup_manager->SelectSetupFile(m_old_file_index, m_input_type, FALSE);
	}

	//this will update the preferences dialog page for both keyboard and mouse
	g_application->SettingsChanged(SETTINGS_KEYBOARD_SETUP);
}
