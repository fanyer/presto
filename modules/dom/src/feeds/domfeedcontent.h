/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_FEEDCONTENT_H
#define DOM_FEEDCONTENT_H

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/dom/src/domobj.h"

class OpFeedContent;
class DOM_Environment;

class DOM_FeedContent
	: public DOM_Object
{
private:
	OpFeedContent*	m_content; ///< The real feedcontent object from the WebFeed backend

	DOM_FeedContent() : m_content(NULL) {}

public:
	static OP_STATUS Make(DOM_FeedContent *&out_content, OpFeedContent *content, DOM_Runtime *runtime);

	virtual ~DOM_FeedContent();

	void	OnDeleted();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_FEEDCONTENT || DOM_Object::IsA(type); }
};

#endif // WEBFEEDS_BACKEND_SUPPORT
#endif // DOM_FEEDCONTENT_H
