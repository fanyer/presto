/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeObjectNode.h"
#include "modules/accessibility/AccessibleDocument.h"
#include "modules/accessibility/acc_utils.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"

AccessibilityTreeObjectNode::AccessibilityTreeObjectNode(HTML_Element* helm, AccessibilityTreeNode* parent)
	: AccessibilityTreeNode(parent)
	, m_accessibility_ready(TRUE)
	, m_elem(helm)
	, m_labelelem(NULL)
{
}

AccessibilityTreeObjectNode::~AccessibilityTreeObjectNode()
{
	m_accessibility_ready = FALSE;
	// Ensure that m_accessible leaves no orphaned children.
	DeleteAllChildren();
}

void AccessibilityTreeObjectNode::UnreferenceHTMLElement(const HTML_Element* helm)
{
	if (helm == m_elem)
	{
		m_elem = NULL;
	}
	AccessibilityTreeNode::UnreferenceHTMLElement(helm);
}

AccessibilityTreeNode* AccessibilityTreeObjectNode::FindElement(HTML_Element* helm)
{
	if (helm == m_elem)
	{
		return this;
	}
	return AccessibilityTreeNode::FindElement(helm);
}

BOOL AccessibilityTreeObjectNode::ClickSelf()
{
	if (m_elem)
	{
		GetDocument()->GetActiveDocument()->HandleEvent(ONCLICK, NULL, m_elem, SHIFTKEY_NONE);
		return TRUE;
	}
	return AccessibilityTreeNode::ClickSelf();
}

BOOL AccessibilityTreeObjectNode::AccessibilitySetFocus()
{
	GetDocument()->AccessibilitySetFocus();
	if (m_elem)
	{
		FramesDocument* doc = GetDocument()->GetActiveDocument();
		doc->GetHtmlDocument()->HighlightElement(m_elem, HTML_Document::FOCUS_ORIGIN_UNKNOWN, FALSE, TRUE);
		return TRUE;
	}
	return FALSE;
}

OP_STATUS AccessibilityTreeObjectNode::GetText(OpString& str) const
{
	str.Empty();
	if (m_elem)
	{
		if (m_elem->Type() == HE_TABLE)
		{
			str.Set(m_elem->GetStringAttr(ATTR_SUMMARY));
			return OpStatus::OK;
		}
		int len = m_elem->GetTextContentLength();
		uni_char* text = OP_NEWA(uni_char, len+1);
		if (text)
		{
			m_elem->GetTextContent(text, len);
			CopyStringClean(str, text);
			OP_DELETEA(text);
			return OpStatus::OK;
		}
	}
	return AccessibilityTreeNode::GetText(str);
}

BOOL AccessibilityTreeObjectNode::Compare(const AccessibilityTreeNode* node) const
{
	if (NodeType() == node->NodeType())
	{
		const AccessibilityTreeObjectNode* obj_node = static_cast<const AccessibilityTreeObjectNode*>(node);
		if (m_elem == obj_node->GetHTMLElement())
		{
			if (m_elem)
				return TRUE;
			else
				return AccessibilityTreeNode::Compare(node);
		}
	}
	return FALSE;
}

void AccessibilityTreeObjectNode::Merge(AccessibilityTreeNode* node)
{
	AccessibilityTreeNode::Merge(node);
	m_elem = static_cast<AccessibilityTreeObjectNode*>(node)->m_elem;
}

void AccessibilityTreeObjectNode::BoundsChanged(BOOL moved, BOOL resized)
{
	if (moved)
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventMoved));
	if (resized)
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventResized));
}

void AccessibilityTreeObjectNode::OnReorder() const
{
	const_cast<AccessibilityTreeObjectNode *>(this)->AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityEventReorder));
}

const AccessibilityTreeNode* AccessibilityTreeObjectNode::FindItemByID(const uni_char* name) const
{
	OP_ASSERT(name && *name);
	if (m_elem)
	{
		const uni_char* id = m_elem->GetStringAttr(ATTR_ID);
		if (id && (uni_strcmp(name, id) == 0))
		{
			return this;
		}
	}
	return AccessibilityTreeNode::FindItemByID(name);
}

AccessibilityTreeNode::TreeNodeType AccessibilityTreeObjectNode::NodeType() const
{
	return TREE_NODE_TYPE_OBJECT;
}

OP_STATUS AccessibilityTreeObjectNode::AccessibilityClicked()
{
	ClickSelf();
	return OpStatus::OK;
}

OP_STATUS AccessibilityTreeObjectNode::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	OpRect doc_bounds;
	GetDocument()->AccessibilityGetAbsolutePosition(doc_bounds);
	GetBounds(rect);
	rect.x += doc_bounds.x;
	rect.y += doc_bounds.y;
	return OpStatus::OK;
}

OP_STATUS AccessibilityTreeObjectNode::AccessibilityGetText(OpString& str)
{
	return GetText(str);
}

