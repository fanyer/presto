/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_TREE_LABEL_NODE_H
#define ACCESSIBILITY_TREE_LABEL_NODE_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeObjectNode.h"
#include "modules/accessibility/AccessibleOpWidgetLabel.h"

class OpWidget;

class AccessibilityTreeLabelNode : public AccessibilityTreeObjectNode, public AccessibleOpWidgetLabel
{
public:
	AccessibilityTreeLabelNode(HTML_Element* helm, AccessibilityTreeNode* parent);
	~AccessibilityTreeLabelNode();
	void SetLabeledControl(const AccessibilityTreeNode * control);
	void RemoveControl(const AccessibilityTreeNode * control);

// AccessibilityTreeNode
	virtual void ParsingDone();
	virtual OP_STATUS GetText(OpString& str) const;
	virtual TreeNodeType NodeType() const;
	virtual void Merge(AccessibilityTreeNode* node);

// OpAccessibilityExtensionListener
/*	virtual int GetAccessibleChildrenCount();
	virtual OpAccessibilityExtension* GetAccessibleChild(int n);
	virtual OpAccessibilityExtension* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibilityExtension* GetAccessibleFocusedChildOrSelf();*/
	virtual OpAccessibleItem* GetAccessibleControlForLabel();
	virtual Accessibility::ElementKind AccessibilityGetRole() const;

// AccessibleWidgetLabel
	virtual OpAccessibleItem* GetLabelExtension();

private:
	const AccessibilityTreeNode * m_labeled_control;
	const AccessibilityTreeNode * m_fallback_labeled_control;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_LABEL_NODE_H
