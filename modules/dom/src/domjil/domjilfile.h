/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILFILE_H
#define DOM_DOMJILFILE_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"

class DOM_JILFile : public DOM_JILObject
{
public:

	virtual ~DOM_JILFile() {}

	/**
	 * Makes new JIL File object from file specified by path
	 * @param path - file path  from which data will be imported to JIL File. The path should be passed
	 *   in proper JIL format, i.e. with slashes as separators regardless of the underlying platform's separator,
	 *   within one of the filesystems returned by GetFileSystemRoots().
	 *   If NULL then output object will be filled with undefined values.
	 * @param new_file_obj - placeholder for the newly created JIL File
	 * @param runtime - runtime to which newly constructed object will be added
	 * @return
	 *  - OpStatus::OK - success
	 *  - OpStatus::ERR_NO_MEMORY
	 *  - OpStatus::ERR_NOT_SUPPORTED - if not all file properties are supported on the system
	 *  - OpStatus::ERR_FILE_NOT_FOUND - if the file path is incorrect
	 *  - OpStatus::ERR_NO_ACCESS - if access to the file is denied
	 */
	static OP_STATUS Make(DOM_JILFile*& new_file_obj, DOM_Runtime* runtime, const uni_char* path = NULL);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_FILE || DOM_Object::IsA(type); }
	virtual void GCTrace();

	time_t GetCreationDate()
	{
		ES_Value val;
		OP_STATUS err = GetRuntime()->GetNativeValueOf(m_create_date, &val);
		return static_cast<time_t>((OpStatus::IsSuccess(err) && val.type == VALUE_NUMBER) ? val.value.number / 1000 : 0);
	}

	time_t GetLastModifyDate()
	{
		ES_Value val;
		OP_STATUS err = GetRuntime()->GetNativeValueOf(m_last_modify_date, &val);
		return  static_cast<time_t>((OpStatus::IsSuccess(err) && val.type == VALUE_NUMBER) ? val.value.number / 1000 : 0);
	}

	const uni_char* GetFileName() { return m_file_name.CStr(); }
	const uni_char* GetFilePath() { return m_file_path.CStr(); }
	OpFileLength GetFileLength() { return m_file_size; }
	/** Checks if the object is representing file or directory.
	 *  MAYBE returned by this function means it's not imoprtant whether it's directory or file.
	 *  This is used for setting search criteria.
	 */
	BOOL3 IsDirectory() { return m_is_directory; }

protected:
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	DOM_JILFile() : m_create_date(NULL), m_last_modify_date(NULL), m_file_size(0), m_is_directory(MAYBE) {}
	ES_Object* m_create_date;
	ES_Object* m_last_modify_date;
	OpString m_file_name;
	OpString m_file_path;
	OpFileLength m_file_size;
	BOOL3 m_is_directory;
private:
	OP_STATUS GetDateFromDateObject(ES_Value& date_object, time_t& date);
};

class DOM_JILFile_Constructor : public DOM_BuiltInConstructor
{
public:
	DOM_JILFile_Constructor() : DOM_BuiltInConstructor(DOM_Runtime::JIL_FILE_PROTOTYPE) { }

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILFILE_H
