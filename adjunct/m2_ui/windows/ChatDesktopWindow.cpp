/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#if defined(M2_SUPPORT) && defined(IRC_SUPPORT)

#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/string/htmlify_more.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "adjunct/m2/src/backend/irc/color-parser.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/m2_ui/dialogs/AccountQuestionDialogs.h"
#include "adjunct/m2_ui/dialogs/ChangeNickDialog.h"
#include "adjunct/m2_ui/models/chattersmodel.h"
#include "adjunct/m2_ui/models/chatroomsmodel.h"
#include "adjunct/m2_ui/widgets/ChatHeaderPane.h"
#include "adjunct/m2_ui/windows/ChatDesktopWindow.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/hotlist/HotlistModelItem.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/widgets/OpFindTextBar.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/widgets/OpSplitter.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"


#include "modules/doc/doc.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpLocale.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/util/htmlify.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"

#include <limits.h>

// This one can be defined from time to time to check if AMSR is useful for
// displaying the chat window. Right now it's not.
// #define CHAT_WINDOW_USE_AMSR

/***********************************************************************************
**
**	IRCTextToHtml
**
***********************************************************************************/

class IRCTextToHtml : public ColorParserHandler
{
public:
	// Constructor.
	IRCTextToHtml(OpStringC const &text);

	// Accessors.
	const OpStringC& HtmlText() const { return m_html_text; }

private:
	// ColorParserHandler.
	virtual void OnPlainTextBegin();
	virtual void OnPlainTextEnd() { EndBlock(); }
	virtual void OnBoldTextBegin();
	virtual void OnBoldTextEnd() { EndBlock(); }
	virtual void OnColorTextBegin(color_type text_color, color_type background_color);
	virtual void OnColorTextEnd() { EndBlock(); }
	virtual void OnReverseTextBegin();
	virtual void OnReverseTextEnd() { EndBlock(); }
	virtual void OnUnderlineTextBegin();
	virtual void OnUnderlineTextEnd() { EndBlock(); }
	virtual void OnCharacter(uni_char character);

	// Methods.
	void AddCurrentBlockText();
	void ColorToString(OpString &color_string, ColorParserHandler::color_type color, bool is_background = FALSE) const;
	void EndBlock();

	// Members.
	OpString m_html_text;
	OpString m_current_block_text;
};


IRCTextToHtml::IRCTextToHtml(OpStringC const &text)
{
	mIRCColorParser color_parser(*this);
	color_parser.Parse(text);
}


void IRCTextToHtml::OnPlainTextBegin()
{
	AddCurrentBlockText();
	m_html_text.Append("<span>");
}


void IRCTextToHtml::OnBoldTextBegin()
{
	AddCurrentBlockText();
	m_html_text.Append("<span style=\"font-weight: bold;\">");
}


void IRCTextToHtml::OnColorTextBegin(color_type text_color, color_type background_color)
{
	AddCurrentBlockText();

	OpString color_css;
	OpString background_css;

	{
		OpString text_color_string;
		OpString background_color_string;

		if (text_color != COLOR_NONE)
		{
			ColorToString(text_color_string, text_color, FALSE);
			if (text_color_string.HasContent())
				color_css.AppendFormat(UNI_L("color: %s;"), text_color_string.CStr());
		}
		if (background_color != COLOR_NONE)
		{
			ColorToString(background_color_string, background_color, TRUE);
			if (background_color_string.HasContent())
				background_css.AppendFormat(UNI_L("background-color: %s;"), background_color_string.CStr());
		}
	}

	m_html_text.AppendFormat(UNI_L("<span style=\"%s %s\">"),
		color_css.HasContent() ? color_css.CStr() : UNI_L(""),
		background_css.HasContent() ? background_css.CStr() : UNI_L(""));
}


void IRCTextToHtml::OnReverseTextBegin()
{
	AddCurrentBlockText();
	m_html_text.Append("<span>");
}


void IRCTextToHtml::OnUnderlineTextBegin()
{
	AddCurrentBlockText();
	m_html_text.Append("<span style=\"text-decoration: underline;\">");
}


void IRCTextToHtml::OnCharacter(uni_char character)
{
	m_current_block_text.Append(&character, 1);
}


void IRCTextToHtml::AddCurrentBlockText()
{
	if (m_current_block_text.HasContent())
	{
		OpString html;
		HTMLify_string(html, m_current_block_text.CStr(), m_current_block_text.Length(), FALSE, FALSE, FALSE, TRUE);
		m_current_block_text.Empty();

		m_html_text.Append(html);
	}
}


void IRCTextToHtml::ColorToString(OpString &color_string, ColorParserHandler::color_type color, bool is_background) const
{
	// The color values are the same as those used in mIRC.
	switch (color)
	{
		case COLOR_WHITE :
			color_string.Set("#ffffff");
			break;
		case COLOR_BLACK :
			color_string.Set("#000000");
			break;
		case COLOR_BLUE :
			color_string.Set("#00007f");
			break;
		case COLOR_GREEN :
			color_string.Set("#009300");
			break;
		case COLOR_LIGHTRED :
			color_string.Set("#ff0000");
			break;
		case COLOR_BROWN :
			color_string.Set("#7f0000");
			break;
		case COLOR_PURPLE :
			color_string.Set("#9c009c");
			break;
		case COLOR_ORANGE :
			color_string.Set("#fc7f00");
			break;
		case COLOR_YELLOW :
			color_string.Set("#ffff00");
			break;
		case COLOR_LIGHTGREEN :
			color_string.Set("#00fc00");
			break;
		case COLOR_CYAN :
			color_string.Set("#009393");
			break;
		case COLOR_LIGHTCYAN :
			color_string.Set("#00ffff");
			break;
		case COLOR_LIGHTBLUE :
			color_string.Set("#0000fc");
			break;
		case COLOR_PINK :
			color_string.Set("#ff00ff");
			break;
		case COLOR_GREY :
			color_string.Set("#7f7f7f");
			break;
		case COLOR_LIGHTGREY :
			color_string.Set("#d2d2d2");
			break;
		case COLOR_TRANSPARENT :
			if (is_background)
				color_string.Set("transparent");
			break;
		default :
			OP_ASSERT(0);
			break;
	}
}


void IRCTextToHtml::EndBlock()
{
	AddCurrentBlockText();
	m_html_text.Append("</span>");
}

/***********************************************************************************
**
**	EditRoomDialog
**
***********************************************************************************/

class EditRoomDialog : public Dialog
{
	public:

		Type					GetType()				{return DIALOG_TYPE_EDIT_ROOM;}
		const char*				GetWindowName()			{return "Edit Room Dialog";}

		void Init(ChatDesktopWindow* chat_window)
		{
			m_chat_window = chat_window;
			Dialog::Init(chat_window);
		}

		void OnInit()
		{
			BOOL is_operator = m_chat_window->IsOperator();

			SetWidgetText("Topic_edit", m_chat_window->GetTopic());
			SetWidgetReadOnly("Topic_edit", m_chat_window->IsTopicProtected() && !is_operator);

			SetWidgetValue("Topic_checkbox", m_chat_window->IsTopicProtected());
			SetWidgetEnabled("Topic_checkbox", is_operator);

			SetWidgetValue("Moderated_checkbox", m_chat_window->IsModerated());
			SetWidgetEnabled("Moderated_checkbox", is_operator);

			SetWidgetValue("Secret_checkbox", m_chat_window->IsSecret());
			SetWidgetEnabled("Secret_checkbox", is_operator);

			SetWidgetValue("Password_checkbox", m_chat_window->HasPassword());
			SetWidgetEnabled("Password_checkbox", is_operator);

			OpEdit* edit = (OpEdit*)GetWidgetByName("Password_edit");
			if (edit)
			{
				edit->SetPasswordMode(TRUE);
			}

			SetWidgetText("Password_edit", m_chat_window->GetPassword());
			SetWidgetReadOnly("Password_edit", !is_operator);

			SetWidgetValue("Limit_checkbox", m_chat_window->HasLimit());
			SetWidgetEnabled("Limit_checkbox", is_operator);

			SetWidgetText("Limit_edit", m_chat_window->GetLimit());
			SetWidgetReadOnly("Limit_edit", !is_operator);

			OpString title;
			g_languageManager->GetString(Str::D_PROPERTIES_DIALOG_CAPTION, title);
			SetTitle(title.CStr());
		}

		void OnChange(OpWidget *widget, BOOL changed_by_mouse)
		{
			if (widget->IsNamed("Password_edit"))
			{
				SetWidgetValue("Password_checkbox", ((OpEdit*)widget)->GetTextLength() > 0);
			}
			else if (widget->IsNamed("Limit_edit"))
			{
				SetWidgetValue("Limit_checkbox", ((OpEdit*)widget)->GetTextLength() > 0);
			}
			Dialog::OnChange(widget, changed_by_mouse);
		}

		UINT32 OnOk()
		{
			OpString string;
			OpString empty;
			INT32 account_id = m_chat_window->GetAccountID();
			INT32 value = 0;

			GetWidgetText("Topic_edit", string);
			if (string.Compare(m_chat_window->GetTopic()))
				g_m2_engine->SetChatProperty(account_id, m_chat_window->GetChatInfo(), empty, EngineTypes::ROOM_TOPIC, string, 0);
			if (m_chat_window->IsOperator())
			{
				value = GetWidgetValue("Topic_checkbox");
				if (value != m_chat_window->IsTopicProtected())
					g_m2_engine->SetChatProperty(account_id, m_chat_window->GetChatInfo(), empty, EngineTypes::ROOM_TOPIC_PROTECTION, empty, value);

				value = GetWidgetValue("Moderated_checkbox");
				if (value != m_chat_window->IsModerated())
					g_m2_engine->SetChatProperty(account_id, m_chat_window->GetChatInfo(), empty, EngineTypes::ROOM_MODERATED, empty, value);

				value = GetWidgetValue("Secret_checkbox");
				if (value != m_chat_window->IsSecret())
					g_m2_engine->SetChatProperty(account_id, m_chat_window->GetChatInfo(), empty, EngineTypes::ROOM_SECRET, empty, value);

				value = GetWidgetValue("Moderated_checkbox");
				if (value != m_chat_window->IsModerated())
					g_m2_engine->SetChatProperty(account_id, m_chat_window->GetChatInfo(), empty, EngineTypes::ROOM_MODERATED, empty, value);

				value = GetWidgetValue("Password_checkbox");
				GetWidgetText("Password_edit", string);

				if (value != m_chat_window->HasPassword())
					g_m2_engine->SetChatProperty(account_id, m_chat_window->GetChatInfo(), empty, EngineTypes::ROOM_PASSWORD, string, value);
				else if (string.HasContent() && m_chat_window->GetPassword() != 0 &&
					uni_strcmp(string.CStr(), m_chat_window->GetPassword()) != 0)
				{
					g_m2_engine->SetChatProperty(account_id, m_chat_window->GetChatInfo(), empty, EngineTypes::ROOM_PASSWORD, m_chat_window->GetPassword(), 0);
					g_m2_engine->SetChatProperty(account_id, m_chat_window->GetChatInfo(), empty, EngineTypes::ROOM_PASSWORD, string, value);
				}

				if (GetWidgetValue("Limit_checkbox"))
					GetWidgetText("Limit_edit", string);
				else
					string.Empty();

				if (string.Compare(m_chat_window->GetLimit()))
					g_m2_engine->SetChatProperty(account_id, m_chat_window->GetChatInfo(), empty, EngineTypes::ROOM_CHATTER_LIMIT, string, string.HasContent());
			}

			return 1;
		}

