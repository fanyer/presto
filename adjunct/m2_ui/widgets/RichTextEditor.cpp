/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#include "core/pch.h"

#ifdef M2_SUPPORT
#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "adjunct/desktop_util/string/htmlify_more.h"
#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2_ui/util/ComposeFontList.h"
#include "adjunct/m2_ui/widgets/RichTextEditor.h"
#include "adjunct/m2_ui/widgets/RichTextEditorListener.h"
#include "adjunct/m2_ui/dialogs/InsertLinkDialog.h"
#include "adjunct/m2_ui/dialogs/InsertHTMLDialog.h"
#include "adjunct/quick/dialogs/SelectFontDialog.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
// Core includes:
#include "modules/documentedit/OpDocumentEdit.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/dochand/docman.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/savewithinline.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/url/url_man.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/scope/src/scope_manager.h"

static const uni_char* plain_text_style=UNI_L("\
<STYLE type=\"text/css\">\
/* This styling is to make all HTML formatting look like plain text  */\
* {font: inherit; margin: 0; padding: 0; text-align: inherit; vertical-align: baseline; text-decoration: none; color: WindowText; background: Window; border: none;}\r\n \
p::after, ol:after, ul::after {content: \"\\A\\A\";}\r\n \
p + blockquote {margin: -2em 0 -1em;}\r\n \
ul ol::after, ul ul::after, ol ol::after, ol ul::after {content: normal;}\r\n \
img {display: none;}\r\n \
img[alt] {display: inline; content: \"[\" attr(alt) \"]\";}\r\n \
ul>li::before {content: \"* \";}\r\n \
ol { counter-reset: item;}\r\n \
li { display: block;}\r\n \
ol>li::before { content: counter(item) \". \"; counter-increment: item;}\r\n \
ol ol>li::before, ul ol>li::before { content: \"\\20\" counter(item) \". \"; counter-increment: item;}\r\n \
hr {display: block; height: 2em; vertical-align: middle; content: \"-----\";}\r\n \
/* handle forms as well */\r\n \
button, input {}\r\n \
fieldset, label {display: block;}\r\n \
select {}\r\n \
/* handle frames etc by hiding them! */\r\n \
frame, frameset, iframe, object, embed, applet {display: none;}\r\n \
/* handle tables */\r\n \
caption, table, tr {display: block;}\r\n \
th, td {display: inline;}\r\n \
th::after, td::after {display: inline; content: \"\\9\";}\r\n \
th:last-of-type::after, td:last-of-type::after {content: normal;}\r\n\
</STYLE>\r\n");

