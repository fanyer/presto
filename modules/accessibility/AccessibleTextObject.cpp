/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/AccessibleTextObject.h"

AccessibleTextObject::AccessibleTextObject(AccessibleTextObjectListener* parent, int index)
	: m_parent(parent), m_index(index)
{
}

AccessibleTextObject::~AccessibleTextObject()
{
}

BOOL AccessibleTextObject::AccessibilitySetFocus()
{
	m_parent->SetFocusToObject(m_index); return TRUE;
}

OP_STATUS AccessibleTextObject::AccessibilityGetText(OpString& str)
{
	return m_parent->GetTextOfObject(m_index, str);
}

OP_STATUS AccessibleTextObject::AccessibilityGetDescription(OpString& str)
{
	return m_parent->GetDecriptionOfObject(m_index, str);
}

OP_STATUS AccessibleTextObject::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	return m_parent->GetAbsolutePositionOfObject(m_index, rect);
}

Accessibility::ElementKind AccessibleTextObject::AccessibilityGetRole() const
{
	return Accessibility::kElementKindStaticText;
}

Accessibility::State AccessibleTextObject::AccessibilityGetState()
{
	return m_parent->GetStateOfObject(m_index);
}

int AccessibleTextObject::GetAccessibleChildrenCount()
{
	return 0;
}

OpAccessibleItem* AccessibleTextObject::GetAccessibleParent()
{
	return m_parent->GetAccessibleParentOfObject();
}

OpAccessibleItem* AccessibleTextObject::GetAccessibleChild(int)
{
	return NULL;
}

int AccessibleTextObject::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	return Accessibility::NoSuchChild;
}

OpAccessibleItem* AccessibleTextObject::GetAccessibleChildOrSelfAt(int x, int y)
{
	return this;
}

OpAccessibleItem* AccessibleTextObject::GetNextAccessibleSibling()
{
	return NULL;
}

OpAccessibleItem* AccessibleTextObject::GetPreviousAccessibleSibling()
{
	return NULL;
}

OpAccessibleItem* AccessibleTextObject::GetAccessibleFocusedChildOrSelf()
{
	if (m_parent->GetStateOfObject(m_index) & Accessibility::kAccessibilityStateFocused)
		return this;
	return NULL;
}

OpAccessibleItem* AccessibleTextObject::GetLeftAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* AccessibleTextObject::GetRightAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* AccessibleTextObject::GetDownAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* AccessibleTextObject::GetUpAccessibleObject()
{
	return NULL;
}

OP_STATUS AccessibleTextObject::AccessibilityClicked()
{
	return m_parent->ObjectClicked(m_index);
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
