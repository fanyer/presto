/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeRadioGroupNode.h"

AccessibilityTreeRadioGroupNode::AccessibilityTreeRadioGroupNode(FormRadioGroup* group, AccessibilityTreeNode* parent)
	: AccessibilityTreeObjectNode(NULL, parent)
	, m_group(group)
{
}

const FormRadioGroup* AccessibilityTreeRadioGroupNode::GetGroup() const
{
	return m_group;
}

OP_STATUS AccessibilityTreeRadioGroupNode::GetBounds(OpRect& bounds) const
{
	long i, count;
	if ((count = GetChildrenCount()))
	{
		bounds.Empty();
		for (i = 0; i < count; i++)
		{
			OpRect t;
			GetChild(i)->GetBounds(t);
			bounds.UnionWith(t);
		}
	}
	return OpStatus::OK;
}

AccessibilityTreeNode::TreeNodeType AccessibilityTreeRadioGroupNode::NodeType() const
{
	return TREE_NODE_TYPE_RADIOGROUP;
}

BOOL AccessibilityTreeRadioGroupNode::Compare(const AccessibilityTreeNode* node) const
{
	return (m_group == static_cast<const AccessibilityTreeRadioGroupNode*>(node)->m_group);
}

Accessibility::ElementKind AccessibilityTreeRadioGroupNode::AccessibilityGetRole() const
{
	return Accessibility::kElementKindRadioTabGroup;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
