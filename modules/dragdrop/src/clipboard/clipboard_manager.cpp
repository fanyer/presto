/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef USE_OP_CLIPBOARD

# include "modules/dragdrop/clipboard_manager.h"
# include "modules/dragdrop/dragdrop_data_utils.h"
# include "modules/pi/OpClipboard.h"
# include "modules/doc/frm_doc.h"
# ifdef DOCUMENT_EDIT_SUPPORT
#  include "modules/documentedit/OpDocumentEdit.h"
# endif // DOCUMENT_EDIT_SUPPORT
# include "modules/doc/html_doc.h"
# include "modules/logdoc/htm_elm.h"
# include "modules/logdoc/logdoc.h"
# include "modules/logdoc/urlimgcontprov.h"
# include "modules/url/url_man.h"

OP_STATUS
ClipboardManager::Initialize()
{
	return OpClipboard::Create(&m_pi_clipboard);
}

ClipboardManager::~ClipboardManager()
{
	OP_DELETE(m_pi_clipboard);
}

/*
 It's responsible for finding the target element as described in
 http://dev.w3.org/2006/webapi/clipops/clipops.html:

 Determine the target node for the event as follows
 In an editable context, the event object's target property must refer to the element that contains the start of the selection in document order,
 i.e. the end that is closer to the beginning of the document. If there is no selection or cursor, the target is the BODY element.
 In a non-editable document, the event's target must refer to a node focused for example by clicking, using the tab key,
 or by an interactive cursor, or to the BODY element if no other node has focus.

 It's only called when the a caller of the clipboard-manager gives the document but gives no target element.
*/
static HTML_Element* DetermineTargetElm(FramesDocument *frm_doc)
{
	if (!frm_doc)
		return NULL;

	// Default: <BODY>.
	HTML_Element* target = frm_doc->GetLogicalDocument() ? frm_doc->GetLogicalDocument()->GetBodyElm() : NULL;

	HTML_Document* doc = frm_doc->GetHtmlDocument();
	if (!doc)
		return target;
# ifdef DOCUMENT_EDIT_SUPPORT
	OpDocumentEdit* docedit = frm_doc->GetDocumentEdit();
# endif // DOCUMENT_EDIT_SUPPORT
	// An editable context.
	if (doc->GetElementWithSelection())
		target = doc->GetElementWithSelection();
# ifdef DOCUMENT_EDIT_SUPPORT
	else if (docedit && docedit->IsFocused())
	{
		if (doc->HasSelectedText())
		{
			SelectionBoundaryPoint start, end;
			doc->GetSelection(start, end);
			if (start.GetElement())
				target = start.GetElement();
		}
		else if (docedit->m_layout_modifier.IsActive())
			target = docedit->m_layout_modifier.m_helm;
	}
# endif // DOCUMENT_EDIT_SUPPORT
	// A non-editable context.
	else if (doc->GetActiveHTMLElement())
		target = doc->GetActiveHTMLElement();
	else if (doc->GetFocusedElement())
		target = doc->GetFocusedElement();
	else if (doc->GetNavigationElement())
		target = doc->GetNavigationElement();

	return target;
}

OP_STATUS
ClipboardManager::SendEvent(DOM_EventType type, FramesDocument* doc, HTML_Element* elm, unsigned int token, ClipboardListener* listener)
{
	OP_ASSERT(doc && elm);
	ClipboardEventData* data = OP_NEW(ClipboardEventData, ());
	RETURN_OOM_IF_NULL(data);
	OpAutoPtr<ClipboardEventData> auto_data(data);
	OpDragObject* event_object;
	RETURN_IF_ERROR(OpDragObject::Create(event_object, OpTypedObject::DRAG_TYPE_TEXT));
	data->data_object = event_object;

# ifndef DND_CLIPBOARD_FORCE_ORIGINAL_LISTENER
	listener->OnEnd();
	data->listener = doc->GetClipboardListener();
# else // DND_CLIPBOARD_FORCE_ORIGINAL_LISTENER
	data->listener = listener;
# endif // DND_CLIPBOARD_FORCE_ORIGINAL_LISTENER

	data->token = token;
# ifdef _X11_SELECTION_POLICY_
	data->mouse_buffer = m_pi_clipboard->GetMouseSelectionMode();
	OP_ASSERT(!data->mouse_buffer || type == ONPASTE);
# endif // _X11_SELECTION_POLICY_

	if (type == ONPASTE)
	{
		OpString text;
		if (m_pi_clipboard->HasText())
		{
			RETURN_IF_ERROR(m_pi_clipboard->GetText(text));
			RETURN_IF_ERROR(event_object->SetData(text.CStr(), UNI_L("text/plain"), FALSE, TRUE));
		}

# ifdef CLIPBOARD_HTML_SUPPORT
		OpString html;
		if (
#  ifdef _X11_SELECTION_POLICY_
			!data->mouse_buffer &&
#  endif // _X11_SELECTION_POLICY_
			m_pi_clipboard->HasTextHTML())
		{
			RETURN_IF_ERROR(m_pi_clipboard->GetTextHTML(html));
			RETURN_IF_ERROR(event_object->SetData(html.CStr(), UNI_L("text/html"), FALSE, TRUE));
		}
# endif // CLIPBOARD_HTML_SUPPORT
	}
	RETURN_IF_ERROR(m_data.Add(data));
	auto_data.release();
	int id = m_data.GetCount() - 1;
	data->in_use = TRUE;
	RETURN_IF_ERROR(doc->HandleEvent(type, NULL, elm, SHIFTKEY_NONE, 0, NULL, NULL, id));
	return OpStatus::OK;
}