/***********************************************************************************
**
**	Construct
**
***********************************************************************************/
OP_STATUS RichTextEditor::Construct(RichTextEditor** obj)
{
	*obj = OP_NEW(RichTextEditor, ());
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/
OP_STATUS RichTextEditor::Init(DesktopWindow* parent_window, BOOL is_HTML, RichTextEditorListener* listener, UINT32 message_id)
{
	m_parent_window = parent_window;
	m_is_HTML_message = is_HTML;
	m_listener = listener;
	if (message_id == 0)
		m_message_id = rand()*10000;
	else
		m_message_id = message_id;

	RETURN_IF_ERROR(OpToolbar::Construct(&m_HTML_formatting_toolbar));
	AddChild(m_HTML_formatting_toolbar);

	m_HTML_formatting_toolbar->SetName("HTML Mail Formatting Toolbar");
	m_HTML_formatting_toolbar->GetBorderSkin()->SetImage("Mail Formatting Toolbar Skin", "Toolbar Skin");

	OpWindowCommander* window_commander;
	RETURN_IF_ERROR(g_windowCommanderManager->GetWindowCommander(&window_commander));
	RETURN_IF_ERROR(OpBrowserView::Construct(&m_HTML_compose_edit,TRUE,window_commander));
	AddChild(m_HTML_compose_edit);
	RETURN_IF_ERROR(m_HTML_compose_edit->init_status);
	window_commander->SetMailClientListener(this);

	m_HTML_compose_edit->GetWindow()->SetType(WIN_TYPE_MAIL_COMPOSE);
	m_HTML_compose_edit->GetBorderSkin()->SetImage("Mail Compose MultilineEdit Skin");
#ifndef _DEBUG
	m_HTML_compose_edit->SetEmbeddedInDialog(TRUE); // disables view source and security info
#endif //_DEBUG

	return InitFontDropdown();
}

/***********************************************************************************
**
**	InitFontDropdown
**
***********************************************************************************/
OP_STATUS RichTextEditor::InitFontDropdown()
{
	if (!m_HTML_formatting_toolbar)
		return OpStatus::ERR;

	//Initializes the list of fonts
	OpDropDown* font_dropdown = (OpDropDown*)m_HTML_formatting_toolbar->GetWidgetByName("Email_Set_Font");

	if (m_composefont_list.GetCount() != 0)
		m_composefont_list.DeleteAll();

	if (font_dropdown && font_dropdown->CountItems() == 0)
	{
		INT32 default_items=0;

		OpStringC face;
		OpString displayed_fontname, html_source_fontname;
		FontAtt font;

		// Add sans-serif
		face = g_pcdisplay->GetStringPref(PrefsCollectionDisplay::CSSFamilySansserif);
		RETURN_IF_ERROR(displayed_fontname.AppendFormat(UNI_L("%s (%s)"), face.CStr(), UNI_L("sans-serif")));
		RETURN_IF_ERROR(html_source_fontname.AppendFormat(UNI_L("'%s',%s"), face.CStr(), UNI_L("sans-serif")));
		RETURN_IF_ERROR(font_dropdown->AddItem(displayed_fontname.CStr(), default_items));
		RETURN_IF_ERROR(m_composefont_list.AddFont(html_source_fontname.CStr(), default_items));
		RETURN_IF_ERROR(m_composefont_list.AddFont(UNI_L("sans-serif"), default_items));
		
		OpInputAction *font_dropdown_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_EMAIL_SET_FONT));
		if (!font_dropdown_action)
			return OpStatus::ERR_NO_MEMORY;
		font_dropdown_action->SetActionDataString(html_source_fontname.CStr());
		font_dropdown_action->SetActionText(font_dropdown->GetItemText(default_items));
		font_dropdown_action->SetActionOperator(OpInputAction::OPERATOR_OR);

		font_dropdown->SetAction(font_dropdown_action);

		default_items++;
		
		// Add serif
		face = g_pcdisplay->GetStringPref(PrefsCollectionDisplay::CSSFamilySerif);
		displayed_fontname.Empty();
		html_source_fontname.Empty();
		RETURN_IF_ERROR(displayed_fontname.AppendFormat(UNI_L("%s (%s)"), face.CStr(), UNI_L("serif")));
		RETURN_IF_ERROR(html_source_fontname.AppendFormat(UNI_L("'%s',%s"), face.CStr(), UNI_L("serif")));
		RETURN_IF_ERROR(font_dropdown->AddItem(displayed_fontname.CStr(), default_items));
		RETURN_IF_ERROR(m_composefont_list.AddFont(UNI_L("serif"), default_items));
		RETURN_IF_ERROR(m_composefont_list.AddFont(html_source_fontname.CStr(), default_items));

		OpInputAction* next_font_dropdown_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_EMAIL_SET_FONT));
		if (!next_font_dropdown_action)
			return OpStatus::ERR_NO_MEMORY;
		next_font_dropdown_action->SetActionDataString(html_source_fontname.CStr());
		next_font_dropdown_action->SetActionText(font_dropdown->GetItemText(default_items));
		next_font_dropdown_action->SetActionOperator(OpInputAction::OPERATOR_OR);

		font_dropdown_action->SetNextInputAction(next_font_dropdown_action);
		font_dropdown_action = font_dropdown_action->GetNextInputAction();

		default_items++;
		
		// Add monospace
		face = g_pcdisplay->GetStringPref(PrefsCollectionDisplay::CSSFamilyMonospace);
		displayed_fontname.Empty();
		html_source_fontname.Empty();
		RETURN_IF_ERROR(displayed_fontname.AppendFormat(UNI_L("%s (%s)"), face.CStr(), UNI_L("monospace")));
		RETURN_IF_ERROR(html_source_fontname.AppendFormat(UNI_L("'%s',%s"), face.CStr(), UNI_L("monospace")));
		RETURN_IF_ERROR(font_dropdown->AddItem(displayed_fontname.CStr(), default_items));
		RETURN_IF_ERROR(m_composefont_list.AddFont(UNI_L("monospace"), default_items));
		RETURN_IF_ERROR(m_composefont_list.AddFont(html_source_fontname.CStr(), default_items));

		next_font_dropdown_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_EMAIL_SET_FONT));
		if (!next_font_dropdown_action)
			return OpStatus::ERR_NO_MEMORY;
		next_font_dropdown_action->SetActionDataString(html_source_fontname.CStr());
		next_font_dropdown_action->SetActionText(font_dropdown->GetItemText(default_items));
		next_font_dropdown_action->SetActionOperator(OpInputAction::OPERATOR_OR);

		font_dropdown_action->SetNextInputAction(next_font_dropdown_action);
		font_dropdown_action = font_dropdown_action->GetNextInputAction();

		default_items++;

		// Add cursive
		face = g_pcdisplay->GetStringPref(PrefsCollectionDisplay::CSSFamilyCursive);
		displayed_fontname.Empty();
		html_source_fontname.Empty();
		RETURN_IF_ERROR(displayed_fontname.AppendFormat(UNI_L("%s (%s)"), face.CStr(), UNI_L("cursive")));
		RETURN_IF_ERROR(html_source_fontname.AppendFormat(UNI_L("'%s',%s"), face.CStr(), UNI_L("cursive")));
		RETURN_IF_ERROR(font_dropdown->AddItem(displayed_fontname.CStr(), default_items));
		RETURN_IF_ERROR(m_composefont_list.AddFont(UNI_L("cursive"), default_items));
		RETURN_IF_ERROR(m_composefont_list.AddFont(html_source_fontname.CStr(), default_items));

		next_font_dropdown_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_EMAIL_SET_FONT));
		if (!next_font_dropdown_action)
			return OpStatus::ERR_NO_MEMORY;
		next_font_dropdown_action->SetActionDataString(html_source_fontname.CStr());
		next_font_dropdown_action->SetActionText(font_dropdown->GetItemText(default_items));
		next_font_dropdown_action->SetActionOperator(OpInputAction::OPERATOR_OR);

		font_dropdown_action->SetNextInputAction(next_font_dropdown_action);
		font_dropdown_action = font_dropdown_action->GetNextInputAction();

		default_items++;

		// Add fantasy
		face = g_pcdisplay->GetStringPref(PrefsCollectionDisplay::CSSFamilyFantasy);
		displayed_fontname.Empty();
		html_source_fontname.Empty();
		RETURN_IF_ERROR(displayed_fontname.AppendFormat(UNI_L("%s (%s)"), face.CStr(), UNI_L("fantasy")));
		RETURN_IF_ERROR(html_source_fontname.AppendFormat(UNI_L("'%s',%s"), face.CStr(), UNI_L("fantasy")));
		RETURN_IF_ERROR(font_dropdown->AddItem(displayed_fontname.CStr(), default_items));
		RETURN_IF_ERROR(m_composefont_list.AddFont(UNI_L("fantasy"), default_items));
		RETURN_IF_ERROR(m_composefont_list.AddFont(html_source_fontname.CStr(), default_items));

		next_font_dropdown_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_EMAIL_SET_FONT));
		if (!next_font_dropdown_action)
			return OpStatus::ERR_NO_MEMORY;
		next_font_dropdown_action->SetActionDataString(html_source_fontname.CStr());
		next_font_dropdown_action->SetActionText(font_dropdown->GetItemText(default_items));
		next_font_dropdown_action->SetActionOperator(OpInputAction::OPERATOR_OR);

		font_dropdown_action->SetNextInputAction(next_font_dropdown_action);
		font_dropdown_action = font_dropdown_action->GetNextInputAction();

		default_items++;

		OpString other_fonts;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_COMPOSE_OTHER_FONTS, other_fonts));
		RETURN_IF_ERROR(font_dropdown->BeginGroup(other_fonts.CStr()));

		int count_fonts=styleManager->GetFontManager()->CountFonts();

		for (int i=0; i<count_fonts; i++)
		{
			const OpFontInfo *item = styleManager->GetFontInfo(i);
			if (item)
			{
				const uni_char *name = item->GetFace();
				int num_items = font_dropdown->CountItems();
				int index;
				for (index=default_items; index<num_items; index++)
				{
					const uni_char* list_item = font_dropdown->GetItemText(index);
					if (list_item && uni_strcmp(name, list_item) < 0)
						break;
				}

				if (index == num_items)
					index = -1; // At end

				RETURN_IF_ERROR(font_dropdown->AddItem(name, index));
			}
		}

		font_dropdown->EndGroup();

		int num_items = font_dropdown->CountItems();
		for (int index = default_items; index < num_items; index++)
		{
			if (OpStatus::IsError(m_composefont_list.AddFont(font_dropdown->GetItemText(index), index)))
			{
				if (m_composefont_list.GetFontIndex(font_dropdown->GetItemText(index)) == -1)
					return OpStatus::ERR_NO_MEMORY;
				
				// the font was already there, so remove this duplicate entry and continue
				font_dropdown->RemoveItem(index);
				index--;
				num_items--;
				continue;
			}

			next_font_dropdown_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_EMAIL_SET_FONT));
			if (!next_font_dropdown_action)
				return OpStatus::ERR_NO_MEMORY;
			next_font_dropdown_action->SetActionDataString(font_dropdown->GetItemText(index));
			next_font_dropdown_action->SetActionText(font_dropdown->GetItemText(index));
			next_font_dropdown_action->SetActionOperator(OpInputAction::OPERATOR_OR);

			font_dropdown_action->SetNextInputAction(next_font_dropdown_action);
			font_dropdown_action = font_dropdown_action->GetNextInputAction();
		}
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL RichTextEditor::OnInputAction(OpInputAction* action)
{
	if (!m_HTML_compose_edit)
		return FALSE;

	switch (action->GetAction())
	{
    	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			if (!m_document_edit)
			{
				 switch(child_action->GetAction())
			    {
					 case OpInputAction::ACTION_EMAIL_TOGGLE_BOLD:
			        case OpInputAction::ACTION_EMAIL_TOGGLE_ITALIC:
			        case OpInputAction::ACTION_EMAIL_TOGGLE_UNDERLINE:
			        case OpInputAction::ACTION_EMAIL_TOGGLE_STRIKETHROUGH:
			        case OpInputAction::ACTION_EMAIL_CLEAR_FORMATTING:
			        case OpInputAction::ACTION_EMAIL_ADD_LINK:
			        case OpInputAction::ACTION_EMAIL_INSERT_HTML:
			        case OpInputAction::ACTION_EMAIL_INSERT_PICTURE:
			        case OpInputAction::ACTION_EMAIL_INSERT_HORIZONTAL_RULE:
			        case OpInputAction::ACTION_EMAIL_SET_FONT:
			        case OpInputAction::ACTION_OPEN_FONT_DIALOG:
			        case OpInputAction::ACTION_EMAIL_INCREASE_TEXT_SIZE:
			        case OpInputAction::ACTION_EMAIL_DECREASE_TEXT_SIZE:
			        case OpInputAction::ACTION_EMAIL_SET_COLOR:
			        case OpInputAction::ACTION_EMAIL_SET_SUPERSCRIPT:
			        case OpInputAction::ACTION_EMAIL_SET_SUBSCRIPT:
			        case OpInputAction::ACTION_EMAIL_INDENT:
			        case OpInputAction::ACTION_EMAIL_OUTDENT:
			        case OpInputAction::ACTION_EMAIL_SET_ORDERED_LIST:
			        case OpInputAction::ACTION_EMAIL_SET_UNORDERED_LIST:
			        case OpInputAction::ACTION_EMAIL_ALIGN_LEFT:
			        case OpInputAction::ACTION_EMAIL_ALIGN_CENTER:
			        case OpInputAction::ACTION_EMAIL_ALIGN_RIGHT:
			        case OpInputAction::ACTION_EMAIL_ALIGN_JUSTIFY:
					 {
						 child_action->SetEnabled(m_is_HTML_message);
			            return TRUE;
					 }
			        default:
			            break;
			    }
			}			
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_TOGGLE_HTML_EMAIL: 
                { 
                    child_action->SetSelected(m_is_HTML_message == child_action->GetActionData()); 
                    return TRUE; 
                } 
				case OpInputAction::ACTION_EMAIL_TOGGLE_BOLD:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_BOLD));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_TOGGLE_ITALIC:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_ITALIC));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_TOGGLE_UNDERLINE:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_UNDERLINE));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_TOGGLE_STRIKETHROUGH:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_STRIKETHROUGH));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_CLEAR_FORMATTING: 
                { 
                    child_action->SetEnabled(m_is_HTML_message); 
                    return TRUE; 
                } 
				case OpInputAction::ACTION_EMAIL_ADD_LINK:
				{
					child_action->SetEnabled(m_is_HTML_message);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_INSERT_HTML:
				{
					child_action->SetEnabled(m_is_HTML_message);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_INSERT_PICTURE:
				{
					child_action->SetEnabled(m_is_HTML_message);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_INSERT_HORIZONTAL_RULE:
				{
					child_action->SetEnabled(m_is_HTML_message);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_SET_FONT:
				{
					if (!m_is_HTML_message)
						child_action->SetEnabled(FALSE);
					else
						child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_OPEN_FONT_DIALOG:
				{
					child_action->SetEnabled(m_is_HTML_message);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_INCREASE_TEXT_SIZE:
				case OpInputAction::ACTION_EMAIL_DECREASE_TEXT_SIZE:
				{
					if (m_is_HTML_message)
					{
						int current_size=m_document_edit->GetFontSize();
						if (child_action->GetAction() == OpInputAction::ACTION_EMAIL_INCREASE_TEXT_SIZE)
						{
							child_action->SetEnabled(current_size < 7);
						}
						else
						{
							child_action->SetEnabled(current_size > 1);
						}
					}
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_SET_COLOR:
				{
					child_action->SetEnabled(m_is_HTML_message);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_SET_SUPERSCRIPT:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_SUPERSCRIPT));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_SET_SUBSCRIPT:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_SUBSCRIPT));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_INDENT:
				{
					if (!m_is_HTML_message)
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_OUTDENT:
				{
					if (!m_is_HTML_message)
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_SET_ORDERED_LIST:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_INSERTORDEREDLIST));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_SET_UNORDERED_LIST:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_INSERTUNORDEREDLIST));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_ALIGN_LEFT:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_JUSTIFYLEFT));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_ALIGN_CENTER:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_JUSTIFYCENTER));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_ALIGN_RIGHT:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_JUSTIFYRIGHT));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_EMAIL_ALIGN_JUSTIFY:
				{
					if (m_is_HTML_message)
						child_action->SetSelected(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_JUSTIFYFULL));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_CHANGE_DIRECTION_AUTOMATIC:
				{				
					child_action->SetSelected(m_autodetect_direction);
					child_action->SetEnabled(!m_autodetect_direction);
					return TRUE;
				}
				case OpInputAction::ACTION_CHANGE_DIRECTION_TO_LTR:
				{
					child_action->SetEnabled(m_is_rtl);
					return TRUE;
				}
				case OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL:
				{
					child_action->SetEnabled(!m_is_rtl);
					return TRUE;
				}
            }
			break;
        }
		case OpInputAction::ACTION_TOGGLE_HTML_EMAIL: 
		{ 
			if (action->GetActionData() == m_is_HTML_message)
				return FALSE;
			
			SwitchHTML(); 
			return TRUE; 
		}
		case OpInputAction::ACTION_CHANGE_DIRECTION_AUTOMATIC:
		{
			SetDirection(AccountTypes::DIR_AUTOMATIC,TRUE);
			return TRUE;
		}
		case OpInputAction::ACTION_CHANGE_DIRECTION_TO_LTR:
		{
			SetDirection(AccountTypes::DIR_LEFT_TO_RIGHT,TRUE);
			return TRUE;
		}
		case OpInputAction::ACTION_CHANGE_DIRECTION_TO_RTL:
		{
			SetDirection(AccountTypes::DIR_RIGHT_TO_LEFT,TRUE);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_TOGGLE_BOLD:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_BOLD);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_TOGGLE_ITALIC:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_ITALIC);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_TOGGLE_UNDERLINE:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_UNDERLINE);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_TOGGLE_STRIKETHROUGH:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_STRIKETHROUGH);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_CLEAR_FORMATTING: 
        { 
            if (m_is_HTML_message) 
                m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_REMOVEFORMAT); 
            return TRUE; 
        } 
		case OpInputAction::ACTION_EMAIL_INSERT_HORIZONTAL_RULE:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_INSERTHORIZONTALRULE);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_INSERT_HTML:
		{
			if (m_is_HTML_message)
			{
				InsertHTMLDialog* dialog = OP_NEW(InsertHTMLDialog, ());
				if (dialog)
					dialog->Init(m_parent_window,this);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_ADD_LINK:
		{
			if (m_is_HTML_message)
			{
				OpString text;
				m_HTML_compose_edit->GetWindow()->GetCurrentDoc()->GetSelectedText(text);
				InsertLinkDialog* dialog = OP_NEW(InsertLinkDialog, ());
				if (dialog)
					dialog->Init(m_parent_window,this,text);
			}

			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_INSERT_PICTURE:
		{
			if (m_is_HTML_message)
			{
				m_listener->OnInsertImage();
			}
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_SET_FONT:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_FONTNAME, FALSE, action->GetActionDataString());
			return TRUE;
		}
		case OpInputAction::ACTION_OPEN_FONT_DIALOG:
		{
			if (m_is_HTML_message)
			{
				FontAtt font;
				font.SetFaceName(m_document_edit->GetFontFace());
				font.SetSize(m_document_edit->GetFontSize());
				font.SetWeight(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_BOLD)? 7 : 4);
				font.SetItalic(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_ITALIC));
				font.SetUnderline(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_UNDERLINE));
				font.SetStrikeOut(m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_STRIKETHROUGH));
				SelectFontDialog *dialog = 0;

				dialog = OP_NEW(SelectMailComposeFontDialog,(font, this));
				if (dialog)
					dialog->Init(m_parent_window);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_INCREASE_TEXT_SIZE:
		{
			if (m_is_HTML_message)
			{
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_INCREASEFONTSIZE);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_DECREASE_TEXT_SIZE:
		{
			if (m_is_HTML_message)
			{
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_DECREASEFONTSIZE);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_SET_COLOR:
		{
			if (m_is_HTML_message)
			{
				COLORREF color = 0;

				OpColorChooser* chooser = NULL;

				OpWidget * color_widget = m_HTML_formatting_toolbar->GetWidgetByName("Email_Set_Color");
				if (color_widget)
					color = color_widget->GetBackgroundColor(color);

				if (OpStatus::IsSuccess(OpColorChooser::Create(&chooser)))
				{
					chooser->Show(color, this, m_parent_window);
					OP_DELETE(chooser);
				}
			}

			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_SET_SUPERSCRIPT:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_SUPERSCRIPT);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_SET_SUBSCRIPT:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_SUBSCRIPT);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_INDENT:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_INDENT);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_OUTDENT:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_OUTDENT);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_SET_ORDERED_LIST:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_INSERTORDEREDLIST);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_SET_UNORDERED_LIST:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_INSERTUNORDEREDLIST);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_ALIGN_LEFT:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_JUSTIFYLEFT);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_ALIGN_CENTER:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_JUSTIFYCENTER);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_ALIGN_RIGHT:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_JUSTIFYRIGHT);
			return TRUE;
		}
		case OpInputAction::ACTION_EMAIL_ALIGN_JUSTIFY:
		{
			if (m_is_HTML_message)
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_JUSTIFYFULL);
			return TRUE;
		}
		case OpInputAction::ACTION_INSERT:
		{
			OpString text;
			if (OpStatus::IsSuccess(text.Set(XHTMLify_string_with_BR(action->GetActionDataString(), uni_strlen(action->GetActionDataString()), FALSE, FALSE, TRUE, TRUE).CStr()))) 
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_INSERTHTML,FALSE,text.CStr());
			return TRUE;
		}
	}
	return m_HTML_compose_edit->OnInputAction(action);
}


