/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_TREE_ROOT_NODE_H
#define ACCESSIBILITY_TREE_ROOT_NODE_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeNode.h"

class AccessibilityTreeRootNode : public AccessibilityTreeNode
{
public:
	AccessibilityTreeRootNode(AccessibleDocument* document);

// AccessibilityTreeNode
	virtual OpAccessibleItem* GetAccessibleObject() const;
	virtual const AccessibilityTreeNode* GetRoot() const {return this;}
	virtual TreeNodeType NodeType() const;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_ROOT_NODE_H
