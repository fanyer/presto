/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_CDATASECTION_H
#define DOM_CDATASECTION_H

#include "modules/dom/src/domcore/text.h"

class DOM_EnvironmentImpl;

class DOM_CDATASection
  : public DOM_Text
{
private:
	friend class DOM_EnvironmentImpl;

	DOM_CDATASection();

public:
	static OP_STATUS Make(DOM_CDATASection *&cdata_section, DOM_Node *reference, const uni_char *contents);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_CDATASECTION || DOM_Text::IsA(type); }
};

#endif // DOM_CDATASECTION_H
