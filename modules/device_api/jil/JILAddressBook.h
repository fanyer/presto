/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DEVICEAPI_JIL_JILADDRESSBOOK_H
#define DEVICEAPI_JIL_JILADDRESSBOOK_H

#include "modules/pi/device_api/OpAddressBook.h"

/** Mapping for JIL address book functionality.
 *
 * JIL specifies two kinds of attributes:
 * - real ones that correspond to fields in the underlying system,
 * - virtual ones that are required by JIL but have no representation
 *   in the underlying system. Such fields are simulated by concatenating
 *   values from several real fields. They cannot be updated in the
 *   underlying system.
 *
 * Attributes in JIL may also simulate multiple values. It is achieved by
 * handling attributes with numbers appended, e.g. 'mobilePhone' and
 * 'mobilePhone2' would be mapped to two values of a field corresponding
 * to mobile phone.
 */
class JIL_AddressBookMappings
{
public:
	~JIL_AddressBookMappings() { };

	static OP_STATUS Make(JIL_AddressBookMappings*& new_obj, OpAddressBook* address_book);

	/** Check whether attribute_name is valid.
	 */
	BOOL HasAttribute(const uni_char* attribute_name);

	/** Get the value of an attribute.
	 *
	 * If the attribute is a virtual one, the value is built from real values.
	 *
	 * @param address_book_item The item for which an attribute is to be read.
	 * @param attribute_name Name of the attribute.
	 * @param value Set to the value of the attribute.
	 *
	 * @return
	 *  - OK - the value has been successfully retrieved
	 *  - ERR_FIELD_INDEX_OUT_OF_RANGE - the attribute_name is not supported
	 *  - ERR_VALUE_INDEX_OUT_OF_RANGE - the attribute_name is not supported
	 */
	OP_ADDRESSBOOKSTATUS GetAttributeValue(const OpAddressBookItem* address_book_item, const uni_char* attribute_name, OpString* value);

	/** Set the value of an attribute.
	 *
	 * If the attribute is a virtual one, ERR_NO_ACCESS is returned.
	 *
	 * @param address_book_item The item for which an attribute is to be set.
	 * @param attribute_name Name of the attribute.
	 * @param value The new value of the attribute.
	 *
	 * @return
	 *  - OK - the value has been successfully set
	 *  - ERR_NO_ACCESS - the field is virtual and the value cannot be set
	 *  - ERR_FIELD_INDEX_OUT_OF_RANGE - the attribute_name is not supported
	 *  - ERR_VALUE_INDEX_OUT_OF_RANGE - the attribute_name is not supported
	 */
	OP_ADDRESSBOOKSTATUS SetAttributeValue(OpAddressBookItem* address_book_item, const uni_char* attribute_name, const uni_char* value);

	/** An interface for building a collection by adding items one by one.
	 */
	class CollectionBuilder
	{
	public:
		virtual ~CollectionBuilder() { }
		virtual OP_STATUS Add(const uni_char* attribute_name) = 0;
	};

	/** Provides list of JIL field names a given item supports.
	 *
	 * (for the purpose of the description below please assume that
	 * "attribute" means an OpAddressBookItem attribute and "field"
	 * means an attribute in JIL representation of the item).
	 *
	 * The set of supported fields is defined by the underlying
	 * address book.
	 *
	 * In some cases an attribute of an OpAddressBookItem may hold multiple
	 * values. This is not supported by JIL, but is simulated by having
	 * multiple fields with consecutive numbers appended to the base name.
	 * E.g. in case of attribute "mobilePhone" having multiple values
	 * JIL fields "mobilePhone", "mobilePhone2", "mobilePhone3" etc.
	 * are present.
	 *
	 * If the number of values an attribute can hold is bounded (e.g. Windows
	 * Mobile has 3 email address fields) this function will list JIL
	 * fields for all the values (e.g. "eMail", "eMail2", "eMail3").
	 *
	 * In case the number of attributes is unbounded by the underlying
	 * implementation we cannot generate unlimited number of JIL fields.
	 * This is where the item parameter comes into play: the JIL fields
	 * are generated for actual values stored in the item.
	 * If there are no values for a given attribute, only the first base
	 * field is listed (i.e. "mobilePhone" in the example above).
	 *
	 * This means that different items may return different sets of available
	 * fields:
	 * e.g. an item with two values for "mobilePhone" attribute will have
	 * JIL "mobilePhone" and "mobilePhone2" fields while item with
	 * no values for "mobilePhone" attribute will have only the JIL
	 * "mobilePhone" field.
	 *
	 * If the item parameter is not provided then only single JIL field
	 * is listed for each unbounded multivalue attribute.
	 *
	 * @param item Item whose fields are enumerated, may be NULL (see above).
	 * @param collection Object to which the enumerated field names are added.
	 */
	OP_STATUS GetAvailableAttributes(OpAddressBookItem* item, CollectionBuilder* collection);

