/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#include "core/pch.h"

#ifdef DATABASE_STORAGE_SUPPORT

#include "modules/database/opdatabase.h"
#include "modules/dom/src/storage/sqlresult.h"
#include "modules/dom/src/storage/storageutils.h"
#include "modules/dom/src/domglobaldata.h"


static OP_STATUS
SqlValueToESValue(const SqlValue* sql_value, ES_Value *value, ES_ValueString *string_w_len_holder)
{
	OP_ASSERT(sql_value != NULL);
	switch(sql_value->Type())
	{
	case SqlValue::TYPE_STRING:
		DOM_Object::DOMSetStringWithLength(value,string_w_len_holder,sql_value->StringValue(), sql_value->StringValueLength());
		break;
	case SqlValue::TYPE_NULL:
		DOM_Object::DOMSetNull(value);
		break;
	case SqlValue::TYPE_INTEGER:
		DOM_Object::DOMSetNumber(value,static_cast<double>(sql_value->IntegerValue()));
		break;
	case SqlValue::TYPE_DOUBLE:
		DOM_Object::DOMSetNumber(value,sql_value->DoubleValue());
		break;
	case SqlValue::TYPE_BLOB:
		//TODO: add blob support in SqlValue::GetToESValue
		//no biggie since it's not possible to ES to pass a blob in the 1st place to ES
		return OpStatus::ERR;
	default:
		OP_ASSERT(!"Forgot a type on SqlValueToESValue(SqlValue,ES_Value)");
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_SQLResultSet::Make(DOM_SQLResultSet *&result_set, SqlResultSet *sql_result_set, DOM_Runtime *runtime)
{
	OP_ASSERT(!sql_result_set->IsIterable() || sql_result_set->IsCachingEnabled());

	return DOMSetObjectRuntime(result_set = OP_NEW(DOM_SQLResultSet, (sql_result_set)), runtime,
		runtime->GetPrototype(DOM_Runtime::SQLRESULTSET_PROTOTYPE), "SQLResultSet");
}

DOM_SQLResultSet::DOM_SQLResultSet(SqlResultSet *sql_result_set)
	: DOM_BuiltInConstructor(DOM_Runtime::SQLRESULTSET_PROTOTYPE),
	  m_sql_result_set(sql_result_set)
{
}

DOM_SQLResultSet::~DOM_SQLResultSet()
{
	OP_DELETE(m_sql_result_set);
	m_sql_result_set = NULL;
}

/* virtual */ ES_GetState
DOM_SQLResultSet::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_insertId:
		if (value)
		{
			if (m_sql_result_set && m_sql_result_set->GetRowsAffected()!=0)
				DOMSetNumber(value, (double)m_sql_result_set->LastInsertId());
			else
				return DOM_GETNAME_DOMEXCEPTION(INVALID_ACCESS_ERR);
		}
		return GET_SUCCESS;

	case OP_ATOM_rowsAffected:
		if (value)
		{
			DOMSetNumber(value, m_sql_result_set != NULL ? m_sql_result_set->GetRowsAffected() : 0);
		}
		return GET_SUCCESS;

	case OP_ATOM_rows:
		if (value)
		{
			if (DOMSetPrivate(value, DOM_PRIVATE_dbResultRows) == GET_SUCCESS)
				return GET_SUCCESS;

			DOM_SQLResultSetRowList *row_list;
			GET_FAILED_IF_ERROR(DOM_SQLResultSetRowList::Make(row_list, this, this->GetRuntime()));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_dbResultRows, *row_list));
			return DOMSetPrivate(value, DOM_PRIVATE_dbResultRows);
		}
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_SQLResultSet::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_insertId:
	case OP_ATOM_rowsAffected:
	case OP_ATOM_rows:
		return PUT_READ_ONLY;
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

DOM_SQLResultSetRowList::~DOM_SQLResultSetRowList()
{
	OP_DELETEA(m_all_rows);
}

/* static */ OP_STATUS
DOM_SQLResultSetRowList::Make(DOM_SQLResultSetRowList *&row_list, DOM_SQLResultSet* parent, DOM_Runtime *runtime)
{
	return DOMSetObjectRuntime(row_list = OP_NEW(DOM_SQLResultSetRowList, (parent)), runtime,
		runtime->GetPrototype(DOM_Runtime::SQLRESULTSETROWLIST_PROTOTYPE), "SQLResultSetRowList");
}

/* virtual */ ES_GetState
DOM_SQLResultSetRowList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, GetLength());
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_SQLResultSetRowList::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