	private:

		ChatDesktopWindow*		m_chat_window;

};

/***********************************************************************************
**
**	InputHistory
**
***********************************************************************************/

InputHistory::InputHistory()
:	m_cur_index(IndexEditLine)
{
}


OP_STATUS InputHistory::AddInput(const OpStringC& input)
{
	OpString *new_str = OP_NEW(OpString, ());
	if (!new_str)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<OpString> input_string(new_str);

	RETURN_IF_ERROR(input_string->Set(input));
	RETURN_IF_ERROR(m_history.Insert(0, input_string.get()));

	if (input == m_edit_line)
		m_edit_line.Empty();

	m_cur_index = IndexEditLine;

	input_string.release();
	return OpStatus::OK;
}


OP_STATUS InputHistory::PreviousInput(const OpStringC& currently_editing, OpString &out)
{
	if (ShouldStoreEditLine(currently_editing))
	{
		RETURN_IF_ERROR(m_edit_line.Set(currently_editing));
		m_cur_index = IndexEditLine;
	}

	if (m_cur_index == IndexTop)
		return OpStatus::OK;

	IncrementIndex();

	if (m_cur_index == IndexTop)
		out.Empty();
	else if (m_cur_index == IndexEditLine)
		RETURN_IF_ERROR(out.Set(m_edit_line));
	else
		RETURN_IF_ERROR(out.Set(*m_history.Get(m_cur_index)));

	return OpStatus::OK;
}


OP_STATUS InputHistory::NextInput(const OpStringC& currently_editing, OpString &out)
{
	if (ShouldStoreEditLine(currently_editing))
	{
		RETURN_IF_ERROR(m_edit_line.Set(currently_editing));
		m_cur_index = IndexEditLine;
	}

	if (m_cur_index == IndexBottom)
		return OpStatus::OK;

	DecrementIndex();

	if (m_cur_index == IndexBottom)
		out.Empty();
	else if (m_cur_index == IndexEditLine)
		RETURN_IF_ERROR(out.Set(m_edit_line));
	else
		RETURN_IF_ERROR(out.Set(*m_history.Get(m_cur_index)));

	return OpStatus::OK;
}


BOOL InputHistory::ShouldStoreEditLine(const OpStringC& input) const
{
	BOOL should_store = FALSE;

	if (m_cur_index == IndexBottom ||
		m_cur_index == IndexTop)
	{
		should_store = input.HasContent();
	}
	else if (m_cur_index == IndexEditLine)
		should_store = (m_edit_line.Compare(input) != 0);
	else
		should_store = (m_history.Get(m_cur_index)->Compare(input) != 0);

	return should_store;
}


void InputHistory::DecrementIndex()
{
	if (m_cur_index == IndexTop)
		m_cur_index = INT32(m_history.GetCount());
	if (m_cur_index > IndexBottom)
		--m_cur_index;

	if (m_cur_index == IndexEditLine && m_edit_line.IsEmpty())
		m_cur_index = IndexBottom;
}


void InputHistory::IncrementIndex()
{
	if (m_cur_index == IndexBottom && m_edit_line.IsEmpty())
		++m_cur_index;

	if (m_cur_index < INT32(m_history.GetCount() - 1))
		++m_cur_index;
	else
		m_cur_index = IndexTop;
}


/***********************************************************************************
**
**	NickCompletion
**
***********************************************************************************/

NickCompletion::NickCompletion()
:	m_tree_view(0),
	m_current_match_index(-1)
{
	m_completion_postfix.Set(": ");
}


OP_STATUS NickCompletion::CompleteNick(const OpStringC& input_line,
	INT32& input_position, OpString& output_line)
{
	OP_ASSERT(m_tree_view != 0);
	OP_ASSERT((input_position >= 0) && (input_position <= input_line.Length()));

	output_line.Set(input_line);

	OpString input_copy;
	RETURN_IF_ERROR(input_copy.Set(input_line));

	// Extract the closest word from the position in the text.
	OpString word;
	int nick_pos;

	{
		input_copy.Delete(input_position);
		const UINT input_length = input_copy.Length();

		if (input_length >= 2 && m_completion_postfix.Compare(input_copy.CStr() + (input_length - 2)) == 0)
			input_copy.Delete(input_length - 2);

		// Find the closest space, comma or newline.
		const int space_pos = input_copy.FindLastOf(' ');
		const int comma_pos = input_copy.FindLastOf(',');
		const int newline_pos = input_copy.FindLastOf('\n');

		int start_pos = max(space_pos, comma_pos);
		start_pos = max(start_pos, newline_pos);

		// Extract a word from the position found.
		if (start_pos == KNotFound)
		{
			RETURN_IF_ERROR(word.Set(input_copy));
			nick_pos = 0;
		}
		else
		{
			RETURN_IF_ERROR(word.Set(input_copy.CStr() + start_pos + 1));
			input_copy.Delete(start_pos + 1);

			nick_pos = start_pos + 1;
		}
	}

	if (word.IsEmpty())
	{
		RETURN_IF_ERROR(word.Set(m_current_match_word));

		if (m_current_match_index > -1)
			--m_current_match_index;
	}

	// Update the current match word if this has changed.
	if ((word.CompareI(m_previous_nick_completed) != 0) &&
		(word.CompareI(m_current_match_word) != 0))
	{
		RETURN_IF_ERROR(m_current_match_word.Set(word));
		m_current_match_index = -1;
	}

	if (word.IsEmpty())
		return OpStatus::OK;

	// Create a temporary tree view that has the nicks in alphabetical order.
	OpTreeView* tree_view;
	RETURN_IF_ERROR(OpTreeView::Construct(&tree_view));

	tree_view->SetForceSortByString(TRUE);
	tree_view->SetTreeModel(m_tree_view->GetTreeModel(), 0);

	// Loop the tree and get a matching nickname, if any.
	OpString matching_nick;

	const INT32 chatter_count = tree_view->GetItemCount();
	INT32 index = NormalizeIndex(m_current_match_index + 1, chatter_count);
	INT32 start_index = index;

	BOOL done = FALSE;
	while (!done)
	{
		OpString cur_chatter;
		GetChatterNick(index, cur_chatter, tree_view);

		if (cur_chatter.CompareI(m_current_match_word.CStr(),
			m_current_match_word.Length()) == 0)
		{
			RETURN_IF_ERROR(matching_nick.Set(cur_chatter));
			m_current_match_index = index;

			done = TRUE;
		}

		index = NormalizeIndex(++index, chatter_count);
		if (index == start_index)
			done = TRUE;
	 }

	tree_view->Delete();
	RETURN_IF_ERROR(m_previous_nick_completed.Set(matching_nick));

	if (matching_nick.HasContent())
	{
		const int word_length = word.Length();
		const int matching_nick_length = matching_nick.Length();

		if (output_line.Length() >= (nick_pos + word_length))
			output_line.Delete(nick_pos, word_length);

		RETURN_IF_ERROR(output_line.Insert(nick_pos, matching_nick));

		input_position = nick_pos + matching_nick_length;

		if (nick_pos == 0)
		{
			// Append the postfix if this is a nickname at the beginning of a
			// line and we don't have anything else written there.
			if (output_line.Length() == matching_nick_length)
				RETURN_IF_ERROR(output_line.Append(m_completion_postfix));

			if (m_completion_postfix.Compare(output_line.CStr() + input_position) == 0)
				input_position += m_completion_postfix.Length();
		}
	}

	return OpStatus::OK;
}


void NickCompletion::GetChatterNick(INT32 index, OpString &nick, OpTreeView* tree_view) const
{
	OP_ASSERT(tree_view != 0);
	OpTreeModelItem* item = tree_view->GetItemByPosition(index);

	if (item != 0)
	{
		OpTreeModelItem::ItemData item_data(OpTreeModelItem::COLUMN_QUERY);
		item_data.column_query_data.column_text = &nick;

		item->GetItemData(&item_data);
	}
}


INT32 NickCompletion::NormalizeIndex(INT32 index, INT32 chatter_count) const
{
	if (index >= chatter_count)
		index = 0;

	return index;
}

/***********************************************************************************
**
**	ChatDesktopWindow
**
***********************************************************************************/

OpVector<ChatDesktopWindow>	ChatDesktopWindow::s_attention_windows;

ChatDesktopWindow::ChatDesktopWindow(OpWorkspace* workspace) :
	m_contact(0),
	m_account_id(0),
	m_is_room(FALSE),
	m_is_room_moderated(FALSE),
	m_is_topic_protected(FALSE),
	m_is_room_secret(FALSE),
	m_is_operator(FALSE),
	m_is_voiced(FALSE),
	m_header_pane(NULL),
	m_splitter(NULL),
	m_text_edit(NULL),
	m_send_button(NULL),
	m_persons_view(NULL),
	m_chat_view(NULL),
	m_url_in_use(m_url),
	m_last_was_double_down(FALSE),
	m_reconnect(FALSE),
	m_personal_attention_count(0)
{
	m_command_character.Set("/");
	OP_STATUS err = DesktopWindow::Init(OpWindow::STYLE_DESKTOP, workspace);
	CHECK_STATUS(err);
}

ChatDesktopWindow::~ChatDesktopWindow()
{
	g_hotlist_manager->GetContactsModel()->RemoveListener(this);
	g_hotlist_manager->GetContactsModel()->RemoveModelListener(this);

	if (m_splitter)
	{
		TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::ChatRoomSplitter, m_splitter->GetDivision()));
	}

	if (IsInsideRoom())
		g_m2_engine->LeaveChatRoom(m_account_id, m_chat_info);

	g_m2_engine->RemoveChatListener(this);
	g_sleep_manager->RemoveSleepListener(this);
}


