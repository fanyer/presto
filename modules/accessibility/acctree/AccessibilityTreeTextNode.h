/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_TREE_TEXT_NODE_H
#define ACCESSIBILITY_TREE_TEXT_NODE_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeObjectNode.h"

class AccessibilityTreeTextNode : public AccessibilityTreeObjectNode
{
public:
	AccessibilityTreeTextNode(HTML_Element* helm, AccessibilityTreeNode* parent);
	void SetTextRange(long offset, long length);

// AccessibilityTreeNode
	virtual OP_STATUS GetText(OpString& str) const;
	virtual TreeNodeType NodeType() const;
	virtual void Merge(AccessibilityTreeNode* node);

// OpAccessibilityExtensionListener
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
private:
	long m_start_offset;
	long m_length;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_TEXT_NODE_H
