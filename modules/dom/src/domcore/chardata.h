/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_CHARDATA_H
#define DOM_CHARDATA_H

#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/dommutationevent.h"

class DOM_CharacterData
  : public DOM_Node
{
protected:
	DOM_CharacterData(DOM_NodeType type);

	static OP_STATUS Make(DOM_CharacterData *&text, DOM_Node *reference, const uni_char *contents, BOOL comment, BOOL cdata_section);

	HTML_Element *this_element;

public:
	virtual ~DOM_CharacterData();

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value_object, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value_object, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CHARACTERDATA || DOM_Node::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);

	OP_STATUS GetValue(TempBuffer *value);
	const uni_char* GetValueString(TempBuffer *buffer);

	OP_STATUS SetValue(const uni_char *value, DOM_Runtime *origining_runtime, DOM_MutationListener::ValueModificationType type = DOM_MutationListener::REPLACING_ALL, unsigned offset = 0, unsigned length1 = 0, unsigned length2 = 0);

	HTML_Element *GetThisElement() { return this_element; }
	void SetThisElement(HTML_Element *element) { this_element = element; }
	void ClearThisElement() { this_element = NULL; }

	DOM_DECLARE_FUNCTION(substringData);
	DOM_DECLARE_FUNCTION(appendData);
	DOM_DECLARE_FUNCTION(insertData);
	DOM_DECLARE_FUNCTION(deleteData);
	DOM_DECLARE_FUNCTION(replaceData);
	enum { FUNCTIONS_ARRAY_SIZE = 6 };
};

#endif
