/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DEVICEAPI_OPDIRECTORYENTRY_H
#define DEVICEAPI_OPDIRECTORYENTRY_H

#ifdef DAPI_DIRECTORY_ENTRY_SUPPORT
#include "modules/util/opfile/opfile.h"
#include "modules/device_api/OpStringHashMultiMap.h"

/** This class implements one value(line) in DirectoryEntry as defined
 * in RFC2425 - A Mime Content-Type for Directory Information
 *
 * A value is a tuple of type name, parameter list and a list of values.
 * Example:
 * name : EMAIL
 * parameters: type = internet, work
 * parameters: charset = utf-8
 * values: person@server.org, human@machine.com
 *
 * name is represented as a simple OpString.
 * parameters are represented as MultiHashMap(map with repetitions)
 * with case insensitive strings as keys. Some standard pre-defined
 * parameters like language are handled separately.
 * value is represented as ValueType structure described above.
 *
 * The values can be complex ie. they can have multiple different fields.
 * For example in vCard 'N' has fields given_name, additional names, family names etc.
 * Each of these fields can have multiple values. To facilitate for this
 * api for adding/removing/setting etc values always assumes that the value is complex
 * and if it is simple it is represented as a value with only one field.
 * As each field can have multiple values to fully *address* one contained value struct
 * 2 indices are used - field_index and value_index - both 0 based.
 * Example:
 *   field_index = 0 , value_index = 1 means second value of the first field.
 * If the method adds/sets a field which does not yet exist then it creates it
 * along with any non existing values/fields before it filling them with NULLs.
 * Example:
 *   OpDirectoryEntryContentLine line;
 *   line.SetValue(2,3,value);
 * wil set first and second field to NULLs and first three values third field to
 * NULLs apart from setting value for indices fourth one to value.
 * For methods which take ValueStruct* as an input parameter - they all take ownership
 * of the object passed to them or delete them if they failed.
 */
class OpDirectoryEntryContentLine
{
public:
	struct ValueStruct
	{
		enum
		{
			UNDEF_VAL,
			STRING_VAL,
			BINARY_VAL,
			INTEGER_VAL,
			FLOAT_VAL,
			BOOL_VAL,
			DATE_VAL,
			TIME_VAL,
			DATETIME_VAL,
		} type;
		union
		{
			uni_char* string_val;
			char* binary_val;
			int integer_val;
			double float_val;
			bool bool_val;
		} value;

		ValueStruct()
			: type(UNDEF_VAL)
		{}
		~ValueStruct()
		{
			switch (type)
			{
			case STRING_VAL:
				OP_DELETEA(value.string_val);
				break;
			case BINARY_VAL:
				OP_DELETEA(value.binary_val);
				break;
			}
		}
	};

	OpDirectoryEntryContentLine();
	~OpDirectoryEntryContentLine(){ m_params.DeleteAll(); }

	/** Sets the name of the field
	 *  RFC2425 says they are case insensitive but by default UPPERCASE names are used
	 */
	OP_STATUS SetName(const uni_char* name) { return m_name.Set(name); }

	/** Gets the name of the field */
	const uni_char* GetName() const { return m_name.CStr(); }

	OP_STATUS SetLanguage(const uni_char* language) { return m_language.Set(language); }
	const uni_char* GetLanguage() const { return m_language.CStr(); }

	/** Gets the map of parameters */
	OpStringHashMultiMap<OpString>* GetParams() { return &m_params; }

	/** Ensures that a content line has at least field_index-1 fields
	 * If the required field does not exist it tries to create it (and all the preceding fields).
	 * @return OK - if the field exists or has been successfully constructed
	 * ERR_NO_MEMORY if it doesn't exist and construction of fields failed
	 */
	OP_STATUS EnsureFieldExist(UINT32 field_index);

	/** Adds a value to the field.*/
	OP_STATUS AddValue(UINT32 field_index, ValueStruct* value);

	/** Sets a value of the field.
	 *
	 * If it succeeds then it deletes previous value
	 */
	OP_STATUS SetValue(UINT32 field_index, UINT32 value_index, ValueStruct* value);

	/** Deletes all the values in the field. Does not delete the field */
	OP_STATUS DeleteValue(UINT32 field_index);

	/** Deletes a value of the field.
	 * This will change the indexes of the values after it.
	 * Example:
	 * lets say we have values :[val1, val2, val3, val4]
	 * then DeleteValue(0,2);
	 * then the result becomes [val1, val2, val4]
	 */
	OP_STATUS DeleteValue(UINT32 field_index, UINT32 value_index);

	/** Gets the number of the fields */
	OP_STATUS GetFieldCount(UINT32& count);

	/** Gets the number values in the fields */
	OP_STATUS GetValueCount(UINT32 field_index, UINT32& count);

