/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
# include "modules/dragdrop/src/private_data.h"
# include "modules/logdoc/htm_elm.h"
# include "modules/doc/html_doc.h"
# include "modules/doc/frm_doc.h"

PrivateDragData::~PrivateDragData()
{
	m_timer.Stop();
	m_scroller.Stop();
	m_source_elm.SetElm(NULL);
}

OP_STATUS
PrivateDragData::AddElement(HTML_Element* elm, HTML_Document* document)
{
	DocElm* new_document_elm = OP_NEW(DocElm, ());
	RETURN_OOM_IF_NULL(new_document_elm);
	new_document_elm->document = document;
	DocElm* current_doc_elm;
	if (OpStatus::IsSuccess(m_elements_list.Remove(elm, &current_doc_elm)))
		OP_DELETE(current_doc_elm);
	OP_STATUS status = m_elements_list.Add(elm, new_document_elm);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(new_document_elm);
		return status;
	}

	return OpStatus::OK;
}

void
PrivateDragData::OnElementRemove(HTML_Element* elm)
{
	DocElm* data;
	if (OpStatus::IsSuccess(m_elements_list.Remove(elm, &data)))
		OP_DELETE(data);
}

class DragDocumentElementInvalidator : public OpHashTableForEachListener
{
	HTML_Document* document_to_be_removed;
public:
	DragDocumentElementInvalidator(HTML_Document* document)
		: document_to_be_removed(document)
	{}

	virtual void HandleKeyData(const void* key, void* data)
	{
		PrivateDragData::DocElm* doc_elm = static_cast<PrivateDragData::DocElm*>(data);
		if (doc_elm->document == document_to_be_removed)
			doc_elm->document = NULL;
	}
};

void
PrivateDragData::OnDocumentUnload(HTML_Document* document)
{
	DragDocumentElementInvalidator invalidator(document);
	m_elements_list.ForEach(&invalidator);

	if (document == m_source_doc)
		m_source_doc = NULL;

	if (document == m_target_doc)
	{
		document->SetPreviousImmediateSelectionElement(NULL);
		document->SetImmediateSelectionElement(NULL);
		document->SetCurrentTargetElement(NULL);
		m_target_doc = NULL;
		m_scroller.Stop();
		m_timer.Stop();
	}
}

HTML_Document*
PrivateDragData::GetElementsDocument(HTML_Element* elm)
{
	if (!elm)
		return NULL;

	if (GetSourceHtmlElement() == elm)
		return m_source_doc;

	if (m_target_doc && m_target_doc->GetCurrentTargetElement() == elm)
		return m_target_doc;

	DocElm* doc_elm;
	if (OpStatus::IsSuccess(m_elements_list.GetData(elm, &doc_elm)))
		return doc_elm->document;

	return NULL;
}

void
PrivateDragData::SetSourceHtmlElement(HTML_Element* source, HTML_Document* document)
{
	OP_ASSERT((!source && !document) || (source && document));
	m_source_elm.SetElm(source);
	m_source_doc = document;
}

void
PrivateDragData::OnTimeOut(OpTimer*)
{
	if (!g_drag_manager->IsBlocked())
	{
		ShiftKeyState modifiers_state = SHIFTKEY_NONE;
		if (m_target_doc)
			modifiers_state = m_target_doc->GetVisualDevice()->GetView()->GetShiftKeys();
		// m_last_known_drag_point is already scaled to the document scale.
		g_drag_manager->OnDragMove(m_target_doc, m_last_known_drag_point.x, m_last_known_drag_point.y, modifiers_state, TRUE);
	}
}

#endif // DRAG_SUPPORT
