/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeNode.h"
#include "modules/accessibility/AccessibleDocument.h"
#include "modules/accessibility/acctree/AccessibilityTreeLabelNode.h"

AccessibilityTreeNode::AccessibilityTreeNode(AccessibilityTreeNode* parent, AccessibleDocument* document)
	: m_parent(parent)
	, m_document(m_parent ? m_parent->GetDocument() : document)
	, m_label(NULL)
{
	OP_ASSERT(m_document);
	if (m_parent)
		m_parent->AddChild(this);
}

AccessibilityTreeNode::~AccessibilityTreeNode()
{
	DeleteAllChildren();
	if (m_parent)
		m_parent->RemoveChild(this);
	if (m_label)
		m_label->RemoveControl(this);
}

void AccessibilityTreeNode::SetBounds(const OpRect& bounds)
{
	m_bounds = bounds;
}

OP_STATUS AccessibilityTreeNode::GetBounds(OpRect& bounds) const
{
	if (!m_bounds.IsEmpty())
		bounds = m_bounds;
	else if (m_children.GetCount())
	{
		OpRect sub_bounds;
		int i, count = m_children.GetCount();
		for (i=0; i<count; i++)
		{
			if (OpStatus::IsSuccess(m_children.Get(i)->GetBounds(sub_bounds)))
			{
				bounds.UnionWith(sub_bounds);
			}
		}
	}
	return OpStatus::OK;
}

BOOL AccessibilityTreeNode::HitTest(const OpPoint& pt) const
{
	if (m_bounds.Contains(pt))
		return TRUE;
	else
	{
		int i, count = m_children.GetCount();
		for (i = 0; i < count; i++)
		{
			if (m_children.Get(i)->HitTest(pt))
				return TRUE;
		}
	}
	return FALSE;
}

BOOL AccessibilityTreeNode::Focused() const
{
	int i, count = m_children.GetCount();
	for (i = 0; i < count; i++)
	{
		if (m_children.Get(i)->Focused())
			return TRUE;
	}
	return FALSE;
}

BOOL AccessibilityTreeNode::ClickSelf()
{
	return FALSE;
}

OP_STATUS AccessibilityTreeNode::GetText(OpString& str) const
{
	str.Empty();
	return OpStatus::OK;
}

BOOL AccessibilityTreeNode::Compare(const AccessibilityTreeNode* node) const
{
	if (node == this)
		return true;

	OpRect my_bounds, other_bounds;
	if (NodeType() == node->NodeType())
	{
		GetBounds(my_bounds);
		if (!my_bounds.IsEmpty())
		{
			node->GetBounds(other_bounds);
			if (my_bounds.Equals(other_bounds))
			{
				OpString my_text, other_text;
				GetText(my_text);
				if (my_text.Length())
				{
					node->GetText(other_text);
					if (my_text == other_text)
					{
						return TRUE;
					}
				}
			}
		}
	}
	// OK, bound/text comparison failed. Try to see if this might be a form container.
	if (m_children.GetCount() == 1)
	{
		if (node->GetChildrenCount() == 1)
		{
			if (m_children.Get(0)->Compare(node->GetChild(0)))
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

void AccessibilityTreeNode::Merge(AccessibilityTreeNode* node)
{
	int i = 0, j = 0;
	AccessibilityTreeNode* my_child = m_children.Get(i);
	AccessibilityTreeNode* your_child = node->m_children.Get(j);
	while (my_child || your_child)
	{
		if (my_child && !your_child)
		{
			OP_DELETE(my_child);
			my_child = m_children.Get(i);
		}
		else if (your_child && !my_child)
		{
			your_child->Reparent(this);
			your_child = node->m_children.Get(j);
		}
		else
		{
			if (my_child->Compare(your_child))
			{
				my_child->Merge(your_child);
				i++;
				j++;
			}
			else
			{
				int j2 = j+1;
				your_child = node->m_children.Get(j2);
				while (your_child)
				{
					if (my_child->Compare(your_child))
					{
						int diff = j2 - j;
						my_child->Merge(your_child);
						for (int k = 0; k < diff; k++)
							node->m_children.Remove(j)->Reparent(this, i + k);
						j++;
						i+= 1+diff;
						break;
					}
					j2++;
					your_child = node->m_children.Get(j2);
				}
				if (!your_child)
				{
					OP_DELETE(my_child);
				}
			}
			my_child = m_children.Get(i);
			your_child = node->m_children.Get(j);
		}
	}
	OpRect bounds;
	node->GetBounds(bounds);
	if (!m_bounds.Equals(bounds))
	{
		BOOL moved = (bounds.x != m_bounds.x) || (bounds.y != m_bounds.y);
		BOOL resized = (bounds.width != m_bounds.width) || (bounds.height != m_bounds.height);
		SetBounds(bounds);
		BoundsChanged(moved, resized);
	}
}

void AccessibilityTreeNode::ParsingDone()
{
	int i, count = m_children.GetCount();
	for (i = 0; i < count; i++)
	{
		m_children.Get(i)->ParsingDone();
	}
}


void AccessibilityTreeNode::OnFocus() const
{
	OpAccessibleItem* ext = GetAccessibleObject();
	if (ext)
	{
		ext->AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateFocused));
	}
}

void AccessibilityTreeNode::OnBlur() const
{
	OpAccessibleItem* ext = GetAccessibleObject();
	if (ext)
	{
		ext->AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateFocused));
	}
}

