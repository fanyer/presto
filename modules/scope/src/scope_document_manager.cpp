/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_DOCUMENT_MANAGER

#include "modules/scope/src/scope_transport.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_document_manager.h"
#include "modules/scope/src/scope_resource_manager.h"
#include "modules/scope/src/scope_window_manager.h"
#include "modules/doc/searchinfo.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/dochand/fdelm.h"
#include "modules/pi/OpSystemInfo.h"

/* OpScopeDocumentManager */

OpScopeDocumentManager::OpScopeDocumentManager()
	: OpScopeDocumentManager_SI()
	, frame_ids(NULL)
	, document_ids(NULL)
	, resource_ids(NULL)
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	, window_manager(NULL)
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
{
}

/* virtual */
OpScopeDocumentManager::~OpScopeDocumentManager()
{
	if (frame_ids)
		frame_ids->Release();
	if (document_ids)
		document_ids->Release();
	if (resource_ids)
		resource_ids->Release();
}

/*virtual*/
OP_STATUS
OpScopeDocumentManager::Construct(OpScopeIDTable<DocumentManager> *o_frame_ids,
                                  OpScopeIDTable<FramesDocument> *o_document_ids,
                                  OpScopeIDTable<unsigned> *o_resource_ids)
{
	OP_ASSERT(o_frame_ids && o_document_ids && o_resource_ids);
	OP_ASSERT(!frame_ids && !document_ids && !resource_ids);
	frame_ids = o_frame_ids->Retain();
	document_ids = o_document_ids->Retain();
	resource_ids = o_resource_ids->Retain();
	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeDocumentManager::OnPostInitialization()
{
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	if (OpScopeServiceManager *manager = GetManager())
		window_manager = OpScopeFindService<OpScopeWindowManager>(manager, "window-manager");
	if (!window_manager)
		return OpStatus::ERR_NULL_POINTER;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeDocumentManager::OnServiceEnabled()
{
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	OP_ASSERT(window_manager); // This service should not be enabled if the window manager was not found
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
	// Only enable the service if all pointers are initialized
	if (!frame_ids || !document_ids || !resource_ids)
		return OpStatus::ERR_NULL_POINTER;

	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeDocumentManager::OnServiceDisabled()
{
	frame_ids->Reset();
	document_ids->Reset();
	resource_ids->Reset();

	return OpStatus::OK;
}

void
OpScopeDocumentManager::ReadyStateChanged(FramesDocument *doc, OpScopeReadyStateListener::ReadyState state)
{
	if (!IsEnabled() || !AcceptWindow(doc->GetWindow()))
		return;

	if (state == OpScopeReadyStateListener::READY_STATE_AFTER_ONLOAD)
	{
		DocumentLoaded msg;
		msg.SetWindowID(doc->GetWindow()->Id());

		unsigned document_id;
		RETURN_VOID_IF_ERROR(GetDocumentID(doc, document_id));
		msg.SetDocumentID(document_id);

		unsigned frame_id;
		RETURN_VOID_IF_ERROR(GetFrameID(doc->GetDocManager(), frame_id));
		msg.SetFrameID(frame_id);

		OpStatus::Ignore(SendOnDocumentLoaded(msg));
	}

	OpStatus::Ignore(OnDocumentEvent(doc, state));
}

OP_STATUS
OpScopeDocumentManager::OnDocumentEvent(FramesDocument *doc, OpScopeReadyStateListener::ReadyState state)
{
	DocumentEventType event_type;
	RETURN_IF_ERROR(Convert(state, event_type));

	DocumentEvent msg;
	msg.SetWindowID(doc->GetWindow()->Id());

	unsigned document_id;
	RETURN_IF_ERROR(GetDocumentID(doc, document_id));
	msg.SetDocumentID(document_id);

	unsigned frame_id;
	RETURN_IF_ERROR(GetFrameID(doc->GetDocManager(), frame_id));
	msg.SetFrameID(frame_id);

	URL &url = doc->GetURL();

	unsigned resource_id;
	RETURN_IF_ERROR(GetResourceID(url.GetRep(), resource_id));
	msg.SetResourceID(resource_id);

	msg.SetEventType(event_type);
	msg.SetTime(g_op_time_info->GetTimeUTC());

	return SendOnDocumentEvent(msg);
}

OP_STATUS
OpScopeDocumentManager::OnAboutToLoadDocument(DocumentManager *docman, const OpScopeDocumentListener::AboutToLoadDocumentArgs &args)
{
	if (!IsEnabled() || !AcceptWindow(docman->GetWindow()))
		return OpStatus::OK;

	unsigned frame_id;
	unsigned resource_id;
	RETURN_IF_ERROR(GetFrameID(docman, frame_id));
	RETURN_IF_ERROR(GetResourceID(args.url, resource_id));

	AboutToLoadDocument msg;
	msg.SetWindowID(docman->GetWindow()->Id());
	msg.SetFrameID(frame_id);
	msg.SetResourceID(resource_id);
	msg.SetTime(g_op_time_info->GetTimeUTC());

	unsigned parent_doc_id;
	unsigned parent_frame_id;
	RETURN_IF_ERROR(GetParentIDs(docman, parent_doc_id, parent_frame_id));

	if (parent_doc_id)
		msg.SetParentDocumentID(parent_doc_id);

	if (parent_frame_id)
		msg.SetParentFrameID(parent_frame_id);

	return SendOnAboutToLoadDocument(msg);
}

OP_STATUS
OpScopeDocumentManager::DoListDocuments(const ListDocumentsArg &in, DocumentList &out)
{
	for (Window *win = g_windowManager->FirstWindow(); win; win = win->Suc())
	{
		if (in.HasWindowID() && win->Id() != in.GetWindowID())
			continue;

		DocumentTreeIterator dociter(win);
		dociter.SetIncludeThis();
		dociter.SetIncludeEmpty();

		while (dociter.Next())
		{
			Document *doc = out.GetDocumentListRef().AddNew();
			RETURN_OOM_IF_NULL(doc);
			doc->SetWindowID(win->Id());

			unsigned frm_id;
			DocumentManager *docman = dociter.GetDocumentManager();
			RETURN_IF_ERROR(GetFrameID(docman, frm_id));
			doc->SetFrameID(frm_id);
			
			FramesDocument *frm_doc = dociter.GetFramesDocument();

			if (frm_doc)
			{
				unsigned doc_id;
				RETURN_IF_ERROR(GetDocumentID(frm_doc, doc_id));
				doc->SetDocumentID(doc_id);

				URL &url = frm_doc->GetURL();

				unsigned resource_id;
				RETURN_IF_ERROR(GetResourceID(url.GetRep(), resource_id));
				doc->SetResourceID(resource_id);

				if (!url.IsEmpty())
					RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_Username_Password_Hidden, doc->GetUrlRef()));

				FramesDocElm *elm = dociter.GetFramesDocElm();

				if (elm && elm->GetName())
					RETURN_IF_ERROR(doc->SetFrameElementName(elm->GetName()));
				if (elm && elm->GetFrameId())
					RETURN_IF_ERROR(doc->SetFrameElementID(elm->GetFrameId()));

				unsigned parent_doc_id;
				unsigned parent_frame_id;
				RETURN_IF_ERROR(GetParentIDs(frm_doc->GetDocManager(), parent_doc_id, parent_frame_id));

				if (parent_doc_id)
					doc->SetParentDocumentID(parent_doc_id);

				if (parent_frame_id)
					doc->SetParentFrameID(parent_frame_id);
			}
		}

		// We were just interested in one Window, so let's skip it.
		if (in.HasWindowID())
			break;
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeDocumentManager::DoLoadDocument(const LoadDocumentArg &in)
{
	DocumentManager *docman;
	RETURN_IF_ERROR(GetFrame(in.GetWindowID(), in.GetFrameID(), docman));

	URL url = g_url_api->GetURL(in.GetUrl());

	if (!url.IsValid())
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Invalid URL"));

	docman->OpenURL(url, DocumentReferrer(URL()), TRUE, TRUE, DocumentManager::OpenURLOptions());

	return OpStatus::OK;
}

OP_STATUS
OpScopeDocumentManager::DoReloadDocument(const ReloadDocumentArg &in)
{
	DocumentManager *docman;
	RETURN_IF_ERROR(GetFrame(in.GetWindowID(), in.GetFrameID(), docman));

	docman->Reload(WasEnteredByUser);

	return OpStatus::OK;
}

OP_STATUS
OpScopeDocumentManager::DoSearchText(const SearchTextArg &in, HighlightList &out)
{
#ifdef SEARCH_MATCHES_ALL
	DocumentManager *docman = GetDocumentManager(in.GetWindowID(), in.GetFrameID());
	FramesDocument *frm_doc = (docman ? docman->GetCurrentDoc() : NULL);

	if (!frm_doc)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("The specified window or frame does not exist"));

	// Unused, but needed.
	OpRect rect;

	// Setup search.
	SearchData data(FALSE, FALSE, FALSE, TRUE, FALSE);
	data.SetIsNewSearch(TRUE);
	RETURN_IF_ERROR(data.SetSearchText(in.GetText().CStr()));

	OpBoolean::Ignore(frm_doc->HighlightNextMatch(&data, rect));

	return AddHighlights(docman, out);
#else // SEARCH_MATCHES_ALL
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SEARCH_MATCHES_ALL
}

OP_STATUS
OpScopeDocumentManager::DoGetHighlights(const GetHighlightsArg &in, HighlightList &out)
{
#ifdef SEARCH_MATCHES_ALL
	DocumentManager *docman = GetDocumentManager(in.GetWindowID(), in.GetFrameID());

	if (!docman)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("The specified window or frame does not exist"));

	return AddHighlights(docman, out);
#else // SAVE_DOCUMENT_AS_TEXT_SUPPORT
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT
}

OP_STATUS
OpScopeDocumentManager::DoGetPageText(const GetPageTextArg &in, PageText &out)
{
#ifdef SAVE_DOCUMENT_AS_TEXT_SUPPORT
	DocumentManager *docman = GetDocumentManager(in.GetWindowID(), in.GetFrameID());

	if (!docman)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("The specified window or frame does not exist"));

	DocumentTreeIterator iter(docman);
	iter.SetIncludeThis();

	while (iter.Next())
	{
		PageText::DocumentText *doctext = out.GetDocumentTextListRef().AddNew();
		RETURN_OOM_IF_NULL(doctext);

		DocumentManager *docman = iter.GetDocumentManager();

		if (!docman)
			continue;

		unsigned document_id;
		unsigned frame_id;

		if (OpStatus::IsError(GetFrameID(docman, document_id)))
			continue;

		if (OpStatus::IsError(GetDocumentID(docman->GetCurrentDoc(), frame_id)))
			continue;

		doctext->SetFrameID(frame_id);
		doctext->SetDocumentID(document_id);

		DocumentTextStream stream;
		RETURN_IF_ERROR(docman->SaveCurrentDocAsText(&stream));
		RETURN_IF_ERROR(doctext->SetText(stream.GetString()));
	}

	return OpStatus::OK;
#else // SAVE_DOCUMENT_AS_TEXT_SUPPORT
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT
}

