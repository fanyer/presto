/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_DOMTOKENLIST
#define DOM_DOMTOKENLIST

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domcore/element.h"

class DOM_DOMTokenList
	: public DOM_Object
{
public:
	class Data
	{
	public:
		virtual int length() = 0;

		/**
		 * @return OpStatus::ERR when index is out of range (which includes
		 * searching in empty list), otherwise OpStatus::OK.
		 */
		virtual OP_STATUS item(int index, TempBuffer *contents) = 0;

		/**
		 * @param[in] string must be non-null.
		 */
		virtual BOOL contains(const uni_char *string) = 0;

		/**
		 * @param[in] string must be non-null.
		 */
		virtual OP_STATUS add(const uni_char *string) = 0;

		/**
		 * @param[in] string must be non-null.
		 */
		virtual OP_STATUS remove(const uni_char *string, TempBuffer* buffer_out) = 0;

		virtual OP_STATUS toString(const uni_char *&contents) = 0;

		virtual ~Data() {}
	protected:
	};

	class DataClassAttribute
		: public Data
	{
	public:
		DataClassAttribute(DOM_Element* attribute_owner)
			: m_attribute_owner(attribute_owner)
			, m_is_in_strict_mode(MAYBE)
		{
		}
		virtual int length();
		virtual OP_STATUS item(int index, TempBuffer *contents);
		virtual BOOL contains(const uni_char *string);
		virtual OP_STATUS add(const uni_char *string);
		virtual OP_STATUS remove(const uni_char *string, TempBuffer* buffer_out);
		virtual OP_STATUS toString(const uni_char *&contents);
		BOOL IsInStrictMode();
	private:
		DOM_Element* m_attribute_owner;
		BOOL3 m_is_in_strict_mode;
	};

	static OP_STATUS Make(DOM_DOMTokenList *&tokenlist, DOM_Element *elm);
	virtual ~DOM_DOMTokenList();
	virtual void GCTrace() { DOM_Object::GCMark(m_element); }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *return_value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DOMTOKENLIST || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(item);
	DOM_DECLARE_FUNCTION(toString);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };

	DOM_DECLARE_FUNCTION_WITH_DATA(access); // contains, add, remove, toggle.
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 5 };
protected:
	DOM_DOMTokenList(DOM_Element* elm);

	DOM_Element* m_element;
	Data* m_data;
};

class DOM_DOMSettableTokenList
	: public DOM_DOMTokenList
{
public:
	class Data : public DOM_DOMTokenList::Data
	{
	public:
		virtual const uni_char* GetValue() = 0;
		virtual OP_STATUS SetValue(const uni_char* str) = 0;
	};
	class DataStringAttribute
		: public Data
	{
	public:
		DataStringAttribute(DOM_Element* attribute_owner, Markup::AttrType attribute)
			: m_attribute_owner(attribute_owner)
			, m_attribute(attribute)
		{
		}
		virtual int length();
		virtual OP_STATUS item(int index, TempBuffer *contents);
		virtual BOOL contains(const uni_char *string);
		virtual OP_STATUS add(const uni_char *string);
		virtual OP_STATUS remove(const uni_char *string, TempBuffer* buffer_out);
		virtual OP_STATUS toString(const uni_char *&contents);
		virtual const uni_char* GetValue();
		virtual OP_STATUS SetValue(const uni_char* str);
		virtual ~DataStringAttribute() {}
	private:
		DOM_Element* m_attribute_owner;
		Markup::AttrType m_attribute;
	};

	static OP_STATUS Make(DOM_DOMSettableTokenList *&tokenlist, DOM_Element* element, Markup::AttrType attribute);
	~DOM_DOMSettableTokenList();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *return_value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DOMSETTABLETOKENLIST || type == DOM_TYPE_DOMTOKENLIST || DOM_Object::IsA(type); }

protected:
	DOM_DOMSettableTokenList(DOM_Element* element, int attribute)
		: DOM_DOMTokenList(element)
	{
	}
};

#endif // DOM_DOMTOKENLIST