/***********************************************************************************
**
**	InsertHTML
**
***********************************************************************************/

void RichTextEditor::InsertHTML(OpString& source_code)
{
	if (m_is_HTML_message)
		m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_INSERTHTML,FALSE,source_code.CStr());
}

/***********************************************************************************
**
**	InsertLink
**
***********************************************************************************/

void RichTextEditor::InsertLink(OpString& title, OpString& address)
{
	if (m_is_HTML_message)
	{
		if (!m_document_edit->queryCommandEnabled(OP_DOCUMENT_EDIT_COMMAND_CREATELINK))
		{
			OpString url_HTML;
			if (OpStatus::IsSuccess(url_HTML.AppendFormat(UNI_L("<a href=\"%s\">%s</a>"),address.CStr(), title.HasContent() ? title.CStr() : address.CStr())))
				m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_INSERTHTML,FALSE,url_HTML.CStr());

		}
		else
		{
			m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_CREATELINK,FALSE,address.CStr());
		}
	}
}

/***********************************************************************************
**
**	InsertImage
**
***********************************************************************************/

void RichTextEditor::InsertImage(OpString& image_url)
{
	if (m_is_HTML_message)
	{
		m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_INSERTIMAGE, FALSE, image_url.CStr());
	}
}

/***********************************************************************************
**
**	SetFocusToBody
**
***********************************************************************************/