unsigned
DOM_SQLResultSetRowList::GetLength()
{
	OP_ASSERT(m_parent);
	OP_ASSERT(m_parent->m_sql_result_set);
	OP_ASSERT(!m_parent->m_sql_result_set->IsIterable() || m_parent->m_sql_result_set->IsCachingEnabled());

	if (m_parent->m_sql_result_set->IsIterable())
	{
		/** GetLength cannot fail given that the result set has caching enabled
		    and therefore all rows have been retrieved, and the length is available
		    without any extra effort. */
		unsigned value;
#ifdef DEBUG_ENABLE_OPASSERT
		OP_STATUS status =
#endif
		m_parent->m_sql_result_set->GetCachedLength(&value);
		OP_ASSERT(OpStatus::IsSuccess(status));
		return value;
	}
	else
		return 0;
}

OP_STATUS
DOM_SQLResultSetRowList::GetItem(unsigned row_index, ES_Value *value)
{
	unsigned length = GetLength();
	if (length <= row_index)
		return OpStatus::ERR_OUT_OF_RANGE;

	if (m_all_rows == NULL)
	{
		RETURN_OOM_IF_NULL(m_all_rows = OP_NEWA(DOM_SQLResultSetRow *, length));
		op_memset(m_all_rows, 0, sizeof(DOM_SQLResultSetRow *) * length);
	}

	DOM_SQLResultSetRow *row_obj = m_all_rows[row_index];
	if (row_obj == NULL)
	{
		RETURN_IF_ERROR(DOMSetObjectRuntime(row_obj = OP_NEW(DOM_SQLResultSetRow, (this, row_index)), GetRuntime()));
		m_all_rows[row_index] = row_obj;
	}

	DOMSetObject(value, row_obj);

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_SQLResultSetRowList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index >= 0)
	{
		OP_STATUS status = GetItem(static_cast<unsigned>(property_index), value);

		OP_ASSERT(status != OpStatus::ERR_OUT_OF_RANGE || property_index >= (int)GetLength());

		if (status == OpStatus::ERR_NO_MEMORY)
			return GET_NO_MEMORY;
		else if (OpStatus::IsSuccess(status))
			return GET_SUCCESS;
	}

	return DOM_Object::GetIndex(property_index, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_SQLResultSetRowList::PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (0 <= property_index && property_index < (int)GetLength())
		return PUT_SUCCESS;

	return DOM_Object::PutIndex(property_index, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_SQLResultSetRowList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = GetLength();
	return GET_SUCCESS;
}

/* static */ int
DOM_SQLResultSetRowList::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(row_list, DOM_TYPE_SQL_RESULTSETROWLIST, DOM_SQLResultSetRowList);
	DOM_CHECK_ARGUMENTS("n");

	if (argv[0].value.number >= 0)
	{
		OP_STATUS status = row_list->GetItem(static_cast<unsigned>(argv[0].value.number), return_value);

		if (status == OpStatus::ERR_NO_MEMORY)
			return ES_NO_MEMORY;
		else if (OpStatus::IsSuccess(status))
			return ES_VALUE;
	}

	return ES_FAILED;
}

void
DOM_SQLResultSetRowList::GCTrace()
{
	GCMark(m_parent);

	if (m_all_rows)
		for (unsigned k = 0, length = GetLength(); k < length; k++)
			GCMark(m_all_rows[k]);
}