	/** Gets the value at specified indexes.
	 *
	 * The value is still owned by OpDirectoryEntryContentLine so if it is to be
	 * preserved anywhere a copy should be made(as it is a POD memcpy is fine)
	 */
	OP_STATUS GetValue(UINT32 field_index, UINT32 value_index, ValueStruct*& value);

	/** Allocates new value and sets its ValueStruct to value as a string copying its content */
	static ValueStruct* NewStringValue(const uni_char* value);
	/** Allocates new value and sets its ValueStruct to value as a binary copying its content */
	static ValueStruct* NewBinaryValue(const char* value);
	/** Allocates new value and sets its ValueStruct to value as a integer */
	static ValueStruct* NewIntegerValue(int value);
	/** Allocates new value and sets its ValueStruct to value as a float */
	static ValueStruct* NewFloatValue(double value);
	/** Allocates new value and sets its ValueStruct to value as a boolean */
	static ValueStruct* NewBooleanValue(bool value);

	/** Prints textual representation of a directory entry to an opened_file */
	virtual OP_STATUS Print(OpFileDescriptor* opened_file);
private:
	OP_STATUS PrintValueStruct(OpFileDescriptor* opened_file, ValueStruct* value_struct);
	/** This class implements printing text using folding/escaping rules included in RFC2425
	 * for printing directory entry content lines
	 * These rules base on the ones described RFC822
	 */
	class TextMessageEncoder
	{
	public:
		TextMessageEncoder() : m_chars_in_buffer(0) {}
		OP_STATUS PrintNonEscaped(OpFileDescriptor* opened_file, const uni_char*, int data_len);
		OP_STATUS PrintEscaped(OpFileDescriptor* opened_file, const uni_char*, int data_len);

		OP_STATUS PutChar(OpFileDescriptor* opened_file, uni_char data);
		OP_STATUS FlushLine(OpFileDescriptor* opened_file, BOOL finish_print);
		enum { MAX_LINE_LENGTH = 75 };
		uni_char m_line_buffer[MAX_LINE_LENGTH]; /* ARRAY OK 2010-09-08 msimonides */
		int m_chars_in_buffer;
	};
	TextMessageEncoder m_encoder;
	OpAutoVector<OpAutoVector<ValueStruct> > m_values;
	OpString m_name;
	OpString m_language;
	OpStringHashMultiMap<OpString> m_params;
};

/** This class implements DirectoryEntry as defined in RFC2425
 * - A Mime Content-Type for Directory Information
 *
 * Generally directory entry is a bag of named and typed values.
 * Particular profiles of directory define specific meanings and types
 * for those values(for exzmple vCard profile defines that there should
 * be one value named fn of string type which means formatted name of
 * a contact).
 *
 * This class deals with storing values and convering them to textual
 * format.
 */
class OpDirectoryEntry
{
public:
	OpDirectoryEntry(){};
	virtual ~OpDirectoryEntry(){}

	/** Adds a content line taking ownership of it
	 *
	 * If adding fails caller retains ownership of the value.
	 */
	OP_STATUS AddContentLine(OpDirectoryEntryContentLine* value) { return m_values.Add(value); }

	/**  Gets the number of the content lines */
	UINT32 GetContentLineCount() { return m_values.GetCount(); }

	/** Gets a content line. This content line is still owned by OpDirectoryEntry
	 *
	 * @param index - index of the value
	 */
	OpDirectoryEntryContentLine* GetContentLine(UINT32 index) { return m_values.Get(index); }

	/** Removes content line from OpDirectoryEntry but doesn't delete it.
	 *
	 * @param index - index of the content line to remove
	 * @return Deleted content line or NULL if there was no line with specified index
	 */
	OpDirectoryEntryContentLine* RemoveContentLine(UINT32 index) { return m_values.Remove(index); }

	/** Removes content line from OpDirectoryEntry and deletes it.
	 *
	 * @param index - index of the content line to delete
	 */
	void DeleteContentLine(UINT32 index) { return m_values.Delete(index); }

	/** Prints out textual representation of a directory entry to an opened_file.
	 *
	 * @param opened_file - file to which the data wil be printed
	 */
	OP_STATUS Print(OpFileDescriptor* opened_file);

protected:
	OpDirectoryEntryContentLine* FindNamedContentLine(const uni_char* name);
	OpDirectoryEntryContentLine* ConstructNewNamedContentLine(const uni_char* name, UINT32 fields_num = 0);
	OpDirectoryEntryContentLine* FindOrConstructNamedContentLine(const uni_char* name, UINT32 fields_num = 0);
private:
	OpAutoVector<OpDirectoryEntryContentLine> m_values;
};

#endif // DAPI_DIRECTORY_ENTRY_SUPPORT

#endif // DEVICEAPI_OPDIRECTORYENTRY_H
