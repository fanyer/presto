/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#ifdef FEATURE_UI_TEST

#include "adjunct/ui_test_framework/OpExtensionContainer.h"
#include "adjunct/desktop_pi/DesktopAccessibilityExtension.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpAccessibilityExtension::Create(OpAccessibilityExtension** extension,
					   OpAccessibilityExtension* parent,
					   OpAccessibilityExtensionListener* listener,
					   ElementKind kind)
{
	OpExtensionContainer* parent_container = static_cast<OpExtensionContainer*>(parent);
	*extension = NULL;

	OpExtensionContainer* container = new OpExtensionContainer();
	if (container == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS error = container->Init(parent_container, listener, kind);
	if (OpStatus::IsError(error))
	{
		delete container;
		return error;
	}
	*extension = container;
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::Init(OpExtensionContainer* parent, OpAccessibilityExtensionListener* listener, ElementKind kind)
{
	m_extension_listener = listener;
	if (!m_extension_listener)
		return OpStatus::ERR;

	OpAccessibilityExtension* platform_extension = 0;
	RETURN_IF_ERROR(DesktopAccessibilityExtension::Create(&platform_extension,
					  parent ? parent->GetExtension() : NULL,
					  this,
					  kind));
	m_extension = platform_extension;
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpExtensionContainer::~OpExtensionContainer()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		OpExtensionContainer::Listener* listener = m_listeners.GetNext(iterator);
		listener->OnExtensionContainerDeleted();
	}
	delete m_extension;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AddChild(OpExtensionContainer* child)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		OpExtensionContainer::Listener* listener = m_listeners.GetNext(iterator);
		RETURN_IF_ERROR(listener->OnChildAdded(child));
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::BlockEvents(BOOL block)
{
	m_extension->BlockEvents(block);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::SetWindow(OpWindow* window)
{
	m_extension->SetWindow(window);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::SetState(AccessibilityState state)
{
	m_extension->SetState(state);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::Moved()
{
	m_extension->Moved();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::Resized()
{
	m_extension->Resized();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::TextChanged()
{
	m_extension->TextChanged();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::TextChangedByKeyboard()
{
	m_extension->TextChangedByKeyboard();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::DescriptionChanged()
{
	m_extension->DescriptionChanged();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::URLChanged()
{
	m_extension->URLChanged();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::KeyboardShortcutChanged()
{
	m_extension->KeyboardShortcutChanged();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::SelectedTextRangeChanged()
{
	m_extension->SelectedTextRangeChanged();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::SelectionChanged()
{
	m_extension->SelectionChanged();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::SetLive()
{
	m_extension->SetLive();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::StartDragAndDrop()
{
	m_extension->StartDragAndDrop();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::EndDragAndDrop()
{
	m_extension->EndDragAndDrop();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::StartScrolling()
{
	m_extension->StartScrolling();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::EndScrolling()
{
	m_extension->EndScrolling();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpExtensionContainer::Reorder()
{
	m_extension->Reorder();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtensionListener* OpExtensionContainer::GetListener()
{
	return m_extension->GetListener();
}






/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilityClicked()
{
	return m_extension_listener->AccessibilityClicked();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilitySetValue(int value)
{
	return m_extension_listener->AccessibilitySetValue(value);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilitySetText(const uni_char* text)
{
	return m_extension_listener->AccessibilitySetText(text);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilitySetSelectedTextRange(int start, int end)
{
	return m_extension_listener->AccessibilitySetSelectedTextRange(start, end);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL OpExtensionContainer::AccessibilityChangeSelection(SelectionSet flags, OpAccessibilityExtension* child)
{
	return m_extension_listener->AccessibilityChangeSelection(flags, child);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL OpExtensionContainer::AccessibilitySetFocus()
{
	return m_extension_listener->AccessibilitySetFocus();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL OpExtensionContainer::AccessibilitySetExpanded(BOOL expanded)
{
	return m_extension_listener->AccessibilitySetExpanded(expanded);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	return m_extension_listener->AccessibilityGetAbsolutePosition(rect);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilityGetValue(int &value)
{
	return m_extension_listener->AccessibilityGetValue(value);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilityGetMinValue(int &value)
{
	return m_extension_listener->AccessibilityGetMinValue(value);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilityGetMaxValue(int &value)
{
	return m_extension_listener->AccessibilityGetMaxValue(value);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilityGetText(OpString& str)
{
	return m_extension_listener->AccessibilityGetText(str);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilityGetSelectedTextRange(int &start, int &end)
{
	return m_extension_listener->AccessibilityGetSelectedTextRange(start, end);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilityGetDescription(OpString& str)
{
	return m_extension_listener->AccessibilityGetDescription(str);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilityGetURL(OpString& str)
{
	return m_extension_listener->AccessibilityGetURL(str);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpExtensionContainer::AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut)
{
	return m_extension_listener->AccessibilityGetKeyboardShortcut(shifts, kbdShortcut);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension::AccessibilityState OpExtensionContainer::AccessibilityGetState()
{
	return m_extension_listener->AccessibilityGetState();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension::ElementKind OpExtensionContainer::AccessibilityGetRole()
{
	return m_extension_listener->AccessibilityGetRole();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetAccessibleControlForLabel()
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetAccessibleControlForLabel();
	return container ? container->GetExtension() : NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetAccessibleLabelForControl()
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetAccessibleLabelForControl();
	return container ? container->GetExtension() : NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
int OpExtensionContainer::GetAccessibleChildrenCount()
{
	return m_extension_listener->GetAccessibleChildrenCount();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetAccessibleChild(int n)
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetAccessibleChild(n);
	return container ? container->GetExtension() : NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetAccessibleChildOrSelfAt(x, y);
	return container ? container->GetExtension() : NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetNextAccessibleSibling()
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetNextAccessibleSibling();
	return container ? container->GetExtension() : NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetPreviousAccessibleSibling()
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetPreviousAccessibleSibling();
	return container ? container->GetExtension() : NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetLeftAccessibleObject()
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetLeftAccessibleObject();
	return container ? container->GetExtension() : NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetRightAccessibleObject()
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetRightAccessibleObject();
	return container ? container->GetExtension() : NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetDownAccessibleObject()
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetDownAccessibleObject();
	return container ? container->GetExtension() : NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetUpAccessibleObject()
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetUpAccessibleObject();
	return container ? container->GetExtension() : NULL;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension* OpExtensionContainer::GetAccessibleFocusedChildOrSelf()
{
	OpExtensionContainer* container = (OpExtensionContainer*)m_extension_listener->GetAccessibleFocusedChildOrSelf();
	return container ? container->GetExtension() : NULL;
}

#endif // FEATURE_UI_TEST