void RichTextEditor::SetFocusToBody()
{
	m_HTML_compose_edit->FocusDocument(FOCUS_REASON_OTHER);
}

/***********************************************************************************
**
**	OnFontSelected
**
***********************************************************************************/

void RichTextEditor::OnFontSelected(FontAtt& font, COLORREF color)
{
	if (m_is_HTML_message)
	{
		if (font.GetUnderline() != m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_UNDERLINE))
			m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_UNDERLINE);
		if (font.GetItalic() != m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_ITALIC))
			m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_ITALIC);
		if ((font.GetWeight() > 4) != m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_BOLD))
			m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_BOLD);
		if (font.GetStrikeOut() != m_document_edit->queryCommandState(OP_DOCUMENT_EDIT_COMMAND_STRIKETHROUGH))
			m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_STRIKETHROUGH);
		m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_FONTNAME,FALSE,font.GetFaceName());
		m_document_edit->InsertFontSize(font.GetSize());
		OnColorSelected(color);
	}

}

/***********************************************************************************
**
**	SetDirection
**
***********************************************************************************/

void RichTextEditor::SetDirection(int direction, BOOL update_body, BOOL force_keep_auto_detecting)
{
	OpWidget *widget = GetFocused();
	SetFocusToBody();
	m_autodetect_direction = force_keep_auto_detecting || direction == AccountTypes::DIR_AUTOMATIC;

	switch (direction)
	{
		case AccountTypes::DIR_AUTOMATIC:
			if (m_document_edit)
				m_document_edit->SetAutodetectDirection(TRUE);
			break;
		case AccountTypes::DIR_LEFT_TO_RIGHT:
			//normally set auto detect direction to false, except if we override it
			if (m_document_edit)
			{
				m_document_edit->SetAutodetectDirection(force_keep_auto_detecting);
				m_document_edit->SetRTL(FALSE);
			}
			m_is_rtl = FALSE;
			break;
		case AccountTypes::DIR_RIGHT_TO_LEFT:
			//normally set auto detect direction to false, except if we override it
			if (m_document_edit)
			{
				m_document_edit->SetAutodetectDirection(force_keep_auto_detecting);
				m_document_edit->SetRTL(TRUE);
			}
			m_is_rtl = TRUE;
			break;
	}
	if (widget)
		widget->SetFocus(FOCUS_REASON_OTHER);
}