OP_STATUS AccessibilityTreeObjectNode::AccessibilityGetDescription(OpString& str)
{
	str.Empty();
	if (m_elem && m_elem->Type() == HE_TABLE)
	{
		if (m_labelelem
			//&& m_labelelem->Type() == HE_CAPTION
			)
		{
			const uni_char* text = m_labelelem->TextContent();
			if (text)
			{
				CopyStringClean(str, text);
				return OpStatus::OK;
			}
		}
	}
	return OpStatus::OK;
}

Accessibility::State AccessibilityTreeObjectNode::AccessibilityGetState()
{
	Accessibility::State state = GetDocument()->AccessibilityGetState();
	if (m_elem && (m_elem == GetDocument()->GetFocusElement()))
		state |= Accessibility::kAccessibilityStateFocused;
	else
		state &= ~Accessibility::kAccessibilityStateFocused;
	return state;
}

Accessibility::ElementKind AccessibilityTreeObjectNode::AccessibilityGetRole() const
{
	if (m_elem)
	{
		switch (m_elem->Type())
		{
			case HE_TABLE:
				return Accessibility::kElementKindTable;
			case HE_TR:
				return Accessibility::kElementKindListRow;
			case HE_TD:
				return Accessibility::kElementKindListCell;
			case HE_TH:
				return Accessibility::kElementKindListHeader;
			case HE_DIV:
				return Accessibility::kElementKindSection;
			case HE_P:
			case HE_CENTER:
			case HE_ADDRESS:
				return Accessibility::kElementKindParagraph;
			case HE_FORM:
				return Accessibility::kElementKindForm;
		}
	}
	return Accessibility::kElementKindView;
}

int AccessibilityTreeObjectNode::GetAccessibleChildrenCount()
{
	return GetChildrenCount();
}

OpAccessibleItem* AccessibilityTreeObjectNode::GetAccessibleChild(int n)
{
	const AccessibilityTreeNode* child = GetChild(n);
	if (child)
		return child->GetAccessibleObject();
	return NULL;
}

int AccessibilityTreeObjectNode::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int n = GetChildrenCount();
	for (int i = 0; i < n; i++)
	{
		if (child == GetAccessibleChild(i))
			return i;
	}

	return Accessibility::NoSuchChild;
}

OpAccessibleItem* AccessibilityTreeObjectNode::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpRect doc_bounds;
	OpPoint relPos;
	GetDocument()->AccessibilityGetAbsolutePosition(doc_bounds);
	relPos.x = x - doc_bounds.x;
	relPos.y = y - doc_bounds.y;
	const AccessibilityTreeNode* child = GetChildAt(relPos);
	if (child)
		return child->GetAccessibleObject();
	else if (HitTest(relPos))
		return this;
	return NULL;
}

OpAccessibleItem* AccessibilityTreeObjectNode::GetNextAccessibleSibling()
{
	const AccessibilityTreeNode* next = GetNextSibling();
	if (next)
		return next->GetAccessibleObject();
	return NULL;
}

OpAccessibleItem* AccessibilityTreeObjectNode::GetPreviousAccessibleSibling()
{
	const AccessibilityTreeNode* prev = GetPrevSibling();
	if (prev)
		return prev->GetAccessibleObject();
	return NULL;
}

OpAccessibleItem* AccessibilityTreeObjectNode::GetLeftAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* AccessibilityTreeObjectNode::GetRightAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* AccessibilityTreeObjectNode::GetDownAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* AccessibilityTreeObjectNode::GetUpAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* AccessibilityTreeObjectNode::GetAccessibleFocusedChildOrSelf()
{
	const AccessibilityTreeNode* child = GetFocusedChild();
	if (child)
		return child->GetAccessibleObject();
	else if (Focused())
		return this;
	return NULL;
}

const HTML_Element* AccessibilityTreeObjectNode::GetHTMLElement() const
{
	return m_elem;
}

BOOL AccessibilityTreeObjectNode::FlattenTextContent()
{
	if (m_elem)
	{
		switch (m_elem->Type())
		{
			case HE_FIELDSET:
			case HE_TABLE:
				return TRUE;
		}
	}
	return FALSE;
}

OP_STATUS AccessibilityTreeObjectNode::AccessibilityScrollTo(Accessibility::ScrollTo scroll_to)
{
	SCROLL_ALIGN scroll;

	switch(scroll_to)
	{
		case Accessibility::kScrollToTop:
			scroll = SCROLL_ALIGN_TOP;
			break;
		case Accessibility::kScrollToBottom:
			scroll = SCROLL_ALIGN_BOTTOM;
			break;
		case Accessibility::kScrollToAnywhere:
			scroll = SCROLL_ALIGN_NEAREST;
			break;
		default:
			OP_ASSERT(!"not yet implemented");
			scroll = SCROLL_ALIGN_TOP;
	}

	if (GetDocument()->GetActiveDocument()->GetHtmlDocument()->ScrollToElement((HTML_Element*)GetHTMLElement(), scroll, FALSE, VIEWPORT_CHANGE_REASON_HIGHLIGHT))
		return OpStatus::OK;
	else
		return OpStatus::ERR;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
