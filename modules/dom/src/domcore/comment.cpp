/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcore/comment.h"

DOM_Comment::DOM_Comment()
	: DOM_CharacterData(COMMENT_NODE)
{
}

/* static */ OP_STATUS
DOM_Comment::Make(DOM_Comment *&comment, DOM_Node *reference, const uni_char *contents)
{
	DOM_CharacterData *characterdata;

	RETURN_IF_ERROR(DOM_CharacterData::Make(characterdata, reference, contents, TRUE, FALSE));
	OP_ASSERT(characterdata->IsA(DOM_TYPE_COMMENT));

	comment = (DOM_Comment *) characterdata;
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_Comment::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    if (property_name == OP_ATOM_nodeName)
    {
        DOMSetString(value, UNI_L("#comment"));
        return GET_SUCCESS;
    }
    else
        return DOM_CharacterData::GetName(property_name, value, origining_runtime);
}

