/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef UI_NODE_H
#define UI_NODE_H

#ifdef FEATURE_UI_TEST

class OpAccessibilityExtension;
class XMLFragment;

#include "adjunct/ui_test_framework/OpExtensionContainer.h"

/**
 * @brief A node representing a piece of the UI during testing
 * @author Patricia Aas
 */

class OpUiNode :
	public OpExtensionContainer::Listener
{
public:

    static OP_STATUS Create(OpExtensionContainer* element, OpUiNode*& node);

	virtual ~OpUiNode();

	OP_STATUS Export(XMLFragment& fragment);

    OP_STATUS ClickAll();

    OP_STATUS AddChild(OpUiNode* child);

	// Implementing the OpExtensionContainer::Listener interface

	virtual OP_STATUS OnChildAdded(OpExtensionContainer* child);
	virtual void OnExtensionContainerDeleted();

private:

    OpUiNode();
    OP_STATUS SetExtensionContainer(OpExtensionContainer* container);

	OP_STATUS ExportNode(XMLFragment& fragment);
	OP_STATUS StoreAllData();

	const uni_char* GetRoleAsString();
	BOOL IsVisible();
	BOOL IsEnabled();
	BOOL IsFocused();
	BOOL HasValue();
	BOOL HasMinValue();
	BOOL HasMaxValue();
	OP_STATUS GetText(OpString& text);
	OP_STATUS GetDescription(OpString& description);
	OP_STATUS GetUrl(OpString& url);
	int GetValue();
	int GetMinValue();
	int GetMaxValue();
	OpAccessibilityExtension::AccessibilityState GetState();
	OpAccessibilityExtension::ElementKind GetRole();

    OpAutoVector<OpUiNode> m_children;
    OpExtensionContainer* m_extension_container;
	OpAccessibilityExtensionListener* m_element;

	// Cached values for items that have been removed
	BOOL m_removed;
	BOOL m_visible;
	BOOL m_enabled;
	BOOL m_focused;
	BOOL m_has_value;
	BOOL m_has_min_value;
	BOOL m_has_max_value;
	int m_value;
	int m_min_value;
	int m_max_value;
	OpAccessibilityExtension::AccessibilityState m_state;
	OpAccessibilityExtension::ElementKind m_role;
	OpString m_text;
	OpString m_description;
	OpString m_url;
};

#endif // FEATURE_UI_TEST

#endif // UI_NODE_H