OP_STATUS ChatDesktopWindow::Init(INT32 account_id, const OpStringC& name, BOOL is_room, BOOL is_joining)
{

// Beware!!
// Changing the order in which widget children are added will change TAB selection order!!

	m_account_id = account_id;

	SetName(name);
	m_is_room = is_room;

	OpWidget* root_widget = GetWidgetContainer()->GetRoot();

	if (!(m_header_pane = OP_NEW(ChatHeaderPane, ("Chatbar skin"))))
		return OpStatus::ERR_NO_MEMORY;

	root_widget->AddChild(m_header_pane);

	OpFindTextBar* search_bar = GetFindTextBar();
	root_widget->AddChild(search_bar);

	if (m_is_room)
	{
		RETURN_IF_ERROR(OpSplitter::Construct(&m_splitter));
		root_widget->AddChild(m_splitter);
	}
	else
		m_splitter = NULL;

	RETURN_IF_ERROR(OpBrowserView::Construct(&m_chat_view));

	if (m_is_room)
	{
		m_splitter->AddChild(m_chat_view);

		RETURN_IF_ERROR(OpTreeView::Construct(&m_persons_view));
		m_persons_view->SetName(WIDGET_NAME_CHAT_PERSONS_VIEW);
		m_splitter->AddChild(m_persons_view);

		m_splitter->SetHorizontal(true);
		m_splitter->SetPixelDivision(true, true);
		m_splitter->SetDivision(g_pcm2->GetIntegerPref(PrefsCollectionM2::ChatRoomSplitter));

		m_persons_view->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
		m_persons_view->SetShowColumnHeaders(FALSE);
	}
	else
	{
		root_widget->AddChild(m_chat_view);
		m_persons_view = NULL;
	}

	RETURN_IF_ERROR(m_chat_view->init_status);
	RETURN_IF_ERROR(OpMultilineEdit::Construct(&m_text_edit));
	m_text_edit->SetName(WIDGET_NAME_CHAT_EDIT);
	root_widget->AddChild(m_text_edit);
	m_text_edit->SetText(UNI_L(""));

	RETURN_IF_ERROR(OpButton::Construct(&m_send_button));
	root_widget->AddChild(m_send_button);

	OpString buttontext;
	g_languageManager->GetString(Str::DI_IDSTR_M2_CHATWINDOWTOOLBAR_SENDMESSAGE, buttontext);

	m_send_button->SetText(buttontext.CStr());
	m_send_button->SetButtonType(OpButton::TYPE_TOOLBAR);
	m_send_button->SetTabStop(TRUE);
	m_send_button->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_SEND_MESSAGE)));
	m_send_button->GetBorderSkin()->SetImage("Chatbar Skin");

	// set up output window
	Window* window = (Window*) m_chat_view->GetWindow();
	OP_ASSERT(window != 0);

	window->SetType(WIN_TYPE_IM_VIEW);
	InitURL(window);

	if (m_is_room)
	{
		m_header_pane->SetImage("Chat Room Header", NULL);

		if (!is_joining)
		{
			GoToChatRoom();
		}
	}
	else
	{
		m_header_pane->SetImage("Chat Private Header", m_contact ? m_contact->GetPictureUrl().CStr() : NULL);

		g_m2_engine->Connect(m_account_id);

		WriteStartTimeInfo();
	}

	if (!g_m2_engine->GetAccountById(m_account_id))
		return OpStatus::ERR;

	RETURN_IF_ERROR(g_m2_engine->AddChatListener(this));
	RETURN_IF_ERROR(g_hotlist_manager->GetContactsModel()->AddListener(this));
	g_hotlist_manager->GetContactsModel()->AddModelListener(this);
	RETURN_IF_ERROR(g_sleep_manager->AddSleepListener(this));

	m_text_edit->SetFocus(FOCUS_REASON_OTHER);

	SetIcon(m_is_room ? "Window Chat Room Icon" : "Window Chat Private Icon");

	m_header_pane->SetHeaderToolbar(IsRoom() ? "Chat Room Header Toolbar" : "Chat Private Header Toolbar");
	m_header_pane->SetToolbar(IsRoom() ? "Chat Room Toolbar" : "Chat Private Toolbar");

	m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
	g_languageManager->GetString(Str::S_CHAT_SEND_FILE_TITLE, m_request.caption);

	UpdateTitleAndInfo();

#ifdef CHAT_WINDOW_USE_AMSR
	if (GetWindowCommander() != 0)
		GetWindowCommander()->SetLayoutMode(OpWindowCommander::AMSR);
#endif
	return OpStatus::OK;
}

OP_STATUS ChatDesktopWindow::SetName(const OpStringC& name)
{
	m_contact = g_hotlist_manager->GetContactsModel()->GetByNickname(name);
	return m_name.Set(name);
}


void ChatDesktopWindow::UpdateTitleAndInfo()
{
	// Update Opera window title 
	SetTitle(GetName());
	// Update Chat window title(header toolbar title)
	SetStatusText(GetName(), STATUS_TYPE_TITLE);

	// Get account information.
	Account* account = 0;
	g_m2_engine->GetAccountManager()->GetAccountById(m_account_id, account);

	// Add network information.
	OpString info;

	if (!IsRoom() || IsInsideRoom())
	{
		if (account->GetAccountName().HasContent())
			OpStatus::Ignore(info.AppendFormat(UNI_L("%s - "), account->GetAccountName().CStr()));
	}

	// Update text specific to room / private chat.
	if (!IsRoom())
	{
		if (m_contact)
		{
			OpStatus::Ignore(info.AppendFormat(UNI_L("%s <%s>"), m_contact->GetName().CStr() ? m_contact->GetName().CStr() : UNI_L(""), m_contact->GetMail().CStr() ? m_contact->GetMail().CStr() : UNI_L("")));
		}
		else
		{
			OpString unknown_contact;
			g_languageManager->GetString(Str::S_CHAT_UNKNOWN_CONTACT, unknown_contact);

			OpStatus::Ignore(info.Append(unknown_contact));
		}

		SetStatusText(info.CStr(), STATUS_TYPE_INFO);
	}
	else if (IsInsideRoom())
	{
		INT32 user_count = m_persons_view->GetItemCount();

		if (m_topic.HasContent())
		{
			OpString chat_room_info;
			g_languageManager->GetString(
				user_count == 1 ? Str::LocaleString(Str::S_CHAT_ROOM_INFO_WITH_TOPIC) : Str::LocaleString(Str::S_CHAT_ROOM_INFO_WITH_TOPIC_PLURAL),
				chat_room_info);

			if (chat_room_info.HasContent())
				OpStatus::Ignore(info.AppendFormat(chat_room_info.CStr(), user_count, m_topic.CStr()));
		}
		else
		{
			OpString loc_str;
			g_languageManager->GetString(
				user_count == 1 ? Str::LocaleString(Str::S_CHAT_WINDOW_NOF_USERS) : Str::LocaleString(Str::S_CHAT_WINDOW_NOF_USERS_PLURAL), loc_str);

			if (loc_str.HasContent())
				OpStatus::Ignore(info.AppendFormat(loc_str.CStr(), user_count));
		}

		SetStatusText(info.CStr(), STATUS_TYPE_INFO);
	}
	else
	{
		SetStatusText(0, STATUS_TYPE_INFO);
	}
}


void ChatDesktopWindow::InitURL(Window* window, BOOL init_with_data)
{
	if (window == 0)
		window = (Window *)(m_chat_view->GetWindow());

	OP_ASSERT(window != 0);

	m_url = urlManager->GetNewOperaURL();
	m_url.SetAttribute(URL::KIsGeneratedByOpera, TRUE);
	m_url.SetAttribute(URL::KMIME_ForceContentType, "text/html");
	m_url.SetAttribute(URL::KMIME_CharSet, "utf-16");
	m_url.SetAttribute(URL::KCacheTypeStreamCache, TRUE);

	URL referer_url;
	window->OpenURL(m_url, DocumentReferrer(referer_url), FALSE, TRUE, -1);

	m_url.SetAttribute(URL::KLoadStatus, URL_LOADING);

	if (!init_with_data)
		return;

	OpString css_id;
	OpStatus::Ignore(MakeValidHTMLId(GetName(), css_id));

	OpStringC direction = GetWidgetContainer()->GetRoot()->GetRTL() ? UNI_L(" dir=\"rtl\"") : UNI_L("");

	OpString data;
	OpStatus::Ignore(data.Append("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"));
	OpStatus::Ignore(data.AppendFormat(UNI_L("<html id=\"%s\"%s>\n<head>\n<title>"), css_id.CStr(), direction.CStr()));
	OpStatus::Ignore(data.Append(GetName()));
	OpStatus::Ignore(data.Append("</title>\n"));

	// FIXME: Provide host name of IRC server to allow for host overrides
	// when selecting style sheet.
	OpString styleurl;
	TRAPD(rc, g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleIMFile, &styleurl));

	OpStatus::Ignore(data.Append("<link rel=\"stylesheet\" href=\""));
	OpStatus::Ignore(data.Append(styleurl));
	OpStatus::Ignore(data.Append("\">\n"));

	OpStatus::Ignore(data.Append("<base target=\"_blank\">\n"));
	OpStatus::Ignore(data.Append("</head>\n<body>\n<table>\n"));

	m_url.WriteDocumentData(URL::KNormal, data.CStr(), data.Length());
}


void ChatDesktopWindow::GoToChat()
{
	Activate();

	m_text_edit->SetFocus(FOCUS_REASON_OTHER);

	if (!m_is_room || IsInsideRoom())
		return;

	GoToChatRoom();
}


void ChatDesktopWindow::GoToChatRoom()
{
	OpString joining;
	g_languageManager->GetString(Str::S_CHAT_JOINING_CHAT_ROOM, joining);

	WriteLine(NULL, joining.CStr(), "status");

	OpString empty;
	g_m2_engine->JoinChatRoom(m_account_id, m_name, empty);
}


BOOL ChatDesktopWindow::IsCorrectChatRoom(UINT32 account_id, const OpStringC& name,
	BOOL is_room)
{
	return ((m_account_id == account_id) &&
		(name.IsEmpty() ||
		((m_name.CompareI(name) == 0) && (m_is_room == is_room))));
}


void ChatDesktopWindow::LeaveChat()
{
	if (IsInsideRoom())
		g_m2_engine->LeaveChatRoom(m_account_id, m_chat_info);
}


void ChatDesktopWindow::OnClose(BOOL user_initiated)
{
	// Double check that this window wasn't left in the list
	if (s_attention_windows.Find(this) != -1)
	{
		// In the current list and removing
		s_attention_windows.RemoveByItem(this);
	}

	m_personal_attention_count = 0;

	// Tell taskbar
	UpdateTaskbar();

	DesktopWindow::OnClose(user_initiated);
}



void ChatDesktopWindow::OnActivate(BOOL activate, BOOL first_time)
{
	if (activate)
	{
		// Focus the text edit; you always want the input field to be activated
		// when changing focus to a chat window.
		m_text_edit->SetFocus(FOCUS_REASON_OTHER);
	}
}


void ChatDesktopWindow::OnLayout()
{
	UINT32 width, height;

	GetInnerSize(width, height);

	INT32 header_height = m_header_pane->GetHeightFromWidth(width);

	m_header_pane->SetRect(OpRect(0, 0, width, header_height));

	int search_bar_height = 0;
	OpFindTextBar* search_bar = GetFindTextBar();
	if (search_bar->GetAlignment() != OpBar::ALIGNMENT_OFF)
	{
		search_bar_height = search_bar->GetHeightFromWidth(width);
		search_bar->SetRect(OpRect(0, header_height, width, search_bar_height));
	}

	if (m_splitter)
	{
		m_splitter->SetRect(OpRect(0, header_height + search_bar_height, width, height - header_height - 40 - search_bar_height));
	}
	else
	{
		m_chat_view->SetRect(OpRect(0, header_height + search_bar_height, width, height - header_height - 40 - search_bar_height));
	}

	OpWidget* root = GetWidgetContainer()->GetRoot();
	root->SetChildRect(m_text_edit, OpRect(0, height - 40, width - 100, 40));
	root->SetChildRect(m_send_button, OpRect(width - 100, height - 40, 100, 40));
}


