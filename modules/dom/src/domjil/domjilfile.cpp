/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domjil/domjilfile.h"
#include "modules/device_api/jil/JILFolderLister.h"
#include "modules/util/opfile/opfile.h"
#include "modules/dom/src/domjil/utils/jilutils.h"

/* static */ OP_STATUS
DOM_JILFile::Make(DOM_JILFile*& jil_file, DOM_Runtime* runtime, const uni_char* path /* = NULL */)
{
	jil_file = OP_NEW(DOM_JILFile, ());
	RETURN_OOM_IF_NULL(jil_file);
	RETURN_IF_ERROR(DOMSetObjectRuntime(jil_file, runtime, runtime->GetPrototype(DOM_Runtime::JIL_FILE_PROTOTYPE), "File"));

	OpFile file;
	OpString directory_path;
	OpString jil_path;
	OpString filename;
	OpString dirname;
	OpString op_str_path;
	if (path)
	{
		RETURN_IF_ERROR(op_str_path.Set(path));

		OpString system_path;
		// If this conversion fails it means access to the path was not configured
		OP_STATUS status = g_DAPI_jil_fs_mgr->JILToSystemPath(path, system_path);

		if (OpStatus::IsSuccess(status))
		{
			RETURN_IF_ERROR(file.Construct(system_path));
			OpFileInfo file_info;
			BOOL exists;
			RETURN_IF_ERROR(file.Exists(exists));
			if (!exists)
				return OpStatus::ERR_FILE_NOT_FOUND;

			file_info.flags = OpFileInfo::LENGTH | OpFileInfo::LAST_MODIFIED | OpFileInfo::CREATION_TIME | OpFileInfo::MODE;
			RETURN_IF_ERROR(file.GetFileInfo(&file_info));	// TODO: handle OpStatus::ERR_NOT_SUPPORTED (it may be tricky)

			jil_file->m_is_directory = (file_info.mode == OpFileInfo::DIRECTORY) ? YES : NO;
			ES_Value tmp_val;
			RETURN_IF_ERROR(DOMSetDate(&tmp_val, runtime, file_info.creation_time * 1000.0));
			jil_file->m_create_date = tmp_val.value.object;
			RETURN_IF_ERROR(DOMSetDate(&tmp_val, runtime, file_info.last_modified * 1000.0));
			jil_file->m_last_modify_date = tmp_val.value.object;
			jil_file->m_file_size = file_info.length;

			RETURN_IF_ERROR(file.GetDirectory(directory_path));
			g_DAPI_jil_fs_mgr->SystemToJILPath(directory_path.CStr(), jil_path);
			if (jil_path.IsEmpty())
				JILFolderLister::GetDirName(op_str_path, &jil_path);
			jil_file->m_file_path.Set(jil_path);
			if (!g_DAPI_jil_fs_mgr->IsVirtualRoot(path))
				jil_file->m_file_name.Set(file.GetName());
			else
			{
				RETURN_IF_ERROR(JILFolderLister::GetFileName(op_str_path, &filename));
				jil_file->m_file_name.Set(filename);
			}
			return OpStatus::OK;
		}
		else
		{
			if (g_DAPI_jil_fs_mgr->IsVirtualRoot(path, TRUE))
			{
				jil_file->m_is_directory = YES;
				RETURN_IF_ERROR(JILFolderLister::GetFileName(op_str_path, &filename));
				RETURN_IF_ERROR(JILFolderLister::GetDirName(op_str_path, &dirname, &filename));
				jil_file->m_file_path.Set(dirname);
				jil_file->m_file_name.Set(filename);
				return OpStatus::OK;
			}
		}

		return OpStatus::ERR_FILE_NOT_FOUND;
	}

	return OpStatus::OK;
}

/* virtual */ void
DOM_JILFile::GCTrace()
{
	GCMark(m_create_date);
	GCMark(m_last_modify_date);
}