	enum { FIELD_INDEX_VIRTUAL = -1 };
private:
	JIL_AddressBookMappings();

#ifdef SELFTEST
public:
#endif // SELFTEST

	struct Mapping
	{
		int field_index;		// Index of the field
		int multiplicity;		// Number of values

		/** Array of real field indexes that comprise this virtual field.
		 *
		 */
		OpINT32Vector virtual_source_indexes;

		/** Flags if the virtual field includes multiple values.
		 *
		 * This is useful if the virtual field should contain all
		 * values of the subfields it comprises of.
		 * One such example is full name which might want to concatenate
		 * all the values of e.g. middle name, like this:
		 *   first_name middle_name[1] middle_name[2] last_name
		 */
		BOOL virtual_includes_multiple_values;

		BOOL IsVirtualField() const { return field_index == FIELD_INDEX_VIRTUAL; }
	};

	/** Gets the field index and value index for JIL address book property.
	 *
	 * @param property_name - name of a JIL address book property.
	 * @param field_index - set to field index of the property in OpAddressBookItem. MUST NOT be NULL.
	 * @param value_index - set to value index of the property in OpAddressBookItem. MUST NOT be NULL.
	 * @return TRUE if a mapping has been found, FALSE otherwise.
	 */
	BOOL GetMapping(const uni_char* property_name,/*out*/ int* field_index,/*out*/ int* value_index);

	/** Gets the mapping information and and value index for JIL address book property.
	 *
	 * @param property_name Name of a JIL address book property. Not NULL.
	 * @param mapping Set to mapping info of the property in OpAddressBookItem. MUST NOT be NULL.
	 * @param index Set to value index of the property in OpAddressBookItem. MUST NOT be NULL.
	 * @return TRUE if a mapping has been found, FALSE otherwise.
	 */
	BOOL GetMapping(const uni_char* property_name, JIL_AddressBookMappings::Mapping** mapping, int* index);

#ifdef SELFTEST
private:
#endif // SELFTEST

	OpAddressBook* m_address_book;
	OpAutoStringHashTable<Mapping> m_mappings;
};

/** JIL functionality wrapper for OpAddressBookItem.
 *
 * Wraps OpAddressBookItem and provides access to the attributes using
 * JIL attribute names.
 *
 * The wrapper enables setting values to JIL virtual fields - the values
 * are stored inside the wrappers and they shadow the values obtained from
 * the underlying OpAddressBookItem (see JIL_PropertyMappings::GetAttributeValue).
 *
 * The method IsVirtualFieldModified may be used to check if any value has been
 * set to any virtual field.
 */
class JIL_AddressBookItemData
{
public:
	JIL_AddressBookItemData() : m_address_book_item(NULL) { }
	JIL_AddressBookItemData(OpAddressBookItem* item) : m_address_book_item(item) { }
	virtual ~JIL_AddressBookItemData() { }

	/** Initializes the object from item_to_copy.
	 *
	 * If the operation doesn't succeed, the object should not be used.
	 */
	OP_STATUS CloneFrom(JIL_AddressBookItemData& item_to_copy);

