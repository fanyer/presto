/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_LOAD

#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domload/lsthreads.h"
#include "modules/ecmascript_utils/essched.h"

DOM_LSParser_InsertThread::DOM_LSParser_InsertThread(DOM_Node *parent, DOM_Node *before, DOM_Node *node, BOOL insertChildren)
	: ES_Thread(NULL),
	  parent(parent),
	  before(before),
	  node(node),
	  restarted(FALSE),
	  insertChildren(insertChildren)
{
}

/* virtual */ OP_STATUS
DOM_LSParser_InsertThread::EvaluateThread()
{
	while (!IsBlocked())
	{
		int result;

		if (!restarted)
		{
			DOM_Node *child;

			if (insertChildren)
			{
				RETURN_IF_ERROR(node->GetFirstChild(child));

				if (!child)
				{
					is_completed = TRUE;
					return OpStatus::OK;
				}
			}
			else
				child = node;

			ES_Value arguments[2];
			DOM_Object::DOMSetObject(&arguments[0], child);
			DOM_Object::DOMSetObject(&arguments[1], before);

			result = DOM_Node::insertBefore(parent, arguments, 2, &return_value, (DOM_Runtime *) scheduler->GetRuntime());
		}
		else
			result = DOM_Node::insertBefore(NULL, NULL, -1, &return_value, (DOM_Runtime *) scheduler->GetRuntime());

		if (result == ES_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;
		else if (result == (ES_SUSPEND | ES_RESTART))
			restarted = TRUE;
		else if (result != ES_VALUE)
		{
			is_completed = is_failed = TRUE;
			break;
		}
		else if (!insertChildren)
		{
			is_completed = TRUE;
			break;
		}
	}

	return OpStatus::OK;
}

#ifdef DOM2_MUTATION_EVENTS

DOM_LSParser_RemoveThread::DOM_LSParser_RemoveThread(DOM_Node *node, BOOL removeChildren)
	: ES_Thread(NULL),
	  node(node),
	  removeChildren(removeChildren),
	  restarted(FALSE)
{
}

/* virtual */ OP_STATUS
DOM_LSParser_RemoveThread::EvaluateThread()
{
	while (!IsBlocked())
	{
		int result;

		if (!restarted)
		{
			DOM_Node *parent, *child;

			if (removeChildren)
			{
				parent = node;
				RETURN_IF_ERROR(parent->GetFirstChild(child));

				if (!child)
				{
					is_completed = TRUE;
					return OpStatus::OK;
				}
			}
			else
			{
				RETURN_IF_ERROR(node->GetParentNode(parent));
				child = node;
			}

			ES_Value arguments[1];
			DOM_Object::DOMSetObject(&arguments[0], child);

			result = DOM_Node::removeChild(parent, arguments, 1, &return_value, (DOM_Runtime *) scheduler->GetRuntime());
		}
		else
		{
			result = DOM_Node::removeChild(NULL, NULL, -1, &return_value, (DOM_Runtime *) scheduler->GetRuntime());
			restarted = FALSE;
		}

		if (result == ES_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;
		else if (result == (ES_SUSPEND | ES_RESTART))
			restarted = TRUE;
		else if (result != ES_VALUE)
		{
			is_completed = is_failed = TRUE;
			return OpStatus::OK;
		}
		else if (!removeChildren)
		{
			is_completed = TRUE;
			return OpStatus::OK;
		}
	}

	return OpStatus::OK;
}

#endif // DOM2_MUTATION_EVENTS
#endif // DOM3_LOAD
