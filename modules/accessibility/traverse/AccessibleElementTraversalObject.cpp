/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/traverse/AccessibleElementTraversalObject.h"
#include "modules/accessibility/AccessibleDocument.h"
#include "modules/accessibility/acctree/AccessibilityTreeAriaNode.h"
#include "modules/accessibility/acctree/AccessibilityTreeImageNode.h"
#include "modules/accessibility/acctree/AccessibilityTreeObjectNode.h"
#include "modules/accessibility/acctree/AccessibilityTreeLabelNode.h"
#include "modules/accessibility/acctree/AccessibilityTreeRadioGroupNode.h"
#include "modules/accessibility/acctree/AccessibilityTreeRootNode.h"
#include "modules/accessibility/acctree/AccessibilityTreeTextNode.h"
#include "modules/accessibility/acctree/AccessibilityTreeURLNode.h"
#include "modules/accessibility/acctree/AccessibilityTreeWidgetNode.h"
#include "modules/accessibility/acc_utils.h"
#include "modules/layout/box/box.h"
#include "modules/layout/box/inline.h"
#include "modules/layout/box/tables.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/forms/piforms.h"
#include "modules/widgets/OpWidget.h"

AccessibleElementTraversalObject::AccessibleElementTraversalObject(AccessibleDocument* doc, FramesDocument* frm_doc)
	: TraversalObject(frm_doc)
	, m_doc(doc)
	, m_frm_doc(frm_doc)
	, m_root_object(NULL)
	, m_current_text_object(NULL)
	, m_current_label_object(NULL)
	, m_current_container(NULL)
	, m_current_button(NULL)
	, translate_x(0)
	, translate_y(0)
	, first_word(0)
{
	OP_ASSERT(m_doc);
	OP_ASSERT(m_frm_doc);
	SetTraverseType(TRAVERSE_ONE_PASS);
}

AccessibilityTreeRootNode*
AccessibleElementTraversalObject::Parse()
{
	translate_x = 0;
	translate_y = 0;
	first_word = 0;
	m_current_text_object = NULL;
	m_current_label_object = NULL;
	m_current_button = NULL;
	m_root_object = NULL;

	m_link_objects.Remove(0, m_link_objects.GetCount());
	m_radio_groups.Remove(0, m_radio_groups.GetCount());

	LogicalDocument* logdoc = m_frm_doc->GetLogicalDocument();
	if (logdoc && logdoc->GetRoot() && logdoc->GetRoot()->GetLayoutBox())
	{
		m_root_object = OP_NEW(AccessibilityTreeRootNode,(m_doc));
		if (m_root_object)
		{
#ifdef _DEBUG
			printf("====Begin Parsing====\n");
#endif
			Traverse(logdoc->GetRoot());
#ifdef _DEBUG
			printf("====End Parsing====\n");
#endif

			// It would be really nice if all links have some text descriptions.
			// Sadly that is not always the case. To help the screen reader out a bit,
			// we remove undescriptive (image) links if (and only if) another link points
			// to the same document.

			long i, count = m_link_objects.GetCount();
			for (i = 0; i < count; i++)
			{
				AccessibilityTreeURLNode* link = m_link_objects.Get(i);
				if (link)
				{
					link->m_links_vector = NULL;
					OpString str;
					link->GetText(str);
					if (str.Length() == 0)
					{
						// Gah, no textual info. Candidate for tossing.
						long j;
						for (j = i + 1; j < count; j++)
						{
							AccessibilityTreeURLNode* link2 = m_link_objects.Get(j);
							if (link2 && (uni_strcmp(link->GetURL(), link2->GetURL()) == 0))
							{
								// Yup, found a better URL for this one. Toss the duplicate.
								m_link_objects.Remove(i);
								OP_DELETE(link);
								i--;	// reset counter and max count to compensate for the item pulled out of the list we are iterating through.
								count--;
								break; // out of j loop
							}
						}
					}
				}
			}
		}
	}
	return m_root_object;
}

void
AccessibleElementTraversalObject::Translate(LayoutCoord x, LayoutCoord y)
{
	translate_x += x;
	translate_y += y;
}

BOOL
AccessibleElementTraversalObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
#ifdef _DEBUG
	printf("EnterVerticalBox\n");