/* virtual */ ES_GetState
DOM_JILFile::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_createDate:
		DOMSetObject(value, m_create_date);
		return GET_SUCCESS;
	case OP_ATOM_lastModifyDate:
		DOMSetObject(value, m_last_modify_date);
		return GET_SUCCESS;
	case OP_ATOM_fileSize:
		DOMSetNumber(value, static_cast<double>(m_file_size));
		return GET_SUCCESS;
	case OP_ATOM_fileName:
		DOMSetString(value, m_file_name);
		return GET_SUCCESS;
	case OP_ATOM_filePath:
		DOMSetString(value, m_file_path);
		return GET_SUCCESS;
	case OP_ATOM_isDirectory:
		DOMSetBoolean(value, m_is_directory == YES);
		return GET_SUCCESS;
	}
	return DOM_JILObject::InternalGetName(property_atom, value, origining_runtime, restart_value);
}

/* virtual */ ES_PutState
DOM_JILFile::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		JIL_PUT_DATE(OP_ATOM_createDate, m_create_date);
		JIL_PUT_DATE(OP_ATOM_lastModifyDate, m_last_modify_date);
	case OP_ATOM_fileSize:
		switch (value->type)
		{
		case VALUE_NUMBER:
			m_file_size = static_cast<OpFileLength>(value->value.number);
			return PUT_SUCCESS;
		case VALUE_UNDEFINED:
		case VALUE_NULL:
			m_file_size = 0;
			return PUT_SUCCESS;
		default:
			return PUT_NEEDS_NUMBER;
		}

	case OP_ATOM_fileName:
		switch (value->type)
		{
		case VALUE_STRING:
			m_file_name.Set(value->value.string);
			return PUT_SUCCESS;
		case VALUE_UNDEFINED:
		case VALUE_NULL:
			m_file_name.Empty();
			return PUT_SUCCESS;
		default:
			return PUT_NEEDS_STRING;
		}

	case OP_ATOM_filePath:
		switch (value->type)
		{
		case VALUE_STRING:
			m_file_path.Set(value->value.string);
			return PUT_SUCCESS;
		case VALUE_UNDEFINED:
		case VALUE_NULL:
			m_file_path.Empty();
			return PUT_SUCCESS;
		default:
			return PUT_NEEDS_STRING;
		}

	case OP_ATOM_isDirectory:
		switch (value->type)
		{
		case VALUE_BOOLEAN:
			m_is_directory = value->value.boolean ? YES : NO;
			return PUT_SUCCESS;
		case VALUE_UNDEFINED:
		case VALUE_NULL:
			m_is_directory = MAYBE;
			return PUT_SUCCESS;
		default:
			return PUT_NEEDS_BOOLEAN;
		}
	}
	return DOM_JILObject::InternalPutName(property_atom, value, origining_runtime, restart_value);
}

/* static */
OP_STATUS
DOM_JILFile::GetDateFromDateObject(ES_Value& date_object, time_t& date)
{
	date = 0;
	if (date_object.type == VALUE_OBJECT)
	{
		if (op_strcmp(ES_Runtime::GetClass(date_object.value.object), "Date") == 0)
		{
			ES_Value date_val;

			RETURN_IF_ERROR(GetRuntime()->GetNativeValueOf(date_object.value.object, &date_val));

			date = static_cast<time_t>(date_val.value.number / 1000.0);

			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}

/* virtual */ int
DOM_JILFile_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* es_origining_runtime)
{
	DOM_Runtime* origining_runtime = static_cast<DOM_Runtime*>(es_origining_runtime);
	DOM_JILFile* new_file_obj;
	CALL_FAILED_IF_ERROR(DOM_JILFile::Make(new_file_obj, origining_runtime, NULL));
	DOMSetObject(return_value, new_file_obj);
	return ES_VALUE;
}
#endif // DOM_JIL_API_SUPPORT
