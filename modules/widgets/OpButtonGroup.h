/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_BUTTON_GROUP_H
#define OP_BUTTON_GROUP_H

#ifdef BUTTON_GROUP_SUPPORT

#include "modules/util/adt/opvector.h"
#include "modules/widgets/OpWidget.h"

class OpButton;

/** OpButtonGroup is a non-visual container that organizes OpButtons.
	If the value of one of its buttons is set, the values of the other buttons in the group
	are reset to 0 (FALSE).
	The most common use of a button group is for radio buttons.
*/
class OpButtonGroup
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	: public OpAccessibleItem
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
{
public:
	OpButtonGroup(OpWidget* parent);
	~OpButtonGroup();

	/** Adds a button to the group.*/
	OP_STATUS RegisterButton(OpButton* button);
	/** Removes a button from the group.*/
	void      RemoveButton(OpButton* button) { m_buttons.RemoveByItem(button); }

	/* Resets the value of all buttons, besides 'button', to FALSE.*/
	void ButtonSet(OpButton* button);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

	virtual BOOL AccessibilityIsReady() const {return TRUE;}
	virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect) {return OpStatus::ERR;}
	virtual OP_STATUS AccessibilityGetText(OpString& str) {return OpStatus::ERR;}
	virtual Accessibility::State AccessibilityGetState();
	virtual Accessibility::ElementKind AccessibilityGetRole() const;

	virtual int GetAccessibleChildrenCount() { return m_buttons.GetCount();}

	virtual OpAccessibleItem* GetAccessibleParent() { return m_parent; }
	virtual OpAccessibleItem* GetAccessibleChild(int child_nr);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();

	virtual OpAccessibleItem* GetNextAccessibleSibling() {return m_parent->GetAccessibleChild(1);}
	virtual OpAccessibleItem* GetPreviousAccessibleSibling() {return NULL;}

	virtual OpAccessibleItem* GetLeftAccessibleObject() {return NULL;}
	virtual OpAccessibleItem* GetRightAccessibleObject() {return NULL;}
	virtual OpAccessibleItem* GetDownAccessibleObject() {return NULL;}
	virtual OpAccessibleItem* GetUpAccessibleObject() {return NULL;}

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

private:

	OpVector<OpButton>  m_buttons;
	OpWidget*           m_parent;

};

#endif // BUTTON_GROUP_SUPPORT
#endif // OP_BUTTON_GROUP_H
