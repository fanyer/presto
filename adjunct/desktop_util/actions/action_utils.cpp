/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
* 
* George Refseth (rfz), Manuela Hutter (manuelah)
*/

#include "core/pch.h"

#include "adjunct/desktop_util/actions/action_utils.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/widgets/OpWidget.h"

void 
ContextMenuResolver::ResolveContextMenu(const OpPoint &point, DesktopMenuContext * menu_context, const uni_char*& menu_name, OpRect &rect, Window_Type window_type/*=WIN_TYPE_DOWNLOAD*/)
{
	if (menu_context->GetDocumentContext())
	{
		ResolveOpDocumentContextMenu(point, menu_context->GetDocumentContext(), menu_name, rect, window_type);
	}
	else if (menu_context->GetFileUrlInfo())
	{
		ResolveFileURLContextMenu(point, menu_context->GetFileUrlInfo(), menu_name, rect, window_type);
	}
	else if (menu_context->m_widget_context.IsInitialized())
	{
		ResolveWidgetContextMenu(point, menu_context->m_widget_context, menu_name, rect, window_type);
	}
	else
	{
		OP_ASSERT(!"Unhandled type of DesktopMenuContext");
	}
}


#ifdef INTERNAL_SPELLCHECK_SUPPORT
/*virtual*/ BOOL
ContextMenuResolver::HasSpellCheckWord(int spell_session_id)
{
	BOOL has_word = FALSE;
	if (spell_session_id != 0)
	{
		OpSpellUiSession *session;
		if(OpStatus::IsSuccess(OpSpellUiSession::Create(spell_session_id, &session)) && session)
		{
			has_word = session->HasWord();
			delete session;
		}
	}
	return has_word;
}


/*virtual*/ BOOL
ContextMenuResolver::CanDisableSpellcheck(int spell_session_id)
{
	BOOL can_disable_spellcheck = FALSE;
	if (spell_session_id != 0)
	{
		OpSpellUiSession *session;
		if(OpStatus::IsSuccess(OpSpellUiSession::Create(spell_session_id, &session)) && session)
		{
			can_disable_spellcheck = session->CanDisableSpellcheck();
			delete session;
		}
	}
	return can_disable_spellcheck;
}
#endif // INTERNAL_SPELLCHECK_SUPPORT


