/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/logdoc/selectionpoint.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/selection.h"

/* static */
int SelectionBoundaryPoint::CalculateOffset(HTML_Element* element)
{
	int offset = 0;

	while ((element = element->PredActualStyle()) != NULL)
		offset++;

	return offset;
}

BOOL
SelectionBoundaryPoint::Precedes(const SelectionBoundaryPoint& other_selection) const
{
	if (m_element == other_selection.m_element)
		return m_offset < other_selection.m_offset;

	// FIXME: CODE COPIED FROM DOM_Range::CompareBoundaryPoint - merge!

	HTML_Element *parent;
	unsigned height1 = 0, height2 = 0;

	parent = m_element;
	while (parent)
	{
		++height1;
		parent = parent->ParentActualStyle();
	}

	parent = other_selection.m_element;
	while (parent)
	{
		++height2;
		parent = parent->ParentActualStyle();
	}

	HTML_Element *iter1 = m_element, *iter2 = other_selection.m_element;

	while (height2 < height1 - 1)
	{
		--height1;
		iter1 = iter1->ParentActualStyle();
	}

	while (height1 < height2 - 1)
	{
		--height2;
		iter2 = iter2->ParentActualStyle();
	}

	HTML_Element *last1 = NULL, *last2 = NULL;

	while (iter1 != iter2)
	{
		if (height1 >= height2)
		{
			last1 = iter1;
			iter1 = iter1->ParentActualStyle();
		}
		if (height2 >= height1)
		{
			last2 = iter2;
			iter2 = iter2->ParentActualStyle();
		}

		height1 = height2 = 0;
	}

	//HTML_Element* commonAncestorContainer = iter1;

	unsigned offset1_actual = m_offset, offset2_actual = other_selection.m_offset;

	if (last1)
		offset1_actual = CalculateOffset(last1);

	if (last2)
		offset2_actual = CalculateOffset(last2);

	int result = offset1_actual < offset2_actual ? -1 : offset1_actual > offset2_actual ? 1 : !last1 && last2 ? -1 : last1 && !last2 ? 1 : 0;

	// ---- END COPIED CODE

	return result == -1;
}

BOOL
SelectionBoundaryPoint::operator==(const SelectionBoundaryPoint& other_selection) const
{
	return m_element == other_selection.m_element &&
			m_offset == other_selection.m_offset;
}

void
SelectionBoundaryPoint::operator=(const SelectionBoundaryPoint& X)
{
	m_element = X.m_element;
	m_offset = X.m_offset;
	m_bind_forward = X.m_bind_forward;
}

void
SelectionBoundaryPoint::SetLogicalPosition(HTML_Element* element, int offset)
{
	OP_ASSERT(element);
	OP_ASSERT(offset >= 0);
	OP_ASSERT(element->Type() != HE_DOC_ROOT);
	OP_ASSERT(element->IsIncludedActualStyle() || element->Type() == HE_TEXT);
	m_element = element;
	m_offset = offset;
	m_bind_forward = 0;
}

void
SelectionBoundaryPoint::SetLogicalPositionBeforeElement(HTML_Element* element)
{
	OP_ASSERT(element);
	OP_ASSERT(element->ParentActualStyle());
	OP_ASSERT(element->ParentActualStyle()->Type() != HE_DOC_ROOT);
	OP_ASSERT(element->IsIncludedActualStyle() || element->Type() == HE_TEXT);

	if (element->Type() == HE_TEXT)
		SetLogicalPosition(element, 0);
	else
		SetLogicalPosition(element->ParentActualStyle(), CalculateOffset(element));
}

void
SelectionBoundaryPoint::SetLogicalPositionAfterElement(HTML_Element* element)
{
	OP_ASSERT(element);
	OP_ASSERT(element->ParentActualStyle());
	OP_ASSERT(element->ParentActualStyle()->Type() != HE_DOC_ROOT);
	OP_ASSERT(element->IsIncludedActualStyle() || element->Type() == HE_TEXT);

	if (element->Type() == HE_TEXT)
		SetLogicalPosition(element, element->GetTextContentLength());
	else
		SetLogicalPosition(element->ParentActualStyle(), CalculateOffset(element) + 1);
}
