/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef CURSOR_H
#define CURSOR_H

#include "modules/search_engine/BlockStorage.h"

#define MAX_FIELD_NAME 8

class BSCursor;

// workaround for MSVC 6 error C2893 (bodies of the template methods cannot be defined separately from declaration)
class FieldBase
{
protected:
	BSCursor *cursor;
	int size;
	BOOL variable_length;
	FieldBase *next;
	char name[MAX_FIELD_NAME];  /* ARRAY OK 2010-09-24 roarl */
public:
	FieldBase()
	{
		cursor = NULL;
		size = 0;
		variable_length = FALSE;
		next = NULL;
		name[0] = 0;
	}
};

class ConstField;
class Field;

/**
 * @brief BSCursor built on top of BlockStorage.
 * @author Pavel Studeny <pavels@opera.com>
 *
 * Cursor provides an access to structured data on top of BlockStorage.
 * It manipulates with data blocks from BlockStorage like rows of a table.
 * You can setup named fields of static/variable length and acces them separately.
 */
class BSCursor : public NonCopyable
{
public:
	typedef OpFileLength RowID;

	/**
	 * empty constructor, use SetStorage later
	 * @param autocreate_transaction automatically enter a transaction when creating new rows
	 *     or modifying a size of the existing ones
	 */
	BSCursor(BOOL autocreate_transaction = FALSE);

	/**
	 * creates BSCursor on given table
	 * @param table opened BlockStorage
	 * @param autocreate_transaction automatically enter a transaction when creating new rows
	 *     or modifying a size of the existing ones
	 */
	BSCursor(BlockStorage *table, BOOL autocreate_transaction = FALSE);

	/**
	 * doesn't Flush unsaved data
	 */
	~BSCursor(void);

	/**
	 * sets a table if it hadn't been already set
	 */
	void SetStorage(BlockStorage *table);

	/**
	 * create a new field
	 * @param name name limited to MAX_FIELD_NAME - 1 characters (rest is ignored)
	 * @param size size of static field, 0 for variable length field
	 */
	CHECK_RESULT(OP_STATUS AddField(const char *name, int size));

	/**
	 * reserve internal buffer to avoid future resizing, doesn't decrease the size
	 * @param size new size of internal buffer
	 */
	CHECK_RESULT(OP_STATUS Reserve(int size));

	/**
	 * @return true if the current row has been modified
	 */
	BOOL Modified() const
	{
		return modified;
	}

	/**
	 * create a new row, it will have its ID after first Flush
	 */
	CHECK_RESULT(OP_STATUS Create(void));

	/**
	 * select a row with given id, flushes modified data; the first RowID is 1
	 */
	CHECK_RESULT(OP_STATUS Goto(RowID id));

	/**
	 * delete current row, Cursor doesn't contatin valid data after successfull operation
	 */
	CHECK_RESULT(OP_STATUS Delete(void));

	/**
	 * write modified data to disk
	 * @return current ID
	 */
	CHECK_RESULT(OP_STATUS Flush(void));

	/**
	 * get current size of internal buffer
	 */
	int Capacity(void) const
	{
		return bufsize;
	}

	/**
	 * @return number of valid elements
	 */
	int Size(void) const
	{
		return datasize;
	}

	/**
	 * @return current ID
	 */
	RowID GetID(void) const
	{
		return filepos / table->GetBlockSize();
	}

	/**
	 * return a pointer to data
	 */
	const unsigned char *CPtr(void) const
	{
		return buf;
	}

	/**
	 * @param name name of previously created field
	 * @return read-only field
	 */
	const ConstField &GetField(const char *name) const;

	/**
	 * @param pos position of previously created field counted from 0
	 * @return read-only field
	 */
	const ConstField &GetField(int pos) const;

	/**
	 * @param name name of previously created field
	 * @return read/write field
	 */
	Field &GetField(const char *name);

	/**
	 * @param pos position of previously created field counted from 0
	 * @return read/write field
	 */
	Field &GetField(int pos);

	/** wrapper for GetField */
	const ConstField &operator[](const char *name) const {return GetField(name);}
	/** wrapper for GetField */
	const ConstField &operator[](int pos) const {return GetField(pos);}
	/** wrapper for GetField */
	Field &operator[](const char *name) {return GetField(name);}
	/** wrapper for GetField */
	Field &operator[](int pos) {return GetField(pos);}

protected:
	friend class ConstField;
	friend class Field;

	CHECK_RESULT(OP_STATUS ModifyField(Field *field, const unsigned char *value, int new_size));
	unsigned char *GetPos(const ConstField *field) const;


	BlockStorage *table;
	unsigned char *buf;
	int bufsize;
	int datasize;

