/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcore/cdatasection.h"

DOM_CDATASection::DOM_CDATASection()
	: DOM_Text(CDATA_SECTION_NODE)
{
}

/* static */ OP_STATUS
DOM_CDATASection::Make(DOM_CDATASection *&cdata_section, DOM_Node *reference, const uni_char *contents)
{
	DOM_CharacterData *characterdata;

	RETURN_IF_ERROR(DOM_CharacterData::Make(characterdata, reference, contents, FALSE, TRUE));
	OP_ASSERT(characterdata->IsA(DOM_TYPE_CDATASECTION));

	cdata_section = (DOM_CDATASection *) characterdata;
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_CDATASection::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    if (property_name == OP_ATOM_nodeName)
    {
        DOMSetString(value, UNI_L("#cdata-section"));
        return GET_SUCCESS;
    }
    else
        return DOM_Text::GetName(property_name, value, origining_runtime);
}