#endif
	TraverseInfo dummy;
	BOOL result = TraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, dummy);
	if (box && result)
	{
		HTML_Element* html_element = box->GetHtmlElement();
#ifdef _DEBUG
#ifdef _DEBUG
		printf("Element=%p\n", html_element);
#endif
		if (html_element)
		{
			const uni_char* text = html_element->TextContent();
			if (text)
			{
				OpString8 str;
				str.Set(text);
				printf("Text: \"%s\"\n", str.CStr());
			}
		}
		if (html_element && html_element->Type() == HE_IMG)
			printf("Image in VerticalBox! he=%p\n", html_element);
#endif
		if (html_element)
		{
			HTML_ElementType type = html_element->Type();
#ifdef _DEBUG
				printf("Type=%d\n", type);
#endif
			const uni_char* role = html_element->GetAttrValue(UNI_L("role"), NS_IDX_ANY_NAMESPACE);
			AccessibilityTreeNode* parent = m_current_container;
			if (!parent)
				parent = m_root_object;
			if (!role)
				role = html_element->GetXmlAttr(UNI_L("role"));
			if (role && (uni_strcmp(role, "presentation") != 0) && html_element->Type() != HE_HTML)
			{
				// Just assume for now that it is an aria thing
				m_current_container = OP_NEW(AccessibilityTreeAriaNode,(html_element, parent));
				m_containers.Add(m_current_container);
			}
			else if (IsContainerType(type) || (type == HE_TD || type == HE_TH) && m_current_container && m_current_container->GetHTMLElement()->Type() == HE_TR)
			{
#ifdef _DEBUG
				printf("Container box\n");
#endif

				m_current_container = OP_NEW(AccessibilityTreeObjectNode,(html_element, parent));
				m_containers.Add(m_current_container);
			}
		}
	}
	return result;
}

void
AccessibleElementTraversalObject::LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
#ifdef _DEBUG
	printf("LeaveVerticalBox\n");
#endif
	if (box)
	{
		HTML_Element* html_element = box->GetHtmlElement();
#ifdef _DEBUG
		printf("Element=%p\n", html_element);
#endif
		if (m_current_container)
		{
			if (m_current_container->GetHTMLElement() == html_element)
			{
				m_containers.RemoveByItem(m_current_container);
				OpRect bounds;
				m_current_container->GetBounds(bounds);
				if (bounds.IsEmpty())
				{
#ifdef _DEBUG
					printf("Bounds empty\n");
#endif
					OP_DELETE(m_current_container);
				}
				else
				{
					HTML_ElementType type = html_element->Type();
#ifdef _DEBUG
					printf("Type=%d\n", type);
#endif
					if (IsContainerType(type) || (type == HE_TD || type == HE_TH) && m_current_container->GetChildrenCount())
					{
						OpRect r(translate_x, translate_y, box->GetWidth(), box->GetHeight());
						m_current_container->SetBounds(r);
					}
					else
						m_current_container->SetBounds(bounds);
				}
				m_current_container = m_containers.Get(m_containers.GetCount() - 1);
			}
		}
	}
	TraversalObject::LeaveVerticalBox(layout_props, box, traverse_info);
}

BOOL
AccessibleElementTraversalObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box,  LayoutCoord baseline, TraverseInfo& traverse_info)
{
#ifdef _DEBUG
	printf("EnterInlineBox\n");
#endif
	BOOL result = TraversalObject::EnterInlineBox(layout_props, box, box_area, segment, start_of_box, end_of_box, baseline, traverse_info);
	if (box && result)
	{
		HTML_Element* html_element = box->GetHtmlElement();
		ButtonContainer* button_container  = box->GetButtonContainer();
		if (button_container)
		{
			OpButton* button = button_container->GetButton();
			if (button)
			{
				AccessibilityTreeWidgetNode *node = OP_NEW(AccessibilityTreeWidgetNode,(html_element, button, m_root_object));
				OpRect r(translate_x + box_area.left, translate_y + box_area.top, box_area.right - box_area.left, box_area.bottom - box_area.top);

				if (node)
					node->SetBounds(r);

				m_current_button = button;
				m_current_button_text.Empty();
			}
		}
		if (html_element && !m_root_object->FindElement(html_element))
		{
			if (html_element->Type() == HE_LABEL)
			{
				AccessibilityTreeNode* parent = m_current_container;
				if (!parent)
					parent = m_root_object;
				m_current_label_object = OP_NEW(AccessibilityTreeLabelNode,(html_element, parent));
				m_current_container = m_current_label_object;
				m_containers.Add(m_current_container);
			}
			else
			{
				const uni_char* role = html_element->GetAttrValue(UNI_L("role"));
				if (!role)
					role = html_element->GetXmlAttr(UNI_L("role"));
				if (role && (uni_strcmp(role, "presentation") != 0))
				{
					// Just assume for now that it is an aria thing
					AccessibilityTreeNode* parent = m_current_container;
					if (!parent)
						parent = m_root_object;
					m_current_container = OP_NEW(AccessibilityTreeAriaNode,(html_element, parent));
					m_containers.Add(m_current_container);
				}
			}
		}
	}
	return result;
}

