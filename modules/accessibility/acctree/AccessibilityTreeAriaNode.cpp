/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acctree/AccessibilityTreeAriaNode.h"
#include "modules/accessibility/acc_utils.h"
#include "modules/logdoc/htm_elm.h"
#ifdef ARIA_ATTRIBUTES_SUPPORT
#include "modules/logdoc/ariaenum.h"
#endif

AccessibilityTreeAriaNode::AccessibilityTreeAriaNode(HTML_Element* helm, AccessibilityTreeNode* parent)
	: AccessibilityTreeObjectNode(helm, parent)
	, m_last_value(0)
	, m_last_min_value(0)
	, m_last_max_value(0)
	, m_ispassword(FALSE)
{
	Accessibility::ElementKind role = AccessibilityTreeObjectNode::AccessibilityGetRole();
	m_namespace = m_elem->GetNsIdx();
	const uni_char* a_role = m_elem->GetAttrValue(UNI_L("role"));
	if (!a_role)
		a_role = m_elem->GetXmlAttr(UNI_L("role"));
	OpString temp_str;
	if (a_role)
	{
		if (uni_strcmp(a_role, UNI_L("alert")) == 0)
			role = Accessibility::kElementKindAlert;
		else if (uni_strcmp(a_role, UNI_L("alertdialog")) == 0)
			role = Accessibility::kElementKindAlertDialog;
		else if (uni_strcmp(a_role, UNI_L("application")) == 0)
			role = Accessibility::kElementKindApplication;
		else if (uni_strcmp(a_role, UNI_L("button")) == 0)
			role = Accessibility::kElementKindButton;
		else if (uni_strcmp(a_role, UNI_L("checkbox")) == 0)
			role = Accessibility::kElementKindCheckbox;
//		else if (uni_strcmp(a_role, UNI_L("checkboxtristate")) == 0)
//			role = Accessibility::kElementKindCheckbox;
		else if (uni_strcmp(a_role, UNI_L("columnheader")) == 0)
			role = Accessibility::kElementKindListHeader;
		else if (uni_strcmp(a_role, UNI_L("combobox")) == 0)
			role = Accessibility::kElementKindDropdown;
		else if (uni_strcmp(a_role, UNI_L("description")) == 0)
			role = Accessibility::kElementKindDescriptor;
		else if (uni_strcmp(a_role, UNI_L("dialog")) == 0)
			role = Accessibility::kElementKindDialog;
		else if (uni_strcmp(a_role, UNI_L("directory")) == 0)
			role = Accessibility::kElementKindUnknown;
		else if (uni_strcmp(a_role, UNI_L("document")) == 0)
			role = Accessibility::kElementKindUnknown;
		else if (uni_strcmp(a_role, UNI_L("grid")) == 0)
			role = Accessibility::kElementKindTable;	// or kElementKindGrid ?
		else if (uni_strcmp(a_role, UNI_L("gridcell")) == 0)
			role = Accessibility::kElementKindListCell; // ?
		else if (uni_strcmp(a_role, UNI_L("group")) == 0)
			role = Accessibility::kElementKindView;
		else if (uni_strcmp(a_role, UNI_L("img")) == 0)
			role = Accessibility::kElementKindImage;
//		else if (uni_strcmp(a_role, UNI_L("imggroup")) == 0)
//			role = Accessibility::kElementKindView;
		else if (uni_strcmp(a_role, UNI_L("label")) == 0)
			role = Accessibility::kElementKindLabel;
		else if (uni_strcmp(a_role, UNI_L("link")) == 0)
			role = Accessibility::kElementKindLink;
		else if (uni_strcmp(a_role, UNI_L("list")) == 0)
			role = Accessibility::kElementKindList;
		else if (uni_strcmp(a_role, UNI_L("listbox")) == 0)
			role = Accessibility::kElementKindList;
		else if (uni_strcmp(a_role, UNI_L("listitem")) == 0)
			role = Accessibility::kElementKindListRow;
		else if (uni_strcmp(a_role, UNI_L("log")) == 0)
			role = Accessibility::kElementKindUnknown;
		else if (uni_strcmp(a_role, UNI_L("menu")) == 0)
			role = Accessibility::kElementKindMenu;
		else if (uni_strcmp(a_role, UNI_L("menubar")) == 0)
			role = Accessibility::kElementKindMenuBar;
		else if (uni_strcmp(a_role, UNI_L("menuitem")) == 0)
			role = Accessibility::kElementKindMenuItem;
		else if (uni_strcmp(a_role, UNI_L("menuitemcheckbox")) == 0)
			role = Accessibility::kElementKindMenuItem;
		else if (uni_strcmp(a_role, UNI_L("menuitemradio")) == 0)
			role = Accessibility::kElementKindMenuItem;
		else if (uni_strcmp(a_role, UNI_L("option")) == 0)
			role = Accessibility::kElementKindListRow;
		else if (uni_strcmp(a_role, UNI_L("presentation")) == 0)
			role = Accessibility::kElementKindUnknown; // FF will hide everything under this element
		else if (uni_strcmp(a_role, UNI_L("progressbar")) == 0)
			role = Accessibility::kElementKindProgress;
		else if (uni_strcmp(a_role, UNI_L("radio")) == 0)
			role = Accessibility::kElementKindRadioButton;
		else if (uni_strcmp(a_role, UNI_L("radiogroup")) == 0)
			role = Accessibility::kElementKindRadioTabGroup;
		else if (uni_strcmp(a_role, UNI_L("region")) == 0)
			role = Accessibility::kElementKindUnknown;
		else if (uni_strcmp(a_role, UNI_L("row")) == 0)
			role = Accessibility::kElementKindListRow;
		else if (uni_strcmp(a_role, UNI_L("rowheader")) == 0)
			role = Accessibility::kElementKindListHeader;
//		else if (uni_strcmp(a_role, UNI_L("secret")) == 0)
//		{
//			role = Accessibility::kElementKindSinglelineTextedit;
//			m_ispassword = TRUE;
//		}
		else if (uni_strcmp(a_role, UNI_L("separator")) == 0)
			role = Accessibility::kElementKindMenuItem; // need seperator item
		else if (uni_strcmp(a_role, UNI_L("slider")) == 0)
			role = Accessibility::kElementKindSlider;
		else if (uni_strcmp(a_role, UNI_L("spinbutton")) == 0)
			role = Accessibility::kElementKindProgress;
		else if (uni_strcmp(a_role, UNI_L("tab")) == 0)
			role = Accessibility::kElementKindTab;
		else if (uni_strcmp(a_role, UNI_L("tablist")) == 0)
			role = Accessibility::kElementKindRadioTabGroup;
		else if (uni_strcmp(a_role, UNI_L("tabpanel")) == 0)
			role = Accessibility::kElementKindView; // ROLE_PROPERTYPAGE, mey need to rework this.
//		else if (uni_strcmp(a_role, UNI_L("textarea")) == 0)
//			role = Accessibility::kElementKindMultilineTextedit;
		else if (uni_strcmp(a_role, UNI_L("textbox")) == 0)
			role = Accessibility::kElementKindMultilineTextedit;
//		else if (uni_strcmp(a_role, UNI_L("textfield")) == 0)
//			role = Accessibility::kElementKindMultilineTextedit;
		else if (uni_strcmp(a_role, UNI_L("toolbar")) == 0)
			role = Accessibility::kElementKindToolbar;
		else if (uni_strcmp(a_role, UNI_L("tree")) == 0)
			role = Accessibility::kElementKindOutlineList;
		else if (uni_strcmp(a_role, UNI_L("treegrid")) == 0)
			role = Accessibility::kElementKindOutlineList;
		else if (uni_strcmp(a_role, UNI_L("treeitem")) == 0)
			role = Accessibility::kElementKindListRow;
//		else if (uni_strcmp(a_role, UNI_L("treegroup")) == 0)
//			role = Accessibility::kElementKindView;
	}
	if (role == Accessibility::kElementKindMultilineTextedit)
	{
		// These can, in theory, be singleline
		const uni_char* multiline = m_elem->GetAttrValue(UNI_L("aria-multiline"), m_namespace);
		if (multiline)
		{
			if (uni_stricmp(a_role, UNI_L("false")) == 0)
				role = Accessibility::kElementKindSinglelineTextedit;
		}
	}
	m_role = role;
	m_last_state = AccessibilityGetState();
	AccessibilityGetValue(m_last_value);
	AccessibilityGetMinValue(m_last_min_value);
	AccessibilityGetMaxValue(m_last_max_value);
}