OP_STATUS ClipboardManager::Copy(ClipboardListener* listener, unsigned int token, FramesDocument* doc, HTML_Element* elm)
{
	OP_ASSERT(listener);
	HTML_Element* target = elm ? elm : DetermineTargetElm(doc);
	if (target
# ifdef _X11_SELECTION_POLICY_
	    && !GetMouseSelectionMode()
# endif // _X11_SELECTION_POLICY_
		)
		return SendEvent(ONCOPY, doc, target, token, listener);
	else
	{
		listener->OnCopy(m_pi_clipboard);
		listener->OnEnd();
	}

	return OpStatus::OK;
}

OP_STATUS
ClipboardManager::CopyURLToClipboard(const uni_char* url, UINT32 token)
{
	return m_pi_clipboard->PlaceText(url, token);
}

OP_STATUS
ClipboardManager::CopyImageToClipboard(URL& url, UINT32 token)
{
	Image image = UrlImageContentProvider::GetImageFromUrl(url);

	if (image.IsEmpty())
		return OpStatus::ERR;

	OpBitmap *bitmap = image.GetBitmap(NULL);
	if (!bitmap)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = PlaceBitmap(bitmap, token);

	image.ReleaseBitmap();
	return status;
}

OP_STATUS
ClipboardManager::Cut(ClipboardListener* listener, unsigned int token, FramesDocument* doc, HTML_Element* elm)
{
	OP_ASSERT(listener);
	HTML_Element* target = elm ? elm : DetermineTargetElm(doc);
	if (target
# ifdef _X11_SELECTION_POLICY_
	    && !GetMouseSelectionMode()
# endif // _X11_SELECTION_POLICY_
		)
		return SendEvent(ONCUT, doc, target, token, listener);
	else
	{
		listener->OnCut(m_pi_clipboard);
		listener->OnEnd();
	}

	return OpStatus::OK;
}

OP_STATUS
ClipboardManager::Paste(ClipboardListener* listener, FramesDocument* doc, HTML_Element* elm)
{
	OP_ASSERT(listener);
	HTML_Element* target = elm ? elm : DetermineTargetElm(doc);
	if (target)
		return SendEvent(ONPASTE, doc, target, 0, listener);
	else
	{
		listener->OnPaste(m_pi_clipboard);
		listener->OnEnd();
	}

	return OpStatus::OK;
}

# ifdef _X11_SELECTION_POLICY_
void
ClipboardManager::SetMouseSelectionMode(BOOL val)
{
	m_pi_clipboard->SetMouseSelectionMode(val);
}

BOOL
ClipboardManager::GetMouseSelectionMode()
{
	return m_pi_clipboard->GetMouseSelectionMode();
}
# endif // _X11_SELECTION_POLICY_

BOOL
ClipboardManager::HasText()
{
	return m_pi_clipboard->HasText();
}

# ifdef CLIPBOARD_HTML_SUPPORT
BOOL
ClipboardManager::HasTextHTML()
{
	return m_pi_clipboard->HasTextHTML();
}
# endif // CLIPBOARD_HTML_SUPPORT

OP_STATUS
ClipboardManager::PlaceBitmap(const class OpBitmap* bitmap, UINT32 token)
{
	return m_pi_clipboard->PlaceBitmap(bitmap, token);
}