void
AccessibleElementTraversalObject::LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info)
{
#ifdef _DEBUG
	printf("LeaveInlineBox\n");
#endif
	if (box)
	{
		if (m_current_button)
		{
			ButtonContainer* button_container  = box->GetButtonContainer();
			if (button_container && button_container->GetButton() == m_current_button)
			{
				m_current_button = NULL;
			}
		}
		if (m_current_label_object)
		{
			if (m_current_label_object->GetHTMLElement() == box->GetHtmlElement())
			{
				m_current_label_object = NULL;
			}
		}
		if (m_current_container)
		{
			if (m_current_container->GetHTMLElement() == box->GetHtmlElement())
			{
				m_containers.RemoveByItem(m_current_container);
				OpRect bounds;
				m_current_container->GetBounds(bounds);
				if (bounds.IsEmpty())
					OP_DELETE(m_current_container);
				m_current_container = m_containers.Get(m_containers.GetCount() - 1);
			}
		}
	}
	TraversalObject::LeaveInlineBox(layout_props, box, box_area, start_of_box, end_of_box, traverse_info);
}

BOOL
AccessibleElementTraversalObject::EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
#ifdef _DEBUG
	printf("EnterLine\n");
	if (containing_element)
	{
		const uni_char* text = containing_element->TextContent();
		if (text)
		{
			OpString8 str;
			str.Set(text);
			printf("Text: \"%s\"\n", str.CStr());
		}
		if (containing_element && containing_element->Type() == HE_IMG)
			printf("Image in Line! he=%p\n", containing_element);
	}
#endif
	return TraversalObject::EnterLine(parent_lprops, line, containing_element, traverse_info);
}

void
AccessibleElementTraversalObject::LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info)
{
#ifdef _DEBUG
	printf("LeaveLine\n");
#endif
	TraversalObject::LeaveLine(parent_lprops, line, containing_element, traverse_info);
}

void
AccessibleElementTraversalObject::EnterTextBox(LayoutProperties* layout_props, Text_Box* box, LineSegment& segment)
{
#ifdef _DEBUG
	printf("EnterTextBox\n");
#endif
	if (box)
	{
		first_word = GetWordIndex();
		HTML_Element* html_element = box->GetHtmlElement();
		if (m_current_button)
		{
			CopyStringClean(m_current_button_text, html_element->TextContent());
			m_current_button->SetAccessibilityText(m_current_button_text.CStr());
			return;
		}
		else if (html_element && !m_root_object->FindElement(html_element))
		{
			OpString test;
			CopyStringClean(test, html_element->TextContent());
			if (test.Length() == 1 && test[0]==' ')
				test.Empty();
			if (test.Length() && (!m_current_container || m_current_container->GetHTMLElement() != html_element))
			{
				if (m_current_container && m_current_container->FlattenTextContent())
				{
					m_current_container->SetTextElement(html_element);
				}
				if (!m_current_container || !m_current_container->FlattenTextContent() || m_current_container->GetHTMLElement()->Type() == HE_TABLE)
				{
					AccessibilityTreeURLNode * url_obj = FindURLForElement(html_element);
					if (url_obj)
						m_current_text_object = OP_NEW(AccessibilityTreeTextNode,(html_element, url_obj));
					else if (m_current_container)
						m_current_text_object = OP_NEW(AccessibilityTreeTextNode,(html_element, m_current_container));
					else
						m_current_text_object = OP_NEW(AccessibilityTreeTextNode,(html_element, m_root_object));
				}
			}
		}
	}

	TraversalObject::EnterTextBox(layout_props, box, segment);
}

void
AccessibleElementTraversalObject::LeaveTextBox(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline)
{
#ifdef _DEBUG
	printf("LeaveTextBox\n");
#endif
	m_current_text_object = NULL;
	TraversalObject::LeaveTextBox(layout_props, segment, LayoutCoord(baseline));
}

BOOL
AccessibleElementTraversalObject::EnterTableRow(TableRowBox* row)
{
	BOOL result = TraversalObject::EnterTableRow(row);
	HTML_Element* html_element = row->GetHtmlElement();
	if (html_element && result)
	{
		if (m_current_container && m_current_container->GetHTMLElement()->Type() == HE_TABLE)
		{
			AccessibilityTreeNode* parent = m_current_container;
			if (!parent)
				parent = m_root_object;
			m_current_container = OP_NEW(AccessibilityTreeObjectNode,(html_element, parent));
			m_containers.Add(m_current_container);
		}
	}
	return result;
}

