/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef USERJSMAGIC_H
#define USERJSMAGIC_H

#ifdef USER_JAVASCRIPT

#include "modules/dom/src/domobj.h"

class DOM_EnvironmentImpl;

class DOM_UserJSMagicFunction
	: public DOM_Object
{
protected:
	uni_char *name;
	ES_Object *impl, *real;
	DOM_Object *data;
	BOOL busy;
	DOM_UserJSMagicFunction *next;

	DOM_UserJSMagicFunction(ES_Object *impl, DOM_Object *data);

public:
	~DOM_UserJSMagicFunction();

	static OP_STATUS Make(DOM_UserJSMagicFunction *&function, DOM_EnvironmentImpl *environment, const uni_char *name, ES_Object *impl);

	virtual int Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_USERJSMAGICFUNCTION || DOM_Object::IsA(type); }
	virtual void GCTrace();

	void SetNonBusy() { busy = FALSE; }

	const uni_char *GetFunctionName() { return name; }

	DOM_UserJSMagicFunction *GetNext() { return next; }
	void SetNext(DOM_UserJSMagicFunction *new_next) { next = new_next; }

	void SetReal(ES_Object *new_real) { real = new_real; }
	ES_Object *GetReal() { return real; }
};

class DOM_UserJSMagicVariable
	: public DOM_Object
{
protected:
	uni_char *name;
	ES_Object *getter, *setter;
	ES_Value last;
	unsigned busy;
	DOM_UserJSMagicVariable *next;

	DOM_UserJSMagicVariable(ES_Object *getter, ES_Object *setter);

public:
	~DOM_UserJSMagicVariable();

	static OP_STATUS Make(DOM_UserJSMagicVariable *&variable, DOM_EnvironmentImpl *environment, const uni_char *name, ES_Object *getter, ES_Object *setter);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_USERJSMAGICVARIABLE || DOM_Object::IsA(type); }
	virtual void GCTrace();

	enum BusyType
	{
		BUSY_READING = 1,
		BUSY_WRITING = 2
	};

	void SetNonBusy(BOOL reading) { busy &= ~(reading ? BUSY_READING : BUSY_WRITING); }

	const uni_char *GetVariableName() { return name; }

	DOM_UserJSMagicVariable *GetNext() { return next; }
	void SetNext(DOM_UserJSMagicVariable *new_next) { next = new_next; }

	ES_GetState GetValue(ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object = NULL);
	ES_PutState SetValue(ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object = NULL);
};

#endif // USER_JAVASCRIPT
#endif // USERJSMAGIC_H