void ChatDesktopWindow::Flush()
{
	if (m_url.GetAttribute(URL::KCacheTypeStreamCache) == TRUE)
		m_url.WriteDocumentDataSignalDataReady();
	else
	{
		Window* window = m_chat_view->GetWindow();
		if (window != 0 &&
			window->DocManager() != 0 &&
			window->DocManager()->GetMessageHandler() != 0)
		{
			URL_ID			id			= 0;
			MessageHandler* msg_handler = window->DocManager()->GetMessageHandler();

			// have to do this because useful Id() function in url is deprecated
			m_url.GetAttribute(URL::K_ID, &id, FALSE);

			msg_handler->PostMessage(MSG_URL_DATA_LOADED, id, 0);
		}
	}
	BroadcastDesktopWindowContentChanged();
}


BOOL ChatDesktopWindow::IsSendingAllowed()
{
	// Get the input text.
	OpString input_text;
	m_text_edit->GetText(input_text);

	if (input_text.IsEmpty())
		return FALSE;

	if (!m_is_room)
		return TRUE;

	// Always allow send if the user is typing a command.
	if (input_text.CompareI(m_command_character.CStr(), 1) == 0)
		return TRUE;

	return IsInsideRoom() &&
		(!m_is_room_moderated || m_is_voiced || m_is_operator);
}


BOOL ChatDesktopWindow::IsInsideRoom()
{
	return m_is_room && m_persons_view->GetTreeModel();
}


void ChatDesktopWindow::ConvertToHTML(OpString& html, const uni_char* text, INT32 len)
{
	if (text)
	{
		OpString text_to_convert;
		text_to_convert.Set(text, len == 0 ? KAll : len);
		IRCTextToHtml to_html(text_to_convert);
		html.Set(to_html.HtmlText());
	}
}


void ChatDesktopWindow::GetNick(OpString& nick)
{
	OpString8 nick8;

	Account* account = NULL;

	g_m2_engine->GetAccountManager()->GetAccountById(m_account_id, account);

	if (account)
	{
		account->GetIncomingUsername(nick8);
		nick.Set(nick8);
	}
}


void ChatDesktopWindow::SendFiles(const OpStringC& nick)
{
	if (nick.IsEmpty() || m_send_files_nick.HasContent())
		return;

	RETURN_VOID_IF_ERROR(m_send_files_nick.Set(nick));

	DesktopFileChooser *chooser;
	RETURN_VOID_IF_ERROR(GetChooser(chooser));

	OpStatus::Ignore(chooser->Execute(GetOpWindow(), this, m_request));
}


BOOL ChatDesktopWindow::ChatCommand(EngineTypes::ChatMessageType command, BOOL only_check_if_available)
{
	OpString nick;
	GetSelectedChatterNick(nick);

	if (nick.IsEmpty())
		return FALSE;

	switch (command)
	{
		case EngineTypes::OP_COMMAND:
		case EngineTypes::DEOP_COMMAND:
		case EngineTypes::VOICE_COMMAND:
		case EngineTypes::DEVOICE_COMMAND:
		case EngineTypes::KICK_COMMAND:
		case EngineTypes::KICK_WITH_REASON_COMMAND:
		case EngineTypes::BAN_COMMAND:
		{
			if (!m_is_room || !m_is_operator)
				return FALSE;
			break;
		}
	}

	if (only_check_if_available)
		return TRUE;

	if (command == EngineTypes::KICK_WITH_REASON_COMMAND)
	{
		AskKickReasonDialog *reason_dialog = OP_NEW(AskKickReasonDialog, (m_account_id, m_chat_info));
		if (reason_dialog)
			reason_dialog->Init(this, nick);

		return TRUE;
	}

	OpString empty;
	ChatInfo t_chat_info;
	g_m2_engine->SendChatMessage(m_account_id, command, empty,
		m_is_room ? m_chat_info : t_chat_info, nick);

	return TRUE;
}


BOOL ChatDesktopWindow::Send()
{
	if (!IsSendingAllowed())
		return TRUE;

	OpString text;
	m_text_edit->GetText(text, FALSE);
	// m_text_edit->Clear();
	m_text_edit->SetText(UNI_L(""));
	m_text_edit->SetFocus(FOCUS_REASON_OTHER);

	m_input_history.AddInput(text);
	g_input_manager->UpdateAllInputStates();

	OpString message;
	OpString room;
	OpString chatter;
	message.Set(text.CStr());

	if (m_is_room)
		room.Set(m_name);
	else
		chatter.Set(m_name);

	EngineTypes::ChatMessageType message_type = m_is_room ? EngineTypes::ROOM_MESSAGE : EngineTypes::PRIVATE_MESSAGE;

	if (message.CompareI(m_command_character.CStr(), 1) == 0)
	{
		message.Delete(0, 1);
		message_type = EngineTypes::RAW_COMMAND;

		// If the command is /query, we want to open up a new window with the
		// given nickname.
		StringTokenizer tokenizer(message);

		OpString command;
		tokenizer.NextToken(command);

		if (command.CompareI("query") == 0)
		{
			OpString query_nick;
			OpString query_message;

			tokenizer.NextToken(query_nick);
			if (query_nick.HasContent())
			{
				ChatInfo chat_info(query_nick, OpStringC());
				g_application->GoToChat(m_account_id, chat_info, FALSE, FALSE, FALSE);
			}

			tokenizer.RemainingString(query_message);
			if (query_message.HasContent()) // Message.
			{
				ChatInfo chat_info(query_nick, OpStringC());
				g_m2_engine->SendChatMessage(m_account_id, EngineTypes::PRIVATE_MESSAGE, query_message, chat_info, query_nick);
			}

			return TRUE;
		}
		// Typing /clear should clear the buffer.
		else if (command.CompareI("clear") == 0)
		{
			m_last_time_stamp.Empty();

			InitURL();
			WriteStartTimeInfo();

			return TRUE;
		}
	}

	g_m2_engine->SendChatMessage(m_account_id, message_type, message,
		m_chat_info, chatter);

	return TRUE;
}


BOOL ChatDesktopWindow::CycleMessage(BOOL cycle_up)
{
	if (cycle_up && !m_text_edit->CaretOnFirstInputLine())
		return FALSE;
	if (!cycle_up && !m_text_edit->CaretOnLastInputLine())
		return FALSE;

	OP_STATUS return_value;

	OpString current_message;
	m_text_edit->GetText(current_message);

	OpString new_message;

	if (cycle_up)
		return_value = m_input_history.PreviousInput(current_message, new_message);
	else
		return_value = m_input_history.NextInput(current_message, new_message);

	if (return_value == OpStatus::OK)
	{
		m_text_edit->SetText(new_message.CStr());
		m_text_edit->SetCaretPos(OpPoint(10000, 10000));
	}
	else
		return FALSE;

	return TRUE;
}


BOOL ChatDesktopWindow::CompleteNick()
{
	if (m_persons_view == 0)
		return TRUE;

	m_nick_completor.SetTreeView(m_persons_view);

	OpString input_text;
	m_text_edit->GetText(input_text);
	if (input_text.IsEmpty())
		return TRUE;

	INT32 character_pos = m_text_edit->GetCaretCharacterPos();

	OpString output_text;
	m_nick_completor.CompleteNick(input_text, character_pos, output_text);

	if (input_text.Compare(output_text) != 0)
	{
		m_text_edit->SetText(output_text.CStr());
		m_text_edit->SetSelection(character_pos, character_pos);
		m_text_edit->SelectNothing();
	}

	return TRUE;
}


INT32 ChatDesktopWindow::GetChatterCount(OpTreeView* view_to_use)
{
	if (!view_to_use)
		view_to_use = m_persons_view;

	return view_to_use ? view_to_use->GetItemCount() : 0;
}


void ChatDesktopWindow::GetChatterNick(INT32 index, OpString& nick, OpTreeView* view_to_use)
{
	if (!view_to_use)
		view_to_use = m_persons_view;

	if (!view_to_use)
		return;

	ChattersModelItem *chatter_item = (ChattersModelItem *)(view_to_use->GetItemByPosition(index));
	if (!chatter_item)
		return;

	nick.Set(chatter_item->GetName());
}


BOOL ChatDesktopWindow::ContainsChatter(OpString& chatter, OpTreeView* view_to_use)
{
	if (!view_to_use)
		view_to_use = m_persons_view;

	const INT32 count = GetChatterCount(view_to_use);
	for (INT32 index = 0; index < count; index++)
	{
		OpString nick;
		GetChatterNick(index, nick, view_to_use);

		if (nick == chatter)
			return TRUE;
	}

	return FALSE;
}


void ChatDesktopWindow::GetSelectedChatterNick(OpString& nick)
{
	if (m_persons_view)
		GetChatterNick(m_persons_view->GetSelectedItemPos(), nick);
	else
		nick.Set(m_name);
}

HotlistModelItem* ChatDesktopWindow::GetSelectedChatterContact()
{
	if (!m_persons_view)
		return m_contact;

	ChattersModelItem *chatter_item = static_cast<ChattersModelItem *>(m_persons_view->GetSelectedItem());
	if (!chatter_item)
		return 0;

	return chatter_item->GetContact();
}


void ChatDesktopWindow::WriteLine(const uni_char* left_text,
	const uni_char* right_text, const char* class_name, BOOL highlight,
	BOOL convert_to_html)
{
	OpString time;
	FormatTime(time, g_oplocale->Use24HourClock() ? UNI_L("%H:%M") : UNI_L("%I:%M"), g_timecache->CurrentTime());

	OpString output_line;
	output_line.Append("<tr><td class=\"time\">");

	if (time.Compare(m_last_time_stamp.CStr()))
	{
		output_line.Append(time.CStr());
		m_last_time_stamp.Set(time.CStr());
	}

	output_line.Append("</td><td class=\"");
	output_line.Append(class_name);
	output_line.Append("\">");
	output_line.Append(left_text);
	output_line.Append("</td><td class=\"");
	output_line.Append(class_name);
	output_line.Append("-message\">");

	BOOL increment_attention_count = FALSE;

	if (convert_to_html)
	{
		// Convert to html.
		OpString formatted_text;
		ConvertToHTML(formatted_text, right_text);

		// Apply highlighting if asked
		if (highlight)
		{
			OpString nick;
			GetNick(nick);

			const BOOL highlight_whole_message = ShouldHighlightWholeMessage(right_text, nick);

			BOOL contains_highlighted_message = FALSE;
			OpStatus::Ignore(InsertHighlightMarkup(formatted_text, nick, highlight_whole_message, contains_highlighted_message));

			if( highlight_whole_message || contains_highlighted_message )
			{
				increment_attention_count = TRUE;
			}
		}

		// Append the formatted message to the output.
		OpStatus::Ignore(output_line.Append(formatted_text));
	}
	else
		OpStatus::Ignore(output_line.Append(right_text));

	OpStatus::Ignore(output_line.Append("</td></tr>"));

	// Restart loading of the URL in case it has stopped.
	if (m_url.GetAttribute(URL::KLoadStatus, TRUE) != URL_LOADING)
	{
		m_url.SetAttribute(URL::KLoadStatus, URL_LOADING);

		URL_Rep* url_rep = m_url.GetRep();
		if (url_rep != 0)
		{
			URL_DataStorage* data_storage = url_rep->GetDataStorage();
			if (data_storage != 0)
				data_storage->UnsetCacheFinished();
		}

		DocumentManager* doc_manager = m_chat_view->GetWindow()->DocManager();
		if (doc_manager != 0)
			doc_manager->UpdateCurrentDoc(FALSE, FALSE, TRUE);
	}

	if( increment_attention_count || !IsRoom() )
	{
		DesktopWindow* window = g_application->GetActiveDesktopWindow(FALSE);
		if( !window || window != this || !window->GetOpWindow()->IsActiveTopmostWindow() )
		{
			m_personal_attention_count ++;
		}
	}

	m_url.WriteDocumentData(URL::KNormal, output_line.CStr(), output_line.Length());

	Flush();
}


