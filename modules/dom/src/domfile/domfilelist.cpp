/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domfile/domfilelist.h"
#include "modules/dom/src/domfile/domfile.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formmanager.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/formats/argsplit.h"

/* static */ OP_BOOLEAN
DOM_FileList::Construct(DOM_FileList *&filelist, HTML_Element *file_input_element, OpString &file_upload_values, ES_Value &previous_file_upload_values, ES_Value &previous_filelist, DOM_Runtime *origining_runtime)
{
	OP_ASSERT(file_input_element);
	OP_ASSERT(file_input_element->IsMatchingType(HE_INPUT, NS_HTML));
	OP_ASSERT(file_input_element->GetInputType() == INPUT_FILE);

	FormValue* form_value = file_input_element->GetFormValue();
	RETURN_IF_ERROR(form_value->GetValueAsText(file_input_element, file_upload_values));

	const uni_char *file_upload_string = file_upload_values.CStr();
	if (file_upload_string && previous_file_upload_values.type == VALUE_STRING && uni_str_eq(previous_file_upload_values.value.string, file_upload_string))
	{
		OP_ASSERT(previous_filelist.type == VALUE_OBJECT);
		filelist = DOM_VALUE2OBJECT(previous_filelist, DOM_FileList);
		return OpBoolean::IS_FALSE;
	}
	else
	{
		RETURN_IF_ERROR(DOM_FileList::Make(filelist, file_upload_string, origining_runtime));
		DOMSetObject(&previous_filelist, filelist);
		DOMSetString(&previous_file_upload_values, file_upload_string);
		return OpBoolean::IS_TRUE;
	}
}

/* static */ OP_STATUS
DOM_FileList::Make(DOM_FileList *&filelist, const uni_char *file_upload_string, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(filelist = OP_NEW(DOM_FileList, ()), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::FILELIST_PROTOTYPE), "FileList"));

	return GetFiles(filelist->m_files, file_upload_string, origining_runtime);
}

/* static */ OP_STATUS
DOM_FileList::GetFiles(OpVector<DOM_File> &files, const uni_char *file_upload_string, DOM_Runtime *runtime)
{
	// Split into individual file names
	UniParameterList file_name_list;
	FormManager::ConfigureForFileSplit(file_name_list, file_upload_string);

	for (UniParameters* file_name_obj = file_name_list.First(); file_name_obj; file_name_obj = file_name_obj->Suc())
	{
		const uni_char* file_name = file_name_obj->Name();
		if (!file_name || !*file_name)
			continue; // next

		DOM_File* file;
		OP_STATUS status = DOM_File::Make(file, file_name, FALSE, FALSE, runtime);
		OP_ASSERT(status == OpStatus::OK || status == OpStatus::ERR_NO_MEMORY || !"Only two return codes that are supported");
		RETURN_IF_ERROR(status);
		RETURN_IF_ERROR(files.Add(file));
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_FileList::Make(DOM_FileList *&filelist, DOM_Runtime *origining_runtime)
{
	return DOMSetObjectRuntime(filelist = OP_NEW(DOM_FileList, ()), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::FILELIST_PROTOTYPE), "FileList");
}

OP_STATUS
DOM_FileList::Add(DOM_File *file)
{
	return m_files.Add(file);
}

void
DOM_FileList::Clear()
{
	m_files.Clear();
}

unsigned
DOM_FileList::GetFileListCount()
{
	return m_files.GetCount();
}

/* virtual */ ES_GetState
DOM_FileList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, GetFileListCount());
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/*virtual */ ES_GetState
DOM_FileList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index < 0 || static_cast<unsigned>(property_index) >= (m_files.GetCount()))
		return GET_FAILED;

	DOMSetObject(value, m_files.Get(property_index));
	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_FileList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/*virtual */ ES_PutState
DOM_FileList::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	// The "array" part of this object is completely readonly.
	return PUT_READ_ONLY;
}

/* virtual */ void
DOM_FileList::GCTrace()
{
	for (unsigned i = 0; i < m_files.GetCount(); i++)
		GCMark(m_files.Get(i));
}

/* virtual */ ES_GetState
DOM_FileList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = static_cast<int>(GetFileListCount());
	return GET_SUCCESS;
}

/* static */ int
DOM_FileList::item(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(filelist, DOM_TYPE_FILELIST, DOM_FileList);
	DOM_CHECK_ARGUMENTS("N");

	int index = TruncateDoubleToInt(argv[0].value.number);
	ES_GetState result = filelist->GetIndex(index, return_value, origining_runtime);
	if (result == GET_FAILED)
	{
		DOMSetNull(return_value);
		result = GET_SUCCESS;
	}
	return ConvertGetNameToCall(result, return_value);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_FileList)
	DOM_FUNCTIONS_FUNCTION(DOM_FileList, DOM_FileList::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_FileList)
