/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_TREE_URL_NODE_H
#define ACCESSIBILITY_TREE_URL_NODE_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeObjectNode.h"

class AccessibilityTreeURLNode : public AccessibilityTreeObjectNode
{
friend class AccessibleElementTraversalObject;

public:
	AccessibilityTreeURLNode(HTML_Element* helm, AccessibilityTreeNode* parent);
	virtual ~AccessibilityTreeURLNode();
	const uni_char* GetURL() const;

// AccessibilityTreeObjectNode
	virtual OP_STATUS GetText(OpString& str) const;
	virtual TreeNodeType NodeType() const;

// OpAccessibleItem
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual OP_STATUS AccessibilityGetURL(OpString& str);
	virtual OP_STATUS GetLinks(OpVector<AccessibilityTreeURLNode>& links);

private:
	OpVector<AccessibilityTreeURLNode>* m_links_vector;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_URL_NODE_H
