/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#if !defined DOMUNITEDEVICEMANAGER_H && defined DOM_UNITEDEVMANAGER_SUPPORT
#define DOMUNITEDEVICEMANAGER_H

#include "modules/dom/src/domobj.h"
#include "modules/opera_auth/opera_account_manager.h"

class DOM_UniteDeviceManager
	: public DOM_Object
	, public CoreOperaAccountManager::Listener
{
public:
	static void MakeL(DOM_UniteDeviceManager *&new_obj, DOM_Runtime *origining_runtime);
	virtual ~DOM_UniteDeviceManager();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_UNITEDEVICEMANAGER || DOM_Object::IsA(type); }
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();

	// Functions
	DOM_DECLARE_FUNCTION(setRange);
	DOM_DECLARE_FUNCTION(resetProperty);
	enum {
		FUNCTIONS_setRange = 1,
		FUNCTIONS_resetProperty,
		FUNCTIONS_ARRAY_SIZE
	};

	// CoreOperaAccountManager::Listener callback
	virtual void OnAccountDeviceRegisteredChange();

private:
	DOM_UniteDeviceManager();
	OP_STATUS ConstructL();

	ES_Object* m_onrangechange;
};

#endif // !DOMUNITEDEVICEMANAGER_H && DOM_UNITEDEVMANAGER_SUPPORT