// NOTE: be sure to run/update selftest "desktop_util.action_utils" (adjunct/desktop_util/selftest/action_utils.ot) if you change any of this
void
ContextMenuResolver::ResolveOpDocumentContextMenu(const OpPoint &point, OpDocumentContext * context, const uni_char*& menu_name, OpRect &rect, Window_Type window_type/*=WIN_TYPE_DOWNLOAD*/)
{
	OP_ASSERT(context);
	if (context)
	{
		if( window_type == WIN_TYPE_JS_CONSOLE )
		{
			if( context->HasTextSelection() )
			{
				rect.x = point.x;
				rect.y = point.y;
				menu_name = UNI_L("Console Hotclick Popup Menu");
			}
		}
		else if( window_type == WIN_TYPE_INFO )
		{
			if( context->HasTextSelection() )
			{
				rect.x = point.x;
				rect.y = point.y;
				menu_name = UNI_L("Hotclick Popup Menu");
			}
			else if (context->HasLink())
			{
				menu_name  = UNI_L("Link Popup Menu");
			}
		}
		else if (window_type == WIN_TYPE_MAIL_COMPOSE)
		{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
			int spell_session_id = context->GetSpellSessionId();
			if (spell_session_id == 0)
				menu_name = UNI_L("Mail Compose Popup Menu");
			else
			{
				menu_name = HasSpellCheckWord(spell_session_id) ? UNI_L("Mail Compose Popup Menu Spellcheck Word") : UNI_L("Mail Compose Popup Menu Spellcheck Enabled");
			}
#else // !INTERNAL_SPELLCHECK_SUPPORT
			menu_name = UNI_L("Mail Compose Popup Menu");
#endif // INTERNAL_SPELLCHECK_SUPPORT
		}
		else if (context->HasTextSelection() && context->HasLink())
		{
			menu_name = UNI_L("Link Selection Popup Menu");
		}
		else if (context->HasMailtoLink())
		{
			if (context->HasImage())
			{
				menu_name = UNI_L("Mailto Link Image Popup Menu");
			}
			else
			{
				menu_name = UNI_L("Mailto Link Popup Menu");
			}
		}
		else if (context->HasLink())
		{
			if (context->HasImage())
			{
				if (context->HasFullQualityImage())
				{
					menu_name = UNI_L("Image Link Popup Menu");
				}
				else
				{
					menu_name = UNI_L("Turbo Image Link Popup Menu");
				}
			}
			else
			{
				menu_name  = UNI_L("Link Popup Menu");
			}
		}
		else if (context->HasImage())
		{
			if (context->HasFullQualityImage())
			{
				menu_name = UNI_L("Image Popup Menu");
			}
			else
			{
				menu_name = UNI_L("Turbo Image Popup Menu");
			}
			
		}
		else if (context->IsVideo())
		{
			menu_name = UNI_L("Video Popup Menu");
		}
		else if (context->HasMedia())
		{
			menu_name = UNI_L("Audio Popup Menu");
		}
#ifdef SVG_SUPPORT
		else if (context->HasSVG())
		{
			rect.x = point.x;
			rect.y = point.y;
			menu_name = UNI_L("SVG Popup Menu");
		}
#endif // SVG_SUPPORT
		else if (context->HasEditableText())
		{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
			int spell_session_id = context->GetSpellSessionId();
			if (spell_session_id == 0)
			{
				menu_name = UNI_L("Edit Form Popup Menu");
			}
			else
			{
				menu_name = HasSpellCheckWord(spell_session_id) ? UNI_L("Edit Form Popup Menu Spellcheck Word") : UNI_L("Edit Form Popup Menu Spellcheck Enabled");
			}
#else // !INTERNAL_SPELLCHECK_SUPPORT
			menu_name = UNI_L("Edit Form Popup Menu");
#endif // INTERNAL_SPELLCHECK_SUPPORT
		}
		else if (context->HasTextSelection())
		{
			rect.x = point.x;
			rect.y = point.y;
			menu_name = UNI_L("Hotclick Popup Menu");
		}
		else
		{
			if ( window_type == WIN_TYPE_MAIL_VIEW || window_type == WIN_TYPE_NEWSFEED_VIEW)
			{
				menu_name = UNI_L("Mail Body Popup Menu");
			}
			else if( window_type == WIN_TYPE_IM_VIEW )
			{
				menu_name  = UNI_L("Chat Popup Menu");
			}
			else if( window_type == WIN_TYPE_GADGET )
			{
				menu_name = UNI_L("Desktop Widget Menu");
			}
			else
			{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
				menu_name = NULL;
				int spell_session_id = context->GetSpellSessionId();
				if (spell_session_id != 0)
				{
					menu_name = CanDisableSpellcheck(spell_session_id) ? UNI_L("Document Popup Menu Spellcheck Enabled") : UNI_L("Document Popup Menu Spellcheck Disabled");
				}
				if(!menu_name)
#endif // INTERNAL_SPELLCHECK_SUPPORT
					menu_name  = UNI_L("Document Popup Menu");
			}
		}
	}
}


// NOTE: be sure to run/update selftest "desktop_util.action_utils" (adjunct/desktop_util/selftest/action_utils.ot) if you change any of this
void
ContextMenuResolver::ResolveFileURLContextMenu(const OpPoint &point, const URLInformation * url_info, const uni_char*& menu_name, OpRect &rect, Window_Type window_type/*=WIN_TYPE_DOWNLOAD*/)
{
	OP_ASSERT(url_info);
	if (url_info)
	{
		menu_name = UNI_L("Transfers Item Popup Menu");
	}
}


