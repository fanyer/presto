/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_TREE_IMAGE_NODE_H
#define ACCESSIBILITY_TREE_IMAGE_NODE_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeObjectNode.h"

class AccessibilityTreeImageNode : public AccessibilityTreeObjectNode
{
public:
	AccessibilityTreeImageNode(HTML_Element* helm, AccessibilityTreeNode* parent);

// AccessibilityTreeObjectNode
	virtual OP_STATUS GetText(OpString& str) const;
	virtual TreeNodeType NodeType() const;

// OpAccessibilityExtensionListener
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_IMAGE_NODE_H
