/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeWidgetNode.h"
#include "modules/accessibility/AccessibleDocument.h"
#include "modules/widgets/OpWidget.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/display/vis_dev.h"

AccessibilityTreeWidgetNode::AccessibilityTreeWidgetNode(HTML_Element* helm, OpWidget* widget, AccessibilityTreeNode* parent)
	: AccessibilityTreeNode(parent)
	, m_elem(helm)
	, m_widget(widget)
{
	OP_ASSERT(helm);
	OP_ASSERT(m_widget);
	m_widget->SetAccessibleParent(GetParent()->GetAccessibleObject());
}

AccessibilityTreeWidgetNode::~AccessibilityTreeWidgetNode()
{
	if (m_widget)
	{
		// The parent of the control is likely dying. Kill the accessibility extension, since the parent of it may be invalid now.
		m_widget->SetAccessibleParent(NULL);
		m_widget->DisableAccessibleItem();
	}
}

OpWidget* AccessibilityTreeWidgetNode::GetWidget() const
{
	return m_widget;
}

BOOL AccessibilityTreeWidgetNode::Compare(const AccessibilityTreeNode* node) const
{
	if (NodeType() == node->NodeType())
	{
		const AccessibilityTreeWidgetNode* widget_node = static_cast<const AccessibilityTreeWidgetNode*>(node);
		if (m_widget && (m_widget == widget_node->m_widget) && (m_elem == widget_node->m_elem))
		{
			return TRUE;
		}
	}
	return FALSE;
}

void AccessibilityTreeWidgetNode::Merge(AccessibilityTreeNode* node)
{
	OP_ASSERT(node->NodeType() == TREE_NODE_TYPE_WIDGET);
	m_widget->SetAccessibleParent(GetParent()->GetAccessibleObject());
	static_cast<AccessibilityTreeWidgetNode*>(node)->m_widget = NULL;
	AccessibilityTreeNode::Merge(node);
}

void AccessibilityTreeWidgetNode::Reparent(AccessibilityTreeNode* parent, int index)
{
	AccessibilityTreeNode::Reparent(parent, index);
	m_widget->SetAccessibleParent(GetParent()->GetAccessibleObject());
}

void AccessibilityTreeWidgetNode::UnreferenceHTMLElement(const HTML_Element* helm)
{
	if (helm == m_elem)
	{
		m_elem = NULL;
	}
	AccessibilityTreeNode::UnreferenceHTMLElement(helm);
}

void AccessibilityTreeWidgetNode::UnreferenceWidget(const OpWidget* widget)
{
	if (widget == m_widget)
	{
		m_widget->SetAccessibleParent(NULL);
		m_widget = NULL;
	}
	AccessibilityTreeNode::UnreferenceWidget(widget);
}

AccessibilityTreeNode* AccessibilityTreeWidgetNode::FindElement(HTML_Element* helm)
{
	if (helm == m_elem)
	{
		return this;
	}
	return AccessibilityTreeNode::FindElement(helm);
}

BOOL AccessibilityTreeWidgetNode::HitTest(const OpPoint& pt) const
{
	if (m_widget)
	{
		OpRect t = m_widget->GetRect(FALSE);
		// pt is in canvas-relative coordinates. Convert to viewbox-relative.
		OpPoint local = pt;
		VisualDevice* vd = GetDocument()->GetVisualDevice();
		local.x -= vd->GetRenderingViewX();
		local.y -= vd->GetRenderingViewY();
		return t.Contains(local);
	}
	return FALSE;
}

BOOL AccessibilityTreeWidgetNode::Focused() const
{
	return (m_widget && m_widget == g_input_manager->GetKeyboardInputContext());
}

BOOL AccessibilityTreeWidgetNode::ClickSelf()
{
	m_widget->AccessibilityClicked();
	return TRUE;
}

OP_STATUS AccessibilityTreeWidgetNode::GetText(OpString& str) const
{
	return m_widget->AccessibilityGetText(str);
}

const AccessibilityTreeNode* AccessibilityTreeWidgetNode::FindItemByID(const uni_char* name) const
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

AccessibilityTreeNode::TreeNodeType AccessibilityTreeWidgetNode::NodeType() const
{
	return TREE_NODE_TYPE_WIDGET;
}

OpAccessibleItem* AccessibilityTreeWidgetNode::GetAccessibleObject() const
{
	return m_widget;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