BOOL AccessibilityTreeAriaNode::FlattenTextContent()
{
	if (m_role == Accessibility::kElementKindCheckbox ||
		m_role == Accessibility::kElementKindRadioButton ||
		m_role == Accessibility::kElementKindButton ||
		m_role == Accessibility::kElementKindMultilineTextedit ||
		m_role == Accessibility::kElementKindSinglelineTextedit ||
		m_role == Accessibility::kElementKindMenu ||
		m_role == Accessibility::kElementKindMenuItem ||
		m_role == Accessibility::kElementKindListHeader ||
		m_role == Accessibility::kElementKindSlider)
	{
		return TRUE;
	}
	return FALSE;
}

void AccessibilityTreeAriaNode::UnreferenceHTMLElement(const HTML_Element* helm)
{
	if (helm == m_labelelem)
	{
		m_labelelem = NULL;
	}
	AccessibilityTreeObjectNode::UnreferenceHTMLElement(helm);
}

OP_STATUS AccessibilityTreeAriaNode::GetText(OpString& str) const
{
	str.Empty();
	if (m_labelelem)
	{
		const uni_char* text = m_labelelem->TextContent();
		if (text)
		{
			CopyStringClean(str, text);
			return OpStatus::OK;
		}
	}
	return AccessibilityTreeObjectNode::GetText(str);
}

