/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef EXTENSION_CONTAINER_H
#define EXTENSION_CONTAINER_H

#ifdef FEATURE_UI_TEST

#include "modules/accessibility/OpAccessibilityExtensionListener.h"
#include "modules/pi/OpAccessibilityExtension.h"
#include "modules/util/adt/oplisteners.h"

/**
 * @brief
 * @author Patricia Aas
 */

class OpExtensionContainer :
	public OpAccessibilityExtension, public OpAccessibilityExtensionListener
{
public:

    class Listener
	{
	public:
		virtual ~Listener(){}
		virtual OP_STATUS OnChildAdded(OpExtensionContainer* child) = 0;
		virtual void OnExtensionContainerDeleted() = 0;
	};

	OpExtensionContainer() : m_extension(NULL), m_extension_listener(NULL) {}
	OP_STATUS Init(OpExtensionContainer* parent, OpAccessibilityExtensionListener* listener, ElementKind kind);
	virtual ~OpExtensionContainer();

	OP_STATUS AddChild(OpExtensionContainer* child);

	/**
	 * @return the assosiated platform OpAccessibilityExtension
	 */
	OpAccessibilityExtension* GetExtension() { return m_extension; }

	// Adding and removing listeners
	OP_STATUS AddListener(OpExtensionContainer::Listener* listener) { return m_listeners.Add(listener); }
	OP_STATUS RemoveListener(OpExtensionContainer::Listener* listener) { return m_listeners.Remove(listener); }

	// Implemeting OpAccessibilityExtension
	virtual void BlockEvents(BOOL block);
	virtual void SetWindow(OpWindow* window);
	virtual void SetState(AccessibilityState state);
	virtual void Moved();
	virtual void Resized();
	virtual void TextChanged();
	virtual void TextChangedByKeyboard();
	virtual void DescriptionChanged();
	virtual void URLChanged();
	virtual void KeyboardShortcutChanged();
	virtual void SelectedTextRangeChanged();
	virtual void SelectionChanged();
	virtual void SetLive();
	virtual void StartDragAndDrop();
	virtual void EndDragAndDrop();
	virtual void StartScrolling();
	virtual void EndScrolling();
	virtual void Reorder();
	virtual OpAccessibilityExtensionListener* GetListener();

	//implementing OpAccessibilityExtensionListener
	virtual OP_STATUS AccessibilityClicked();
	virtual OP_STATUS AccessibilitySetValue(int value);
	virtual OP_STATUS AccessibilitySetText(const uni_char* text);
	virtual OP_STATUS AccessibilitySetSelectedTextRange(int start, int end);
	virtual BOOL AccessibilityChangeSelection(SelectionSet flags, OpAccessibilityExtension* child);
	virtual BOOL AccessibilitySetFocus();
	virtual BOOL AccessibilitySetExpanded(BOOL expanded);
	virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual OP_STATUS AccessibilityGetValue(int &value);
	virtual OP_STATUS AccessibilityGetMinValue(int &value);
	virtual OP_STATUS AccessibilityGetMaxValue(int &value);
	virtual OP_STATUS AccessibilityGetText(OpString& str);
	virtual OP_STATUS AccessibilityGetSelectedTextRange(int &start, int &end);
	virtual OP_STATUS AccessibilityGetDescription(OpString& str);
	virtual OP_STATUS AccessibilityGetURL(OpString& str);
	virtual OP_STATUS AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut);
	virtual OpAccessibilityExtension::AccessibilityState AccessibilityGetState();
	virtual OpAccessibilityExtension::ElementKind AccessibilityGetRole();
	virtual OpAccessibilityExtension* GetAccessibleControlForLabel();
	virtual OpAccessibilityExtension* GetAccessibleLabelForControl();
	virtual int GetAccessibleChildrenCount();
	virtual OpAccessibilityExtension* GetAccessibleChild(int n);
	virtual OpAccessibilityExtension* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibilityExtension* GetNextAccessibleSibling();
	virtual OpAccessibilityExtension* GetPreviousAccessibleSibling();
	virtual OpAccessibilityExtension* GetLeftAccessibleObject();
	virtual OpAccessibilityExtension* GetRightAccessibleObject();
	virtual OpAccessibilityExtension* GetDownAccessibleObject();
	virtual OpAccessibilityExtension* GetUpAccessibleObject();
	virtual OpAccessibilityExtension* GetAccessibleFocusedChildOrSelf();

private:
	OpAccessibilityExtension* m_extension;
	OpAccessibilityExtensionListener* m_extension_listener;
	OpListeners<Listener> m_listeners;
};

#endif // FEATURE_UI_TEST

#endif // EXTENSION_CONTAINER_H
