/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeTextNode.h"
#include "modules/accessibility/acc_utils.h"
#include "modules/logdoc/htm_elm.h"

AccessibilityTreeTextNode::AccessibilityTreeTextNode(HTML_Element* helm, AccessibilityTreeNode* parent)
	: AccessibilityTreeObjectNode(helm, parent)
	, m_start_offset(0)
	, m_length(-1)
{
}

void AccessibilityTreeTextNode::SetTextRange(long offset, long length)
{
	m_start_offset = offset;
	m_length = length;
}

OP_STATUS AccessibilityTreeTextNode::GetText(OpString& str) const
{
	str.Empty();
	const HTML_Element* helm = GetHTMLElement();
	if (helm)
	{
		const uni_char* text = helm->TextContent();
		if (text)
		{
			CopyStringClean(str, text + m_start_offset, m_length);
			return OpStatus::OK;
		}
	}
	return AccessibilityTreeNode::GetText(str);
}

AccessibilityTreeNode::TreeNodeType AccessibilityTreeTextNode::NodeType() const
{
	return TREE_NODE_TYPE_TEXT;
}

void AccessibilityTreeTextNode::Merge(AccessibilityTreeNode* node)
{
	AccessibilityTreeObjectNode::Merge(node);
	AccessibilityTreeTextNode* text_node = static_cast<AccessibilityTreeTextNode*>(node);
	m_start_offset = text_node->m_start_offset;
	m_length = text_node->m_length;
}

Accessibility::ElementKind AccessibilityTreeTextNode::AccessibilityGetRole() const
{
	return Accessibility::kElementKindStaticText;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