/* static */ OP_STATUS
DOM_SQLResultSetRow::Make(DOM_SQLResultSetRow *&row_obj, DOM_SQLResultSetRowList* parent, unsigned row_index, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(row_obj = OP_NEW(DOM_SQLResultSetRow, (parent, row_index)), runtime));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_SQLResultSetRow::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	const SqlValue *sql_value = NULL;
	GET_FAILED_IF_ERROR(m_parent->m_parent->m_sql_result_set->GetCachedValueAtColumn(m_row_index, property_name, sql_value));

	if (sql_value)
	{
		if (value)
			GET_FAILED_IF_ERROR(SqlValueToESValue(sql_value, value, &m_parent->m_parent->m_value_string));
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_SQLResultSetRow::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	unsigned index;
	if (m_parent->m_parent->m_sql_result_set->GetColumnIndex(property_name, &index))
		return PUT_READ_ONLY;
	return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_SQLResultSetRow::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	uni_char number_str[16]; /* ARRAY OK joaoe 2009-09-03 */
	uni_itoa(property_index, number_str, 10);
	return GetName(number_str, OP_ATOM_UNASSIGNED, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_SQLResultSetRow::PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime)
{
	uni_char number_str[16]; /* ARRAY OK joaoe 2009-09-03 */
	uni_itoa(property_index, number_str, 10);
	return PutName(number_str, OP_ATOM_UNASSIGNED, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_SQLResultSetRow::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	SqlResultSet *rs = m_parent->m_parent->m_sql_result_set;
	const uni_char *col_name;
	if (rs->IsIterable())
		for(unsigned index = 0, limit = rs->GetColumnCount(); index < limit; index++)
			if (rs->GetColumnName(index, &col_name))
				enumerator->AddPropertyL(col_name);

	return GET_SUCCESS;
}

/* virtual */ void
DOM_SQLResultSetRow::GCTrace()
{
	GCMark(m_parent);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_SQLResultSetRowList)
	DOM_FUNCTIONS_FUNCTION(DOM_SQLResultSetRowList, DOM_SQLResultSetRowList::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_SQLResultSetRowList)

// NOTE!! Keep this in sync with the DOM_SQLError::ErrorCode enum
CONST_ARRAY(g_sql_error_strings, uni_char*)
CONST_ENTRY(UNI_L("Unknown error")),
CONST_ENTRY(UNI_L("Statement failed")),
CONST_ENTRY(UNI_L("The database version didn't match the expected version")),
CONST_ENTRY(UNI_L("Result set too large")),
CONST_ENTRY(UNI_L("Quota exceeded")),
CONST_ENTRY(UNI_L("SQL error")),
CONST_ENTRY(UNI_L("Constraint failure")),
CONST_ENTRY(UNI_L("Operation timed out"))
CONST_END(g_sql_error_strings)

DOM_SQLError::DOM_SQLError(ErrorCode code) :
	DOM_BuiltInConstructor(DOM_Runtime::SQLERROR_PROTOTYPE),
	m_code(code),
	m_custom_message(NULL)
{}

DOM_SQLError::~DOM_SQLError()
{
	OP_DELETEA((uni_char *)m_custom_message);
}

/*static*/ OP_STATUS
DOM_SQLError::Make(DOM_SQLError *&error_obj, DOM_SQLError::ErrorCode code, const uni_char *custom_message, DOM_Runtime *runtime)
{
	DOM_SQLError* new_error_obj;
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_error_obj = OP_NEW(DOM_SQLError, (code)), runtime, runtime->GetPrototype(DOM_Runtime::SQLERROR_PROTOTYPE), "SQLError"));

	if (custom_message)
		RETURN_OOM_IF_NULL(new_error_obj->m_custom_message = UniSetNewStr(custom_message));

	error_obj = new_error_obj;

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_SQLError::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_code:
		DOMSetNumber(value, m_code);
		return GET_SUCCESS;
	case OP_ATOM_message:
		DOMSetString(value, m_custom_message ? m_custom_message : GetErrorDescription(m_code));
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_SQLError::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_code || property_name == OP_ATOM_message)
		return PUT_READ_ONLY;

	return PUT_FAILED;
}

/* static */ const uni_char *
DOM_SQLError::GetErrorDescription(ErrorCode code)
{
	if (code >= UNKNOWN_ERR && code < LAST_SQL_ERROR_CODE)
		return g_sql_error_strings[code];
	else
		return g_sql_error_strings[UNKNOWN_ERR];
}

/* static */ void
DOM_SQLError::ConstructSQLErrorObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	// http://dev.w3.org/html5/webdatabase/#errors-and-exceptions
	PutNumericConstantL(object, "UNKNOWN_ERR", UNKNOWN_ERR, runtime);
	PutNumericConstantL(object, "DATABASE_ERR", DATABASE_ERR, runtime);
	PutNumericConstantL(object, "VERSION_ERR", VERSION_ERR, runtime);
	PutNumericConstantL(object, "TOO_LARGE_ERR", TOO_LARGE_ERR, runtime);
	PutNumericConstantL(object, "QUOTA_ERR", QUOTA_ERR, runtime);
	PutNumericConstantL(object, "SYNTAX_ERR", SQL_SYNTAX_ERR, runtime);
	PutNumericConstantL(object, "CONSTRAINT_ERR", CONSTRAINT_ERR, runtime);
	PutNumericConstantL(object, "TIMEOUT_ERR", TIMEOUT_ERR, runtime);
}

#endif // DATABASE_STORAGE_SUPPORT
