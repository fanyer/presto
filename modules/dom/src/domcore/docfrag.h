/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOCFRAG_H
#define DOM_DOCFRAG_H

#include "modules/dom/src/domcore/node.h"

class HTML_Element;

class DOM_DocumentFragment
	: public DOM_Node
{
protected:
	DOM_DocumentFragment();

	HTML_Element *placeholder;

public:
	/** Deletes |placeholder| with DOMFreeElement if Make fails. */
	static OP_STATUS Make(DOM_DocumentFragment *&document_fragment, DOM_Node *reference, HTML_Element *placeholder = NULL);

	virtual ~DOM_DocumentFragment();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DOCUMENTFRAGMENT || DOM_Node::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);
	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

	HTML_Element *GetPlaceholderElement() { return placeholder; }
	void SetPlaceholderElement(HTML_Element *new_placeholder);
	void ClearPlaceholderElement() { placeholder = NULL; }

#ifdef DOM_SELECTORS_API
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };
#else // DOM_SELECTORS_API
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 1 };
#endif // DOM_SELECTORS_API
};

#endif // DOM_DOCFRAG_H
