/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_TEXT_H
#define DOM_TEXT_H

#include "modules/dom/src/domcore/chardata.h"

class DOM_EnvironmentImpl;

class DOM_Text
	: public DOM_CharacterData
{
protected:
	friend class DOM_EnvironmentImpl;

	DOM_Text(DOM_NodeType type = TEXT_NODE);

public:
	static OP_STATUS Make(DOM_Text *&text, DOM_Node *reference, const uni_char *contents);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_TEXT || DOM_CharacterData::IsA(type); }

	OP_STATUS IsWhitespace(BOOL &is_whitespace);

	ES_GetState GetWholeText(ES_Value *value);

	DOM_DECLARE_FUNCTION(splitText);
	DOM_DECLARE_FUNCTION(replaceWholeText);

	enum
	{
		FUNCTIONS_BASIC = 1,
		FUNCTIONS_replaceWholeText,
		FUNCTIONS_ARRAY_SIZE
	};
};

#endif // DOM_TEXT_H
