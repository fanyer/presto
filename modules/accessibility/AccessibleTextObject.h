/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBLETEXTOBJECT_H
#define ACCESSIBLETEXTOBJECT_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/opaccessibleitem.h"

// A simple class that allows you to create a single object to manage a list of strings, ans with a simplified API compared to OpAccessibilityExtensionListener

class AccessibleTextObjectListener
{
public:
	virtual ~AccessibleTextObjectListener() {}
	virtual OP_STATUS GetAbsolutePositionOfObject(int i, OpRect& rect) = 0;
	virtual OP_STATUS SetFocusToObject(int i) = 0;
	virtual OP_STATUS GetTextOfObject(int i, OpString& str) = 0;
	virtual OP_STATUS GetDecriptionOfObject(int i, OpString& str) { str.Empty(); return OpStatus::OK; }
	virtual Accessibility::State GetStateOfObject(int i) = 0;
	virtual OP_STATUS ObjectClicked(int i) { return OpStatus::OK; }
	virtual OpAccessibleItem* GetAccessibleParentOfObject() = 0;
};

class AccessibleTextObject
		: public OpAccessibleItem
{
public:
	AccessibleTextObject(AccessibleTextObjectListener* parent, int index);
	~AccessibleTextObject();
	virtual BOOL		AccessibilityIsReady() const { return TRUE; }
	virtual BOOL		AccessibilitySetFocus();
	virtual OP_STATUS	AccessibilityGetText(OpString& str);
	virtual OP_STATUS	AccessibilityGetDescription(OpString& str);
	virtual OP_STATUS	AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual Accessibility::State AccessibilityGetState();
	virtual int	GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleParent();
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
	virtual OP_STATUS AccessibilityClicked();
	int GetIndex() const { return m_index; }
	void SetIndex(int i) { m_index = i; }
protected:
	AccessibleTextObjectListener*	m_parent;
	int								m_index;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBLETEXTOBJECT_H