void
AccessibleElementTraversalObject::LeaveTableRow(TableRowBox* row, TableContent* table)
{
	HTML_Element* html_element = row->GetHtmlElement();
	if (html_element)
	{
		if (m_current_container)
		{
			if (m_current_container->GetHTMLElement() == html_element)
			{
				m_containers.RemoveByItem(m_current_container);
				OpRect bounds;
				m_current_container->GetBounds(bounds);
				if (bounds.IsEmpty())
					OP_DELETE(m_current_container);
				m_current_container = m_containers.Get(m_containers.GetCount() - 1);
			}
		}
	}
}

BOOL
AccessibleElementTraversalObject::EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info)
{
	BOOL result = TraversalObject::EnterTableContent(layout_props, content, table_top, table_height, traverse_info);
	HTML_Element* html_element = content->GetHtmlElement();
	if (html_element && result)
	{
		const uni_char* role = html_element->GetAttrValue(UNI_L("role"));
		if (!role)
			role = html_element->GetXmlAttr(UNI_L("role"));
		if (!role || ((uni_strcmp(role, "presentation") != 0)))
		{
			AccessibilityTreeNode* parent = m_current_container;
			if (!parent)
				parent = m_root_object;
			m_current_container = OP_NEW(AccessibilityTreeObjectNode,(html_element, parent));
			m_containers.Add(m_current_container);
		}
	}
	return result;
}

void
AccessibleElementTraversalObject::LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info)
{
	TraversalObject::LeaveTableContent(content, layout_props, traverse_info);

	HTML_Element* html_element = content->GetHtmlElement();
	if (html_element)
	{
		if (m_current_container)
		{
			if (m_current_container->GetHTMLElement() == html_element)
			{
				m_containers.RemoveByItem(m_current_container);
				OpRect bounds;
				m_current_container->GetBounds(bounds);
				if (bounds.IsEmpty())
					OP_DELETE(m_current_container);
				else
					m_current_container->SetBounds(bounds);
				m_current_container = m_containers.Get(m_containers.GetCount() - 1);
			}
		}
	}
}

BOOL
AccessibleElementTraversalObject::TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element)
{
	return TraversalObject::TraversePositionedElement(element, containing_element);
}

void
AccessibleElementTraversalObject::HandleTextContent(LayoutProperties* layout_props,
													Text_Box* box,
													const uni_char* word,
													int word_length,
													LayoutCoord word_width,
													LayoutCoord space_width,
													LayoutCoord justified_space_extra,
													const WordInfo& word_info,
													LayoutCoord x,
													LayoutCoord virtual_pos,
													LayoutCoord baseline,
													LineSegment& segment,
													LayoutCoord line_height)
{
#ifdef _DEBUG
	printf("HandleTextContent\n");
#endif
	AccessibilityTreeNode* node = m_current_text_object;
	if (!node)
		node = m_current_container;
	if (node)
	{
		OpRect obj_rect;
		OpRect r(translate_x + x, translate_y, word_width, line_height);
		node->GetBounds(obj_rect);
		obj_rect.UnionWith(r);
		node->SetBounds(obj_rect);
	}
//	TraversalObject::HandleTextContent(layout_props, box, word, word_length, word_width, space_width, justified_space_extra,
//											 word_info, x, virtual_pos, baseline, segment, line_height);
}

void
AccessibleElementTraversalObject::HandleTextContent(LayoutProperties* layout_props, FormObject* form_obj)
{
#ifdef _DEBUG
	printf("HandleTextContent\n");
#endif
//	TraversalObject::HandleTextContent(layout_props, form_obj);
}

