/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_TREE_RADIO_GROUP_NODE_H
#define ACCESSIBILITY_TREE_RADIO_GROUP_NODE_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeObjectNode.h"

class FormRadioGroup;

class AccessibilityTreeRadioGroupNode : public AccessibilityTreeObjectNode
{
public:
	AccessibilityTreeRadioGroupNode(FormRadioGroup* group, AccessibilityTreeNode* parent);
	const FormRadioGroup* GetGroup() const;

// AccessibilityTreeNode
	virtual OP_STATUS GetBounds(OpRect& bounds) const;
	virtual TreeNodeType NodeType() const;
	virtual BOOL Compare(const AccessibilityTreeNode* node) const;

// OpAccessibilityExtensionListener
	virtual Accessibility::ElementKind AccessibilityGetRole() const;

private:
	FormRadioGroup* m_group;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_RADIO_GROUP_NODE_H
