 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DOMSQLRESULT_H
#define DOMSQLRESULT_H

#ifdef DATABASE_STORAGE_SUPPORT

#include "modules/dom/src/domobj.h"

class SqlResultSet;
class DOM_SQLResultSet;
class DOM_SQLResultSetRow;
class DOM_SQLResultSetRowList;

class DOM_SQLResultSet : public DOM_BuiltInConstructor
{
private:
	SqlResultSet *m_sql_result_set;
	ES_ValueString m_value_string; //<* used to return values back to ES
	friend class DOM_SQLResultSetRow;
	friend class DOM_SQLResultSetRowList;

public:
	DOM_SQLResultSet(SqlResultSet *sql_result_set);
	virtual ~DOM_SQLResultSet();

	static OP_STATUS	Make(DOM_SQLResultSet *&result_set, SqlResultSet *sql_result_set, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL		IsA(int type) { return type == DOM_TYPE_SQL_RESULTSET || DOM_Object::IsA(type); }

	void ClearResultSet() { m_sql_result_set = NULL; }
};

class DOM_SQLResultSetRowList : public DOM_BuiltInConstructor
{
private:
	DOM_SQLResultSet     *m_parent;
	DOM_SQLResultSetRow **m_all_rows;

	friend class DOM_SQLResultSet; //for DOM_SQLResultSet to be able to create instances
	friend class DOM_SQLResultSetRow; //for the row to access m_parent

	DOM_SQLResultSetRowList(DOM_SQLResultSet *parent)
		: DOM_BuiltInConstructor(DOM_Runtime::SQLRESULTSETROWLIST_PROTOTYPE)
		, m_parent(parent)
		, m_all_rows(NULL){}

	virtual ~DOM_SQLResultSetRowList();

	OP_STATUS GetItem(unsigned row_index, ES_Value *value);
	/**< Returns row at given index

	   @param row_index   row index starting at 0
	   @param value       argument which will hold the row

	   @returns ERR_NO_MEMORY if OOM
	            ERR_OUT_OF_RANGE if row_index is out of bounds
	            OK if a row was found
	 */

	unsigned GetLength();
	/**< Returns number of rows **/

public:
	static OP_STATUS	Make(DOM_SQLResultSetRowList *&row_list, DOM_SQLResultSet *parent, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL		IsA(int type) { return type == DOM_TYPE_SQL_RESULTSETROWLIST || DOM_Object::IsA(type); }

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(item);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_SQLResultSetRow : public DOM_Object
{
private:
	friend class DOM_SQLResultSetRowList; //for DOM_SQLResultSetRowList to be able to create instances
	DOM_SQLResultSetRowList* m_parent;
	unsigned m_row_index;

	DOM_SQLResultSetRow(DOM_SQLResultSetRowList* parent, unsigned row_index) : m_parent(parent), m_row_index(row_index){OP_ASSERT(parent);}

public:
	static OP_STATUS	Make(DOM_SQLResultSetRow *&row_obj, DOM_SQLResultSetRowList* parent, unsigned row_index, DOM_Runtime *runtime);

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	virtual void GCTrace();
};

class DOM_SQLError : public DOM_BuiltInConstructor
{
public:
	enum ErrorCode
	{
		// http://dev.w3.org/html5/webstorage/#errors-and-exceptions
		UNKNOWN_ERR = 0,
		DATABASE_ERR = 1,
		VERSION_ERR = 2,
		TOO_LARGE_ERR = 3,
		QUOTA_ERR = 4,
		SQL_SYNTAX_ERR = 5,
		CONSTRAINT_ERR = 6,
		TIMEOUT_ERR = 7,
		LAST_SQL_ERROR_CODE
	};

	virtual ~DOM_SQLError();

	static OP_STATUS	Make(DOM_SQLError *&error, DOM_SQLError::ErrorCode code, const uni_char *custom_message, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL		IsA(int type) { return type == DOM_TYPE_SQL_ERROR || DOM_Object::IsA(type); }

	static void ConstructSQLErrorObjectL(ES_Object *object, DOM_Runtime *runtime);
	static const uni_char* GetErrorDescription(ErrorCode code);
private:
	ErrorCode m_code;
	const uni_char *m_custom_message;

	DOM_SQLError(ErrorCode code);
};

#endif // DATABASE_STORAGE_SUPPORT
#endif // DOMSQLRESULT_H
