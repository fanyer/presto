/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeImageNode.h"
#include "modules/accessibility/acc_utils.h"
#include "modules/logdoc/htm_elm.h"

AccessibilityTreeImageNode::AccessibilityTreeImageNode(HTML_Element* helm, AccessibilityTreeNode* parent)
	: AccessibilityTreeObjectNode(helm, parent)
{
}

OP_STATUS AccessibilityTreeImageNode::GetText(OpString& str) const
{
	const HTML_Element* helm = GetHTMLElement();
	if (helm)
	{
		const uni_char* text = helm->GetStringAttr(ATTR_ALT);
		if (!text || !*text)
			text = helm->GetStringAttr(ATTR_LONGDESC);

		if (text)
			CopyStringClean(str, text);
	}
	return OpStatus::OK;
}

AccessibilityTreeNode::TreeNodeType AccessibilityTreeImageNode::NodeType() const
{
	return TREE_NODE_TYPE_IMAGE;
}

Accessibility::ElementKind AccessibilityTreeImageNode::AccessibilityGetRole() const
{
	return Accessibility::kElementKindImage;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
