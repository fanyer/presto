/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_TREE_OBJECT_NODE_H
#define ACCESSIBILITY_TREE_OBJECT_NODE_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeNode.h"
#include "modules/accessibility/opaccessibleitem.h"

class HTML_Element;

class AccessibilityTreeObjectNode : public AccessibilityTreeNode, public OpAccessibleItem
{
public:
	AccessibilityTreeObjectNode(HTML_Element* helm, AccessibilityTreeNode* parent);
	virtual ~AccessibilityTreeObjectNode();
	const HTML_Element* GetHTMLElement() const;
	virtual BOOL FlattenTextContent();
	virtual void SetTextElement(HTML_Element* helm) { m_labelelem = helm; }

// AccessibilityTreeNode
	virtual void UnreferenceHTMLElement(const HTML_Element* helm);
	virtual AccessibilityTreeNode* FindElement(HTML_Element* helm);
	virtual const AccessibilityTreeNode* GetRoot() const { return GetParent()->GetRoot(); }
	virtual BOOL ClickSelf();
	virtual OP_STATUS GetText(OpString& str) const;
	virtual BOOL Compare(const AccessibilityTreeNode* node) const;
	virtual void Merge(AccessibilityTreeNode* node);
	virtual void BoundsChanged(BOOL moved, BOOL resized);
	virtual void OnReorder() const;
	virtual const AccessibilityTreeNode* FindItemByID(const uni_char* name) const;
	virtual TreeNodeType NodeType() const;
	virtual OpAccessibleItem* GetAccessibleObject() const {return const_cast<AccessibilityTreeObjectNode*>(this);}

// OpAccessibilityExtensionListener
	virtual BOOL AccessibilityIsReady() const {return m_accessibility_ready; }
	virtual OP_STATUS AccessibilityClicked();
	virtual OP_STATUS AccessibilityScrollTo(Accessibility::ScrollTo scroll_to);
	virtual BOOL AccessibilitySetFocus();
	virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual OP_STATUS AccessibilityGetText(OpString& str);
	virtual OP_STATUS AccessibilityGetDescription(OpString& str);
	virtual Accessibility::State AccessibilityGetState();
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual int GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleParent() { return m_parent?m_parent->GetAccessibleObject():NULL; }
	virtual OpAccessibleItem* GetAccessibleChild(int n);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetNextAccessibleSibling();
	virtual OpAccessibleItem* GetPreviousAccessibleSibling();
	virtual OpAccessibleItem* GetLeftAccessibleObject();
	virtual OpAccessibleItem* GetRightAccessibleObject();
	virtual OpAccessibleItem* GetDownAccessibleObject();
	virtual OpAccessibleItem* GetUpAccessibleObject();
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();

protected:
	BOOL m_accessibility_ready;
	HTML_Element* m_elem;
	HTML_Element* m_labelelem;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_OBJECT_NODE_H
