/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#ifndef DOM_DOMJILATTACHMENT_H
#define DOM_DOMJILATTACHMENT_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpMessaging.h"
#include "modules/dom/src/domjil/utils/jilutils.h"

class FindMessagesCallbackAsyncImpl;

class DOM_JILAttachment : public DOM_JILObject
{
	friend class FindMessagesCallbackAsyncImpl;
public:
	static OP_STATUS Make(DOM_JILAttachment*& new_obj, DOM_Runtime* origining_runtime, const OpMessaging::Attachment* attachment_contents=NULL, const uni_char* full_filename=NULL);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_ATTACHMENT || DOM_JILObject::IsA(type); }
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

	OP_STATUS SetFile(const uni_char* full_filename);
	OP_STATUS SetAttachment(DOM_Runtime* origining_runtime, const OpMessaging::Attachment& attachment_contents);
	OP_STATUS GetCopy(OpMessaging::Attachment*& new_attachment);
	BOOL operator==(const DOM_JILAttachment& other);
	virtual ~DOM_JILAttachment() {}
private:
	DOM_JILAttachment();

	struct Null_or_undef_st
	{
		unsigned int fileName:2;
		unsigned int MIMEType:2;
		unsigned int size:2;
	} m_undefnull;

	OpString m_full_filename;
	OpString m_id;
	BOOL m_inside_message;

	OpString m_MIMEType;
	OpFileLength m_size;
	OpString m_fileName;
};

class DOM_JILAttachment_Constructor : public DOM_BuiltInConstructor
{
public:
	DOM_JILAttachment_Constructor() : DOM_BuiltInConstructor(DOM_Runtime::JIL_ATTACHMENT_PROTOTYPE) {}
	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

class DOM_JILAttachment_Array : public DOM_Object
{
public:
	static OP_STATUS Make(DOM_JILAttachment_Array*& new_obj, DOM_Runtime* origining_runtime, const OpAutoVector<OpMessaging::Attachment>* to_add=NULL);
	static OP_STATUS Make(DOM_JILAttachment_Array*& new_obj, DOM_Runtime* origining_runtime, ES_Object* from_object, int length);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_ATTACHMENT_ARRAY || DOM_Object::IsA(type); }
	virtual ES_GetState GetName(OpAtom property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetIndex(int index, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
	int GetLength() { return m_length; }
	BOOL GetIndex(UINT32 index, ES_Value*& result);
	OP_STATUS GetCopy(DOM_JILAttachment_Array*& to_array, DOM_Runtime* origining_runtime);

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	virtual void GCTrace();

	void Empty();
	OP_STATUS Remove(const DOM_JILAttachment& to_remove);
	virtual ~DOM_JILAttachment_Array() {}
private:
	DOM_JILAttachment_Array();
	int FindIndex(int to_find);

	int m_length;
	OpAutoVector<DOM_JIL_Array_Element> m_array;
};

#endif // DOM_JIL_API_SUPPORT
#endif // DOM_DOMJILATTACHMENT_H