/***********************************************************************************
**
**	GetTextEquivalent
**
***********************************************************************************/

OP_STATUS RichTextEditor::GetTextEquivalent(OpString& equivalent) const
{
	equivalent.Empty();

	if (m_document_edit)
	{
		return m_document_edit->GetText(equivalent, TRUE);
	}
	else
	{
		return OpStatus::ERR;
	}


}


/***********************************************************************************
**
**	GetEncodedHTMLPart
**
***********************************************************************************/

OP_STATUS RichTextEditor::GetEncodedHTMLPart(OpAutoPtr<Upload_Multipart>& html_multipart, const OpStringC8 &charset_to_use)
{
	// Update the HTML source in the URL before getting the HTML part
	if (!m_document_edit)
		return OpStatus::ERR;

	OpString html_part;
	RETURN_IF_ERROR(m_document_edit->GetTextHTMLFromElement(html_part, m_document_edit->GetRoot(), FALSE));

	// we want to stuff the content in a new URL with the same context id so that the images (with content id URLs) are accessible
	URL temp_url_with_mail_content;
	OpString generated_url;
	RETURN_IF_ERROR(generated_url.AppendFormat(UNI_L("draft://%d/%d"),m_message_id,m_draft_number++));
	temp_url_with_mail_content = urlManager->GetURL(generated_url,m_context_id);

	RETURN_IF_ERROR(SetMailContentToURL(html_part, temp_url_with_mail_content, FALSE, charset_to_use));
	RETURN_IF_LEAVE(temp_url_with_mail_content.SetAttributeL(URL::KMIME_CharSet,charset_to_use));
	// Then get the HTML part with inlined attachments
	html_multipart = OP_NEW(Upload_Multipart, ());
	if (!html_multipart.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_LEAVE(SaveAsArchiveHelper::GetArchiveL(temp_url_with_mail_content, *html_multipart, NULL, TRUE));
		
	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetMailContent
**
***********************************************************************************/

OP_STATUS RichTextEditor::GetHTMLSource(OpString &content, BOOL body_only) const
{
	if (!m_document_edit)
		return OpStatus::ERR;
	
	if (!body_only)
		return m_document_edit->GetTextHTMLFromElement(content, m_document_edit->GetRoot(), FALSE);

	LogicalDocument* log_doc =m_HTML_compose_edit->GetWindow()->GetCurrentDoc()->GetLogicalDocument();

	if (log_doc && log_doc->GetRoot())
	{
		if (log_doc->GetRoot()->IsDirty())
			m_HTML_compose_edit->GetWindow()->GetCurrentDoc()->Reflow(TRUE, TRUE, FALSE);
		HTML_Element* body_elm = log_doc->GetBodyElm();
		if (body_elm)
		{
			TempBuffer tmp_buff;
			RETURN_IF_ERROR(body_elm->AppendEntireContentAsString(&tmp_buff, FALSE, FALSE));
			return content.Set(tmp_buff.GetStorage());
		}
	}
	return OpStatus::ERR;
}


/***********************************************************************************
**
**	OnColorSelected
**
***********************************************************************************/

void RichTextEditor::OnColorSelected(COLORREF color)
{
	const uni_char* sharp= UNI_L("#");
	uni_char color_hex[8];
	uni_sprintf(color_hex+1, UNI_L("%06X"), color & 0xFFFFFF);
	color_hex[0]=color_hex[5];
	color_hex[5]=color_hex[1];
	color_hex[1]=color_hex[0];
	color_hex[0]=color_hex[6];
	color_hex[6]=color_hex[2];
	color_hex[2]=color_hex[0];
	color_hex[0]=sharp[0];

	m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_FORECOLOR, FALSE, color_hex);
}

/*********************************************************************************** 
**	SetPlainTextBody
**  Used with mailto's and "send link by mail", to set the body content
** 
***********************************************************************************/ 
 
OP_STATUS RichTextEditor::SetPlainTextBody(const OpStringC& plaintext_body)
{
	OpString body, sig;
	if (m_is_HTML_message)
		RETURN_IF_ERROR(body.Set(XHTMLify_string_with_BR(plaintext_body.CStr(),plaintext_body.Length(),FALSE, FALSE, TRUE, FALSE).CStr()));
	else
		RETURN_IF_ERROR(body.Set(plaintext_body.CStr()));
	
	RETURN_IF_ERROR(SetMailContentAndUpdateView(body, TRUE, 0));

	OnTextChanged();
	return OpStatus::OK;
}

/*********************************************************************************** 
** 
**  Set the mail content to the URL in parameter 
** 
***********************************************************************************/ 
 
OP_STATUS RichTextEditor::SetMailContentToURL(const OpStringC &content, URL& url, BOOL body_only, const OpStringC8 &charset)
{ 
    OpString final_content; 
    OpString body_elm; 
    RETURN_IF_ERROR(final_content.Set(content)); 

	OpString face;
	INT32 font_size;
	RETURN_IF_ERROR(GetDefaultFont(face, font_size));
	
	OpString formatted_signature;
	if (m_update_signature && m_signature.HasContent())
	{
		RETURN_IF_ERROR(formatted_signature.AppendFormat(UNI_L("<DIV id=\"M2Signature\">%s</DIV>"), m_signature.CStr()));
	}
    if(!m_is_HTML_message) 
    { 
        if (body_only) 
        { 
            RETURN_IF_ERROR(final_content.Set(XHTMLify_string_with_BR(final_content.CStr(), final_content.Length(), FALSE, FALSE, TRUE, FALSE).CStr())); 
            RETURN_IF_ERROR(final_content.Insert(0,"<DIV>")); 
            RETURN_IF_ERROR(final_content.Append("</DIV>")); 
        } 
        OpString head_section;
		RETURN_IF_ERROR(head_section.AppendFormat(UNI_L("<!DOCTYPE html><HTML>\r\n<HEAD>\r\n%s</HEAD>\r\n<BODY %sstyle=\"margin: 4px; padding: 0; white-space: pre-wrap; font-family:'%s'; font-size:%dpx\">"),plain_text_style, m_is_rtl? UNI_L("dir=\"rtl\" "): UNI_L(""),face.CStr(),font_size));
		RETURN_IF_ERROR(final_content.Insert(0,head_section));

    } 
    else if(body_only) 
    { 
		OpString head_section;
		RETURN_IF_ERROR(head_section.AppendFormat(UNI_L("<!DOCTYPE html>\r\n<HTML><HEAD><STYLE type=\"text/css\">body { font-family:'%s'; font-size:%dpx}</STYLE></HEAD><BODY%s>"), face.CStr(), font_size, m_is_rtl? UNI_L(" dir=\"rtl\""): UNI_L("")));
		RETURN_IF_ERROR(final_content.Insert(0,head_section));
    } 

	if (m_update_signature && m_signature.HasContent())
		RETURN_IF_ERROR(final_content.Append(formatted_signature));
	
	if (body_only)
		RETURN_IF_ERROR(final_content.Append("</BODY></HTML>")); 
 
    url.Unload(); 
 
    RETURN_IF_ERROR(url.SetAttribute(URL::KMIME_ForceContentType, "text/html")); 
    RETURN_IF_ERROR(url.SetAttribute(URL::KMIME_CharSet,charset.CStr())); 
    RETURN_IF_ERROR(url.SetAttribute(URL::KIsGeneratedByOpera, TRUE)); 
    RETURN_IF_ERROR(url.SetAttribute(URL::KCachePolicy_NoStore, TRUE)); 
	RETURN_IF_ERROR(url.SetAttribute(URL::KSuppressScriptAndEmbeds, g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SuppressExternalEmbeds)));
 
    OpString8 final_content_8;
	RETURN_IF_LEAVE(SetToEncodingL(&final_content_8, charset.CStr(), final_content.CStr(), final_content.Length()));
 
    RETURN_IF_ERROR(url.WriteDocumentData(URL::KNormal, final_content_8)); 
    url.WriteDocumentDataFinished(); 
 
	return OpStatus::OK;
} 
 
 
/*********************************************************************************** 
** 
**  Set the mail content and update the compose window 
** 
***********************************************************************************/ 
OP_STATUS RichTextEditor::SetMailContentAndUpdateView(const OpStringC &content, BOOL body_only, URL_CONTEXT_ID context_id)
{ 
    URL url; 
    OpString8 charset; 
	m_document_edit = NULL;
	
	if (context_id == 0)
	{
		context_id = m_context_id;
	}
	else
	{
		m_context_id = context_id;
	}

    // The window should use utf-8 to be able to represent all characters that a user can input,  
    //the actual charset used and sent, is determined another place 
    RETURN_IF_ERROR(charset.Set("utf-8")); 
     
    OpString generated_url; 
    RETURN_IF_ERROR(generated_url.AppendFormat(UNI_L("opera:draft_%d/%d"),m_message_id,m_draft_number++)); 
    // if the context id is != 0 then use that context_id for the url 
    url = urlManager->GetURL(generated_url,context_id); 
	
	m_context_id = url.GetContextId();
 
    RETURN_IF_ERROR(SetMailContentToURL(content,url,body_only,charset));
    URL dummy; 
 
    // Open the URL in the compose window 
 	DocumentManager::OpenURLOptions options;
	options.create_doc_now = TRUE;
	options.es_terminated = TRUE;
	options.entered_by_user = NotEnteredByUser;

	m_HTML_compose_edit->GetWindow()->DocManager()->OpenURL(url, DocumentReferrer(dummy), FALSE, FALSE, options);	
    m_HTML_compose_edit->GetWindow()->GetCurrentDoc()->GetVisualDevice()->SetParentInputContext(this); 
 
	if (m_parent_window)
		m_parent_window->BroadcastDesktopWindowContentChanged();

	m_HTML_compose_edit->GetWindowCommander()->SetLoadingListener(this);
	m_HTML_compose_edit->GetWindowCommander()->SetLayoutMode(OpWindowCommander::NORMAL);

	OnCaretMoved();

    return OpStatus::OK; 
} 

