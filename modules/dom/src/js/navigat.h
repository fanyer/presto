/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _JS11_NAVIGATOR_
#define _JS11_NAVIGATOR_

#include "modules/dom/src/js/js_plugin.h"
#include "modules/dom/src/js/mimetype.h"
#include "modules/dom/src/domobj.h"

#if defined WEB_HANDLERS_SUPPORT
class RegisterHandlerPermissionCallback;
#endif // WEB_HANDLERS_SUPPORT

class JS_Navigator
	: public DOM_Object
{
public:
	static void MakeL(DOM_Object *&object, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_NAVIGATOR || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(javaEnabled);
	DOM_DECLARE_FUNCTION(taintEnabled);
#ifdef DOM_STREAM_API_SUPPORT
	DOM_DECLARE_FUNCTION(getUserMedia);
#endif //DOM_STREAM_API_SUPPORT
	enum {
		FUNCTIONS_BASIC = 2,
#ifdef DOM_STREAM_API_SUPPORT
		FUNCTIONS_getUserMedia,
#endif // DOM_STREAM_API_SUPPORT
		FUNCTIONS_ARRAY_SIZE
	};

#if defined WEB_HANDLERS_SUPPORT
	static void BeforeUnload(DOM_EnvironmentImpl* env);
	DOM_DECLARE_FUNCTION_WITH_DATA(registerHandler);
	DOM_DECLARE_FUNCTION_WITH_DATA(unregisterHandler);
	DOM_DECLARE_FUNCTION_WITH_DATA(isHandlerRegistered);
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 7};
#else
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 1 };
#endif // defined WEB_HANDLERS_SUPPORT

private:
#ifdef DOM_STREAM_API_SUPPORT
	friend class GetUserMediaSecurityCallback;

	enum GetUserMediaErrorCodes {
		GETUSERMEDIA_PERMISSION_DENIED = 1
	};

	/** Creates an object implementing the NavigatorUserMediaError interface. */
	static OP_STATUS CreateUserMediaErrorObject(ES_Object** new_object, int error_code, DOM_Runtime* runtime);
	static OP_STATUS CallUserMediaErrorCallback(ES_Object* error_callback, int error_code, DOM_Runtime* runtime);
#endif // DOM_STREAM_API_SUPPORT
#if defined WEB_HANDLERS_SUPPORT
	List<RegisterHandlerPermissionCallback> register_handler_permission_callbacks;
#endif // WEB_HANDLERS_SUPPORT
};

#ifdef DOM_WEBWORKERS_SUPPORT
class JS_Navigator_Worker
	: public JS_Navigator
{
public:
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};
#endif // DOM_WEBWORKERS_SUPPORT

#endif /* _JS11_NAVIGATOR_ */