// NOTE: be sure to run/update selftest "desktop_util.action_utils" (adjunct/desktop_util/selftest/action_utils.ot) if you change any of this
void 
ContextMenuResolver::ResolveWidgetContextMenu(const OpPoint &point, const DesktopMenuContext::WidgetContext & widget_context, const uni_char*& menu_name, OpRect &rect, Window_Type window_type/*=WIN_TYPE_DOWNLOAD*/)
{
	OP_ASSERT(widget_context.IsInitialized());
	if (widget_context.IsInitialized())
	{
		if (widget_context.m_packed.is_form_element)
		{
			int spell_session_id = widget_context.m_spell_session_id;
			if (spell_session_id == 0)
			{
				menu_name = UNI_L("Edit Form Popup Menu");
			}
			else
			{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
				menu_name = HasSpellCheckWord(spell_session_id) ? UNI_L("Edit Form Popup Menu Spellcheck Word") : UNI_L("Edit Form Popup Menu Spellcheck Enabled");
#else // !INTERNAL_SPELLCHECK_SUPPORT
				menu_name = UNI_L("Edit Form Popup Menu");
#endif // INTERNAL_SPELLCHECK_SUPPORT
			}
		}
		else if(widget_context.m_packed.is_enabled && IsEditableWidget(widget_context.m_widget_type))
		{
			if (widget_context.m_widget_type == OpTypedObject::WIDGET_TYPE_FILECHOOSER_EDIT)
			{
				menu_name = UNI_L("Edit Filechooser Popup Menu");
			}
			else if(widget_context.m_packed.is_read_only)
			{
				menu_name = UNI_L("Readonly Edit Widget Popup Menu");
			}
			else
			{
				int spell_session_id = widget_context.m_spell_session_id;
				if (spell_session_id == 0)
				{
					menu_name = UNI_L("Edit Widget Popup Menu");
				}
				else
				{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
				menu_name = HasSpellCheckWord(spell_session_id) ? UNI_L("Edit Widget Popup Menu Spellcheck Word") : UNI_L("Edit Widget Popup Menu Spellcheck Enabled");
#else // !INTERNAL_SPELLCHECK_SUPPORT
				menu_name = UNI_L("Edit Widget Popup Menu");
#endif // INTERNAL_SPELLCHECK_SUPOPORT
				}
			}
		}
	}
}

BOOL ContextMenuResolver::IsEditableWidget(INT32 widget_type)
{
	// Not all the types actually end up running here, they don't if
	// OnContextMenu is handled properly, but just include all of them
	// to be safe
	switch (widget_type)
	{
	case OpTypedObject::WIDGET_TYPE_DROPDOWN:
	case OpTypedObject::WIDGET_TYPE_EDIT:
	case OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT:
	case OpTypedObject::WIDGET_TYPE_FILECHOOSER_EDIT:
	case OpTypedObject::WIDGET_TYPE_FOLDERCHOOSER_EDIT:
	case OpTypedObject::WIDGET_TYPE_ADDRESS_DROPDOWN:
	case OpTypedObject::WIDGET_TYPE_SEARCH_DROPDOWN:
	case OpTypedObject::WIDGET_TYPE_SEARCH_EDIT:
	case OpTypedObject::WIDGET_TYPE_NUMBER_EDIT:
	case OpTypedObject::WIDGET_TYPE_ACCOUNT_DROPDOWN:
	case OpTypedObject::WIDGET_TYPE_COMPOSE_EDIT:
	case OpTypedObject::WIDGET_TYPE_MAIL_SEARCH_FIELD:
		return TRUE;
	}
	return FALSE;
}