/* void ChatDesktopWindow::HighlightText(OpString& output_line, const uni_char* text,
	INT32 start_of_highlight, INT32 highlight_len)
{
	OpString html_text;

	if (start_of_highlight > 0)
	{
		ConvertToHTML(html_text, text, start_of_highlight);
		output_line.Append(html_text.CStr());
	}

	if (highlight_len == 0)
		output_line.Append("<div class=\"highlight\">");
	else
		output_line.Append("<span class=\"highlight\">");

	ConvertToHTML(html_text, text + start_of_highlight, highlight_len);
	output_line.Append(html_text.CStr());

	if (highlight_len == 0)
		output_line.Append("</div>");
	else
		output_line.Append("</span>");
} */


void ChatDesktopWindow::WriteStartTimeInfo()
{
	// Need to fetch the time if this is the first time this function is
	// called.
	if (m_time_created.IsEmpty())
	{
		OpString time;
		FormatTime(time, UNI_L(" %A %x %X"), g_timecache->CurrentTime());

		if (m_is_room)
		{
			OpString started_talking_in;
			g_languageManager->GetString(Str::S_CHAT_STARTED_TALKING_IN, started_talking_in);

			m_time_created.Empty();

			if (started_talking_in.HasContent())
				m_time_created.AppendFormat(started_talking_in.CStr(), GetName());
		}
		else
		{
			OpString started_talking_with;
			g_languageManager->GetString(Str::S_CHAT_STARTED_TALKING_WITH, started_talking_with);

			m_time_created.Empty();

			if (started_talking_with.HasContent())
				m_time_created.AppendFormat(started_talking_with.CStr(), GetName());
		}

		m_time_created.Append(time.CStr());
	}

	WriteLine(NULL, m_time_created.CStr(), "status");
}


void ChatDesktopWindow::EditRoomProperties()
{
	EditRoomDialog* dialog = OP_NEW(EditRoomDialog, ());
	if (dialog)
		dialog->Init(this);
}


BOOL ChatDesktopWindow::OnInputAction(OpInputAction* action)
{
	OpString nick;
	GetSelectedChatterNick(nick);
	HotlistModelItem* contact = GetSelectedChatterContact();

	OpString my_nick;
	GetNick(my_nick);

	BOOL myself_selected = my_nick.CompareI(nick.CStr()) == 0;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SEND_MESSAGE:
				{
					child_action->SetEnabled(IsSendingAllowed());
					return TRUE;
				}
				case OpInputAction::ACTION_ADD_CONTACT:
				case OpInputAction::ACTION_ADD_NICK_TO_CONTACT:
				{
					child_action->SetEnabled(!contact);
					return TRUE;
				}
				case OpInputAction::ACTION_COMPOSE_MAIL:
				{
					child_action->SetEnabled(m_is_room || contact);
					return TRUE;
				}
				case OpInputAction::ACTION_EDIT_PROPERTIES:
				{
					child_action->SetEnabled(contact!=0);
					return TRUE;
				}
				case OpInputAction::ACTION_EDIT_CHAT_ROOM:
				{
					child_action->SetEnabled(m_is_room);
					return TRUE;
				}
				case OpInputAction::ACTION_CHAT_COMMAND:
				{
					child_action->SetEnabled(ChatCommand((EngineTypes::ChatMessageType) child_action->GetActionData(), TRUE));
					return TRUE;
				}
				case OpInputAction::ACTION_SEND_FILE:
				{
					BOOL enable = TRUE;

					if (myself_selected)
						enable = FALSE;
					else if (KioskManager::GetInstance()->GetNoSave() ||
						KioskManager::GetInstance()->GetNoDownload())
					{
						enable = FALSE;
					}

					child_action->SetEnabled(enable);
					return TRUE;
				}
				case OpInputAction::ACTION_JOIN_PRIVATE_CHAT:
				{
					child_action->SetEnabled(!myself_selected);
					return TRUE;
				}
				case OpInputAction::ACTION_SET_CHAT_STATUS:
				{
					if (child_action->IsActionDataStringEqualTo(UNI_L("online-any")))
					{
						child_action->SetSelected(g_application->GetChatStatus(m_account_id) != AccountTypes::OFFLINE);
						child_action->SetEnabled(g_application->SetChatStatus(m_account_id, AccountTypes::ONLINE, TRUE));
					}
					else
					{
						AccountTypes::ChatStatus chat_status = g_application->GetChatStatusFromString(child_action->GetActionDataString());
						child_action->SetSelected(g_application->GetChatStatus(m_account_id) == chat_status);
						child_action->SetEnabled(g_application->SetChatStatus(m_account_id, chat_status, TRUE));
					}
					return TRUE;
				}
				case OpInputAction::ACTION_SAVE_DOCUMENT:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_SET_ENCODING:
				{
					child_action->SetEnabled(FALSE);
					return TRUE;
				}
			}

			break;
		}
		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
				{
					if (child_action->GetFirstInputContext() == m_text_edit && !child_action->GetShiftKeys())
					{
						switch (child_action->GetActionData())
						{
							case OP_KEY_ENTER:
								return Send();
								break;
							case OP_KEY_TAB:
								return CompleteNick();
								break;
							case OP_KEY_UP:
								return CycleMessage(TRUE);
								break;
							case OP_KEY_DOWN :
								return CycleMessage(FALSE);
								break;
						}
					}

					break;
				}
			}

			break;
		}
		case OpInputAction::ACTION_SHOW_FINDTEXT:
		{
			ShowFindTextBar(action->GetActionData());
			return TRUE;
		}
		case OpInputAction::ACTION_SAVE_DOCUMENT:
		{
			SaveDocument(FALSE);
			return TRUE;
		}
		case OpInputAction::ACTION_COMPOSE_MAIL:
		{
			if (contact)
			{
				OpINT32Vector id_list;
				id_list.Add(contact->GetID());
				MailCompose::ComposeMail(id_list);
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			if (contact)
			{
				g_hotlist_manager->EditItem(contact->GetID(), this);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_EDIT_CHAT_ROOM:
		{
			if (!m_is_room)
				return FALSE;

			EditRoomProperties();
			return TRUE;
		}

		case OpInputAction::ACTION_ADD_CONTACT:
		{
			HotlistManager::ItemData item_data;

			item_data.name.Set(nick.CStr());
			item_data.shortname.Set(nick.CStr());

			g_hotlist_manager->SetupPictureUrl(item_data);

			g_hotlist_manager->NewContact(item_data, action->GetActionData(), this, TRUE);
			return TRUE;
		}
		case OpInputAction::ACTION_ADD_NICK_TO_CONTACT:
		{
			return g_hotlist_manager->AddNickToContact(action->GetActionData(), nick);
		}
		case OpInputAction::ACTION_SEND_MESSAGE:
		{
			return Send();
		}
		case OpInputAction::ACTION_CHAT_COMMAND:
		{
			return ChatCommand((EngineTypes::ChatMessageType) action->GetActionData());
		}
		case OpInputAction::ACTION_SEND_FILE :
		{
			SendFiles(nick);
			return TRUE;
		}
		case OpInputAction::ACTION_CHANGE_NICKNAME:
		{
			Account* account = 0;
			g_m2_engine->GetAccountManager()->GetAccountById(m_account_id, account);

			ChangeNickDialog* dialog = OP_NEW(ChangeNickDialog, (ChangeNickDialog::CHANGE_NORMAL, m_account_id, account ? account->GetIncomingUsername().CStr() : NULL));
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}
		case OpInputAction::ACTION_LIST_CHAT_ROOMS:
		case OpInputAction::ACTION_JOIN_CHAT_ROOM:
		case OpInputAction::ACTION_NEW_CHAT_ROOM:
		{
			if (!action->GetActionData())
			{
				// set account and let action continue
				action->SetActionData(m_account_id);
			}
			break;
		}
		case OpInputAction::ACTION_JOIN_PRIVATE_CHAT:
		{
			if (!action->GetActionData())
			{
				// set account and let action continue
				action->SetActionData(m_account_id);
			}
			if (!action->GetActionDataString() && !myself_selected)
			{
				action->SetActionDataString(nick.CStr());
			}
			break;
		}
		case OpInputAction::ACTION_FOCUS_CHAT_INPUT:
		{
			m_text_edit->SetFocus(FOCUS_REASON_ACTION);
			return TRUE;

			break;
		}
		case OpInputAction::ACTION_SET_CHAT_STATUS:
		{
			if (action->IsActionDataStringEqualTo(UNI_L("online-any")))
			{
				if (g_application->GetChatStatus(m_account_id) != AccountTypes::OFFLINE)
					return FALSE;

				return g_application->SetChatStatus(m_account_id, AccountTypes::ONLINE);
			}
			else
			{
				return g_application->SetChatStatus(m_account_id, g_application->GetChatStatusFromString(action->GetActionDataString()));
			}
		}
		case OpInputAction::ACTION_INSERT:
		{
			// Make smilies insert from toolbar work.
			return m_text_edit->OnInputAction(action);
		}
	}

	if (m_chat_view->OnInputAction(action))
		return TRUE;

	return DesktopWindow::OnInputAction(action);
}


void ChatDesktopWindow::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (m_send_files_nick.HasContent())
	{
		for (UINT32 i = 0; i < result.files.GetCount(); i++)
			g_m2_engine->SendFile(m_account_id, m_send_files_nick, *result.files.Get(i));
		m_send_files_nick.Empty();
	}
	else
	{
		DesktopTab::OnFileChoosingDone(chooser, result);
	}
}


void ChatDesktopWindow::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_text_edit)
	{
		m_send_button->SetEnabled(IsSendingAllowed());
		m_send_button->SetUpdateNeeded(TRUE);
		m_send_button->UpdateActionStateIfNeeded();
	}
}


void ChatDesktopWindow::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget != m_persons_view)
	{
		return;
	}

#if defined(_UNIX_DESKTOP_)
	if( m_last_was_double_down )
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_JOIN_PRIVATE_CHAT, 0, NULL, this);
		m_last_was_double_down = FALSE;
	}