	OpAddressBookItem* GetAddressBookItem() const    { return m_address_book_item; }
	void SetAddressBookItem(OpAddressBookItem* item) { m_address_book_item = item; }
	BOOL HasItem() const                             { return m_address_book_item != NULL; }

	OP_STATUS GetId(OpString* id)                    { return m_address_book_item->GetId(id); }

	/** Reads value of an attribute.
	 *
	 * May also be used to check for existence of an attribute if the value
	 * argument is NULL.
	 *
	 * See the description of the class for discussion on virtual attribute
	 * handling.
	 *
	 * @param attribute_name Name of the attribute to read.
	 * @param value Set to the value of the attribute.
	 * May be NULL, in which case the operation is reduced to just checking
	 * whether ther attribute_name is valid.
	 *
	 * @return
	 *  - OK,
	 *  - ERR_NO_MEMORY,
	 *  - ERR_FIELD_INDEX_OUT_OF_RANGE - the attribute_name is not supported
	 *  - ERR_VALUE_INDEX_OUT_OF_RANGE - the attribute_name is not supported
	 */
	OP_ADDRESSBOOKSTATUS ReadAttributeValue(const uni_char* attribute_name, OpString* value);

	/** Writes value of an attribute.
	 *
	 * See the description of the class for discussion on virtual attribute
	 * handling.
	 *
	 * @param attribute_name Name of the attribute to read.
	 * @param value Set to the value of the attribute.
	 *
	 * @return
	 *  - OK,
	 *  - ERR_NO_MEMORY,
	 *  - ERR_FIELD_INDEX_OUT_OF_RANGE - the attribute_name is not supported
	 *  - ERR_VALUE_INDEX_OUT_OF_RANGE - the attribute_name is not supported
	 */
	OP_ADDRESSBOOKSTATUS WriteAttributeValue(const uni_char* attribute_name, const uni_char* new_value);

	/** Checks that an item matches this reference item.
	 *
	 * Matching is performed in accordance to the JIL specification.
	 * This object is a reference item, meaning it specifies the search
	 * criteria, i.e.it may contain wildcards.
	 *
	 * The matching rules are as follows:
	 * - "*" matches any substring,
	 * - "\*" represents a literal "*",
	 * - no attribute value matches any value,
	 * - empty string in attribute matches only empty string.
	 *
	 * @param matched_item Specifes the item to match against this.
	 */
	OP_BOOLEAN IsMatch(JIL_AddressBookItemData* matched_item);

	/** Checks whether this item had any of it's virtual fields modified.
	 *
	 * A virtual field is modified if it has been assigned a value with
	 * WriteAttributeValue.
	 *
	 * @param modified_attribute_name If the return value is IS_TRUE, this argument
	 * is set to point to the name of one of the modified attributes.
	 * Needs to be copied by the client. May be NULL.
	 *
	 * @return IS_TRUE if there is at least one field modified.
	 */
	OP_BOOLEAN IsVirtualFieldModified(const uni_char** modified_attribute_name);

protected:

	/** A structure for storing both key and the value in a hash table */
	struct VirtualFieldDataItem
	{
		uni_char* key;
		uni_char* value;

		VirtualFieldDataItem() : key(NULL), value(NULL){}
		~VirtualFieldDataItem()
		{
			op_free(key);
			op_free(value);
		}
		OP_STATUS Construct(const uni_char* key, const uni_char* value);
		OP_STATUS SetValue(const uni_char* value);
	};

	OpAddressBookItem* m_address_book_item;
	OpAutoStringHashTable<VirtualFieldDataItem> m_virtual_fields;
};

class JIL_AutoAddressBookItemData : public JIL_AddressBookItemData
{
public:
	JIL_AutoAddressBookItemData() : JIL_AddressBookItemData() { }
	JIL_AutoAddressBookItemData(OpAddressBookItem* item) : JIL_AddressBookItemData(item) { }
	~JIL_AutoAddressBookItemData() { OP_DELETE(m_address_book_item); }
};

#endif // DEVICEAPI_JIL_JILADDRESSBOOK_H
