/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/AccessibleDocument.h"
#include "modules/accessibility/traverse/AccessibleElementManager.h"
#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/docman.h"

AccessibleDocument::AccessibleDocument(OpAccessibleItem* parent, DocumentManager* doc_man)
	: m_parent(parent)
	, m_doc_man(doc_man)
	, m_accessible_children(NULL)
	, m_highlight_element(NULL)
	, m_was_loading(FALSE)
	, m_fake_loading(TRUE)
	, m_is_ready(TRUE)
{
	m_reflow_timer.SetTimerListener(this);
}

AccessibleDocument::~AccessibleDocument()
{
	OP_DELETE(m_accessible_children);
}

void AccessibleDocument::DocumentUndisplayed(const FramesDocument* doc)
{
	if (m_accessible_children //&& doc == m_accessible_children->GetFramesDocument()
		)
	{
		OP_DELETE(m_accessible_children);
		m_accessible_children = NULL;
		m_highlight_element = NULL;
		m_reflow_timer.Stop();
		m_fake_loading = TRUE;
	}
	if (GetActiveDocument() && m_was_loading != !GetActiveDocument()->IsLoaded())
	{
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateBusy));
	}
}

void AccessibleDocument::DocumentReflowed(const FramesDocument* doc)
{
	if (m_accessible_children)
	{
		if (m_accessible_children->GetFramesDocument() == doc)
		{
			m_accessible_children->MarkDirty();
		}
		else
		{
			OP_DELETE(m_accessible_children);
			m_accessible_children = NULL;
			m_highlight_element = NULL;
			m_fake_loading = TRUE;
		}
	}

	AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventReorder));
	if (g_op_system_info->IsScreenReaderRunning())
	{
		m_reflow_timer.Stop();
		m_reflow_timer.Start(1000);
	}
	if (GetActiveDocument() && (m_was_loading != (!GetActiveDocument()->IsLoaded() || m_fake_loading)))
	{
		if (m_fake_loading && GetActiveDocument()->IsLoaded())
		{
			// It is loaded, so send TWO events: first a busy and then a non-busy
			AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateBusy));
			m_fake_loading = FALSE;
		}
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateBusy));
	}
}

void AccessibleDocument::DocumentLoaded(const FramesDocument* doc)
{
	if (m_accessible_children)
	{
		if (m_accessible_children->GetFramesDocument() == doc)
		{
			m_accessible_children->MarkDirty();
		}
		else
		{
			OP_DELETE(m_accessible_children);
			m_accessible_children = NULL;
			m_highlight_element = NULL;
			m_fake_loading = TRUE;
		}
	}
	if (!m_was_loading)
	{
		m_fake_loading = TRUE;
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateBusy));
		m_fake_loading = FALSE;
	}
	AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateBusy));
}

void AccessibleDocument::ElementRemoved(const FramesDocument* doc, const HTML_Element* element)
{
	if (m_accessible_children && doc == m_accessible_children->GetFramesDocument())
	{
		m_accessible_children->UnreferenceElement(element);
	}
}

void AccessibleDocument::WidgetRemoved(const OpWidget* widget)
{
	if (m_accessible_children)
	{
		m_accessible_children->UnreferenceWidget(widget);
	}
}


void AccessibleDocument::HighlightElement(HTML_Element* element)
{
	if (element != m_highlight_element)
	{
		HTML_Element* old_highlight = m_highlight_element;
		m_highlight_element = element;
		if (m_accessible_children)
		{
			m_accessible_children->HighlightElement(m_highlight_element, old_highlight);
		}
	}
}

DocumentManager* AccessibleDocument::GetDocumentManager()
{
	return m_doc_man;
}

FramesDocument* AccessibleDocument::GetActiveDocument() const
{
	if (m_accessible_children)
		return m_accessible_children->GetFramesDocument();

	return m_doc_man->GetCurrentVisibleDoc();
}

VisualDevice* AccessibleDocument::GetVisualDevice() const
{
	return m_doc_man->GetVisualDevice();
}

HTML_Element* AccessibleDocument::GetFocusElement()
{
	return m_highlight_element;
}


void
AccessibleDocument::CreateAccessibleChildrenIfNeeded()
{
	if (!m_accessible_children)
	{
		m_is_ready = FALSE;
		m_accessible_children = OP_NEW(AccessibleElementManager, (this, GetActiveDocument()));
		m_accessible_children->HighlightElement(m_highlight_element);
		
		if (m_fake_loading) {
			AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateBusy));
			m_fake_loading = FALSE;
		}
		if (GetActiveDocument() && GetActiveDocument()->IsLoaded()) {
			AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateBusy));
		}
		m_is_ready = TRUE;
	}
}