#endif

	if ((nclicks == 2) && (button == MOUSE_BUTTON_1))
	{
#if defined(_UNIX_DESKTOP_)
		// Fix for bug #209822
		// We have to delay the creation of the new window until the mouse has been released
		// because the m_persons_view will otherwise receive a mouse press after the window
		// it belongs to is no longer active. In the UNIX port non-active windows are moved
		// out of the main window area and the mouse press in question will trigger the OpTreeView
		// code to ask for the mouse position. That fails when the window has been moved.
		// We should probably try to fix this somewhere else, but at the moment I am out of
		// ideas what to do [espen 2006-05-30]
		m_last_was_double_down = TRUE;
#else
		g_input_manager->InvokeAction(OpInputAction::ACTION_JOIN_PRIVATE_CHAT, 0, NULL, this);
#endif
	}
	else if (!down && button == MOUSE_BUTTON_2)
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Chat User Menu", PopupPlacement::AnchorAtCursor());
	}
}

void ChatDesktopWindow::OnTreeChanged(OpTreeModel* tree_model)
{
	if (m_persons_view)
		m_persons_view->Redraw();
}

void ChatDesktopWindow::OnChatMessageReceived(UINT16 account_id,
	EngineTypes::ChatMessageType type, const OpStringC& message,
	const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat)
{
	if (type == EngineTypes::SERVER_MESSAGE)
	{
		if ((account_id == m_account_id) &&
			(this == g_application->GetActiveDesktopWindow(FALSE)))
		{
			WriteLine(chat_info.ChatName().CStr(), message.CStr(), "status", FALSE);
		}

		return;
	}

	if (!IsCorrectChatRoom(account_id, chat_info.ChatName(), !is_private_chat))
		return;

	UpdateChatInfoIfNeeded(chat_info);

	OpString personal_nick;
	GetNick(personal_nick);

	switch (type)
	{
		case EngineTypes::PRIVATE_SELF_ACTION_MESSAGE:
		case EngineTypes::ROOM_SELF_ACTION_MESSAGE:
		{
			OpString text;
			text.Append(personal_nick.CStr());
			text.Append(" ");
			text.Append(message.CStr());

			WriteLine(NULL, text.CStr(), "self-action", FALSE);
			break;
		}

		case EngineTypes::PRIVATE_ACTION_MESSAGE:
		case EngineTypes::ROOM_ACTION_MESSAGE:
		{
			OpString text;
			text.Append(chatter.CStr());
			text.Append(" ");
			text.Append(message.CStr());

			WriteLine(NULL, text.CStr(), "action");
			break;
		}

		case EngineTypes::PRIVATE_SELF_MESSAGE:
		case EngineTypes::ROOM_SELF_MESSAGE:
		{
			WriteLine(personal_nick.CStr(), message.CStr(), "self", FALSE);
			break;
		}

		case EngineTypes::PRIVATE_MESSAGE:
		case EngineTypes::ROOM_MESSAGE:
		{
			WriteLine(chatter.CStr(), message.CStr(), "sender");
			break;
		}
	}

	BOOL b = (GetOpWindow()->IsActiveTopmostWindow() && IsActive()) ? TRUE : FALSE;

	if (!b && (!m_is_room ||
		message.FindI(personal_nick.CStr()) != KNotFound ||
		GetParentWorkspace()))
	{
		// Don't attention the window if we just did stuff ourselves.
		if (type != EngineTypes::PRIVATE_SELF_ACTION_MESSAGE &&
			type != EngineTypes::ROOM_SELF_ACTION_MESSAGE &&
			type != EngineTypes::PRIVATE_SELF_MESSAGE &&
			type != EngineTypes::ROOM_SELF_MESSAGE)
		{
			SetAttention(TRUE);
		}
	}
}


void ChatDesktopWindow::SetAttention(BOOL attention)
{
	// Check if the window is in the current list of windows that need attention
	if (s_attention_windows.Find(this) == -1)
	{
		// Not in the current list and adding
		if (attention)
			s_attention_windows.Add(this);
	}
	else
	{
		// In the current list and removing
		if (!attention)
			s_attention_windows.RemoveByItem(this);
	}

	if( !attention )
	{
		m_personal_attention_count = 0;
	}

	UpdateTaskbar();

	// Call the base class to do the usual Attention stuff
	DesktopWindow::SetAttention(attention);
}

///////////////////////////////////////////////////////////////////////////////////////////

void ChatDesktopWindow::UpdateTaskbar()
{
	INT32 unread_count = 0;

	if (g_pcui->GetIntegerPref(PrefsCollectionUI::LimitAttentionToPersonalChatMessages))
	{
		// Loop all chatwindows that are open
		for (UINT32 i = 0; i < s_attention_windows.GetCount(); i++)
		{
			// Sum up all the m_personal_attention_count for every window
			unread_count += s_attention_windows.Get(i)->m_personal_attention_count;
		}
	}
	else
	{
		unread_count = s_attention_windows.GetCount();
	}

	g_m2_engine->OnUnattendedChatCountChanged(GetOpWindow(), unread_count);
}

///////////////////////////////////////////////////////////////////////////////////////////

void ChatDesktopWindow::OnChatReconnecting(UINT16 account_id, const ChatInfo& room)
{
	if (!IsCorrectChatRoom(account_id, room.ChatName(), TRUE))
	{
		return;
	}
	OpString disconnected;
	g_languageManager->GetString(Str::S_CHAT_DISCONNECTED_FROM, disconnected);

	WriteLine(NULL, disconnected.CStr(), "status");

	OpString joining;
	g_languageManager->GetString(Str::S_CHAT_JOINING_CHAT_ROOM, joining);

	WriteLine(NULL, joining.CStr(), "status");
}

///////////////////////////////////////////////////////////////////////////////////////////

void ChatDesktopWindow::OnChatRoomJoined(UINT16 account_id, const ChatInfo& room)
{
	if (IsCorrectChatRoom(account_id, room.ChatName(), TRUE) == FALSE)
	{
		return;
	}

	UpdateChatInfoIfNeeded(room);
	WriteStartTimeInfo();

	OpTreeModel* chatters_model;

	if (g_m2_engine->GetChattersModel(&chatters_model, account_id, room.ChatName()) == OpStatus::OK)
	{
		m_persons_view->SetTreeModel(chatters_model, 0);
	}

	m_topic.Empty();
	UpdateTitleAndInfo();

	g_input_manager->UpdateAllInputStates();
}


void ChatDesktopWindow::OnChatRoomLeaving(UINT16 account_id, const ChatInfo& room,
	const OpStringC& kicker, const OpStringC& kick_reason)
{
	if (!IsCorrectChatRoom(account_id, room.ChatName(), TRUE))
	{
		return;
	}

	UpdateChatInfoIfNeeded(room);

	if (m_persons_view != 0)
		m_persons_view->SetTreeModel(NULL);

	Account* account = NULL;
	g_m2_engine->GetAccountManager()->GetAccountById(account_id, account);

	if (account)
	{
		if (!room.HasName())
		{
			OpString disconnected;
			g_languageManager->GetString(Str::S_CHAT_DISCONNECTED_FROM, disconnected);

			WriteLine(NULL, disconnected.CStr(), "status");
		}
		else if (kicker.HasContent())
		{
			OpString kicked_unformatted;
			g_languageManager->GetString(Str::S_CHAT_YOU_ARE_KICKED, kicked_unformatted);

			OpString kicked;

			if (kicked_unformatted.HasContent())
			{
				kicked.AppendFormat(kicked_unformatted.CStr(), kicker.CStr(), kick_reason.CStr());
				WriteLine(NULL, kicked.CStr(), "status");
			}
		}
		else
		{
			OpString left;
			g_languageManager->GetString(Str::S_CHAT_YOU_HAVE_LEFT_ROOM, left);

			WriteLine(NULL, left.CStr(), "status");
		}

		g_input_manager->UpdateAllInputStates();
		UpdateTitleAndInfo();
	}
	else
	{
		Close();
	}
}


void ChatDesktopWindow::OnChatRoomLeft(UINT16 account_id,
	const ChatInfo& room, const OpStringC& kicker,
	const OpStringC& kick_reason)
{
	m_last_time_stamp.Empty();
	m_time_created.Empty();
}


void ChatDesktopWindow::OnChatterJoined(UINT16 account_id, const ChatInfo& room,
	const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix,
	BOOL initial)
{
	if (!IsCorrectChatRoom(account_id, room.ChatName(), TRUE))
	{
		return;
	}

	UpdateChatInfoIfNeeded(room);

	OpString nick;
	GetNick(nick);

	if (nick.CompareI(chatter.CStr()) == 0)
	{
		m_is_operator = is_operator;
		m_is_voiced = is_voiced;
	}

	if (initial)
	{
		return;
	}

	OpString text;

	text.Set("-> ");

	{
		OpString has_joined_unformatted;
		g_languageManager->GetString(Str::S_CHAT_CHATTER_HAS_JOINED, has_joined_unformatted);

		if (has_joined_unformatted.HasContent())
			text.AppendFormat(has_joined_unformatted.CStr(), chatter.CStr(), room.ChatName().CStr());
	}

	WriteLine(NULL, text.CStr(), "join");
	UpdateTitleAndInfo();
}


void ChatDesktopWindow::OnChatterLeaving(UINT16 account_id, const ChatInfo& room,
	const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker)
{
	if (!IsCorrectChatRoom(account_id, room.ChatName(), TRUE))
		return;

	UpdateChatInfoIfNeeded(room);

	OpString text;
	text.Set("<- ");

	// If room is empty, it means that a person has quit IRC.
	if (!room.HasName())
	{
		ChatRoom *chat_room = g_m2_engine->GetChatRoom(account_id, m_name);
		if ((chat_room != 0) && chat_room->GetChatter(chatter))
		{
			OpString has_disconnected_unformatted;
			g_languageManager->GetString(Str::S_CHAT_CHATTER_HAS_DISCONNECTED, has_disconnected_unformatted);

			if (has_disconnected_unformatted.IsEmpty())
				return;

			text.AppendFormat(has_disconnected_unformatted.CStr(), chatter.CStr());

			if (message.HasContent())
				text.AppendFormat(UNI_L(" (%s)"), message.CStr());

			WriteLine(NULL, text.CStr(), "disconnect");
		}
	}

	// If kicker has content, it means that this person was kicked by someone.
	else if (kicker.HasContent())
	{
		OpString kicked_unformatted;
		g_languageManager->GetString(Str::S_CHAT_CHATTER_KICKED, kicked_unformatted);

		if (kicked_unformatted.IsEmpty())
			return;

		text.AppendFormat(kicked_unformatted.CStr(), chatter.CStr(), room.ChatName().CStr(), kicker.CStr(), message.HasContent() ? message.CStr() : UNI_L(""));
		WriteLine(NULL, text.CStr(), "leave");
	}

	// This means that a person has left the room.
	else if (room.HasName())
	{
		OpString has_left_unformatted;
		g_languageManager->GetString(Str::S_CHAT_CHATTER_HAS_LEFT, has_left_unformatted);

		if (has_left_unformatted.IsEmpty())
			return;

		text.AppendFormat(has_left_unformatted.CStr(), chatter.CStr(), room.ChatName().CStr());

		if (message.HasContent())
			text.AppendFormat(UNI_L(" (%s)"), message.CStr());

		WriteLine(NULL, text.CStr(), "leave");
	}

	UpdateTitleAndInfo();
}