#ifdef SAVE_DOCUMENT_AS_TEXT_SUPPORT
/* virtual */
OpScopeDocumentManager::DocumentTextStream::~DocumentTextStream()
{
}

/* virtual */ void
OpScopeDocumentManager::DocumentTextStream::WriteStringL(const uni_char* str, int len)
{
	if (str != NULL && len != 0)
		buf.AppendL(str, (len < 0 ? (size_t)-1 : len));
}

const uni_char *
OpScopeDocumentManager::DocumentTextStream::GetString()
{
	return buf.GetStorage();
}

/* virtual */ OP_STATUS
OpScopeDocumentManager::DocumentTextStream::Close()
{
	return OpStatus::OK;
}
#endif // SAVE_DOCUMENT_AS_TEXT_SUPPORT

OP_STATUS
OpScopeDocumentManager::GetWindow(unsigned id, Window *&window)
{
	window = g_windowManager->GetWindow(id);

	if (!window)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("No such window"));

	return OpStatus::OK;
}

OP_STATUS
OpScopeDocumentManager::GetDocumentID(FramesDocument *frm_doc, unsigned &id)
{
	return document_ids->GetID(frm_doc, id);
}

OP_STATUS
OpScopeDocumentManager::GetFrameID(DocumentManager *docman, unsigned &id)
{
	return frame_ids->GetID(docman, id);
}

