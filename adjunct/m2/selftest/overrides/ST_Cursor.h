/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ST_CURSOR_H
#define ST_CURSOR_H

#ifdef SELFTEST

typedef class ST_Cursor CursorDB;
typedef class CursorWrapper CursorDBCreate;

#include "modules/search_engine/Cursor.h"
#include "adjunct/m2/selftest/overrides/ST_BlockStorage.h"

class ST_Field
{
public:
	template<typename T>T &GetValue(T *val) const { INT64 tmp; GetValueInternal(&tmp); *val = tmp; return *val; }
	virtual INT64 &GetValueInternal(INT64 *val) const = 0;
	virtual BOOL IsVariableLength() const = 0;
	virtual OP_STATUS GetStringValue(OpString& val) const = 0;
	virtual OP_STATUS GetStringValue(OpString8& val) const = 0;
	virtual OP_STATUS SetValue(const INT64 &val) = 0;
	virtual OP_STATUS SetStringValue(const uni_char *val) = 0;
	virtual OP_STATUS SetStringValue(const char *val) = 0;
};

class ST_Cursor
{
public:
	virtual ~ST_Cursor() {}

	virtual OP_STATUS Flush() = 0;
	virtual ST_Field &GetField(int pos) = 0;
	virtual OP_STATUS Create() = 0;
	virtual OP_STATUS Delete() = 0;
	virtual OP_STATUS Goto(BSCursor::RowID id) = 0;
	virtual BSCursor::RowID GetID() const = 0;
	virtual void SetStorage(ST_BlockStorage *table) = 0;
	virtual OP_STATUS AddField(const char *name, int size) = 0;
};

struct FieldWrapper : public ST_Field
{
	virtual INT64 &GetValueInternal(INT64 *val) const { return m_field->GetValue(val); }
	virtual BOOL IsVariableLength() const { return m_field->IsVariableLength(); }
	virtual OP_STATUS GetStringValue(OpString& val) const { return m_field->GetStringValue(val); }
	virtual OP_STATUS GetStringValue(OpString8& val) const { return m_field->GetStringValue(val); }
	virtual OP_STATUS SetValue(const INT64 &val) { return m_field->SetValue(val); }
	virtual OP_STATUS SetStringValue(const uni_char *val) { return m_field->SetStringValue(val); }
	virtual OP_STATUS SetStringValue(const char *val) { return m_field->SetStringValue(val); }

	Field* m_field;
};

class CursorWrapper : public ST_Cursor
{
public:
	CursorWrapper(BOOL autocreate_transaction) : m_cursor(autocreate_transaction) {}
	virtual OP_STATUS Flush() { return m_cursor.Flush(); }
	virtual ST_Field &GetField(int pos) { m_field.m_field = &m_cursor.GetField(pos); return m_field; }
	virtual OP_STATUS Create() { return m_cursor.Create(); }
	virtual OP_STATUS Delete() { return m_cursor.Delete(); }
	virtual OP_STATUS Goto(BSCursor::RowID id) { return m_cursor.Goto(id); }
	virtual BSCursor::RowID GetID() const { return m_cursor.GetID(); }
	virtual void SetStorage(ST_BlockStorage *table) { m_cursor.SetStorage(table->GetBlockStorage()); }
	virtual OP_STATUS AddField(const char *name, int size) { return m_cursor.AddField(name, size); }

private:
	BSCursor m_cursor;
	FieldWrapper m_field;
};

#else
typedef class BSCursor CursorDB;
typedef class BSCursor CursorDBCreate;
#endif // SELFTEST

#endif // ST_CURSOR_H
