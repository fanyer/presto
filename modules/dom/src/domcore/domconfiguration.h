/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMCONFIGURATION
#define DOM_DOMCONFIGURATION

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domcore/domstringlist.h"
#include "modules/util/simset.h"

class DOM_DOMConfiguration
	: public DOM_Object,
	  public DOM_DOMStringList::Generator
{
public:
	static OP_STATUS Make(DOM_DOMConfiguration *&configuration, DOM_EnvironmentImpl *environment);

	/* Should return DOMEXCEPTION_NO_ERR, NOT_SUPPORTED_ERR or TYPE_MISMATCH_ERR. */
#define DOM_DECLARE_CHECKPARAMETERVALUE_FUNCTION(name) \
	static DOM_Object::DOMException (name)(const char *name, ES_Value *value)
	typedef DOM_Object::DOMException (CheckParameterValue)(const char *name, ES_Value *value);

	~DOM_DOMConfiguration();

	OP_STATUS AddParameter(const char *name, ES_Value *value, CheckParameterValue *check);
	OP_STATUS GetParameter(const uni_char *name, ES_Value *value);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_DOMCONFIGURATION || DOM_Object::IsA(type); }
	virtual void GCTrace();

	/* From DOM_StringList::Generator. */
	virtual unsigned StringList_length();
	virtual OP_STATUS StringList_item(int index, const uni_char *&name);
	virtual BOOL StringList_contains(const uni_char *string);

	DOM_DECLARE_FUNCTION_WITH_DATA(accessParameter); // getParameter, setParameter and canSetParameter
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 4 };

	DOM_DECLARE_CHECKPARAMETERVALUE_FUNCTION(acceptTrue);
	DOM_DECLARE_CHECKPARAMETERVALUE_FUNCTION(acceptFalse);
	DOM_DECLARE_CHECKPARAMETERVALUE_FUNCTION(acceptBoolean);
	DOM_DECLARE_CHECKPARAMETERVALUE_FUNCTION(acceptNull);
	DOM_DECLARE_CHECKPARAMETERVALUE_FUNCTION(acceptObject);
	DOM_DECLARE_CHECKPARAMETERVALUE_FUNCTION(acceptNothing);

protected:
	Head parameters;
	DOM_Object *parameter_values;
};

class DOM_DOMConfiguration_StaticParameter
{
public:
	enum
	{
		PARAMETER_canonical_form,
		PARAMETER_cdata_sections,
		PARAMETER_check_character_normalization,
		PARAMETER_comments,
		PARAMETER_datatype_normalization,
		PARAMETER_entities,
		PARAMETER_error_handler,
		PARAMETER_infoset,
		PARAMETER_namespaces,
		PARAMETER_namespace_declarations,
		PARAMETER_normalize_characters,
		PARAMETER_schema_location,
		PARAMETER_schema_type,
		PARAMETER_split_cdata_sections,
		PARAMETER_validate,
		PARAMETER_validate_if_schema,
		PARAMETER_well_formed,
		PARAMETER_element_content_whitespace,

		PARAMETERS_COUNT
	};

	const char *name, *initial;
	DOM_DOMConfiguration::CheckParameterValue *check;
};

#endif // DOM_DOMCONFIGURATION