void AccessibilityTreeAriaNode::Merge(AccessibilityTreeNode* node)
{
	AccessibilityTreeObjectNode::Merge(node);
	if (NodeType() == node->NodeType()) {
		AccessibilityTreeAriaNode* aria_node = static_cast<AccessibilityTreeAriaNode *>(node);
		if (m_last_state != aria_node->m_last_state) {
			AccessibilitySendEvent(m_last_state ^ aria_node->m_last_state);
			m_last_state = aria_node->m_last_state;
		}
		if (m_last_value != aria_node->m_last_value) {
			m_last_value = aria_node->m_last_value;
			// SHOULD HAVE NOTIFICATION ON VALUE CHANGES
		}
		if (m_last_min_value != aria_node->m_last_min_value) {
			m_last_min_value = aria_node->m_last_min_value;
		}
		if (m_last_max_value != aria_node->m_last_max_value) {
			m_last_max_value = aria_node->m_last_max_value;
		}
	}
}

AccessibilityTreeNode::TreeNodeType AccessibilityTreeAriaNode::NodeType() const
{
	return TREE_NODE_TYPE_ARIA;
}

Accessibility::State AccessibilityTreeAriaNode::AccessibilityGetState()
{
	Accessibility::State state = AccessibilityTreeObjectNode::AccessibilityGetState();
	const uni_char* value;
	if (m_ispassword)
		state |= Accessibility::kAccessibilityStatePassword;

	if (!m_elem)
		return state;

	if (m_role == Accessibility::kElementKindListRow ||
		m_role == Accessibility::kElementKindMenuItem ||
		m_role == Accessibility::kElementKindCheckbox ||
		m_role == Accessibility::kElementKindRadioButton)
	{
		// these support checked.
		value = m_elem->GetAttrValue(UNI_L("aria-checked"), m_namespace);
		if (value)
		{
			if (uni_stricmp(value, UNI_L("true")) == 0)
				state |= Accessibility::kAccessibilityStateCheckedOrSelected;
		}
		else
			state |= Accessibility::kAccessibilityStateReadOnly;
	}
	if (m_role == Accessibility::kElementKindListHeader ||
		m_role == Accessibility::kElementKindListRow)
	{
		// these support selected.
		value = m_elem->GetAttrValue(UNI_L("aria-selected"), m_namespace);
		if (value)
		{
			if (uni_stricmp(value, UNI_L("true")) == 0)
				state |= Accessibility::kAccessibilityStateCheckedOrSelected;
		}
		else
			state |= Accessibility::kAccessibilityStateReadOnly;
	}
	if (m_role == Accessibility::kElementKindListRow)
	{
		// these support expanded.
		value = m_elem->GetAttrValue(UNI_L("aria-expanded"), m_namespace);
		if (value)
		{
			state |= Accessibility::kAccessibilityStateExpandable;
			if (uni_stricmp(value, UNI_L("true")) == 0)
				state |= Accessibility::kAccessibilityStateExpanded;
		}
	}
	if (m_role == Accessibility::kElementKindListRow ||
		m_role == Accessibility::kElementKindMenuItem ||
		m_role == Accessibility::kElementKindCheckbox ||
		m_role == Accessibility::kElementKindRadioButton ||
		m_role == Accessibility::kElementKindButton ||
		m_role == Accessibility::kElementKindDropdown ||
		m_role == Accessibility::kElementKindListHeader ||
		m_role == Accessibility::kElementKindOutlineList ||
		m_role == Accessibility::kElementKindList ||
		m_role == Accessibility::kElementKindTable ||
		m_role == Accessibility::kElementKindGrid || 
		m_role == Accessibility::kElementKindMenuBar ||
		m_role == Accessibility::kElementKindMenu ||
		m_role == Accessibility::kElementKindMenuItem)
	{
		// these support disabled.
		value = m_elem->GetAttrValue(UNI_L("aria-disabled"), m_namespace);
		if (value && (uni_stricmp(value, UNI_L("true")) == 0))
		{
			state |= Accessibility::kAccessibilityStateDisabled;
		}
	}
	if (m_role == Accessibility::kElementKindCheckbox ||
		m_role == Accessibility::kElementKindRadioButton ||
		m_role == Accessibility::kElementKindDropdown ||
		m_role == Accessibility::kElementKindMultilineTextedit ||
		m_role == Accessibility::kElementKindSinglelineTextedit)
	{
		// these support readonly.
		value = m_elem->GetAttrValue(UNI_L("aria-readonly"), m_namespace);
		if (value && (uni_stricmp(value, UNI_L("true")) == 0))
		{
			state |= Accessibility::kAccessibilityStateReadOnly;
		}
	}
	return state;
}

