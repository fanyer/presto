/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_TREE_ARIA_NODE_H
#define ACCESSIBILITY_TREE_ARIA_NODE_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeObjectNode.h"
#include "modules/logdoc/namespace.h"

class AccessibilityTreeAriaNode : public AccessibilityTreeObjectNode
{
public:
	AccessibilityTreeAriaNode(HTML_Element* helm, AccessibilityTreeNode* parent);
	virtual BOOL FlattenTextContent();
	virtual void UnreferenceHTMLElement(const HTML_Element* helm);
	virtual OP_STATUS GetText(OpString& str) const;
	virtual void Merge(AccessibilityTreeNode* node);
	virtual TreeNodeType NodeType() const;

// OpAccessibleItem
	virtual Accessibility::State AccessibilityGetState();
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual OP_STATUS AccessibilityGetValue(int &value);
	virtual OP_STATUS AccessibilityGetMinValue(int &value);
	virtual OP_STATUS AccessibilityGetMaxValue(int &value);

private:
	Accessibility::ElementKind m_role;
	Accessibility::State m_last_state;
	int m_last_value;
	int m_last_min_value;
	int m_last_max_value;
	int m_namespace;
	BOOL m_ispassword;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_ARIA_NODE_H
