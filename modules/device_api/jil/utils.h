/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DEVICEAPI_JIL_UTILS_H
#define DEVICEAPI_JIL_UTILS_H

// helpers for storing values in C++ while retaining information about undefined/null
enum Undefnull
{
  IS_VALUE = 0,
  IS_NULL = 1,
  IS_UNDEF = 2
};

// Macros to simplify using undefnull in DOM put/get methods
#define JIL_PUT_BOOLEAN(atom_name, var_name, undefnull_var)\
	case atom_name:                       \
	do {                                            \
		switch(value->type)                         \
		{                                           \
			case VALUE_BOOLEAN:						\
				undefnull_var = IS_VALUE;	        \
				var_name = value->value.boolean;    \
				return PUT_SUCCESS;                 \
			case VALUE_NULL:                        \
				undefnull_var = IS_NULL;            \
				var_name = FALSE;                   \
				return PUT_SUCCESS;                 \
			case VALUE_UNDEFINED:                   \
				undefnull_var = IS_UNDEF;           \
				var_name = FALSE;                   \
				return PUT_SUCCESS;                 \
			default:                                \
				return PUT_NEEDS_BOOLEAN;           \
		}                                           \
	} while(0)

#define JIL_PUT_STRING(atom_name, var_name, undefnull_var) \
	case atom_name:                       \
	do {                                            \
		switch(value->type)                         \
		{                                           \
			case VALUE_STRING:                      \
				undefnull_var = IS_VALUE;           \
				PUT_FAILED_IF_ERROR(var_name.Set(value->value.string)); \
				return PUT_SUCCESS;                 \
			case VALUE_NULL:                        \
				undefnull_var = IS_NULL;            \
				var_name.Empty();                   \
				return PUT_SUCCESS;                 \
			case VALUE_UNDEFINED:                   \
				undefnull_var = IS_UNDEF;           \
				var_name.Empty();                   \
				return PUT_SUCCESS;                 \
			default:                                \
				return PUT_NEEDS_STRING;            \
		}                                           \
	} while(0)

#define JIL_PUT_NUMBER(atom_name, var_name, undefnull_var) \
	case atom_name:                       \
	do {                                            \
		switch(value->type)                         \
		{                                           \
			case VALUE_NUMBER:                      \
				undefnull_var = IS_VALUE;           \
				var_name = value->value.number;     \
				return PUT_SUCCESS;                 \
			case VALUE_NULL:                        \
				undefnull_var = IS_NULL;            \
				var_name = 0.0;                     \
				return PUT_SUCCESS;                 \
			case VALUE_UNDEFINED:                   \
				undefnull_var = IS_UNDEF;           \
				var_name = 0.0;                     \
				return PUT_SUCCESS;                 \
			default:                                \
				return PUT_NEEDS_NUMBER;            \
		}                                           \
	} while(0)

#define JIL_PUT_DATE(atom_name, var_name)            \
case atom_name:                                      \
	return DOM_JILObject::PutDate(value, var_name);

#define JIL_GET_BOOLEAN(atom_name, var_name, undefnull_var) \
	case atom_name:                       \
	do {                                            \
		switch(undefnull_var)                       \
		{                                           \
		case IS_VALUE: DOMSetBoolean(value, var_name); break; \
		case IS_NULL: DOMSetNull(value); break;     \
		case IS_UNDEF: DOMSetUndefined(value); break; \
		default: OP_ASSERT(FALSE); break;           \
		}                                           \
		return GET_SUCCESS;                         \
	} while(0)

#define JIL_GET_STRING(atom_name, var_name, undefnull_var) \
	case atom_name:                       \
	do {                                            \
		switch(undefnull_var)                       \
		{                                           \
		case IS_VALUE: DOMSetString(value, var_name.CStr()); break; \
		case IS_NULL: DOMSetNull(value); break;     \
		case IS_UNDEF: DOMSetUndefined(value); break; \
		default: OP_ASSERT(FALSE); break;           \
		}                                           \
		return GET_SUCCESS;                         \
	} while(0)

#define JIL_GET_NUMBER(atom_name, var_name, undefnull_var) \
	case atom_name:                       \
	do {                                            \
		switch(undefnull_var)                       \
		{                                           \
		case IS_VALUE: DOMSetNumber(value, var_name); break; \
		case IS_NULL: DOMSetNull(value); break;     \
		case IS_UNDEF: DOMSetUndefined(value); break; \
		default: OP_ASSERT(FALSE); break;           \
		}                                           \
		return GET_SUCCESS;                         \
	} while(0)

#endif // DEVICEAPI_JIL_UTILS_H