Accessibility::ElementKind AccessibilityTreeAriaNode::AccessibilityGetRole() const
{
	return m_role;
}

OP_STATUS AccessibilityTreeAriaNode::AccessibilityGetValue(int &value)
{
	const uni_char* a_value;
	if (m_role == Accessibility::kElementKindProgress ||
		m_role == Accessibility::kElementKindSlider)
	{
		a_value= m_elem->GetAttrValue(UNI_L("aria-valuenow"), m_namespace);
		if (a_value)
		{
			value = uni_atoi(a_value);
			return OpStatus::OK;
		}
	}
	if (m_role == Accessibility::kElementKindCheckbox ||
		m_role == Accessibility::kElementKindRadioButton)
	{
		a_value = m_elem->GetAttrValue(UNI_L("aria-checked"), m_namespace);
		if (a_value)
		{
			if (uni_stricmp(a_value, UNI_L("true")) == 0)
				value = 1;
			else
				value = 0;
			return OpStatus::OK;
		}
	}
	value = 0;
	return OpStatus::ERR;
}


OP_STATUS AccessibilityTreeAriaNode::AccessibilityGetMinValue(int &value)
{
	const uni_char* a_value;
	if (m_role == Accessibility::kElementKindProgress ||
		m_role == Accessibility::kElementKindSlider)
	{
		a_value= m_elem->GetAttrValue(UNI_L("aria-valuemin"), m_namespace);
		if (a_value)
		{
			value = uni_atoi(a_value);
			return OpStatus::OK;
		}
	}
	value = 0;
	return OpStatus::ERR;
}

OP_STATUS AccessibilityTreeAriaNode::AccessibilityGetMaxValue(int &value)
{
	const uni_char* a_value;
	if (m_role == Accessibility::kElementKindProgress ||
		m_role == Accessibility::kElementKindSlider)
	{
		a_value= m_elem->GetAttrValue(UNI_L("aria-valuemax"), m_namespace);
		if (a_value)
		{
			value = uni_atoi(a_value);
			return OpStatus::OK;
		}
	}
	value = 0;
	return OpStatus::ERR;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