/***********************************************************************************
**
**	SwitchHTML
**
***********************************************************************************/

void RichTextEditor::SwitchHTML(BOOL update_view)
{
	m_is_HTML_message = !m_is_HTML_message;
	if (update_view)
	{
		OpString content;
		if (OpStatus::IsSuccess(GetHTMLSource(content, TRUE)))
		{
			OpStatus::Ignore(SetMailContentAndUpdateView(content, m_is_HTML_message));
			OnTextChanged();
		}
	}
}

/***********************************************************************************
**
**	SetSignature
**
***********************************************************************************/

OP_STATUS RichTextEditor::SetSignature(const OpStringC signature, BOOL is_HTML_signature, BOOL update_view)
{
	OpString current_sig;
	if (m_document_edit && 
		OpStatus::IsSuccess(m_document_edit->GetTextHTMLFromNamedElement(current_sig, UNI_L("M2Signature"), FALSE)))
	{
		// only replace the signature if there is a signature already, we don't want to add a new one
		// if it's the same one as the old one (the user hasn't changed it) -> we replace the signature
		if (m_signature.Compare(current_sig.CStr())!= 0)
			return OpStatus::OK;
		// delete the HTML_Element containing the signature
		m_document_edit->DeleteNamedElement(UNI_L("M2Signature"));
	}

	if (signature.IsEmpty())
		return OpStatus::OK;

	// don't insert -- \r\n if it exists already (DSK-246355) (only for plaintext signatures)
	BOOL has_sig_separator = !is_HTML_signature && (signature.Find(UNI_L("-- \n")) != KNotFound || signature.Find(UNI_L("-- \r\n")) != KNotFound);

	OpString htmlified_sig;
	if (!is_HTML_signature)
		RETURN_IF_ERROR(htmlified_sig.Set(XHTMLify_string_with_BR(signature.CStr(), signature.Length(), FALSE, FALSE, TRUE, FALSE).CStr()));
	else
		RETURN_IF_ERROR(htmlified_sig.Set(signature));

	m_signature.Empty();
	if (!has_sig_separator)
		RETURN_IF_ERROR(m_signature.AppendFormat(UNI_L("<DIV>-- </DIV><DIV>%s</DIV>"),htmlified_sig.CStr()));
	else
		RETURN_IF_ERROR(m_signature.AppendFormat(UNI_L("<DIV>%s</DIV>"),htmlified_sig.CStr()));

	if (update_view && m_signature.HasContent())
	{
		OpString mail_content;

		RETURN_IF_ERROR(GetHTMLSource(mail_content,TRUE));

		m_update_signature = TRUE;

		// Update the URL and make sure it adds the new signature
		RETURN_IF_ERROR(SetMailContentAndUpdateView(mail_content,m_is_HTML_message,0));
	}

	return OpStatus::OK;
}


