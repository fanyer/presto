/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/traverse/AccessibleElementManager.h"
#include "modules/accessibility/traverse/AccessibleElementTraversalObject.h"
#include "modules/accessibility/AccessibleDocument.h"
#include "modules/accessibility/acctree/AccessibilityTreeRootNode.h"
#include "modules/accessibility/acctree/AccessibilityTreeURLNode.h"
#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/dochand/fdelm.h"

AccessibleElementManager::AccessibleElementManager(AccessibleDocument* doc, FramesDocument* frm_doc)
	: m_doc(doc)
	, m_frm_doc(frm_doc)
	, m_root_node(NULL)
	, m_dirty(TRUE)
	, m_scanning(FALSE)
{
	MarkDirty();
}

AccessibleElementManager::~AccessibleElementManager()
{
	OP_DELETE(m_root_node);
}

void AccessibleElementManager::MarkDirty()
{
	m_dirty = TRUE;

	if (!m_scanning) {
//		Rescan();
	}
}

FramesDocument* AccessibleElementManager::GetFramesDocument()
{
	return m_frm_doc;
}

void AccessibleElementManager::UnreferenceElement(const HTML_Element* elem)
{
	if (m_root_node)
		m_root_node->UnreferenceHTMLElement(elem);
}

void AccessibleElementManager::UnreferenceWidget(const OpWidget* widget)
{
	if (m_root_node)
		m_root_node->UnreferenceWidget(widget);
}

void AccessibleElementManager::HighlightElement(HTML_Element* new_elem, HTML_Element* old_elem)
{
	AccessibilityTreeNode* node;

	if (m_dirty)
		return; // Do NOT send any events if we are not ready.

	if (m_root_node && old_elem)
	{
		node = m_root_node->FindElement(old_elem);
		if (node)
			node->OnBlur();
	}
	if (m_root_node && new_elem)
	{
		node = m_root_node->FindElement(new_elem);
		if (node)
			node->OnFocus();
		if (!node)
		{
			//HACKHACK this is to catch header elements. These SHOULD be in the acc tree.
			if (new_elem->FirstChild() && new_elem->FirstChild() == new_elem->LastChild())
			{
				m_doc->HighlightElement((HTML_Element*)new_elem->FirstChild());
			}
		}
	}
}

int AccessibleElementManager::GetAccessibleElementCount()
{
	if (m_dirty)
		Rescan();

	if (m_root_node)
		return m_frm_doc->CountFrames() + (m_frm_doc->GetIFrmRoot() ? m_frm_doc->GetIFrmRoot()->CountFrames() : 0) + m_root_node->GetChildrenCount();

	return 0;
}

OpAccessibleItem* AccessibleElementManager::GetAccessibleElement(int i)
{
	if (m_dirty)
		Rescan();

	if (!m_root_node)
		return NULL;

	if (i < m_frm_doc->CountFrames())
	{
		int frame_number = i + 1;
		FramesDocElm* frm_elm = m_frm_doc->GetFrameByNumber(frame_number);
		OP_ASSERT(frm_elm);
		if (frm_elm)
		{
			VisualDevice* vis_dev = frm_elm->GetVisualDevice();
			OP_ASSERT(vis_dev);
			return vis_dev;
		}
		return NULL;
	}
	i -= m_frm_doc->CountFrames();
	FramesDocElm* ifrm_root = m_frm_doc->GetIFrmRoot();
	if (ifrm_root)
	{
		if (i < ifrm_root->CountFrames())
		{
			int frame_number = i + 1;
			FramesDocElm* frm_elm = ifrm_root->GetFrameByNumber(frame_number);
			OP_ASSERT(frm_elm);
			if (frm_elm)
			{
				VisualDevice* vis_dev = frm_elm->GetVisualDevice();
				OP_ASSERT(vis_dev);
				return vis_dev;
			}
			return NULL;
		}
		i -= ifrm_root->CountFrames();
	}


	const AccessibilityTreeNode *node = m_root_node->GetChild(i);
	if (node)
		return node->GetAccessibleObject();

	return NULL;
}

int AccessibleElementManager::GetIndexOfElement(OpAccessibleItem* elem)
{
	if (m_dirty)
		Rescan();

	if (!m_root_node)
		return Accessibility::NoSuchChild;

	int i;
	int n = m_frm_doc->CountFrames();

	for (i = 0; i < n; i++)
	{
		int frame_number = i + 1;
		FramesDocElm* frm_elm = m_frm_doc->GetFrameByNumber(frame_number);
		if (frm_elm)
		{
			VisualDevice* vis_dev = frm_elm->GetVisualDevice();
			if (vis_dev == elem)
				return i;
		}
	}

	int index_offset = n;

	FramesDocElm* ifrm_root = m_frm_doc->GetIFrmRoot();
	if (ifrm_root)
	{
		n = ifrm_root->CountFrames();
		for (i = 0; i < n; i++)
		{
			int frame_number = i + 1;
			FramesDocElm* frm_elm = ifrm_root->GetFrameByNumber(frame_number);
			if (frm_elm)
			{
				VisualDevice* vis_dev = frm_elm->GetVisualDevice();
				if (vis_dev == elem)
					return i + index_offset;
			}
		}
	}

	index_offset += n;

	n = m_root_node->GetChildrenCount();
	for (int i = 0; i < n; i++)
		if (m_root_node->GetChild(i)->GetAccessibleObject() == elem)
			return i + index_offset;

	return Accessibility::NoSuchChild;
}