OP_STATUS
OpScopeDocumentManager::GetFrame(unsigned wid, unsigned fid, DocumentManager *&docman)
{	
	Window *window;
	RETURN_IF_ERROR(GetWindow(wid, window));

	OP_STATUS err = frame_ids->GetObject(fid, docman);

	if (OpStatus::IsError(err))
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("No such frame"));

	DocumentTreeIterator dociter(window);
	dociter.SetIncludeThis();
	dociter.SetIncludeEmpty();

	while (dociter.Next())
		if (dociter.GetDocumentManager() == docman)
			return OpStatus::OK;

	return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("That frame no longer exists"));
}


OP_STATUS
OpScopeDocumentManager::GetParentIDs(DocumentManager *child, unsigned &pid, unsigned &fid)
{
	FramesDocument *parent_doc = child->GetParentDoc();
	DocumentManager *parent_frame = parent_doc ? parent_doc->GetDocManager() : NULL;

	if (parent_doc)
		RETURN_IF_ERROR(GetDocumentID(parent_doc, pid));
	else
		pid = 0;

	if (parent_frame)
		RETURN_IF_ERROR(GetFrameID(parent_frame, fid));
	else
		fid = 0;

	return OpStatus::OK;
}

OP_STATUS
OpScopeDocumentManager::GetResourceID(URL_Rep *url, unsigned &id)
{
	return resource_ids->GetID(reinterpret_cast<unsigned*>(static_cast<UINTPTR>(url->GetID())), id);
}

