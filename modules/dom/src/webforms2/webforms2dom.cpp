 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
  *
  * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.	It may not be distributed
  * under any circumstances.
  */

/**
 * @author Daniel Bratell
 */
#include "core/pch.h"

#include "modules/dom/src/webforms2/webforms2dom.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domhtml/htmldoc.h"

#include "modules/forms/webforms2support.h"
#include "modules/logdoc/htm_elm.h"

// LabelCollectionFilter
/* virtual */
void LabelCollectionFilter::Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc)
{
	visit_children = TRUE;
	include = FALSE; // Fallback
	if (element->IsMatchingType(HE_LABEL, NS_HTML))
	{
		const uni_char* for_attr = element->GetStringAttr(ATTR_FOR);
		if (for_attr)
		{
			const uni_char* id_attr = m_form_element->GetId();
			if (id_attr)
				include = uni_str_eq(id_attr, for_attr);
		}
		else // No |for| attribute
			include = element->IsAncestorOf(m_form_element);
	}
}

/* virtual */
DOM_CollectionFilter *LabelCollectionFilter::Clone() const
{
	LabelCollectionFilter* clone = OP_NEW(LabelCollectionFilter, (m_form_element));
	if (clone)
		clone->allocated = TRUE;

	return clone; // Might be null if OOM
}

/* virtual */ BOOL
LabelCollectionFilter::IsMatched(unsigned collections)
{
	return (collections & (DOM_Environment::COLLECTION_NAME_OR_ID | DOM_Environment::COLLECTION_LABELS)) != 0;
}

/* virtual */ BOOL
LabelCollectionFilter::IsEqual(DOM_CollectionFilter *other)
{
	return FALSE; // No sharing (via collection cache) of label collections.
}
