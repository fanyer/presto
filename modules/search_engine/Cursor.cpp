/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SEARCH_ENGINE  // to remove compilation errors with ADVANCED_OPVECTOR

#include "modules/search_engine/Cursor.h"

// how many bytes to preallocate for variable fields
#define ALLOC_VARIABLE_FIELD 32

BSCursor::BSCursor(BOOL autocreate_transaction) : null_field()
{
	buf = NULL;
	bufsize = 0;
	datasize = 0;

	this->autocreate_transaction = autocreate_transaction;

	filepos = INVALID_FILE_LENGTH;

	table = NULL;

	fields = NULL;

	current_field = NULL;
	current_pos = NULL;

	modified = FALSE;
}

BSCursor::BSCursor(BlockStorage *table, BOOL autocreate_transaction) : null_field()
{
	buf = NULL;
	bufsize = 0;
	datasize = 0;

	this->autocreate_transaction = autocreate_transaction;

	filepos = INVALID_FILE_LENGTH;

	this->table = table;

	fields = NULL;

	current_field = NULL;
	current_pos = NULL;

	modified = FALSE;
}

BSCursor::~BSCursor(void)
{
	while (fields != NULL)
	{
		current_field = (ConstField *)fields->next;
		OP_DELETE(fields);
		fields = current_field;
	}

	if (buf != NULL)
		OP_DELETEA(buf);
}

void BSCursor::SetStorage(BlockStorage *table)
{
	if (this->table != NULL)
	{
		if (buf != NULL)
		{
			OP_DELETEA(buf);
			buf = NULL;
		}

		bufsize = 0;
		datasize = 0;

		filepos = INVALID_FILE_LENGTH;
	}

	this->table = table;
}

OP_STATUS BSCursor::AddField(const char *name, int size)
{
	Field *f;
	
	RETURN_OOM_IF_NULL(f = OP_NEW(Field, (size, name, this)));

	if (fields == NULL)
	{
		fields = f;
		current_field = f;
		return OpStatus::OK;
	}

	if (current_field != NULL && current_field->next == NULL && op_strlen(name) < MAX_FIELD_NAME)
	{
		current_field->next = f;
		current_field = f;
		return OpStatus::OK;
	}

	current_field = fields;
	while (current_field->next != NULL)
	{
		if (op_strncmp(current_field->name, name, MAX_FIELD_NAME - 1) == 0)
		{
			OP_DELETE(f);
			return OpStatus::ERR;
		}
		current_field = (ConstField *)current_field->next;
	}

	current_field->next = f;
	current_field = f;
	return OpStatus::OK;
}

OP_STATUS BSCursor::Reserve(int size)
{
	unsigned char *res_buf;

	if (bufsize >= size)
		return OpStatus::OK;

	RETURN_OOM_IF_NULL(res_buf = OP_NEWA(unsigned char, size));

	if (buf != NULL)
	{
		op_memcpy(res_buf, buf, datasize);
		OP_DELETEA(buf);
	}

	buf = res_buf;
	bufsize = size;

	current_field = NULL;
	current_pos = NULL;

	return OpStatus::OK;
}

OP_STATUS BSCursor::Create(void)
{
	int row_size;

	if (Modified())
	{
		RETURN_IF_ERROR(Flush());
	}

	if (fields == NULL)
		return OpStatus::ERR;

	if (autocreate_transaction && !table->InTransaction())
		RETURN_IF_ERROR(table->BeginTransaction());

	row_size = 0;
	datasize = 0;
	for (current_field = fields; current_field != NULL; current_field = (ConstField *)current_field->next)
	{
		if (current_field->variable_length)
		{
			row_size += ALLOC_VARIABLE_FIELD + 4;
			datasize += 4;
			current_field->size = 0;
		}
		else {
			row_size += current_field->size;
			datasize += current_field->size;
		}
	}

	RETURN_IF_ERROR(Reserve(row_size));

	current_field = fields;
	current_pos = buf;

	op_memset(buf, 0, datasize);

	filepos = 0;

	return OpStatus::OK;
}

OP_STATUS BSCursor::Goto(RowID id)
{
	int size;

	if (table == NULL || id == 0)
		return OpStatus::ERR;

	if (Modified())
	{
		RETURN_IF_ERROR(Flush());
	}

	size = table->DataLength(((OpFileLength)id) * table->GetBlockSize());

	if (size > bufsize)
	{
		RETURN_IF_ERROR(Reserve(size));
	}

	filepos = ((OpFileLength)id) * table->GetBlockSize();

	if (!table->Read(buf, size, filepos))
		return OpStatus::ERR_NO_ACCESS;

	current_pos = buf;
	for (current_field = fields; current_field != NULL; current_field = (ConstField *)current_field->next)
	{
		if (current_field->variable_length)
		{
#ifdef NEEDS_RISC_ALIGNMENT
			op_memcpy(&current_field->size, current_pos, 4);
#else
			current_field->size = *(int *)current_pos;
#endif
			if (current_field->size < 0)
				return OpStatus::ERR_PARSING_FAILED;

			current_pos += current_field->size + 4;
		}
		else
			current_pos += current_field->size;

		if (current_pos > buf + size || current_pos < buf)
			return OpStatus::ERR_PARSING_FAILED;
	}

	if (current_pos < buf + size)
		return OpStatus::ERR_PARSING_FAILED;

	current_pos = NULL;

	datasize = size;

	return OpStatus::OK;
}

