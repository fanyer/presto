/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_TREE_WIDGET_NODE_H
#define ACCESSIBILITY_TREE_WIDGET_NODE_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeNode.h"

class HTML_Element;
class OpWidget;

class AccessibilityTreeWidgetNode : public AccessibilityTreeNode
{
public:
	AccessibilityTreeWidgetNode(HTML_Element* helm, OpWidget* widget, AccessibilityTreeNode* parent);
	virtual ~AccessibilityTreeWidgetNode();
	OpWidget* GetWidget() const;

// AccessibilityTreeNode
	virtual BOOL Compare(const AccessibilityTreeNode* node) const;
	virtual void Merge(AccessibilityTreeNode* node);
	virtual void Reparent(AccessibilityTreeNode* parent, int index = -1);
	virtual void UnreferenceHTMLElement(const HTML_Element* helm);
	virtual void UnreferenceWidget(const OpWidget* widget);
	virtual AccessibilityTreeNode* FindElement(HTML_Element* helm);
	virtual const AccessibilityTreeNode* GetRoot() const { return GetParent()->GetRoot(); }
	virtual BOOL HitTest(const OpPoint& pt) const;
	virtual BOOL Focused() const;
	virtual BOOL ClickSelf();
	virtual OP_STATUS GetText(OpString& str) const;
	virtual const AccessibilityTreeNode* FindItemByID(const uni_char* name) const;
	virtual TreeNodeType NodeType() const;
	virtual OpAccessibleItem* GetAccessibleObject() const;

private:
	HTML_Element* m_elem;
	mutable OpWidget* m_widget;
};


#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_WIDGET_NODE_H