	BOOL autocreate_transaction;

	OpFileLength filepos;

	ConstField *fields;
	BOOL modified;

	ConstField *current_field;
	unsigned char *current_pos;

	FieldBase null_field;
};

/** @brief Read-only BSCursor field. */
class ConstField : public FieldBase
{
protected:
	friend class BSCursor;

	ConstField(int size, const char *name, BSCursor *cursor)
	{
		this->cursor = cursor;
		this->size = size;

		this->variable_length = size == 0;

		this->next = NULL;
		op_strncpy(this->name, name, MAX_FIELD_NAME);
		this->name[MAX_FIELD_NAME - 1] = 0;
	}

public:
	ConstField(void)
	{
		cursor = NULL;
	}

	/**
	 * @return size of current field, doesn't count in the ending 0 for strings
	 */
	int GetSize(void) const
	{
		return size;
	}

	/**
	 * @return whether this field has a variable length
	 */
	BOOL IsVariableLength(void) const
	{
		return variable_length;
	}

	/**
	 * get value of a static-sized field
	 * @param val variable to store the value
	 * @return reference to val
	 */
	template<typename T>T &GetValue(T *val) const
	{
		int maxlen;
		if (variable_length || cursor == NULL)  // variable length fields shouldn't use this method
		{
			*val = 0;
			return *val;
		}
		maxlen = (int)sizeof(T) > size ? size : sizeof(T);

		if ((int)sizeof(T) > maxlen)
			op_memset(val, 0, sizeof(T));
#ifndef OPERA_BIG_ENDIAN
		op_memcpy(val, cursor->GetPos(this), maxlen);
#else
		op_memcpy(((char *)val) + sizeof(T) - maxlen, (char *)cursor->GetPos(this) + size - maxlen, maxlen);
#endif
		return *val;
	}

	/**
	 * get value of a field as a string, maxlen must be one character longer than GetSize() should the string include the ending 0
	 * @param val buffer at least maxlen characters long
	 * @param maxlen length val or 0 not to check
	 * @return val
	 */
	char *GetStringValue(char *val, int maxlen = 0) const
	{
		int copy_len;

		if (cursor == NULL)
		{
			val[0] = 0;
			return val;
		}

		copy_len = maxlen;
		if (copy_len > size || copy_len <= 0)
			copy_len = size;

		if (variable_length)
			op_strncpy(val, (char *)(cursor->GetPos(this) + 4), copy_len);
		else op_strncpy(val, (char *)cursor->GetPos(this), copy_len);

		if ((size < maxlen || size == 0 || maxlen == 0) && (size == 0 || val[size - 1] != 0))
			val[size] = 0;

		return val;
	}

	/**
	 * get value of a field as a string, maxlen must be one character longer than GetSize() should the string include the ending 0
	 * @param val buffer at least maxlen characters long
	 * @param maxlen length val or 0 not to check
	 * @return val
	 */
	uni_char *GetStringValue(uni_char *val, int maxlen = 0) const
	{
		int copy_len;

		if (cursor == NULL)
		{
			val[0] = 0;
			return val;
		}

		copy_len = maxlen * 2;
		if (copy_len > size || copy_len <= 0)
			copy_len = size + 1;

		if (variable_length)
			op_memcpy(val, cursor->GetPos(this) + 4, copy_len);
		else op_memcpy(val, cursor->GetPos(this), copy_len);

		if ((size < maxlen || size <= 1 || maxlen == 0) && (size <= 1 || val[size / 2 - 1] != 0))
			val[size / 2] = 0;

		return val;
	}

	/**
	 * get value of a filed as OpString8
	 */
	CHECK_RESULT(OP_STATUS GetStringValue(OpString8& val) const)
	{
		char* char_val;
		
		if ((char_val = val.Reserve(size)) == NULL)
			return OpStatus::ERR_NO_MEMORY;
		
		GetStringValue(char_val);
		
		return OpStatus::OK;
	}

	/**
	 * get value of a filed as OpString
	 */
	CHECK_RESULT(OP_STATUS GetStringValue(OpString& val) const)
	{
		uni_char* uni_val;
		
		if ((uni_val = val.Reserve(size / 2)) == NULL)
			return OpStatus::ERR_NO_MEMORY;
		
		GetStringValue(uni_val);
		
		return OpStatus::OK;
	}

	/**
	 * allocate a buffer and copy a string value into it
	 * @return NULL on out of memory
	 */
	char *CopyStringValue(char **val) const
	{
		if (cursor == NULL)
		{
			return NULL;
		}

		if ((*val = OP_NEWA(char, size + 1)) == NULL)
			return NULL;

		if (variable_length)
			op_strncpy(*val, (char *)(cursor->GetPos(this) + 4), size);
		else op_strncpy(*val, (char *)cursor->GetPos(this), size);

		(*val)[size] = 0;

		return *val;
	}

