/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeRootNode.h"
#include "modules/accessibility/AccessibleDocument.h"

AccessibilityTreeRootNode::AccessibilityTreeRootNode(AccessibleDocument* document)
	: AccessibilityTreeNode(NULL, document)
{
}

OpAccessibleItem* AccessibilityTreeRootNode::GetAccessibleObject() const
{
	return GetDocument();
}

AccessibilityTreeNode::TreeNodeType AccessibilityTreeRootNode::NodeType() const
{
	return TREE_NODE_TYPE_ROOT;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