void ChatDesktopWindow::OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room,
	const OpStringC& chatter, const OpStringC& changed_by,
	EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value)
{
	if (m_account_id != account_id)
		return;

	OpString nick;
	GetNick(nick);

	BOOL chatter_is_me = nick.CompareI(chatter.CStr()) == 0;
	BOOL chatter_is_this_window = !m_is_room && chatter.CompareI(m_name) == 0;

	if (room.HasName())
	{
		if (!IsCorrectChatRoom(account_id, room.ChatName(), TRUE))
		{
			return;
		}
	}
	else if (m_is_room)
	{
		ChatRoom* chat_room = g_m2_engine->GetChatRoom(account_id, m_name);
		if (!chat_room || (!chat_room->GetChatter(chatter) && !chat_room->GetChatter(property_string)))
			return;
	}
	else if (!chatter_is_this_window)
	{
		return;
	}

	UpdateChatInfoIfNeeded(room);

	switch (property)
	{
		case EngineTypes::ROOM_STATUS:
		{
			UpdateTitleAndInfo();
			break;
		}

		case EngineTypes::ROOM_TOPIC:
		{
			if (changed_by.IsEmpty())
			{
				OpString room_topic_unformatted;
				g_languageManager->GetString(Str::S_CHAT_ROOM_TOPIC_IS, room_topic_unformatted);

				if (room_topic_unformatted.HasContent())
				{
					OpString room_topic;
					room_topic.AppendFormat(room_topic_unformatted.CStr(), property_string.CStr());

					WriteLine(NULL, room_topic.CStr(), "topic");
				}
			}
			else
			{
				OpString room_topic_unformatted;
				g_languageManager->GetString(Str::S_CHAT_CHANGES_ROOM_TOPIC_TO, room_topic_unformatted);

				if (room_topic_unformatted.HasContent())
				{
					OpString room_topic;
					room_topic.AppendFormat(room_topic_unformatted.CStr(), changed_by.CStr(), property_string.CStr());
					WriteLine(NULL, room_topic.CStr(), "topic");
				}
			}
			IRCColorStripper color_stripper;
			color_stripper.Init(property_string);

			m_topic.Set(color_stripper.StrippedText());
			UpdateTitleAndInfo();
			break;
		}

		case EngineTypes::ROOM_MODERATED:
		{
			m_is_room_moderated = property_value;

			if (changed_by.IsEmpty())
			{
				if (property_value)
				{
					OpString moderated;
					g_languageManager->GetString(Str::S_CHAT_ROOM_MODERATION_IS_ON, moderated);

					WriteLine(NULL, moderated.CStr(), "moderated");
				}
			}
			else if (property_value)
			{
				OpString unformatted;
				g_languageManager->GetString(Str::S_CHAT_ROOM_IS_NOW_MODERATED, unformatted);

				if (unformatted.HasContent())
				{
					OpString text;
					text.AppendFormat(unformatted.CStr(), changed_by.CStr());

					WriteLine(NULL, text.CStr(), "moderated");
				}
			}
			else
			{
				OpString unformatted;
				g_languageManager->GetString(Str::S_CHAT_ROOM_IS_NO_LONGER_MODERATED, unformatted);

				if (unformatted.HasContent())
				{
					OpString text;
					text.AppendFormat(unformatted.CStr(), changed_by.CStr());

					WriteLine(NULL, text.CStr(), "moderated");
				}
			}
			break;
		}

		case EngineTypes::ROOM_CHATTER_LIMIT:
		{
			if (property_value)
				m_limit.Set(property_string.CStr());
			else
				m_limit.Empty();

			if (changed_by.IsEmpty())
			{
				break;
			}
			else if (property_value)
			{
				OpString unformatted;
				g_languageManager->GetString(Str::S_CHAT_USER_LIMIT_IS_NOW, unformatted);

				if (unformatted.HasContent())
				{
					OpString text;
					text.AppendFormat(unformatted.CStr(), changed_by.CStr(), property_string.CStr());

					WriteLine(NULL, text.CStr(), "limit");
				}
			}
			else
			{
				OpString unformatted;
				g_languageManager->GetString(Str::S_CHAT_USER_LIMIT_IS_NOW_OFF, unformatted);

				if (unformatted.HasContent())
				{
					OpString text;
					text.AppendFormat(unformatted.CStr(), changed_by.CStr());

					WriteLine(NULL, text.CStr(), "limit");
				}
			}

			break;
		}

		case EngineTypes::ROOM_TOPIC_PROTECTION:
		{
			m_is_topic_protected = property_value;

			if (changed_by.IsEmpty())
			{
				break;
			}
			OpString unformatted;
			g_languageManager->GetString(property_value ? Str::LocaleString(Str::S_CHAT_TOPIC_CHANGE_BY_OPERATORS_ONLY) : Str::LocaleString(Str::S_CHAT_TOPIC_CHANGE_BY_EVERYONE), unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), changed_by.CStr());

				WriteLine(NULL, text.CStr(), "topic-protection");
			}
			break;
		}

		case EngineTypes::ROOM_PASSWORD:
		{
			if (property_value)
				m_password.Set(property_string.CStr());
			else
				m_password.Empty();

			if (changed_by.IsEmpty())
			{
				break;
			}
			OpString unformatted;
			g_languageManager->GetString(property_value ? Str::LocaleString(Str::S_CHAT_ROOM_IS_NOW_PASSWORD_PROTECTED) : Str::LocaleString(Str::S_CHAT_ROOM_IS_NO_LONGER_PASSWORD_PROTECTED), unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), changed_by.CStr());

				WriteLine(NULL, text.CStr(), "password");
			}
			break;
		}

		case EngineTypes::ROOM_SECRET:
		{
			m_is_room_secret = property_value;

			if (changed_by.IsEmpty())
			{
				break;
			}
			OpString unformatted;
			g_languageManager->GetString(property_value ? Str::LocaleString(Str::S_CHAT_ROOM_IS_NOW_SECRET) : Str::LocaleString(Str::S_CHAT_ROOM_IS_NO_LONGER_SECRET), unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), changed_by.CStr());

				WriteLine(NULL, text.CStr(), "secret");
			}
			break;
		}

		case EngineTypes::ROOM_UNKNOWN_MODE :
		{
			if (changed_by.IsEmpty())
				break;

			OpString unformatted;
			g_languageManager->GetString(Str::S_CHAT_ROOM_UNKNOWN_MODE, unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), changed_by.CStr(), property_string.CStr());

				WriteLine(NULL, text.CStr(), "unknown-mode");
			}

			break;
		}

		case EngineTypes::CHATTER_UNKNOWN_MODE_OPERATOR :
		case EngineTypes::CHATTER_UNKNOWN_MODE_VOICED :
		case EngineTypes::CHATTER_UNKNOWN_MODE :
		{
			if (changed_by.IsEmpty())
				break;

			OpString unformatted;
			g_languageManager->GetString(Str::S_CHAT_ROOM_UNKNOWN_USER_MODE, unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), changed_by.CStr(), property_string.CStr());

				WriteLine(NULL, text.CStr(), "unknown-mode");
			}

			break;
		}

		case EngineTypes::CHATTER_NICK:
		{
			if (chatter_is_this_window)
			{
				SetName(property_string);
				m_chat_info.SetChatName(property_string);

				UpdateTitleAndInfo();
			}

			OpString unformatted;
			g_languageManager->GetString(Str::S_CHAT_CHATTER_CHANGES_NICK, unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), chatter.CStr(), property_string.CStr());

				WriteLine(NULL, text.CStr(), "nick");
			}

			break;
		}

		case EngineTypes::CHATTER_OPERATOR:
		{
			if (chatter_is_me)
			{
				m_is_operator = property_value;
			}

			OpString unformatted;
			g_languageManager->GetString(property_value ? Str::LocaleString(Str::S_CHAT_CHATTER_IS_NOW_OPERATOR) : Str::LocaleString(Str::S_CHAT_CHATTER_IS_NO_LONGER_OPERATOR), unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), changed_by.CStr(), chatter.CStr());

				WriteLine(NULL, text.CStr(), "operator");
			}
			break;
		}

		case EngineTypes::CHATTER_VOICED:
		{
			if (chatter_is_me)
			{
				m_is_voiced = property_value;
			}

			OpString unformatted;
			g_languageManager->GetString(property_value ? Str::LocaleString(Str::S_CHAT_CHATTER_GETS_VOICE) : Str::LocaleString(Str::S_CHAT_CHATTER_REMOVES_VOICE), unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), changed_by.CStr(), chatter.CStr());

				WriteLine(NULL, text.CStr(), "voiced");
			}
			break;
		}
	}

	g_input_manager->UpdateAllInputStates();
}


void ChatDesktopWindow::OnWhoisReply(UINT16 account_id, const OpStringC& nick,
	const OpStringC& user_id, const OpStringC& host,
	const OpStringC& real_name, const OpStringC& server,
	const OpStringC& server_info, const OpStringC& away_message,
	const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle,
	int signed_on, const OpStringC& channels)
{
	if (account_id == m_account_id && this == g_application->GetActiveDesktopWindow(FALSE))
	{
		{
			OpString unformatted;
			g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_USER_IS, unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), nick.CStr(), user_id.CStr(), host.CStr(), real_name.CStr());

				WriteLine(NULL, text.CStr(), "whois", FALSE);
			}
		}

		if (channels.HasContent())
		{
			OpString unformatted;
			g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_IN_ROOMS, unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), nick.CStr(), channels.CStr());

				WriteLine(NULL, text.CStr(), "whois", FALSE);
			}
		}

		if (server.HasContent())
		{
			OpString unformatted;
			g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_USING_SERVER, unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), nick.CStr(), server.CStr(), server_info.CStr());

				WriteLine(NULL, text.CStr(), "whois", FALSE);
			}
		}

		if (is_irc_operator)
		{
			OpString unformatted;
			g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_IS_IRC_OPERATOR, unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), nick.CStr());

				WriteLine(NULL, text.CStr(), "whois", FALSE);
			}
		}

		if (away_message.HasContent())
		{
			OpString unformatted;
			g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_IS_AWAY, unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), nick.CStr(), away_message.CStr());

				WriteLine(NULL, text.CStr(), "whois", FALSE);
			}
		}

		if (logged_in_as.HasContent())
		{
			OpString unformatted;
			g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_LOGGED_IN_AS, unformatted);

			if (unformatted.HasContent())
			{
				OpString text;
				text.AppendFormat(unformatted.CStr(), nick.CStr(), logged_in_as.CStr());

				WriteLine(NULL, text.CStr(), "whois", FALSE);
			}
		}

		if (signed_on != -1)
		{
			OpString signon_text;
			OpString idle_text;

			if (!signon_text.Reserve(128))
				return;

			idle_text.Reserve(128);

			{
				time_t signon_time = signed_on;
				tm *local = localtime(&signon_time);
				g_oplocale->op_strftime(signon_text.CStr(), signon_text.Capacity(), UNI_L("%c"), local);
			}

			if (seconds_idle != -1)
			{
				const unsigned int hours = seconds_idle / 3600;
				seconds_idle -= hours * 3600;

				const unsigned int minutes = seconds_idle / 60;
				seconds_idle -= minutes * 60;

				const unsigned int seconds = seconds_idle;
				OP_ASSERT(seconds <= 60);

				OpString hour_text;
				OpString minute_text;
				OpString second_text;

				if (hours > 0)
				{
					if (hours == 1)
					{
						OpString unformatted;
						g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_IDLE_HOUR, unformatted);

						if (unformatted.HasContent())
							hour_text.AppendFormat(unformatted.CStr(), hours);
					}
					else
					{
						OpString unformatted;
						g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_IDLE_HOURS, unformatted);

						if (unformatted.HasContent())
							hour_text.AppendFormat(unformatted.CStr(), hours);
					}
				}

				if (minutes > 0)
				{
					if (minutes == 1)
					{
						OpString unformatted;
						g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_IDLE_MINUTE, unformatted);

						if (unformatted.HasContent())
							minute_text.AppendFormat(unformatted.CStr(), minutes);
					}
					else
					{
						OpString unformatted;
						g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_IDLE_MINUTES, unformatted);

						if (unformatted.HasContent())
							minute_text.AppendFormat(unformatted.CStr(), minutes);
					}
				}

				if (seconds > 0)
				{
					if (seconds == 1)
					{
						OpString unformatted;
						g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_IDLE_SECOND, unformatted);

						if (unformatted.HasContent())
							second_text.AppendFormat(unformatted.CStr(), seconds);
					}
					else
					{
						OpString unformatted;
						g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_IDLE_SECONDS, unformatted);

						if (unformatted.HasContent())
							second_text.AppendFormat(unformatted.CStr(), seconds);
					}
				}

				{
					OpString and_string;
					g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_AND, and_string);

					if (hour_text.HasContent())
					{
						idle_text.Append(hour_text);

						if (minute_text.HasContent() && second_text.HasContent())
							idle_text.Append(", ");
						else if (minute_text.HasContent() || second_text.HasContent())
							idle_text.Append(and_string);
					}
					if (minute_text.HasContent())
					{
						idle_text.Append(minute_text);

						if (second_text.HasContent())
							idle_text.Append(and_string);
					}
					if (second_text.HasContent())
					{
						idle_text.Append(second_text);
					}
				}

			}

			if (idle_text.HasContent())
			{
				OpString unformatted;
				g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_SIGNED_ON_AND_IDLE, unformatted);

				if (unformatted.HasContent())
				{
					OpString text;
					text.AppendFormat(unformatted.CStr(), nick.CStr(), signon_text.CStr(), idle_text.CStr());

					WriteLine(NULL, text.CStr(), "whois", FALSE);
				}
			}
			else
			{
				OpString unformatted;
				g_languageManager->GetString(Str::S_CHAT_WHOIS_REPLY_SIGNED_ON_UNKNOWN_IDLE, unformatted);

				if (unformatted.HasContent())
				{
					OpString text;
					text.AppendFormat(unformatted.CStr(), nick.CStr(), signon_text.CStr());

					WriteLine(NULL, text.CStr(), "whois", FALSE);
				}
			}
		}
	}
}