OP_STATUS
ClipboardManager::EmptyClipboard(UINT32 token)
{
	return m_pi_clipboard->EmptyClipboard(token);
}

void
ClipboardManager::HandleClipboardEventDefaultAction(DOM_EventType event, BOOL cancelled, unsigned int id)
{
	ClipboardEventData* data = m_data.Get(id);
	if (!data || !data->data_object)
	{
		OP_ASSERT(!"Wrong id");
		return;
	}

	switch (event)
	{
		case ONCOPY:
		case ONCUT:
		{
			if (cancelled)
			{
				if (data->cleared && data->data_object->GetDataCount() == 0)
					EmptyClipboard(data->token);

				const uni_char* text = DragDrop_Data_Utils::GetText(data->data_object);
# ifdef CLIPBOARD_HTML_SUPPORT
				const uni_char* html = DragDrop_Data_Utils::GetStringData(data->data_object, UNI_L("text/html"));
				if (html)
					m_pi_clipboard->PlaceTextHTML(html, text, data->token);
				else
# endif // CLIPBOARD_HTML_SUPPORT
				if (text)
					m_pi_clipboard->PlaceText(text, data->token);
			}
			else
			{
				if (data->listener)
				{
					if (event == ONCOPY)
						data->listener->OnCopy(m_pi_clipboard);
					else
						data->listener->OnCut(m_pi_clipboard);
				}
			}
		}
		break;
		case ONPASTE:
		{
			if (!cancelled)
				if (data->listener)
				{
# ifdef _X11_SELECTION_POLICY_
					BOOL mouse_sel_mode = GetMouseSelectionMode();
					SetMouseSelectionMode(data->mouse_buffer);
# endif // _X11_SELECTION_POLICY_
					data->listener->OnPaste(m_pi_clipboard);
# ifdef _X11_SELECTION_POLICY_
					SetMouseSelectionMode(mouse_sel_mode);
# endif // _X11_SELECTION_POLICY_
				}
		}
		break;
		default:
		{
			OP_ASSERT(!"Wrong event passed in here!");
			return;
		}
	}

	if (data->listener)
		data->listener->OnEnd();
	data->listener = NULL;

	data->in_use = FALSE;

	DeleteAllIfUnused();
}

void
ClipboardManager::OnDataObjectClear(OpDragObject* object)
{
	for (UINT32 idx = 0; idx < m_data.GetCount(); ++idx)
	{
		ClipboardEventData* data = m_data.Get(idx);
		if (data->data_object == object)
		{
			data->cleared = TRUE;
			return;
		}
	}
}

void
ClipboardManager::OnDataObjectSet(OpDragObject* object)
{
	for (UINT32 idx = 0; idx < m_data.GetCount(); ++idx)
	{
		ClipboardEventData* data = m_data.Get(idx);
		if (data->data_object == object)
		{
			data->cleared = FALSE;
			return;
		}
	}
}

OpDragObject*
ClipboardManager::GetEventObject(unsigned int id)
{
	if (id >= m_data.GetCount())
		return NULL;

	return m_data.Get(id)->data_object;
}

void
ClipboardManager::DeleteAllIfUnused()
{
	for (UINT32 idx = 0; idx < m_data.GetCount(); ++idx)
	{
		ClipboardEventData* data = m_data.Get(idx);
		if (data->in_use)
			return;
	}

	m_data.DeleteAll();
}

void
ClipboardManager::UnregisterListener(ClipboardListener* listener)
{
	for (UINT32 idx = 0; idx < m_data.GetCount(); ++idx)
	{
		ClipboardEventData* data = m_data.Get(idx);
		if (data->listener == listener)
		{
			data->listener = NULL;
			return;
		}
	}
}

BOOL
ClipboardManager::ForceEnablingClipboardAction(OpInputAction::Action action, FramesDocument* doc, HTML_Element* elm /* = NULL */)
{
	OP_ASSERT(action == OpInputAction::ACTION_COPY || action == OpInputAction::ACTION_CUT || action == OpInputAction::ACTION_PASTE);

	if (!doc)
		return FALSE;

	HTML_Element* target = elm ? elm : DetermineTargetElm(doc);
	if (!target)
		return FALSE;

	DOM_EventType event = (action == OpInputAction::ACTION_COPY) ? ONCOPY: (action == OpInputAction::ACTION_CUT ? ONCUT : ONPASTE);
	return target->HasEventHandler(doc, event, FALSE);
}
#endif // USE_OP_CLIPBOARD