	/**
	 * allocate a buffer and copy a string value into it
	 * @return NULL on out of memory
	 */
	uni_char *CopyStringValue(uni_char **val) const
	{
		int maxlen;

		if (cursor == NULL)
		{
			return NULL;
		}

		maxlen = size / 2;

		if ((*val = OP_NEWA(uni_char, maxlen + 1)) == NULL)
			return NULL;

		if (variable_length)
			op_memcpy(*val, cursor->GetPos(this) + 4, maxlen * 2);
		else op_memcpy(*val, cursor->GetPos(this), maxlen * 2);

		(*val)[maxlen] = 0;

		return *val;
	}

	/**
	 * get value of a general variable-length field
	 */
	template<typename T>T *GetValue(T *val, int maxsize) const
	{
		if (cursor == NULL)
		{
			op_memset(val, 0, maxsize);
			return val;
		}
		if (maxsize > GetSize())
			maxsize = GetSize();

		if (variable_length)
			op_memcpy(val, cursor->GetPos(this) + 4, maxsize);
		else op_memcpy(val, cursor->GetPos(this), maxsize);

		return val;
	}

	/**
	 * typecasting to anything else than char * might cause a bus error on sparc solaris
	 * because of byte alignment
	 * @return address of the field's data
	 */
	const void *GetAddress() const
	{
		if (cursor == NULL)
			return NULL;

		return variable_length && size > 0 ? cursor->GetPos(this) + 4 : cursor->GetPos(this);
	}
};

/** @brief Read/write BSCursor field. */
class Field : public ConstField
{
protected:
	friend class BSCursor;

	Field(int size, const char *name, BSCursor *cursor) : ConstField(size, name, cursor) {};

public:
	Field(void) : ConstField() {};

	/**
	 * set value of a static-sized field, resizes integers automatically
	 */
	template<typename T>OP_STATUS SetValue(const T &val)
	{
		if (cursor == NULL)
		{
			return OpStatus::ERR;
		}

		if ((int)sizeof(T) > size)
#ifndef OPERA_BIG_ENDIAN
			return cursor->ModifyField(this, (const unsigned char *)&val, size);
#else
			return cursor->ModifyField(this, ((const unsigned char *)&val) + sizeof(T) - size, size);
#endif

		if ((int)sizeof(T) < size)
		{
			OP_STATUS rv;
			unsigned char *tmpbuf = OP_NEWA(unsigned char, size);
			if (tmpbuf == NULL)
				return OpStatus::ERR_NO_MEMORY;

			op_memset(tmpbuf, 0, size);
#ifndef OPERA_BIG_ENDIAN
			op_memcpy(tmpbuf, &val, sizeof(T));  // sizeof(T) < size here
#else
			op_memcpy(tmpbuf + size - sizeof(T), &val, sizeof(T));
#endif
			rv = cursor->ModifyField(this, tmpbuf, size);

			OP_DELETEA(tmpbuf);

			return rv;
		}

		return cursor->ModifyField(this, (const unsigned char *)&val, size);
	}

	/**
	 * set value of variable-length field as a string
	 */
	CHECK_RESULT(OP_STATUS SetStringValue(const char *val))
	{
		if (cursor == NULL)
			return OpStatus::ERR;

		if (val == NULL)
			return cursor->ModifyField(this, (const unsigned char *)"", 1);

		// it would be better not to include the ending 0 in the data,
		// but then the string wouldn't be finished when accessed by GetAddress()
		return cursor->ModifyField(this, (const unsigned char *)val, (int)op_strlen(val) + 1);
	}

	/**
	 * set value of variable-length field as a string
	 */
	CHECK_RESULT(OP_STATUS SetStringValue(const uni_char *val))
	{
		if (cursor == NULL)
			return OpStatus::ERR;

		if (val == NULL)
			return cursor->ModifyField(this, (const unsigned char *)(UNI_L("")), 2);

		// it would be better not to include the ending 0 in the data,
		// but then the string wouldn't be finished when accessed by GetAddress()
		return cursor->ModifyField(this, (const unsigned char *)val, (int)uni_strlen(val) * 2 + 2);
	}

	/**
	 * get value of a general variable-length field
	 */
	CHECK_RESULT(OP_STATUS SetValue(const void *val, int len))
	{
		if (cursor == NULL)
		{
			return OpStatus::ERR;
		}
		return cursor->ModifyField(this, (const unsigned char *)val, len);
	}
};



#endif  // CURSOR_H

