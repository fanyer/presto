/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_MEDIAERROR
#define DOM_MEDIAERROR

#ifdef MEDIA_HTML_SUPPORT

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"
#include "modules/media/mediaelement.h"

class DOM_MediaError : public DOM_Object
{
public:
	static OP_STATUS Make(DOM_MediaError*& error, DOM_Environment* environment, MediaError code);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_MEDIAERROR || DOM_Object::IsA(type); }

	static void ConstructMediaErrorObjectL(ES_Object *object, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

private:
	MediaError m_code;
};

#endif // MEDIA_HTML_SUPPORT
#endif // DOM_MEDIAERROR
