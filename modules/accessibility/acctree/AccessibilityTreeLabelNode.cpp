/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeLabelNode.h"
#include "modules/accessibility/acctree/AccessibilityTreeWidgetNode.h"
#include "modules/accessibility/AccessibleDocument.h"
#include "modules/widgets/OpWidget.h"
#include "modules/logdoc/html_att.h"
#include "modules/logdoc/htm_elm.h"

AccessibilityTreeLabelNode::AccessibilityTreeLabelNode(HTML_Element* helm, AccessibilityTreeNode* parent)
	: AccessibilityTreeObjectNode(helm, parent)
	, AccessibleOpWidgetLabel()
	, m_labeled_control(NULL)
	, m_fallback_labeled_control(NULL)
{
}

AccessibilityTreeLabelNode::~AccessibilityTreeLabelNode()
{
	if (m_labeled_control)
		m_labeled_control->SetLabel(NULL);
	if (m_fallback_labeled_control)
		m_fallback_labeled_control->SetLabel(NULL);
}

void AccessibilityTreeLabelNode::SetLabeledControl(const AccessibilityTreeNode * control)
{
	if (m_labeled_control)
		m_labeled_control->SetLabel(NULL);
	m_labeled_control = control;
	if (m_labeled_control)
		m_labeled_control->SetLabel(this);
}

void AccessibilityTreeLabelNode::RemoveControl(const AccessibilityTreeNode * control)
{
	if (control == m_labeled_control)
		m_labeled_control = NULL;
	if (control == m_fallback_labeled_control)
		m_fallback_labeled_control = NULL;
}

void AccessibilityTreeLabelNode::ParsingDone()
{
	if (m_fallback_labeled_control && !m_labeled_control)
	{
		m_labeled_control = m_fallback_labeled_control;
		m_fallback_labeled_control = NULL;
	}
	if (!m_labeled_control)
	{
		// This will happen if the control is not the child of the label element
		const HTML_Element * helm = GetHTMLElement();
		if (helm)
		{
			const uni_char* name = helm->GetStringAttr(ATTR_FOR);
			if (name && *name)
			{
				const AccessibilityTreeNode* control = GetRoot()->FindItemByID(name);
				if (control)
				{
					SetLabeledControl(control);
				}
			}
		}
	}
	if (m_labeled_control)
	{
		OpWidget* label_widget = NULL;
		if (m_labeled_control->NodeType() == TREE_NODE_TYPE_WIDGET)
		{
			label_widget = static_cast<const AccessibilityTreeWidgetNode*>(m_labeled_control)->GetWidget();
			if (label_widget && label_widget->GetType() == OpTypedObject::WIDGET_TYPE_FILECHOOSER_EDIT)
			{
				OpWidget* child_widget = label_widget->GetFirstChild();
				if (child_widget)
				{
					label_widget = child_widget;
				}
			}
		}
		AssociateLabelWithOpWidget(label_widget);
	}
	AccessibilityTreeObjectNode::ParsingDone();
}

OP_STATUS AccessibilityTreeLabelNode::GetText(OpString& str) const
{
	AccessibilityTreeObjectNode::GetText(str);
	if (!str.Length())
	{
		int count = GetChildrenCount();
		for (int i = 0; i < count; i++)
		{
			OpString text;
			if (GetChild(i)->NodeType() != TREE_NODE_TYPE_WIDGET)
			{
				GetChild(i)->GetText(text);
				str.Append(text);
			}
		}
	}
	return OpStatus::OK;
}

AccessibilityTreeNode::TreeNodeType AccessibilityTreeLabelNode::NodeType() const
{
	return TREE_NODE_TYPE_LABEL;
}
void AccessibilityTreeLabelNode::Merge(AccessibilityTreeNode* node)
{
	OP_ASSERT(node->NodeType() == TREE_NODE_TYPE_LABEL);
	AccessibilityTreeLabelNode* label_node = static_cast<AccessibilityTreeLabelNode*>(node);
	if (m_fallback_labeled_control)
		m_fallback_labeled_control->SetLabel(NULL);
	m_fallback_labeled_control = label_node->m_labeled_control;
	label_node->m_labeled_control = NULL;
	if (m_fallback_labeled_control)
		m_fallback_labeled_control->SetLabel(this);
	AccessibilityTreeNode::Merge(node);
}
/*
int AccessibilityTreeLabelNode::GetAccessibleChildrenCount()
{
	return 0;
}

OpAccessibilityExtension* AccessibilityTreeLabelNode::GetAccessibleChild(int n)
{
	return NULL;
}

OpAccessibilityExtension* AccessibilityTreeLabelNode::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpRect doc_bounds;
	OpPoint relPos;
	GetDocument()->AccessibilityGetAbsolutePosition(doc_bounds);
	relPos.x = x - doc_bounds.x;
	relPos.y = y - doc_bounds.y;
	if (HitTest(relPos))
		return GetOrCreateAccessibleObject();
	return NULL;
}

OpAccessibilityExtension* AccessibilityTreeLabelNode::GetAccessibleFocusedChildOrSelf()
{
	if (Focused())
		return GetOrCreateAccessibleObject();
	return NULL;
}
*/
OpAccessibleItem* AccessibilityTreeLabelNode::GetAccessibleControlForLabel()
{
	return GetControlExtension();
}

Accessibility::ElementKind AccessibilityTreeLabelNode::AccessibilityGetRole() const
{
	if (GetOpWidget())
		return Accessibility::kElementKindLabel;
	else
		return Accessibility::kElementKindStaticText;
}

OpAccessibleItem* AccessibilityTreeLabelNode::GetLabelExtension()
{
	return GetAccessibleObject();
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
