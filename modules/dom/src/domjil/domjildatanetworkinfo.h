/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILDATANETWORKINFO_H
#define DOM_DOMJILDATANETWORKINFO_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/network/OpNetworkInterface.h"

class DOM_JILDataNetworkInfo
	: public DOM_JILObject
	, public OpNetworkInterfaceListener
{
public:
	static OP_STATUS Make(DOM_JILDataNetworkInfo*& data_network_info, DOM_Runtime* runtime);
	virtual ~DOM_JILDataNetworkInfo();
	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_DATANETWORKINFO || DOM_Object::IsA(type); }

	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

	DOM_DECLARE_FUNCTION(getNetworkConnectionName);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };

// from OpNetworkInterfaceListener
	virtual void OnInterfaceAdded(OpNetworkInterface* network_interface);
	virtual void OnInterfaceRemoved(OpNetworkInterface* network_interface);
	virtual void OnInterfaceChanged(OpNetworkInterface* network_interface);

private:
	void CheckHTTPConnection();
	static const uni_char* GetConnectionTypeString(OpNetworkInterface* network_interface);

	static OP_STATUS MakeDataNetworkConnectionTypes(ES_Object*& data_network_connection_types, DOM_Runtime* runtime);

	ES_GetState GetNetworkConnectonType(ES_Value* value, DOM_Object* this_object, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	/// This is our 'best approximation'(TM) of currently used network interface
	OpNetworkInterface* GetActiveNetworkInterface();
	BOOL IsDataNetworkConnected();
	DOM_JILDataNetworkInfo();

	ES_Object* m_on_network_connection_changed;
	OpNetworkInterfaceManager* m_network_interface_manager;
	OpNetworkInterface* m_http_connection;
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILDATANETWORKINFO_H
