/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_LSTHREADS
#define DOM_LSTHREADS

#ifdef DOM3_LOAD

#include "modules/ecmascript_utils/esthread.h"

class DOM_Node;

class DOM_LSParser_InsertThread
	: public ES_Thread
{
protected:
	DOM_Node *parent, *before, *node;
	BOOL restarted, insertChildren;
	ES_Value return_value;

public:
	DOM_LSParser_InsertThread(DOM_Node *parent, DOM_Node *before, DOM_Node *node, BOOL insertChildren);

	virtual OP_STATUS EvaluateThread();
};

#ifdef DOM2_MUTATION_EVENTS

class DOM_LSParser_RemoveThread
	: public ES_Thread
{
protected:
	DOM_Node *node;
	BOOL removeChildren, restarted;
	ES_Value return_value;

public:
	DOM_LSParser_RemoveThread(DOM_Node *node, BOOL removeChildren);

	virtual OP_STATUS EvaluateThread();
};

#endif // DOM2_MUTATION_EVENTS
#endif // DOM3_LOAD
#endif // DOM_LSTHREADS