OP_STATUS
OpScopeDocumentManager::Convert(OpScopeReadyStateListener::ReadyState state_in, DocumentEventType &state_out)
{
	switch(state_in)
	{
	case OpScopeReadyStateListener::READY_STATE_BEFORE_ONLOAD:
		state_out = DOCUMENTEVENTTYPE_LOAD_START;
		break;
	case OpScopeReadyStateListener::READY_STATE_AFTER_ONLOAD:
		state_out = DOCUMENTEVENTTYPE_LOAD_END;
		break;
	case OpScopeReadyStateListener::READY_STATE_BEFORE_DOM_CONTENT_LOADED:
		state_out = DOCUMENTEVENTTYPE_DOMCONTENTLOADED_START;
		break;
	case OpScopeReadyStateListener::READY_STATE_AFTER_DOM_CONTENT_LOADED:
		state_out = DOCUMENTEVENTTYPE_DOMCONTENTLOADED_END;
		break;
	case OpScopeReadyStateListener::READY_STATE_DOM_ENVIRONMENT_CREATED:
		return OpStatus::ERR_NOT_SUPPORTED;
	default:
		OP_ASSERT(!"Explicitly ignore unknown state or convert to a proper state!");
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	return OpStatus::OK;
}

BOOL
OpScopeDocumentManager::AcceptWindow(Window *window)
{
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	return window_manager && window_manager->AcceptWindow(window);
#else // SCOPE_WINDOW_MANAGER_SUPPORT
	return TRUE;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
}

#ifdef SEARCH_MATCHES_ALL
OP_STATUS
OpScopeDocumentManager::AddHighlights(DocumentManager *docman, HighlightList &out)
{
	if (!docman)
		return OpStatus::ERR;

	DocumentTreeIterator iter(docman);
	iter.SetIncludeThis();

	while (iter.Next())
	{
		DocumentManager *docman = iter.GetDocumentManager();

		if (!docman)
			continue;

		unsigned frame_id;
		RETURN_IF_ERROR(GetFrameID(docman, frame_id));

		FramesDocument *frm_doc = iter.GetFramesDocument();

		if (!frm_doc)
			continue;

		HTML_Document *html_doc = frm_doc->GetHtmlDocument();

		if (!html_doc)
			continue;

		SelectionElm *elm = html_doc->GetFirstSelectionElm();

		while (elm)
		{
			Highlight *highlight = out.GetHighlightListRef().AddNew();
			RETURN_OOM_IF_NULL(highlight);
			highlight->SetFrameID(frame_id);

			TextSelection *selection = elm->GetSelection();
			HTML_Element *start = selection->GetStartElement();
			HTML_Element *end = selection->GetEndElement();

			HTML_Element *ancestor = FindCommonAncestor(start, end);

			if (ancestor)
			{
				OpAutoPtr<Node> node(OP_NEW(Node, ()));
				RETURN_OOM_IF_NULL(node.get());

				RETURN_IF_ERROR(node->SetTag(ancestor->GetTagName()));

				if (ancestor->GetId())
					RETURN_IF_ERROR(node->SetId(ancestor->GetId()));

				highlight->SetNode(node.release());
			}

			elm = static_cast<SelectionElm *>(elm->Suc());
		}
	}

	return OpStatus::OK;
}

HTML_Element*
OpScopeDocumentManager::FindCommonAncestor(HTML_Element *e1, HTML_Element *e2)
{
	if (!e1 || !e2)
		return NULL;

	HTML_Element *parent = e1->Parent();

	while (parent)
	{
		if (parent->IsAncestorOf(e2))
			return parent;

		parent = parent->Parent();
	}

	return NULL;
}
#endif // SEARCH_MATCHES_ALL

DocumentManager *
OpScopeDocumentManager::GetDocumentManager(unsigned window_id, unsigned frame_id)
{
	OP_ASSERT(window_id != 0);

	DocumentManager *docman = NULL;

	if (frame_id != 0)
		RETURN_VALUE_IF_ERROR(GetFrame(window_id, frame_id, docman), NULL);
	else
	{
		Window *window;
		RETURN_VALUE_IF_ERROR(GetWindow(window_id, window), NULL);
		docman = window->GetDocManagerById(-1);
	}

	return docman;
}

#endif // SCOPE_DOCUMENT_MANAGER