OpAccessibleItem* AccessibleElementManager::GetAccessibleFocusedElement()
{
	if (m_dirty)
		Rescan();

	if (m_root_node)
	{
		OpInputContext* focused = g_input_manager->GetKeyboardInputContext();
		while (focused)
		{
			if (focused->GetType() >= OpTypedObject::WIDGET_TYPE && focused->GetType() < OpTypedObject::WIDGET_TYPE_LAST)
			{
				OpWidget* widget = static_cast<OpWidget*>(focused);
				if (widget->GetVisualDevice() == m_doc->GetVisualDevice())
				{
					return widget;
				}
			}
			else if (focused == m_doc->GetVisualDevice())
			{
				if (m_doc->GetFocusElement())
				{
					const AccessibilityTreeNode *node = m_root_node->FindElement(m_doc->GetFocusElement());
					if (node)
						return node->GetAccessibleObject();
				}
				break;
			}
			focused = focused->GetParentInputContext();
		}
	}
	return NULL;
}

OpAccessibleItem* AccessibleElementManager::GetAccessibleElementAt(int x, int y)
{
	if (m_dirty)
		Rescan();

	if (m_root_node)
	{
		long count;
		long i;
		OpRect view_bounds;
		VisualDevice* vis_dev = m_doc->GetVisualDevice();
		vis_dev->GetAbsoluteViewBounds(view_bounds);
		OpPoint point(x - view_bounds.x + vis_dev->GetRenderingViewX(), y - view_bounds.y + vis_dev->GetRenderingViewY());
		count = m_frm_doc->CountFrames();
		for (i = 0; i < count; i++)
		{
			int frame_number = i + 1;
			FramesDocElm* frm_elm = m_frm_doc->GetFrameByNumber(frame_number);
			if (frm_elm)
			{
				VisualDevice* vis_dev = frm_elm->GetVisualDevice();
				if (vis_dev)
				{
					OpRect r;
					vis_dev->GetAbsoluteViewBounds(r);
					OpPoint global_point(x, y);
					if (r.Contains(global_point))
						return vis_dev;
				}
			}
		}
		FramesDocElm* ifrm_root = m_frm_doc->GetIFrmRoot();
		if (ifrm_root)
		{
			count = ifrm_root->CountFrames();
			for (i = 0; i < count; i++)
			{
				int frame_number = i + 1;
				FramesDocElm* frm_elm = ifrm_root->GetFrameByNumber(frame_number);
				if (frm_elm)
				{
					VisualDevice* vis_dev = frm_elm->GetVisualDevice();
					if (vis_dev)
					{
						OpRect r;
						vis_dev->GetAbsoluteViewBounds(r);
						OpPoint global_point(x, y);
						if (r.Contains(global_point))
							return vis_dev;
					}
				}
			}
		}
		const AccessibilityTreeNode* node = m_root_node->GetChildAt(point);
		if (node)
			return node->GetAccessibleObject();
	}
	return NULL;
}

OP_STATUS AccessibleElementManager::GetLinks(OpVector<OpAccessibleItem>& links)
{
	int i, len=m_link_objects.GetCount();
	for (i=0;i<len;i++) {
		AccessibilityTreeURLNode *link = m_link_objects.Get(i);
		OP_ASSERT(link);
		OpAccessibleItem *ext = link->GetAccessibleObject();
		if (ext) {
			RETURN_IF_ERROR(links.Add(ext));
		}
	}
	return OpStatus::OK;
}

void AccessibleElementManager::Rescan()
{
	if (!m_doc || !m_frm_doc)
		return;

	m_dirty = FALSE;

//	OP_DELETE(m_root_node);
//	m_root_node = NULL;

	AccessibleElementTraversalObject traverse(m_doc, m_frm_doc);

	m_scanning = TRUE;
	AccessibilityTreeRootNode* new_root_node = traverse.Parse();
	if (m_root_node && new_root_node) {
		m_root_node->Merge(new_root_node);
		OP_DELETE(new_root_node);
	}
	else {
		OP_DELETE(m_root_node);
		m_root_node = new_root_node;
	}
	if (m_root_node)
		m_root_node->ParsingDone();

	m_link_objects.Clear();
	if (m_root_node)
		m_root_node->GetLinks(m_link_objects);
	m_scanning = FALSE;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