void AccessibilityTreeNode::OnReorder() const
{
}

void AccessibilityTreeNode::UnreferenceHTMLElement(const HTML_Element* helm)
{
	int i, count = m_children.GetCount();
	for (i = 0; i < count; i++)
	{
		m_children.Get(i)->UnreferenceHTMLElement(helm);
	}
}

void AccessibilityTreeNode::UnreferenceWidget(const OpWidget* widget)
{
	int i, count = m_children.GetCount();
	for (i = 0; i < count; i++)
	{
		m_children.Get(i)->UnreferenceWidget(widget);
	}
}

AccessibilityTreeNode* AccessibilityTreeNode::FindElement(HTML_Element* helm)
{
	int i, count = m_children.GetCount();
	for (i = 0; i < count; i++)
	{
		AccessibilityTreeNode* elem = m_children.Get(i)->FindElement(helm);
		if (elem)
			return elem;
	}
	return NULL;
}

AccessibilityTreeNode::TreeNodeType AccessibilityTreeNode::NodeType() const
{
	return TREE_NODE_TYPE_UNKNOWN;
}

void AccessibilityTreeNode::BoundsChanged(BOOL moved, BOOL resized)
{
}

int AccessibilityTreeNode::GetChildrenCount() const
{
	return m_children.GetCount();
}

const AccessibilityTreeNode* AccessibilityTreeNode::GetChild(int i) const
{
	return m_children.Get(i);
}

const AccessibilityTreeNode* AccessibilityTreeNode::GetChildAt(const OpPoint& pt) const
{
	int i, count = m_children.GetCount();
	for (i = 0; i < count; i++)
	{
		AccessibilityTreeNode* child = m_children.Get(i);
		if (child->HitTest(pt))
			return child;
	}
	return NULL;
}

const AccessibilityTreeNode* AccessibilityTreeNode::GetFocusedChild() const
{
	int i, count = m_children.GetCount();
	for (i = 0; i < count; i++)
	{
		AccessibilityTreeNode* child = m_children.Get(i);
		if (child->Focused())
			return child;
	}
	return NULL;
}

const AccessibilityTreeNode* AccessibilityTreeNode::GetNextSibling() const
{
	if (m_parent)
		return m_parent->GetChildAfter(this);
	else
		return NULL;
}

const AccessibilityTreeNode* AccessibilityTreeNode::GetPrevSibling() const
{
	if (m_parent)
		return m_parent->GetChildBefore(this);
	else
		return NULL;
}

void AccessibilityTreeNode::AddChild(AccessibilityTreeNode* child, int index)
{
	if (index < 0 || index > int(m_children.GetCount()))
		m_children.Add(child);
	else
		m_children.Insert(index, child);
	OnReorder();
}

void AccessibilityTreeNode::RemoveChild(AccessibilityTreeNode* child)
{
	m_children.RemoveByItem(child);
	OnReorder();
}

const AccessibilityTreeNode* AccessibilityTreeNode::GetParent() const
{
	return m_parent;
}

void AccessibilityTreeNode::Reparent(AccessibilityTreeNode* parent, int index)
{
	if (parent != m_parent)
	{
		if (m_parent)
			m_parent->RemoveChild(this);
		m_parent = parent;
		if (m_parent)
			m_parent->AddChild(this, index);
	}
}

AccessibleDocument* AccessibilityTreeNode::GetDocument() const
{
	return m_document;
}

const AccessibilityTreeNode* AccessibilityTreeNode::GetChildBefore(const AccessibilityTreeNode* node) const
{
	int i, count = m_children.GetCount();
	AccessibilityTreeNode* prev_node = NULL;
	for (i = 0; i < count; i++)
	{
		AccessibilityTreeNode* child = m_children.Get(i);
		if (child == node)
			return prev_node;
		prev_node = child;
	}
	return NULL;
}

const AccessibilityTreeNode* AccessibilityTreeNode::GetChildAfter(const AccessibilityTreeNode* node) const
{
	int i, count = m_children.GetCount();
	for (i = 0; i < count; i++)
	{
		if (node == m_children.Get(i))
		{
			return m_children.Get(i + 1);
		}
	}
	return NULL;
}

const AccessibilityTreeNode* AccessibilityTreeNode::FindItemByID(const uni_char* name) const
{
	int i, count = m_children.GetCount();
	const AccessibilityTreeNode* node;
	for (i = 0; i < count; i++)
	{
		node = m_children.Get(i)->FindItemByID(name);
		if (node)
		{
			return node;
		}
	}
	return NULL;
}

void AccessibilityTreeNode::SetLabel(AccessibilityTreeLabelNode* label) const
{
	if (m_label)
		m_label->RemoveControl(this);
	m_label = label;
}
 
void AccessibilityTreeNode::DeleteAllChildren()
{
	UINT c;
	while ((c = m_children.GetCount()) > 0)
		OP_DELETE(m_children.Get(c - 1));
}

OP_STATUS AccessibilityTreeNode::GetLinks(OpVector<AccessibilityTreeURLNode>& links)
{
	int i, count = m_children.GetCount();
	for (i = 0; i < count; i++)
	{
		m_children.Get(i)->GetLinks(links);
	}
	return OpStatus::OK;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