/***********************************************************************************
**
**	SetAvailableRect
**
***********************************************************************************/

void RichTextEditor::OnLayout()
{
	OpRect rect = GetBounds();

	INT32 formatting_toolbar_height = 0;

	if (m_HTML_formatting_toolbar)
	{
		if (m_is_HTML_message || !m_HTML_formatting_toolbar->GetAutoAlignment())
			formatting_toolbar_height = m_HTML_formatting_toolbar->GetHeightFromWidth(rect.width);
		m_HTML_formatting_toolbar->SetRect(OpRect(0,rect.y, rect.width, formatting_toolbar_height));
	}

	rect.width += 2; // Align scrollbar with growbox
	if (m_HTML_compose_edit)
		m_HTML_compose_edit->SetRect(OpRect(0, rect.y + formatting_toolbar_height, rect.width, rect.height - formatting_toolbar_height));
}

/***********************************************************************************
**
**	SetEmbeddedInDialog
**
***********************************************************************************/

void RichTextEditor::SetEmbeddedInDialog()
{
	m_HTML_formatting_toolbar->SetAutoAlignment(FALSE);
	m_HTML_formatting_toolbar->SetAlignment(OpBar::ALIGNMENT_TOP);
	m_HTML_formatting_toolbar->GetBorderSkin()->SetImage("", "");
}

