/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_PROCINST_H
#define DOM_PROCINST_H

#include "modules/dom/src/domcore/chardata.h"

class DOM_EnvironmentImpl;

class DOM_ProcessingInstruction
	: public DOM_CharacterData
{
protected:
	friend class DOM_EnvironmentImpl;

	DOM_ProcessingInstruction();

public:
	static OP_STATUS Make(DOM_ProcessingInstruction *&procinst, DOM_Node *reference, const uni_char *target, const uni_char *data);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_PROCESSINGINSTRUCTION || DOM_CharacterData::IsA(type); }
};

#endif  // DOM_PROCINST_H
