/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef BUTTON_GROUP_SUPPORT

#include "modules/widgets/OpButtonGroup.h"

#include "modules/widgets/OpButton.h"

OpButtonGroup::OpButtonGroup(OpWidget* parent) :
	m_parent(parent)

{}

OpButtonGroup::~OpButtonGroup()
{
}

OP_STATUS OpButtonGroup::RegisterButton(OpButton* button)
{
	if (button->GetValue())
		ButtonSet(button);

	return m_buttons.Add(button);
}

void OpButtonGroup::ButtonSet(OpButton* button)
{
	for (unsigned int i = 0; i < m_buttons.GetCount(); i++)
	{
		OpButton* button2 = m_buttons.Get(i);
		if (button != button2)
		{
			button2->SetValue(FALSE);
		}
	}
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

Accessibility::State OpButtonGroup::AccessibilityGetState()
{
	return Accessibility::kAccessibilityStateNone;
}

Accessibility::ElementKind OpButtonGroup::AccessibilityGetRole() const
{
	return Accessibility::kElementKindRadioTabGroup;
}

OpAccessibleItem* OpButtonGroup::GetAccessibleChild(int child_nr)
{
	return m_buttons.Get(child_nr);
}

int OpButtonGroup::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int index = m_buttons.Find(static_cast<OpButton*>(child));
	if (index == -1)
		return Accessibility::NoSuchChild;
	return index;
}

OpAccessibleItem* OpButtonGroup::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpAccessibleItem* child;
	for (unsigned int i = 0; i < m_buttons.GetCount(); i++)
	{
		OpButton* button = m_buttons.Get(i);
		child = button->GetAccessibleChildOrSelfAt(x,y);
		if (child)
			return button;
	}
	return NULL;
}

OpAccessibleItem* OpButtonGroup::GetAccessibleFocusedChildOrSelf()
{
	OpAccessibleItem* child;
	for (unsigned int i = 0; i < m_buttons.GetCount(); i++)
	{
		OpButton* button = m_buttons.Get(i);
		child = button->GetAccessibleFocusedChildOrSelf();
		if (child)
			return button;
	}
	return NULL;
}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#endif // BUTTON_GROUP_SUPPORT