/***********************************************************************************
**
**	GetPreferredWidth
**
***********************************************************************************/

unsigned RichTextEditor::GetPreferredWidth()
{
	INT32 width, height;
	m_HTML_formatting_toolbar->GetRequiredSize(width, height);
	return width;
}

/***********************************************************************************
**
**	OnLoadingFinished
**
***********************************************************************************/

void RichTextEditor::OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status)
{
	// Set document editable to TRUE 
	         
	if (OpStatus::IsError(m_HTML_compose_edit->GetWindow()->GetCurrentDoc()->SetEditable(FramesDocument::DESIGNMODE)))
		return;
	m_document_edit = m_HTML_compose_edit->GetWindow()->GetCurrentDoc()->GetDocumentEdit();
	m_document_edit->SetListener(this); 
	m_document_edit->SetSplitBlockquote(TRUE); 
	m_document_edit->execCommand(OP_DOCUMENT_EDIT_COMMAND_DEFAULT_BLOCK, FALSE, UNI_L("div"));
	if (m_is_HTML_message)
	{
		m_document_edit->SetPlainTextMode(FALSE);
		m_document_edit->SetWantsTab(FALSE); 
	}
	else 
	{
	    m_document_edit->SetPlainTextMode(TRUE);
	    m_document_edit->SetWantsTab(TRUE);
	}

	m_document_edit->SetAutodetectDirection(m_autodetect_direction);
	 	
	// we want to store the signature as it is returned from core
	// core returns a strange mix of source but with non-escaped characters that is different than what we give them
	// eg. we give them -- <br>&quot;quoted text&quot; and get back -- <br> "quoted text"
	// we compare signatures before we remove them, so they need to be exactly the same
	if (m_update_signature)
	{
		OpString current_sig;
		if (OpStatus::IsSuccess(m_document_edit->GetTextHTMLFromNamedElement(current_sig, UNI_L("M2Signature"), FALSE)))
		{
			OpStatus::Ignore(m_signature.Set(current_sig.CStr()));
		}

		m_update_signature = FALSE;
	}

	g_scope_manager->desktop_window_manager->PageLoaded(m_parent_window);
}

/***********************************************************************************
**
**	GetDefaultFont
**
***********************************************************************************/

OP_STATUS RichTextEditor::GetDefaultFont(OpString& default_font, INT32 &font_size) const
{
	FontAtt mailfont;
	g_pcfontscolors->GetFont(m_is_HTML_message ? OP_SYSTEM_FONT_HTML_COMPOSE : OP_SYSTEM_FONT_EMAIL_COMPOSE, mailfont); 
	font_size = mailfont.GetSize();
#ifdef HAS_FONT_FOUNDRY 
	OpFontManager* fontManager = styleManager->GetFontManager(); 
	return fontManager->GetFamily(mailfont.GetFaceName(), default_font); 
#else 
	return default_font.Set(mailfont.GetFaceName());
#endif //HAS_FONT_FOUNDRY 
}


/***********************************************************************************
**
**	UpdateFontDropdownSelection
**
***********************************************************************************/

OP_STATUS RichTextEditor::UpdateFontDropdownSelection()
{
	if (m_is_HTML_message)
	{
		TempBuffer font;

		if (m_document_edit)
			OpStatus::Ignore(m_document_edit->GetFontFace(font)); // we have fallbacks, return error if they fail
		
		// if still empty, it means either that the documentedit hasn't reflowed or it isn't focused
		// fallback to default
		if (font.Length() == 0)
		{
			INT32 size;
			OpString sfont;
			RETURN_IF_ERROR(GetDefaultFont(sfont, size));
			RETURN_IF_ERROR(font.Append(sfont));
		}

		OpDropDown* font_dropdown = (OpDropDown*)m_HTML_formatting_toolbar->GetWidgetByName("Email_Set_Font");
		if (font_dropdown)
		{
			INT32 index = m_composefont_list.GetFontIndex(font.GetStorage());
			font_dropdown->SelectItem(index, TRUE);
		}
	}
	return OpStatus::OK;
}
#endif //M2_SUPPORT