#ifdef WIDGET_RUNTIME_SUPPORT
void ResolveGadgetContextMenu(DesktopMenuContext& menu_context,
		const uni_char*& menu_name)
{
	menu_name = NULL;

	OpDocumentContext* doc_context = menu_context.GetDocumentContext();
	if (NULL == doc_context)
	{
		return;
	}

	if (doc_context->HasEditableText())
	{
#ifdef INTERNAL_SPELLCHECK_SUPPORT
		int spell_session_id = doc_context->GetSpellSessionId();
		if (spell_session_id == 0)
		{
			menu_name = UNI_L("Gadget Edit Form Popup Menu");
		}
		else
		{
			BOOL has_word = FALSE;
			OpSpellUiSession* session = NULL;
			if(OpStatus::IsSuccess(OpSpellUiSession::Create(
							spell_session_id, &session)))
			{
				has_word = session->HasWord();
				OP_DELETE(session);
			}
			menu_name = has_word
					? UNI_L("Gadget Edit Form Popup Menu Spellcheck Word")
					: UNI_L("Gadget Edit Form Popup Menu Spellcheck Enabled");
		}
#else // !INTERNAL_SPELLCHECK_SUPPORT
		menu_name = UNI_L("Gadget Edit Form Popup Menu");
#endif // INTERNAL_SPELLCHECK_SUPPORT
	}
	else if (doc_context->HasLink())
	{
		if (doc_context->HasImage())
		{
			menu_name = UNI_L("Gadget Image Link Popup Menu");
		}
		else
		{
			menu_name  = UNI_L("Gadget Link Popup Menu");
		}
	}
	else if (doc_context->HasImage())
	{
		menu_name = UNI_L("Gadget Image Popup Menu");
	}
	else
	{
		if(doc_context->HasTextSelection())
			menu_name = UNI_L("Desktop Widget Runtime Menu With Copy");
		else
			menu_name = UNI_L("Desktop Widget Runtime Menu");
	}
}
#endif // WIDGET_RUNTIME_SUPPORT


void ResolveContextMenu(const OpPoint &point, DesktopMenuContext * menu_context, const uni_char*& menu_name, OpRect &rect, Window_Type window_type/*=WIN_TYPE_DOWNLOAD*/)
{
	ContextMenuResolver resolver;
	resolver.ResolveContextMenu(point, menu_context, menu_name, rect, window_type);
}


BOOL InvokeContextMenuAction(const OpPoint & point, DesktopMenuContext* context, const uni_char* menu_name, OpRect &rect)
{
	INTPTR action_data = reinterpret_cast<INTPTR>(context);

	if (menu_name)
	{
		OpInputAction *action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU_WITH_MENU_CONTEXT));
		if (!action)
			return FALSE;
		action->SetActionData(action_data);
		action->SetActionDataString(menu_name);
		action->SetActionPosition(rect);

		OpDocumentContext* doc_context = context->GetDocumentContext();
		if (doc_context && doc_context->HasKeyboardOrigin())
			action->SetActionMethod(OpInputAction::METHOD_KEYBOARD);

		OpInputAction async_action(OpInputAction::ACTION_DELAY);
		async_action.SetActionData(1);
		async_action.SetNextInputAction(action);

		g_input_manager->InvokeAction(&async_action);
		return TRUE;
	}
	return FALSE;
}


BOOL HandleWidgetContextMenu(OpWidget* widget, const OpPoint & pos)
{
	if (!widget)
		return FALSE;
	
	widget->SetFocus(FOCUS_REASON_OTHER);

	OpAutoPtr<DesktopMenuContext> menu_context (OP_NEW(DesktopMenuContext, ()));
	if (!menu_context.get())
		return FALSE;

	RETURN_VALUE_IF_ERROR(menu_context->Init(widget, pos), FALSE);

	const uni_char* menu_name  = NULL;
	OpRect rect;
	OpPoint point(menu_context->m_widget_context.m_screen_pos);
	ResolveContextMenu(point, menu_context.get(), menu_name, rect);

	if (!InvokeContextMenuAction(point, menu_context.get(), menu_name, rect))
		return FALSE;

	menu_context.release();
	return TRUE;
}
