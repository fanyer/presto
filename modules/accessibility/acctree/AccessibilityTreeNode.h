/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBILITY_TREE_NODE_H
#define ACCESSIBILITY_TREE_NODE_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/util/opstring.h"
#include "modules/util/adt/opvector.h"

class AccessibleDocument;
class HTML_Element;
class OpAccessibleItem;
class OpWidget;
class AccessibilityTreeLabelNode;
class AccessibilityTreeURLNode;

/** Nodes representing core structures. These are made by AccessibleElementTraversalObject,
  * and owhen by AccessibleElementManager. Their purpose is to define a simple hierarchy of
  * objects in a way that interferes as little as possible with Core.
  *
  * To this end we note down HTML_Elements (if applicable) and document position during traversal.
  * We also mark elemnts as referenced, so we will get notified if they disappear.
  */

class AccessibilityTreeNode
{
public:

	typedef enum {
		TREE_NODE_TYPE_ROOT,
		TREE_NODE_TYPE_TEXT,
		TREE_NODE_TYPE_IMAGE,
		TREE_NODE_TYPE_LINK,
		TREE_NODE_TYPE_WIDGET,
		TREE_NODE_TYPE_URL,
		TREE_NODE_TYPE_LABEL,
		TREE_NODE_TYPE_RADIOGROUP,
		TREE_NODE_TYPE_OBJECT,
		TREE_NODE_TYPE_ARIA,
		TREE_NODE_TYPE_UNKNOWN,
	} TreeNodeType;

	AccessibilityTreeNode(AccessibilityTreeNode* parent, AccessibleDocument* document = NULL);
	virtual ~AccessibilityTreeNode();

/** Set the bounds of the object. If no bounds are set, the object will adopt the bounds of it's children.
  * @param bounds The bounds of the node, in document-relative coordinates.
  */
	virtual void SetBounds(const OpRect& bounds);

/** Get the bounds of the object as it should be rendered by screan reader.
  * @param bounds On exit, the bounds of the node, in document-relative coordinates.
  * @return OpStatus::OK if the call succeeded, otherwise error.
  */
	virtual OP_STATUS GetBounds(OpRect& bounds) const;

/** Test if mouse hit this object. Note that this may be different fron the bounds, since some objects have
  * non-rectangular areas.
  * @return TRUE if the object or it's children was hit by the mouse event, otherwise FALSE.
  */
	virtual BOOL HitTest(const OpPoint& pt) const;

/** Check if the node (or it's children) have keyboard focus.
  * @return TRUE if the object has focus, otherwise FALSE.
  */
	virtual BOOL Focused() const;

/** The node should click itself.
  * @return TRUE if the object was clicked, otherwise FALSE.
  */
	virtual BOOL ClickSelf();

/** Get text of the item. Not all items will handle this. Some may instead get the text of their children (links).
  * @return OpStatus::OK if the call succeeded, otherwise error.
  */
	virtual OP_STATUS GetText(OpString& str) const;

/** Compare this branch of the tree with the given branch, based on position, kind, text, and (possibly) HTML elements.
  * This will be a fuzzy compare: If they are similar *enough* return TRUE, and remaining discrepancies will be reported
  * as changes within the objects. But be careful on the level of fuzzyness: You don't want to return TRUE if another branch matches better.
  * @return TRUE if the trees are (more or less) identical, otherwise FALSE.
  */
	virtual BOOL Compare(const AccessibilityTreeNode* node) const;

/** Merge the information from the given node into this node. This means updated information on text, rectangle,
  * children and html element or widget.
  * The idea is to avoid destroying/creating accessibility nodes unless we have to.
  */
	virtual void Merge(AccessibilityTreeNode* node);

/** Inform the tree that the traversal is finished. Things that need to know the entire tree to finish initialising,
  * can do so now.
  */
	virtual void ParsingDone();

/** Notify screen reader that object got focus.
  */
	virtual void OnFocus() const;

/** Notify screen reader that object lost focus.
  */
	virtual void OnBlur() const;

/** Notify screen reader that the order/number of children changed.
  */
	virtual void OnReorder() const;

/** Recursively notify tree that an HTML_Element has died. Subclasses that retains an HTML element should override and set their pointer to NULL,
  * or otherwise invalidate.
  */
	virtual void UnreferenceHTMLElement(const HTML_Element* helm);

/** Recursively notify tree that an OpWidget has died. Subclasses that retains an OpWidget should override and set their pointer to NULL,
  * or otherwise invalidate.
  */
	virtual void UnreferenceWidget(const OpWidget* widget);

/** Traverse the tree looking for nodes matching the given element. Must be non-zero.
  */
	virtual AccessibilityTreeNode* FindElement(HTML_Element* helm);

/** Node type.
  */
	virtual TreeNodeType NodeType() const;

/** Item moved.
  */
	virtual void BoundsChanged(BOOL moved, BOOL resized);

/** Number of child nodes.
  */
	int GetChildrenCount() const;

/** Get a direct child by (0-based) index
  */
	const AccessibilityTreeNode* GetChild(int i) const;

/** Get a direct child by document-relative position.
  */
	const AccessibilityTreeNode* GetChildAt(const OpPoint& pt) const;

/** Get the child with keyboard focus, if any.
  */
	const AccessibilityTreeNode* GetFocusedChild() const;

/** Get next sibling.
  */
	const AccessibilityTreeNode* GetNextSibling() const;

/** Get previous sibling.
  */
	const AccessibilityTreeNode* GetPrevSibling() const;

/** Add a child. Object will be deleted by parent unless removed.
  */
	void AddChild(AccessibilityTreeNode* child, int index = -1);

/** Remove a child.
  */
	void RemoveChild(AccessibilityTreeNode* child);

/** Get the root node.
  */
	virtual const AccessibilityTreeNode* GetRoot() const = 0;

/** Get the parent node.
  */
	const AccessibilityTreeNode* GetParent() const;

/** Reparent the node.
  */
	virtual void Reparent(AccessibilityTreeNode* parent, int index = -1);

/** Get the document.
  */
	AccessibleDocument* GetDocument() const;

/** Locate child before given child.
  */
	const AccessibilityTreeNode* GetChildBefore(const AccessibilityTreeNode* node) const;

/** Locate child after given child.
  */
	const AccessibilityTreeNode* GetChildAfter(const AccessibilityTreeNode* node) const;

/** Get the accessible item refered by this node.
  */
	virtual OpAccessibleItem* GetAccessibleObject() const = 0;

/** Scan tree for node matching an ID string.
  */
	virtual const AccessibilityTreeNode* FindItemByID(const uni_char* name) const;

	virtual void SetLabel(AccessibilityTreeLabelNode* label) const;

	virtual OP_STATUS GetLinks(OpVector<AccessibilityTreeURLNode>& links);

protected:
/** Manually delete all children. This happens automatically during destruction. This function is provided
  * if a subclass needs to ensure children are deleted before they do subclass-specific cleanup.
  * Nodes that create an OpAccessibilityExtension would normally call this function before they delete their
  * accessibility extension (so parent isn't deleted before children).
  */
	void DeleteAllChildren();

protected:
	AccessibilityTreeNode* m_parent;

private:
	OpRect m_bounds;
	mutable AccessibleDocument* m_document;
	mutable AccessibilityTreeLabelNode* m_label;
	OpAutoVector<AccessibilityTreeNode> m_children;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBILITY_TREE_NODE_H