BOOL
AccessibleDocument::AccessibilitySetFocus()
{
	GetVisualDevice()->SetFocus(FOCUS_REASON_OTHER);
	return TRUE;
}

OP_STATUS
AccessibleDocument::AccessibilityGetText(OpString& str)
{
	str.Empty();
	return OpStatus::OK;
}

OP_STATUS
AccessibleDocument::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	OpRect view_bounds;
	VisualDevice* vis_dev = GetVisualDevice();
	FramesDocument* doc = GetActiveDocument();
	if (vis_dev && doc)
	{
		vis_dev->GetAbsoluteViewBounds(view_bounds);
		rect.x = view_bounds.x - vis_dev->GetRenderingViewX();
		rect.y = view_bounds.y - vis_dev->GetRenderingViewY();
		rect.width = doc->Width();
		rect.height = doc->Height();
	}
	return OpStatus::OK;
}

Accessibility::ElementKind AccessibleDocument::AccessibilityGetRole() const
{
	return Accessibility::kElementKindWebView;
}

Accessibility::State AccessibleDocument::AccessibilityGetState()
{
	Accessibility::State state = GetVisualDevice()->AccessibilityGetState();
	if (!GetActiveDocument())
	{
		state |= Accessibility::kAccessibilityStateInvisible;
	}
	if (GetAccessibleFocusedChildOrSelf() == this)
	{
		state |= Accessibility::kAccessibilityStateFocused;
	}
	m_was_loading = GetActiveDocument() && (!GetActiveDocument()->IsLoaded() || m_fake_loading);
	if (m_was_loading)
	{
		state |= Accessibility::kAccessibilityStateBusy;
	}
	return state;
}

int
AccessibleDocument::GetAccessibleChildrenCount()
{
	CreateAccessibleChildrenIfNeeded();
	return m_accessible_children->GetAccessibleElementCount();
}

OpAccessibleItem*
AccessibleDocument::GetAccessibleChild(int i)
{
	CreateAccessibleChildrenIfNeeded();
	return m_accessible_children->GetAccessibleElement(i);
}

int
AccessibleDocument::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	CreateAccessibleChildrenIfNeeded();
	return m_accessible_children->GetIndexOfElement(child);
}

OpAccessibleItem*
AccessibleDocument::GetAccessibleChildOrSelfAt(int x, int y)
{
	CreateAccessibleChildrenIfNeeded();
	OpAccessibleItem* elem = m_accessible_children->GetAccessibleElementAt(x, y);
	if (elem)
		return elem;

	return this;
}

OpAccessibleItem*
AccessibleDocument::GetNextAccessibleSibling()
{
	return NULL;
}

OpAccessibleItem*
AccessibleDocument::GetPreviousAccessibleSibling()
{
	return NULL;
}

OpAccessibleItem*
AccessibleDocument::GetAccessibleFocusedChildOrSelf()
{
	VisualDevice *vd = GetVisualDevice();
	OpInputContext* focused = g_input_manager->GetKeyboardInputContext();
	while (focused)
	{
		if (focused == vd) {
			break;
		}
		focused = focused->GetParentInputContext();
	}
	if (!focused)
		return NULL;

	CreateAccessibleChildrenIfNeeded();
	OpAccessibleItem* elem = m_accessible_children->GetAccessibleFocusedElement();
	if (elem)
		return elem;

	return this;
}

OpAccessibleItem*
AccessibleDocument::GetLeftAccessibleObject()
{
	return NULL;
}

OpAccessibleItem*
AccessibleDocument::GetRightAccessibleObject()
{
	return NULL;
}

OpAccessibleItem*
AccessibleDocument::GetDownAccessibleObject()
{
	return NULL;
}

OpAccessibleItem*
AccessibleDocument::GetUpAccessibleObject()
{
	return NULL;
}

OP_STATUS
AccessibleDocument::GetAccessibleHeaders(int level, OpVector<OpAccessibleItem> &headers)
{
	return OpStatus::ERR;
}

OP_STATUS
AccessibleDocument::GetAccessibleLinks(OpVector<OpAccessibleItem> &links)
{
	if (m_accessible_children)
		return m_accessible_children->GetLinks(links);
	return OpStatus::OK;
}

void
AccessibleDocument::OnTimeOut(OpTimer* timer)
{
	if (GetActiveDocument())
	{
		CreateAccessibleChildrenIfNeeded();
		m_accessible_children->GetAccessibleElementCount();
	}
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
