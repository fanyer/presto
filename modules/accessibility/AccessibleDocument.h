/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBLEDOCUMENT_H
#define ACCESSIBLEDOCUMENT_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/opaccessibleitem.h"
#include "modules/hardcore/timer/optimer.h"
class DocumentManager;
class FramesDocument;
class VisualDevice;
class AccessibleElementManager;
class OpWidget;

/* The AccessibleDocument represents the accessibility node of the concept of a web page.
   Since the page itself never goes away, the accessibility node shouldn't either.
   To make this concept work, the AccessibleDocument essentially wraps the DocumentManager.

   By retaining the same node for the document when the DocumentManager changes the page,
   screen readers that had focus on the old page will automatically get moved to the new page.

	It really doesn't make sense from an acceesibility PoV to distinguish between an
	all-new document, or one that has merely fundamentally changed itself.
*/

class AccessibleDocument : public OpAccessibleItem, OpTimerListener
{
public:
	AccessibleDocument(OpAccessibleItem* parent, DocumentManager* doc_man);
	~AccessibleDocument();
	void DocumentUndisplayed(const FramesDocument* doc);
	void DocumentReflowed(const FramesDocument* doc);
	void DocumentLoaded(const FramesDocument* doc);
	void ElementRemoved(const FramesDocument* doc, const HTML_Element* element);
	void WidgetRemoved(const OpWidget* widget);
	void HighlightElement(HTML_Element* element);
	DocumentManager* GetDocumentManager();
	FramesDocument* GetActiveDocument() const;
	VisualDevice* GetVisualDevice() const;
	HTML_Element* GetFocusElement();
	void CreateAccessibleChildrenIfNeeded();

// OpAccessibilityExtensionListener
	virtual BOOL		AccessibilityIsReady() const {return m_is_ready;}
	virtual BOOL		AccessibilitySetFocus();
	virtual OP_STATUS	AccessibilityGetText(OpString& str);
	virtual OP_STATUS	AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual Accessibility::State AccessibilityGetState();
	virtual int	GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleParent() {return m_parent;}
	virtual OpAccessibleItem* GetAccessibleChild(int);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetNextAccessibleSibling();
	virtual OpAccessibleItem* GetPreviousAccessibleSibling();
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
	virtual OpAccessibleItem* GetLeftAccessibleObject();
	virtual OpAccessibleItem* GetRightAccessibleObject();
	virtual OpAccessibleItem* GetDownAccessibleObject();
	virtual OpAccessibleItem* GetUpAccessibleObject();

	virtual OP_STATUS GetAccessibleHeaders(int level, OpVector<OpAccessibleItem> &headers);
	virtual OP_STATUS GetAccessibleLinks(OpVector<OpAccessibleItem> &links);

// OpTimerListener
	virtual void OnTimeOut(OpTimer* timer);

private:
	OpAccessibleItem*				m_parent;
	DocumentManager*				m_doc_man;
	AccessibleElementManager*		m_accessible_children;
	OpTimer							m_reflow_timer;
	HTML_Element*					m_highlight_element;
	mutable BOOL					m_was_loading;
	BOOL							m_fake_loading;
	BOOL							m_is_ready;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBLEDOCUMENT_H
