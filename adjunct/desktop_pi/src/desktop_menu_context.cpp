/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/quick/Application.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpWidget.h"

DesktopMenuContext::DesktopMenuContext()
	: m_context(NULL)
	, m_file_url_info(NULL)
	, m_desktop_window(NULL)
	, m_message(NULL)
	, m_window_commander(NULL)
{
}

DesktopMenuContext::~DesktopMenuContext()
{
	g_application->SetLastSeenDocumentContext(NULL);
	OP_DELETE(m_context);
	if (m_file_url_info)
		m_file_url_info->URL_Information_Done();
	OP_DELETE(m_message);
}

void DesktopMenuContext::Init(URLInformation* file_url_info)
{
	m_file_url_info = file_url_info;
}

OP_STATUS DesktopMenuContext::Init(OpWidget * widget, const OpPoint & pos)
{
	return m_widget_context.Init(widget, pos);
}

OpPopupMenuRequestMessage* DesktopMenuContext::CopyMessage(const OpPopupMenuRequestMessage& message)
{
	/*
	This is a temporary solution until we get either reference counted messages or a proper copy
	call in the message itself. The proper solution will presumably be introduced with multiprocess.
	*/
	OpData serialized_message;
	message.Serialize(serialized_message);
	return OpPopupMenuRequestMessage::Deserialize(OpMessageAddress(), OpMessageAddress(), 0, serialized_message);
}

OP_STATUS DesktopMenuContext::Init(OpWindowCommander* commander, OpDocumentContext* context, const OpPopupMenuRequestMessage* message)
{
	m_window_commander = commander;
	m_context = context->CreateCopy();
	RETURN_OOM_IF_NULL(m_context);
	g_application->SetLastSeenDocumentContext(m_context);

	// clone OpPopupMenuRequestMessage
	if (message != NULL)
	{
		m_message = CopyMessage(*message);
		RETURN_OOM_IF_NULL(m_message);
	}

	return OpStatus::OK;
}

void DesktopMenuContext::SetContext(OpDocumentContext* context)
{
	OP_ASSERT(!m_context || context == m_context || !"We'll leak something");
	m_context = context;
	g_application->SetLastSeenDocumentContext(context);
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
int DesktopMenuContext::GetSpellSessionID()
{
	if (m_context)
	{
		return m_context->GetSpellSessionId();
	}
	else if (m_widget_context.IsInitialized())
	{
		return m_widget_context.m_spell_session_id;
	}
	return 0;
}
#endif // INTERNAL_SPELLCHECK_SUPPORT

OP_STATUS DesktopMenuContext::WidgetContext::Init(OpWidget * widget, const OpPoint & pos)
{
	if (!widget)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	SetScreenPosFromWidget(widget, pos);

	m_widget_type = widget->GetType();
	m_packed.is_form_element = widget->IsForm();
	m_packed.is_enabled = widget->IsEnabled();

	m_packed.has_text_selection = widget->HasSelectedText();
	RETURN_IF_ERROR(SetSelectedTextFromWidget(widget));

	switch (widget->GetType())
	{
	case OpTypedObject::WIDGET_TYPE_EDIT:
		{
			OpEdit * edit = static_cast<OpEdit *>(widget);
			m_packed.is_read_only = edit->IsReadOnly();

#ifdef INTERNAL_SPELLCHECK_SUPPORT 
			OpPoint p(pos);
			m_spell_session_id = edit->CreateSpellSessionId(&p);
#endif // INTERNAL_SPELLCHECK_SUPPORT 
		}
		break;
	case OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT:
		{
			OpMultilineEdit * edit = static_cast<OpMultilineEdit *>(widget);
			m_packed.is_read_only = edit->IsReadOnly();

#ifdef INTERNAL_SPELLCHECK_SUPPORT 
			OpPoint p(pos);
			m_spell_session_id = edit->CreateSpellSessionId(&p);
#endif // INTERNAL_SPELLCHECK_SUPPORT 
		}
		break;
		case OpTypedObject::WIDGET_TYPE_LABEL:
		{
			// We don't want context menus on labels
			m_packed.is_enabled = FALSE;
			m_packed.is_read_only = TRUE;
			break;
		}

	};

	m_packed.is_initialized = TRUE;
	return OpStatus::OK;
}

OP_STATUS DesktopMenuContext::WidgetContext::SetSelectedTextFromWidget(OpWidget * widget)
{
	INT32 start, stop;
	SELECTION_DIRECTION direction;
	widget->GetSelection(start, stop, direction);

	OpString selection;
	RETURN_IF_ERROR(widget->GetText(selection));
	return m_selected_text.Set(selection.SubString(start).CStr(), stop);

}

void DesktopMenuContext::WidgetContext::SetScreenPosFromWidget(OpWidget * widget, const OpPoint & pos)
{
	m_screen_pos.x = pos.x + widget->GetScreenRect().x;
	m_screen_pos.y = pos.y + widget->GetScreenRect().y;
}
