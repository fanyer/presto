/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeURLNode.h"
#include "modules/accessibility/AccessibleDocument.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/url/url2.h"
#include "modules/doc/frm_doc.h"

AccessibilityTreeURLNode::AccessibilityTreeURLNode(HTML_Element* helm, AccessibilityTreeNode* parent)
	: AccessibilityTreeObjectNode(helm, parent)
{
}

AccessibilityTreeURLNode::~AccessibilityTreeURLNode()
{
	if (m_links_vector)
		m_links_vector->RemoveByItem(this);
}

const uni_char* AccessibilityTreeURLNode::GetURL() const
{
	const HTML_Element* helm = GetHTMLElement();
	if (helm)
	{
		URL url = const_cast<HTML_Element*>(helm)->GetAnchor_URL(GetDocument()->GetActiveDocument());
		return url.UniName(PASSWORD_SHOW_UNESCAPE_URL);
	}
	return NULL;
}

OP_STATUS AccessibilityTreeURLNode::GetText(OpString& str) const
{
	// Base strings on children.
	long i, count = GetChildrenCount();
	BOOL image = true, first = TRUE;
	for (i = 0; i < count; i++)
	{
		const AccessibilityTreeNode* node = GetChild(i);
		TreeNodeType type = node->NodeType();
		if (image && type == TREE_NODE_TYPE_TEXT) {
			str.Empty();
			image = FALSE;
			first = TRUE;
		}
		if (type == TREE_NODE_TYPE_TEXT || (image && type == TREE_NODE_TYPE_IMAGE))
		{
			OpString t;
			GetChild(i)->GetText(t);
			if (!first)
				str.Append(UNI_L(" "));
			str.Append(t);
		}
		first = FALSE;
	}
	return OpStatus::OK;
}

AccessibilityTreeNode::TreeNodeType AccessibilityTreeURLNode::NodeType() const
{
	return TREE_NODE_TYPE_LINK;
}

Accessibility::ElementKind AccessibilityTreeURLNode::AccessibilityGetRole() const
{
	return Accessibility::kElementKindLink;
}

OP_STATUS AccessibilityTreeURLNode::AccessibilityGetURL(OpString& str)
{
	str.Set(GetURL());
	return OpStatus::OK;
}

OP_STATUS AccessibilityTreeURLNode::GetLinks(OpVector<AccessibilityTreeURLNode>& links)
{
	RETURN_IF_ERROR(links.Add(this));
	return AccessibilityTreeNode::GetLinks(links);
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