void ChatDesktopWindow::OnInvite(UINT16 account_id, const OpStringC& nick,
	const ChatInfo& room)
{
	if (IsCorrectChatRoom(account_id, nick, FALSE))
	{
		OpString href;
		OpStatus::Ignore(href.AppendFormat(UNI_L("<a href=\"irc:///%s\" target=\"_self\">%s</a>"),
			room.ChatName().CStr(), room.ChatName().CStr()));

		OpString invite_format;
		g_languageManager->GetString(Str::S_CHATROOM_INVITATION, invite_format);

		OpString invite_message;
		OpStatus::Ignore(invite_message.AppendFormat(
			invite_format.CStr(),
			nick.CStr(),
			href.CStr()));

		if (invite_message.HasContent())
		{
			WriteLine(0, invite_message.CStr(), "action", FALSE, FALSE);
			SetAttention(TRUE);
		}
	}
}


void ChatDesktopWindow::OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender,
	const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id)
{
	if (IsCorrectChatRoom(account_id, sender, FALSE))
	{
		OpString href;
		OpStatus::Ignore(href.AppendFormat(UNI_L("<a href=\"opera-chattransfer:accountid=%u&port=%u&transferid=%u\" target=\"_self\">%s</a>"),
			account_id, port_number, id, filename.CStr()));

		uni_char file_size_buffer[100];
		StrFormatByteSize(file_size_buffer, 100-1, file_size, SFBS_DEFAULT);

		OpString format;
		g_languageManager->GetString(Str::S_CHAT_RECEIVE_FILE_NOTICE, format);

		OpString receive_message;
		OpStatus::Ignore(receive_message.AppendFormat(
			format.CStr(),
			sender.CStr(), href.CStr(), file_size_buffer));

		if (receive_message.HasContent())
		{
			WriteLine(0, receive_message.CStr(), "action", FALSE, FALSE);
			SetAttention(TRUE);
		}
	}
}


void ChatDesktopWindow::OnSessionReadL(const OpSessionWindow* session_window)
{
	OpString name;

	INT32 account_id = session_window->GetValueL("account id");
	name.Set(session_window->GetStringL("name"));
	BOOL is_room = session_window->GetValueL("is room");

	if (OpStatus::IsError(Init(account_id, name, is_room)))
		Close();
}


void ChatDesktopWindow::OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown)
{
	session_window->SetTypeL(OpSessionWindow::CHAT_WINDOW);
	session_window->SetValueL("account id", m_account_id);
	session_window->SetStringL("name", m_name);
	session_window->SetValueL("is room", m_is_room);
}


void ChatDesktopWindow::UpdateChatInfoIfNeeded(const ChatInfo& chat_info)
{
	if (!m_chat_info.HasName())
		m_chat_info = chat_info;
/*
	else
	{
		m_chat_info.SetStringData(chat_info.StringData());
		m_chat_info.SetIntegerData(chat_info.IntegerData());
	}
*/
}


OP_STATUS ChatDesktopWindow::MakeValidHTMLId(const OpStringC& id, OpString& valid_id) const
{
	int len = id.Length();
	if (!len)
		return OpStatus::ERR;

	if (valid_id.Reserve(len + 3) == 0)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(valid_id.Append("oc-"));

	const uni_char* cur_id_char = id.CStr();
	while (*cur_id_char != '\0')
	{
		// ID tokens must begin with a letter ([A-Za-z]) and may be followed
		// by any number of letters, digits ([0-9]), hyphens ("-"),
		// underscores ("_"), colons (":"), and periods (".").
		if (uni_isalpha(*cur_id_char) ||
			uni_isdigit(*cur_id_char) ||
			*cur_id_char == '-' ||
			*cur_id_char == '_' ||
			*cur_id_char == ':' ||
			*cur_id_char == '.')
		{
			RETURN_IF_ERROR(valid_id.Append(cur_id_char, 1));
		}
		else // Insert an underscore for all non-valid characters.
			RETURN_IF_ERROR(valid_id.Append("_"));

		++cur_id_char;
	}

	return OpStatus::OK;
}


BOOL ChatDesktopWindow::ShouldHighlightWholeMessage(const OpStringC& message,
	const OpStringC& nick) const
{
	// If a message starts with "mynick:", "mynick," or contains "mynick?",
	// the whole message should be highlighted.
	const int message_length = message.Length();
	const int nick_length = nick.Length();

	if (nick_length <= 0) // If the nick is blank, there is nothing to check.
		return FALSE;

	BOOL highlight_whole_message = FALSE;

	if (message.CompareI(nick.CStr(), nick_length) == 0 &&
		message_length > nick_length)
	{
		const uni_char following_character = message[nick_length];
		if (following_character == ':' || following_character == ',')
			highlight_whole_message = TRUE;
	}

	if (!highlight_whole_message)
	{
		const uni_char* work_string = message.CStr();
		int work_string_length = message_length;

		while (const uni_char* start = uni_stristr(work_string, nick.CStr()))
		{
			const int find_pos = start - work_string;
			if (work_string_length > find_pos + nick_length)
			{
				const uni_char following_character = *(start + nick_length);
				if (following_character == '?')
				{
					highlight_whole_message = TRUE;
					break;
				}
			}

			work_string = start + nick_length;
			work_string_length -= (find_pos + nick_length);
		}
	}

	return highlight_whole_message;
}


OP_STATUS ChatDesktopWindow::InsertHighlightMarkup(OpString& formatted_message,
	const OpStringC& nick, BOOL highlight_whole_message, BOOL& contains_highlighted_message) const
{
	if (highlight_whole_message)
	{
		RETURN_IF_ERROR(formatted_message.Insert(0, "<div class=\"highlight\">"));
		RETURN_IF_ERROR(formatted_message.Append("</div>"));
		contains_highlighted_message = TRUE;
	}
	else
	{
		static const char span_begin_markup[] = "<span class=\"highlight\">";
		static const char span_end_markup[] = "</span>";
		OpStringC delimiters(UNI_L(" .,:;/&?"));

		const int nick_length = nick.Length();

		BOOL inside_link_tag = FALSE;

		const uni_char* cur_char = formatted_message.CStr();
		while (cur_char != 0 && *cur_char != '\0')
		{
			if (!inside_link_tag && (nick_length > 0) && uni_strnicmp(cur_char, nick.CStr(), nick_length) == 0)
			{
				const uni_char* prev_char = (cur_char == formatted_message.CStr() ? 0 : cur_char - 1);
				const uni_char* succ_char = cur_char + nick_length;

				const BOOL prev_char_is_ok = (prev_char == 0 || *prev_char == '>' || delimiters.FindFirstOf(*prev_char) != KNotFound);
				const BOOL succ_char_is_ok = (*succ_char == '\0' || *succ_char == '<' || delimiters.FindFirstOf(*succ_char) != KNotFound);

				if (prev_char_is_ok && succ_char_is_ok)
				{
					int cur_pos = cur_char - formatted_message.CStr();
					RETURN_IF_ERROR(formatted_message.Insert(cur_pos, span_begin_markup));

					cur_char = formatted_message.CStr() + cur_pos + strlen(span_begin_markup) + nick_length;

					cur_pos = cur_char - formatted_message.CStr();
					RETURN_IF_ERROR(formatted_message.Insert(cur_pos, span_end_markup));

					cur_char = formatted_message.CStr() + cur_pos + strlen(span_end_markup);
					contains_highlighted_message = TRUE;
				}
				else
					++cur_char;
			}
			else if (uni_strnicmp(cur_char, "<a", 2) == 0)
			{
				cur_char += 2;
				inside_link_tag = TRUE;
			}
			else if (inside_link_tag && uni_strnicmp(cur_char, ">", 1) == 0)
			{
				++cur_char;
				inside_link_tag = FALSE;
			}
			else
				++cur_char;
		}
	}

	return OpStatus::OK;
}

ChatDesktopWindow* ChatDesktopWindow::FindChatRoom(UINT32 account_id, const OpStringC& name, BOOL is_room)
{
	OpVector<DesktopWindow> windows;
	g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_CHAT, windows);
	g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_CHAT_ROOM, windows);

	for (UINT32 i = 0; i < windows.GetCount(); i++)
	{
		ChatDesktopWindow* window = static_cast<ChatDesktopWindow*>(windows.Get(i));
		
		if (window->IsCorrectChatRoom(account_id, name, is_room))
				return window;
	}
	
	return NULL;
}

#endif // M2_SUPPORT && IRC_SUPPORT