void
AccessibleElementTraversalObject::HandleReplacedContent(LayoutProperties* props, ReplacedContent* content)
{
#ifdef _DEBUG
	printf("HandleReplacedContent\n");
#endif
	if (content)
	{
		HTML_Element* html_element = content->GetHtmlElement();
		if (html_element && !m_root_object->FindElement(html_element))
		{
			OpRect r(translate_x, translate_y, content->GetWidth(), content->GetHeight());
			FormObject* form_obj;
			if (html_element->Type() == HE_IMG)
			{
				AccessibilityTreeURLNode * url_obj = FindURLForElement(html_element);
				const uni_char* text = html_element->GetStringAttr(ATTR_ALT);
				if (!text || !*text)
					text = html_element->GetStringAttr(ATTR_LONGDESC);

				AccessibilityTreeImageNode* node = NULL;
				AccessibilityTreeNode* parent = url_obj;
				if (!parent)
					parent = m_current_container;
				if (!parent)
					parent = m_root_object;
				if (url_obj)
					node = OP_NEW(AccessibilityTreeImageNode,(html_element, parent));
				else if (text)
					node = OP_NEW(AccessibilityTreeImageNode,(html_element, parent));

				if (node)
					node->SetBounds(r);
			}
			else if ((form_obj = content->GetFormObject()) != NULL)
			{
				AccessibilityTreeRadioGroupNode* radio_group = NULL;
				if (html_element && html_element->GetInputType() == INPUT_RADIO && html_element->GetName())
					radio_group = FindRadioGroupForRadioButton(html_element);
				OpWidget* widget = form_obj->GetWidget();
				if (widget)
				{
					AccessibilityTreeNode* parent = radio_group;
					if (!parent)
						parent = m_current_container;
					if (!parent)
						parent = m_root_object;
					AccessibilityTreeWidgetNode *node = OP_NEW(AccessibilityTreeWidgetNode,(html_element, widget, parent));

					if (node)
					{
						node->SetBounds(r);
						if (m_current_label_object)
							m_current_label_object->SetLabeledControl(node);
					}
				}
			}
		}
	}
//	TraversalObject::HandleReplacedContent(props, content);
}

AccessibilityTreeURLNode*
AccessibleElementTraversalObject::FindURLForElement(HTML_Element* html_element)
{
	AccessibilityTreeURLNode* url_node = NULL;
	HTML_Element* anchor_element = html_element->GetAnchorTags();
	const uni_char* url_str = NULL;
	if (anchor_element) {
		URL url = anchor_element->GetAnchor_URL(m_frm_doc);
		url_str = url.UniName(PASSWORD_SHOW_UNESCAPE_URL);
	}
	AccessibilityTreeNode* parent = m_current_container;
	if (!parent) {
		parent = m_root_object;
	}
	if (url_str) {
		long i, count = m_link_objects.GetCount();
		for (i = 0; i < count; i++)
		{
			AccessibilityTreeURLNode* node = m_link_objects.Get(i);
			OP_ASSERT(node);
			if (node->GetHTMLElement() == anchor_element)
			{
				url_node = node;
				break;
			}
		}
		if (!url_node) {
			url_node = OP_NEW(AccessibilityTreeURLNode,(anchor_element, parent));
			if (url_node)
			{
				m_link_objects.Add(url_node);
				url_node->m_links_vector = &m_link_objects;
			}
		}
	}
	return url_node;
}

AccessibilityTreeRadioGroupNode*
AccessibleElementTraversalObject::FindRadioGroupForRadioButton(HTML_Element* radio_elm)
{
	if (!radio_elm->GetName())
		return NULL;

	AccessibilityTreeRadioGroupNode* radio_node = NULL;
	DocumentRadioGroups& docgroups = m_frm_doc->GetLogicalDocument()->GetRadioGroups();
	FormRadioGroups* groups = docgroups.GetFormRadioGroupsForRadioButton(m_frm_doc, radio_elm, FALSE);
	if (groups) // else either not inserted into the document yet or we got OOM while registering all radio buttons
	{
		FormRadioGroup* group = groups->Get(radio_elm->GetName(), FALSE);
		if (group)  // else we got OOM while registering all radio buttons
		{
			long i, count = m_radio_groups.GetCount();
			for (i = 0; i < count; i++)
			{
				AccessibilityTreeRadioGroupNode* node = m_radio_groups.Get(i);
				OP_ASSERT(node);
				if (node->GetGroup() == group)
				{
					radio_node = node;
					break;
				}
			}
			if (!radio_node)
			{
				AccessibilityTreeNode* rg_parent = m_root_object;
				for (i = m_containers.GetCount() - 1; i >= 0; i--)
					if (m_containers.Get(i)->GetHTMLElement()->Type() == HE_FORM)
					{
						rg_parent = m_containers.Get(i);
						break;
					}
#ifdef _MACINTOSH_
				rg_parent = OP_NEW(AccessibilityTreeObjectNode,(NULL, m_root_object));
#endif
				radio_node = OP_NEW(AccessibilityTreeRadioGroupNode,(group, rg_parent));
				if (radio_node)
					m_radio_groups.Add(radio_node);
			}
		}
	}
	return radio_node;
}

BOOL AccessibleElementTraversalObject::IsContainerType(HTML_ElementType type)
{
	return (type == HE_DIV
		||	type == HE_P
		||	type == HE_ADDRESS
		||	type == HE_CENTER
		||	type == HE_FORM
		);
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
