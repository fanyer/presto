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

#include "adjunct/ui_test_framework/OpUiNode.h"
#include "adjunct/ui_test_framework/OpExtensionContainer.h"
#include "modules/accessibility/OpAccessibilityExtensionListener.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

/***********************************************************************************
 ** static factory method
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiNode::Create(OpExtensionContainer* container, OpUiNode*& node)
{
    if(!container)
		return OpStatus::ERR;

    OpAutoPtr<OpUiNode> ui_node(OP_NEW(OpUiNode, ()));

    if(!ui_node.get())
		return OpStatus::ERR_NO_MEMORY;

    RETURN_IF_ERROR(ui_node->SetExtensionContainer(container));

    node = ui_node.release();
    return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpUiNode::OpUiNode() :
	m_extension_container(NULL), 
	m_element(NULL),
	m_removed(FALSE),
	m_visible(FALSE),
	m_enabled(FALSE),
	m_focused(FALSE),
	m_has_value(FALSE),
	m_has_min_value(FALSE),
	m_has_max_value(FALSE),
	m_value(0),
	m_min_value(0),
	m_max_value(0),
	m_state(OpAccessibilityExtension::kAccessibilityStateInvalid),
	m_role(OpAccessibilityExtension::kElementKindUnknown)
{

}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpUiNode::~OpUiNode()
{
	if(m_extension_container)
		m_extension_container->RemoveListener(this);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void OpUiNode::OnExtensionContainerDeleted()
{
//	printf("OpUiNode::OnExtensionContainerDeleted\n");

	StoreAllData();

	if(m_extension_container)
		m_extension_container->RemoveListener(this);

    m_extension_container = NULL;
	m_element = NULL;
	m_removed = TRUE;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiNode::OnChildAdded(OpExtensionContainer* child)
{
//	printf("OpUiNode::OnChildAdded\n");

	OpUiNode* child_node = 0;
	RETURN_IF_ERROR(OpUiNode::Create(child, child_node));
	return AddChild(child_node);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiNode::SetExtensionContainer(OpExtensionContainer* container)
{
	RETURN_IF_ERROR(container->AddListener(this));

    m_extension_container = container;
	m_element = m_extension_container->GetListener();

    return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiNode::AddChild(OpUiNode* child)
{
	if(!child)
		return OpStatus::ERR;

	if(m_children.Find(child) >= 0)
		return OpStatus::OK;

	OpString8 tmp;
	tmp.Set(GetRoleAsString());

	printf("%s\tgot a new child\t", tmp.CStr());

	tmp.Set(child->GetRoleAsString());
	
	printf("the child is a %s\n", tmp.CStr());

    return m_children.Add(child);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiNode::Export(XMLFragment& fragment)
{
	// Open the element tag
	RETURN_IF_ERROR(fragment.OpenElement(GetRoleAsString()));

	// Output the info on this node
	RETURN_IF_ERROR(ExportNode(fragment));

	// Process the children
	for (unsigned int i = 0; i < m_children.GetCount(); i++)
		RETURN_IF_ERROR(m_children.Get(i)->Export(fragment));

	// Close the element
	fragment.CloseElement();

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiNode::ClickAll()
{
	if(m_element)
	{
		OP_STATUS status = m_element->AccessibilityClicked();

		OP_ASSERT(OpStatus::IsSuccess(status));
		
		if(OpStatus::IsMemoryError(status))
			return status;
	}

	// Process the children
	for (unsigned int i = 0; i < m_children.GetCount(); i++)
		RETURN_IF_ERROR(m_children.Get(i)->ClickAll());

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiNode::ExportNode(XMLFragment& fragment)
{
    // -----------------
    // Default is present
    // -----------------
    if(m_removed)
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("removed"), UNI_L("true")));

    // -----------------
    // Default is visible
    // -----------------
    if(!IsVisible())
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("visible"), UNI_L("false")));
    
    // -----------------
    // Default is enabled
    // -----------------
    if(!IsEnabled())
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("enabled"), UNI_L("false")));
    
    // -----------------
    // Default is unfocused
    // -----------------
    if(IsFocused())
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("focused"), UNI_L("true")));
    
    // -----------------
    // Get the text
    // -----------------
    OpString text;
	RETURN_IF_ERROR(GetText(text));
    if(text.HasContent())
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("text"), text.CStr()));
    
    // -----------------
    // Get the description
    // -----------------
    OpString description;
	RETURN_IF_ERROR(GetDescription(description));
    if(description.HasContent())
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("description"), description.CStr()));
    
    // -----------------
    // Get the url
    // -----------------
    OpString url;
	RETURN_IF_ERROR(GetUrl(url));
    if(url.HasContent())
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("url"), url.CStr()));
    
    // -----------------
    // Integer value
    // -----------------
    if(HasValue())
    {
		OpString val;
		RETURN_IF_ERROR(val.AppendFormat(UNI_L("%ld"), GetValue()));
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("value"), val.CStr()));
    }
    
    // -----------------
    // Integer value - min
    // -----------------
    if(HasMinValue())
    {
		OpString val;
		RETURN_IF_ERROR(val.AppendFormat(UNI_L("%ld"), GetMinValue()));
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("min"), val.CStr()));
    }
    
    // -----------------
    // Integer value - max
    // -----------------
    if(HasMaxValue())
    {
		OpString val;
		RETURN_IF_ERROR(val.AppendFormat(UNI_L("%ld"), GetMaxValue()));
		RETURN_IF_ERROR(fragment.SetAttribute(UNI_L("max"), val.CStr()));
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
OP_STATUS OpUiNode::StoreAllData()
{
	m_visible = IsVisible();
	m_enabled = IsEnabled();
	m_focused = IsFocused();
	m_has_value = HasValue();
	m_has_min_value = HasMinValue();
	m_has_max_value = HasMaxValue();

	if(m_has_value)	    m_value     = GetValue();
	if(m_has_min_value) m_min_value = GetMinValue();
	if(m_has_max_value) m_max_value = GetMaxValue();

	RETURN_IF_ERROR(GetText(m_text));
	RETURN_IF_ERROR(GetDescription(m_description));
	RETURN_IF_ERROR(GetUrl(m_url));

	m_state = GetState();
	m_role  = GetRole();

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
const uni_char* OpUiNode::GetRoleAsString()
{
	switch (GetRole())
	{
		case OpAccessibilityExtension::kElementKindApplication:				return UNI_L("Application");
		case OpAccessibilityExtension::kElementKindWindow:					return UNI_L("Window");
		case OpAccessibilityExtension::kElementKindAlert:					return UNI_L("Alert");
		case OpAccessibilityExtension::kElementKindAlertDialog:				return UNI_L("AlertDialog");
		case OpAccessibilityExtension::kElementKindDialog:					return UNI_L("Dialog");
		case OpAccessibilityExtension::kElementKindView:					return UNI_L("View");
		case OpAccessibilityExtension::kElementKindWebView:					return UNI_L("WebView");
		case OpAccessibilityExtension::kElementKindToolbar:					return UNI_L("Toolbar");
		case OpAccessibilityExtension::kElementKindButton:					return UNI_L("Button");
		case OpAccessibilityExtension::kElementKindCheckbox:				return UNI_L("Checkbox");
		case OpAccessibilityExtension::kElementKindDropdown:				return UNI_L("Dropdown");
		case OpAccessibilityExtension::kElementKindRadioTabGroup:			return UNI_L("RadioTabGroup");
		case OpAccessibilityExtension::kElementKindRadioButton:				return UNI_L("RadioButton");
		case OpAccessibilityExtension::kElementKindTab:						return UNI_L("Tab");
		case OpAccessibilityExtension::kElementKindStaticText:				return UNI_L("StaticText");
		case OpAccessibilityExtension::kElementKindSinglelineTextedit:		return UNI_L("SinglelineTextedit");
		case OpAccessibilityExtension::kElementKindMultilineTextedit:		return UNI_L("MultilineTextedit");
		case OpAccessibilityExtension::kElementKindHorizontalScrollbar:		return UNI_L("HorizontalScrollbar");
		case OpAccessibilityExtension::kElementKindVerticalScrollbar:		return UNI_L("VerticalScrollbar");
		case OpAccessibilityExtension::kElementKindScrollview:				return UNI_L("Scrollview");
		case OpAccessibilityExtension::kElementKindSlider:					return UNI_L("Slider");
		case OpAccessibilityExtension::kElementKindProgress:				return UNI_L("Progress");
		case OpAccessibilityExtension::kElementKindLabel:					return UNI_L("Label");
		case OpAccessibilityExtension::kElementKindDescriptor:				return UNI_L("Descriptor");
		case OpAccessibilityExtension::kElementKindLink:					return UNI_L("Link");
		case OpAccessibilityExtension::kElementKindImage:					return UNI_L("Image");
		case OpAccessibilityExtension::kElementKindMenuBar:					return UNI_L("MenuBar");
		case OpAccessibilityExtension::kElementKindMenu:					return UNI_L("Menu");
		case OpAccessibilityExtension::kElementKindMenuItem:				return UNI_L("MenuItem");
		case OpAccessibilityExtension::kElementKindList:					return UNI_L("List");
		case OpAccessibilityExtension::kElementKindGrid:					return UNI_L("Grid");
		case OpAccessibilityExtension::kElementKindOutlineList:				return UNI_L("OutlineList");
		case OpAccessibilityExtension::kElementKindTable:					return UNI_L("Table");
		case OpAccessibilityExtension::kElementKindHeaderList:				return UNI_L("HeaderList");
		case OpAccessibilityExtension::kElementKindListRow:					return UNI_L("ListRow");
		case OpAccessibilityExtension::kElementKindListHeader:				return UNI_L("ListHeader");
		case OpAccessibilityExtension::kElementKindListColumn:				return UNI_L("ListColumn");
		case OpAccessibilityExtension::kElementKindListCell:				return UNI_L("ListCell");
		case OpAccessibilityExtension::kElementKindScrollbarPartArrowUp:	return UNI_L("ScrollbarPartArrowUp");
		case OpAccessibilityExtension::kElementKindScrollbarPartArrowDown:	return UNI_L("ScrollbarPartArrowDown");
		case OpAccessibilityExtension::kElementKindScrollbarPartPageUp:		return UNI_L("ScrollbarPartPageUp");
		case OpAccessibilityExtension::kElementKindScrollbarPartPageDown:	return UNI_L("ScrollbarPartPageDown");
		case OpAccessibilityExtension::kElementKindScrollbarPartKnob:		return UNI_L("ScrollbarPartKnob");
		case OpAccessibilityExtension::kElementKindDropdownButtonPart:		return UNI_L("DropdownButtonPart");
		case OpAccessibilityExtension::kElementKindDirectory:				return UNI_L("Directory");
		case OpAccessibilityExtension::kElementKindDocument:				return UNI_L("Document");
		case OpAccessibilityExtension::kElementKindLog:						return UNI_L("Log");
		case OpAccessibilityExtension::kElementKindPanel:					return UNI_L("Panel");
		case OpAccessibilityExtension::kElementKindWorkspace:				return UNI_L("Workspace");
		case OpAccessibilityExtension::kElementKindSplitter:				return UNI_L("Splitter");
		case OpAccessibilityExtension::kElementKindUnknown:					return UNI_L("Unknown");
	}

	OP_ASSERT(FALSE);
	return UNI_L("???");
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL OpUiNode::IsVisible()
{
	if(m_removed)
		return m_visible;

	return !(GetState() & OpAccessibilityExtension::kAccessibilityStateInvisible);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL OpUiNode::IsEnabled()
{
	if(m_removed)
		return m_enabled;

	return !(GetState() & OpAccessibilityExtension::kAccessibilityStateDisabled);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL OpUiNode::IsFocused()
{
	if(m_removed)
		return m_focused;

	return (GetState() & OpAccessibilityExtension::kAccessibilityStateFocused);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiNode::GetText(OpString& text)
{
	if(m_removed)
		return text.Set(m_text.CStr());

    if(OpStatus::IsMemoryError(m_element->AccessibilityGetText(text)))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiNode::GetDescription(OpString& description)
{
	if(m_removed)
		return description.Set(m_description.CStr());

    if(OpStatus::IsMemoryError(m_element->AccessibilityGetDescription(description)))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiNode::GetUrl(OpString& url)
{
	if(m_removed)
		return url.Set(m_url.CStr());

    if(OpStatus::IsMemoryError(m_element->AccessibilityGetURL(url)))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL OpUiNode::HasValue()
{
	if(m_removed)
		return m_has_value;

    int value = 0;
    return OpStatus::IsSuccess(m_element->AccessibilityGetValue(value));
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL OpUiNode::HasMinValue()
{
	if(m_removed)
		return m_has_min_value;

    int min = 0;
    return OpStatus::IsSuccess(m_element->AccessibilityGetMinValue(min));
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL OpUiNode::HasMaxValue()
{
	if(m_removed)
		return m_has_max_value;

    int max = 0;
    return OpStatus::IsSuccess(m_element->AccessibilityGetMaxValue(max));
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
int OpUiNode::GetValue()
{
	if(m_removed)
		return m_value;

	OP_ASSERT(HasValue());

    int value = 0;
    m_element->AccessibilityGetValue(value);
	return value;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
int OpUiNode::GetMinValue()
{
	OP_ASSERT(HasMinValue());

	if(m_removed)
		return m_min_value;

    int min = 0;
    m_element->AccessibilityGetMinValue(min);
	return min;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
int OpUiNode::GetMaxValue()
{
	OP_ASSERT(HasMaxValue());

	if(m_removed)
		return m_max_value;

    int max = 0;
    m_element->AccessibilityGetMaxValue(max);
	return max;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension::AccessibilityState OpUiNode::GetState()
{
	if(m_removed)
		return m_state;

	return m_element->AccessibilityGetState();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpAccessibilityExtension::ElementKind OpUiNode::GetRole()
{
	if(m_removed)
		return m_role;

	return m_element->AccessibilityGetRole();
}
#endif // FEATURE_UI_TEST
