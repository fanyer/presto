/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domstylesheets/stylesheet.h"
#include "modules/dom/src/domstylesheets/medialist.h"
#include "modules/dom/src/domcore/element.h"

#include "modules/logdoc/htm_elm.h"

DOM_StyleSheet::DOM_StyleSheet(DOM_Node *owner_node)
	: owner_node(owner_node)
{
}

/* virtual */ ES_GetState
DOM_StyleSheet::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	HTML_Element *element = ((DOM_Element *) owner_node)->GetThisElement();
	BOOL is_imported = (element->GetInserted() == HE_INSERTED_BY_CSS_IMPORT);

	switch (property_name)
	{
	case OP_ATOM_href:
		if (element->Type() == HE_STYLE)
		{
			DOMSetNull(value);
			return GET_SUCCESS;
		}
		/* fall through */

	case OP_ATOM_disabled:
	case OP_ATOM_title:
	case OP_ATOM_type:
		if (value)
			DOMSetString(value, element->DOMGetAttribute(GetEnvironment(), DOM_AtomToHtmlAttribute(property_name), NULL, NS_IDX_DEFAULT, FALSE, TRUE));
		return GET_SUCCESS;

	case OP_ATOM_media:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_media);
			if (result != GET_FAILED)
				return result;

			DOM_MediaList *medialist;

			GET_FAILED_IF_ERROR(DOM_MediaList::Make(medialist, this));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_media, *medialist));

			DOMSetObject(value, medialist);
		}
		return GET_SUCCESS;

	case OP_ATOM_ownerNode:
		if (!is_imported)
			DOMSetObject(value, owner_node);
		else
			DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_parentStyleSheet:
		if (value)
			if (is_imported)
			{
				DOM_Node *node;
				GET_FAILED_IF_ERROR(GetEnvironment()->ConstructNode(node, element->Parent(), owner_node->GetOwnerDocument()));
				return node->GetName(OP_ATOM_sheet, value, origining_runtime);
			}
			else
				DOMSetNull(value);
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_StyleSheet::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_disabled)
		return owner_node->PutName(property_name, value, origining_runtime);
	else if (DOM_StyleSheet::GetName(property_name, value, origining_runtime) == GET_SUCCESS)
		return PUT_READ_ONLY;
	else
		return PUT_FAILED;
}

/* virtual */ void
DOM_StyleSheet::GCTrace()
{
	GCMark(owner_node);
}

