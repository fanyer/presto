/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ACCESSIBLE_ELEMENT_TRAVERSALOBJECT_H
#define ACCESSIBLE_ELEMENT_TRAVERSALOBJECT_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/layout/traverse/traverse.h"
#include "modules/util/adt/opvector.h"

class AccessibilityTreeNode;
class AccessibilityTreeRadioGroupNode;
class AccessibilityTreeURLNode;
class AccessibilityTreeRootNode;
class AccessibilityTreeLabelNode;
class AccessibilityTreeTextNode;
class AccessibilityTreeObjectNode;
class FramesDocument;
class AccessibleDocument;

//#define USE_VISUAL_TRAVERSAL_OBJECT

/** AccessibleElementTraversalObject
	This class generates a tree of accessibility tree nodes based on the current document.
	It is called from AccessibleElementManager, which maintains the tree, and ensures the tree items are correctly deleted.
	This Split-up happens because often very minor changes causes the document to re-flow, and we want as many of the
	OpAccessibilityExtension objects as possible to survive. So, we call the Parse function to generate the new tree,
	and then we copy applicable differences to the old tree.
  */


class AccessibleElementTraversalObject
	: protected TraversalObject
{
public:
	AccessibleElementTraversalObject(AccessibleDocument* doc, FramesDocument* frm_doc);

	AccessibilityTreeRootNode* Parse();

private:
	virtual BOOL	EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info);
	virtual void	LeaveVerticalBox(LayoutProperties* layout_props, VerticalBox* box, TraverseInfo& traverse_info);
	virtual BOOL	EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info);
	virtual void	LeaveInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, BOOL start_of_box, BOOL end_of_box, TraverseInfo& traverse_info);
	virtual BOOL	EnterLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);
	virtual void	LeaveLine(LayoutProperties* parent_lprops, const Line* line, HTML_Element* containing_element, TraverseInfo& traverse_info);
	virtual void	EnterTextBox(LayoutProperties* layout_props, Text_Box* box, LineSegment& segment);
	virtual void	LeaveTextBox(LayoutProperties* layout_props, LineSegment& segment, LayoutCoord baseline);
	virtual BOOL	EnterTableRow(TableRowBox* row);
	virtual void	LeaveTableRow(TableRowBox* row, TableContent* table);
	virtual BOOL	EnterTableContent(LayoutProperties* layout_props, TableContent* content, LayoutCoord table_top, LayoutCoord table_height, TraverseInfo& traverse_info);
	virtual void	LeaveTableContent(TableContent* content, LayoutProperties* layout_props, TraverseInfo& traverse_info);
	virtual BOOL	TraversePositionedElement(HTML_Element* element, HTML_Element* containing_element);
	virtual void	HandleTextContent(LayoutProperties* layout_props,
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
									  LayoutCoord line_height);
	virtual void	HandleTextContent(LayoutProperties* layout_props, FormObject* form_obj);
	virtual void	HandleReplacedContent(LayoutProperties* props, ReplacedContent* content);
	virtual void	Translate(LayoutCoord x, LayoutCoord y);
private:
	AccessibilityTreeURLNode* FindURLForElement(HTML_Element* html_element);
	AccessibilityTreeRadioGroupNode* FindRadioGroupForRadioButton(HTML_Element* radio_elm);
	BOOL IsContainerType(HTML_ElementType type);
	AccessibleDocument* m_doc;
	FramesDocument* m_frm_doc;
	AccessibilityTreeRootNode* m_root_object;
	AccessibilityTreeTextNode* m_current_text_object;
	AccessibilityTreeLabelNode* m_current_label_object;
	AccessibilityTreeObjectNode* m_current_container;
	OpButton* m_current_button;
	OpString m_current_button_text;
	OpVector<AccessibilityTreeObjectNode> m_containers;
	OpVector<AccessibilityTreeURLNode> m_link_objects;
	OpVector<AccessibilityTreeRadioGroupNode> m_radio_groups;
	int translate_x;
	int translate_y;
	int first_word;
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // ACCESSIBLE_ELEMENT_TRAVERSALOBJECT_H
