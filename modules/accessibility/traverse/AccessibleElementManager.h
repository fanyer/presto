/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBLE_ELEMENT_MANAGER_H
#define ACCESSIBLE_ELEMENT_MANAGER_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

class AccessibleElementTraversalObject;
class OpAccessibleItem;
class AccessibleDocument;
class FramesDocument;
class AccessibilityTreeRootNode;
class AccessibilityTreeURLNode;
class HTML_Element;
class OpWidget;

class AccessibleElementManager
{
public:
	AccessibleElementManager(AccessibleDocument* doc, FramesDocument* frm_doc);
	~AccessibleElementManager();
	void MarkDirty();
	FramesDocument* GetFramesDocument();
	void UnreferenceElement(const HTML_Element* elem);
	void UnreferenceWidget(const OpWidget* widget);
	void HighlightElement(HTML_Element* new_elem, HTML_Element* old_elem = NULL);
	int GetAccessibleElementCount();
	OpAccessibleItem* GetAccessibleElement(int i);
	int GetIndexOfElement(OpAccessibleItem* elem);
	OpAccessibleItem* GetAccessibleElementAt(int x, int y);
	OpAccessibleItem* GetAccessibleFocusedElement();
	OP_STATUS GetLinks(OpVector<OpAccessibleItem>& links);

private:
	void Rescan();
	AccessibleDocument* m_doc;
	FramesDocument* m_frm_doc;
	AccessibilityTreeRootNode* m_root_node;
	OpVector<AccessibilityTreeURLNode> m_link_objects;
	BOOL m_dirty;
	BOOL m_scanning;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBLE_ELEMENT_MANAGER_H