OP_STATUS BSCursor::Delete(void)
{
	if (table == NULL || filepos == INVALID_FILE_LENGTH)
		return OpStatus::ERR_OUT_OF_RANGE;

	if (filepos != 0)
	{
		if (!table->Delete(filepos))
			return OpStatus::ERR;
	}

	modified = FALSE;

	filepos = INVALID_FILE_LENGTH;
	return OpStatus::OK;
}

OP_STATUS BSCursor::Flush(void)
{
	if (!Modified())
		return OpStatus::OK;

	if (table == NULL || filepos == INVALID_FILE_LENGTH)
		return OpStatus::ERR;
	
	if (filepos == 0)
	{
		OpFileLength pos;
		if ((pos = table->Write(buf, datasize)) == 0)
			return OpStatus::ERR_NO_DISK;
		filepos = pos;
	}
	else {
		if (!table->Update(buf, datasize, filepos))
			return OpStatus::ERR_NO_DISK;
	}

	modified = FALSE;

	return OpStatus::OK;
}

const ConstField &BSCursor::GetField(const char *name) const
{
	ConstField *tmpf;

	if (buf == NULL)
		return *(ConstField *)(&null_field);

	tmpf = fields;
	while (tmpf != NULL && op_strncmp(tmpf->name, name, MAX_FIELD_NAME - 1) != 0)
		tmpf = (ConstField *)tmpf->next;

	if (tmpf != NULL)
		return *tmpf;

	return *(ConstField *)(&null_field);
}

const ConstField &BSCursor::GetField(int pos) const
{
	ConstField *tmpf;

	if (buf == NULL)
		return *(ConstField *)(&null_field);

	tmpf = fields;
	while (tmpf != NULL && pos-- > 0)
		tmpf = (ConstField *)tmpf->next;

	if (tmpf != NULL)
		return *tmpf;

	return *(ConstField *)(&null_field);
}


Field &BSCursor::GetField(const char *name)
{
	if (buf == NULL)
		return *(Field *)(&null_field);

	current_field = fields;
	current_pos = buf;
	while (current_field != NULL && op_strncmp(current_field->name, name, MAX_FIELD_NAME - 1) != 0)
	{
		current_pos += current_field->size + (current_field->variable_length ? 4 : 0);
		current_field = (ConstField *)current_field->next;
	}

	if (current_field != NULL)
		return *(Field *)current_field;

	current_pos = NULL;

	return *(Field *)(&null_field);
}

Field &BSCursor::GetField(int pos)
{
	if (buf == NULL)
		return *(Field *)(&null_field);

	current_field = fields;
	current_pos = buf;
	while (current_field != NULL && pos-- > 0)
	{
		current_pos += current_field->size + (current_field->variable_length ? 4 : 0);
		current_field = (ConstField *)current_field->next;
	}

	if (current_field != NULL)
		return *(Field *)current_field;

	current_pos = NULL;

	return *(Field *)(&null_field);
}

OP_STATUS BSCursor::ModifyField(Field *field, const unsigned char *value, int new_size)
{
	int variable_off;

	variable_off = field->variable_length ? 4 : 0;

	if (autocreate_transaction && field->variable_length && new_size != field->size && !table->InTransaction())
		RETURN_IF_ERROR(table->BeginTransaction());

	if (new_size > field->size)
	{
		if (datasize + new_size - field->size > bufsize)
		{
			RETURN_IF_ERROR(Reserve(datasize + new_size - field->size + ALLOC_VARIABLE_FIELD));
		}
		current_pos = GetPos(field);
		current_field = field;
		if (datasize > current_pos - buf + field->size + variable_off)
			op_memmove(current_pos + new_size + variable_off, current_pos + field->size + variable_off,
			 datasize - (current_pos - buf) - field->size - variable_off);

		datasize += new_size - field->size;
	}
	else if (new_size < field->size)
	{
		current_pos = GetPos(field);
		current_field = field;
		if (datasize > current_pos - buf + field->size + variable_off)
			op_memmove(current_pos + new_size + variable_off, current_pos + field->size + variable_off,
			 datasize - (current_pos - buf) - field->size - variable_off);

		datasize -= field->size - new_size;
	}
	else {
		current_pos = GetPos(field);
		current_field = field;
	}

	if (field->variable_length)
	{
#ifdef NEEDS_RISC_ALIGNMENT
		op_memcpy(current_pos, &new_size, 4);
#else
		*(int *)current_pos = new_size;
#endif
		op_memcpy(current_pos + 4, value, new_size);
	}
	else
		op_memcpy(current_pos, value, new_size);

	field->size = new_size;

	modified = TRUE;

#ifdef _DEBUG
	Field *tmpfield = (Field *)fields;
	int size = 0;
	while (tmpfield != NULL)
	{
		size += tmpfield->size + (tmpfield->variable_length ? 4 : 0);
		tmpfield = (Field *)tmpfield->next;
	}
	OP_ASSERT(datasize == size);
#endif
	return OpStatus::OK;
}

unsigned char *BSCursor::GetPos(const ConstField *field) const
{
	ConstField *tmpf;
	unsigned char *field_pos;

	OP_ASSERT(buf != NULL);

	if (field == current_field && current_pos != NULL)
		return current_pos;

	tmpf = fields;
	field_pos = buf;
	while (tmpf != field && tmpf != NULL)
	{
		field_pos += tmpf->size + (tmpf->variable_length ? 4 : 0);
		tmpf = (ConstField *)tmpf->next;
	}

	OP_ASSERT(tmpf != NULL);
	OP_ASSERT(field_pos <= buf + bufsize);

	return field_pos;
}

#endif  // SEARCH_ENGINE



