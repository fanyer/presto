/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"

#define INITIALIZED_FLAG 0x01
#define CHAINED_FLAG 0x10


// These numbers are calculated by searching for the highest prime
// inside each 2^n group.

#define NR_OF_HASH_TABLE_SIZES 24

const UINT32 hashTableSizes[] = {
	31,
	61,
	127,
	251,
	509,
	1021,
	2039,
	4093,
	8191,
	16381,
	32749,
	65521,
	131071,
	262139,
	524287,
	1048573,
	2097143,
	4194301,
	8388593,
	16777213,
	33554393,
	67108859,
	134217689,
	268435399
};

// These values tells us the smallest number of elements, before
// the hashtable shrinks. For the lowest level, this value is 0,
// for the others, it is 0.2 * nrOfElements, rounded upwards.

const UINT32 minimumNrOfElements[] = {
	0,
	13,
	26,
	51,
	102,
	205,
	408,
	819,
	1639,
	3277,
	6550,
	13105,
	26215,
	52428,
	104858,
	209715,
	419429,
	838861,
	1677719,
	3355443,
	6710879,
	13421772,
	26843538,
	53687080
};

// These values is the highest number of values before the
// hash table grows to the next size (and is rehashed)

// It is the size * 0.70, rounded down to the nearest integer.

const UINT32 maximumNrOfElements[] = {
	21,
	42,
	88,
	175,
	356,
	714,
	1427,
	2865,
	5733,
	11466,
	22924,
	45864,
	81749,
	183497,
	367000,
	734001,
	1468000,
	2936010,
	5872015,
	11744049,
	23488075,
	46976201,
	93952382,
	187904779
};

//---------------------------------------------------------------------//
//----- PointerHashFunctions ------------------------------------------//
//---------------------------------------------------------------------//

UINT32 PointerHashFunctions::Hash(const void* key)
{
	return (UINTPTR)key;
}

BOOL PointerHashFunctions::KeysAreEqual(const void* key1, const void* key2)
{
	return key1 == key2;
}

PointerHashFunctions g_pointer_hash_functions;

//---------------------------------------------------------------------//
//----- OpGenericStringHashTable ------------------------------------------//
//---------------------------------------------------------------------//
/* static */
UINT32 OpGenericStringHashTable::HashString(const uni_char* str, BOOL case_sensitive)
{
	/* Hash: djb2 This algorithm was first reported by Dan
	* Bernstein many years ago in comp.lang.c.  It is easily
	* implemented and has relatively good statistical properties.
	*/
	UINT32 hashval = 5381;
	if (case_sensitive)
	{
		while (*str)
		{
			hashval = ((hashval << 5) + hashval) + *str++; // hash*33 + c
		}
	}
	else
	{
		while (*str)
		{
			uni_char c = *str++;
			// 99.9% of all keys are ascii and the function call
			// overhead was 75% of the time in this function
			if (c < 128)
			{
				if (c >= 'a' && c <= 'z')
				{
					c -= 0x20; // 'A'-'a'
				}
			}
			else
			{
				c = uni_toupper(c);
			}
			hashval = ((hashval << 5) + hashval) + c; // hash*33 + c
		}
	}
	return hashval;
}

UINT32 OpGenericStringHashTable::Hash(const void* key)
{
	const uni_char* str = static_cast<const uni_char*>(key);

	return OpGenericStringHashTable::HashString(str, m_case_sensitive);
}

BOOL OpGenericStringHashTable::KeysAreEqual(const void* key1, const void* key2)
{
	const uni_char* string1 = (const uni_char*) key1;
	const uni_char* string2 = (const uni_char*) key2;

	if (m_case_sensitive)
	{
		return uni_str_eq(string1, string2);
	}
	else
	{
		return uni_stri_eq(string1, string2);
	}
}

/* static */
UINT32 OpGenericString8HashTable::HashString(const char* str, BOOL case_sensitive)
{
	/* Hash: djb2 This algorithm was first reported by Dan
	* Bernstein many years ago in comp.lang.c.  It is easily
	* implemented and has relatively good statistical properties.
	*/
	UINT32 hashval = 5381;
	if (case_sensitive)
	{
		while (*str)
		{
			hashval = ((hashval << 5) + hashval) + *str++; // hash*33 + c
		}
	}
	else
	{
		while (*str)
		{
			char c = *str++;
			if (c >= 'a' && c <= 'z')
				c &= 0xDF;

			hashval = ((hashval << 5) + hashval) + c; // hash*33 + c
		}
	}
	return hashval;
}

UINT32 OpGenericString8HashTable::Hash(const void* key)
{
	const char* str = static_cast<const char*>(key);

	return OpGenericString8HashTable::HashString(str, m_case_sensitive);
}

BOOL OpGenericString8HashTable::KeysAreEqual(const void* key1, const void* key2)
{
	const char* string1 = (const char*) key1;
	const char* string2 = (const char*) key2;

	if (m_case_sensitive)
	{
		return !op_strcmp(string1, string2);
	}
	else
	{
		return !op_stricmp(string1, string2);
	}
}

//---------------------------------------------------------------------//
//----- ChainedHashIterator -------------------------------------------//
//---------------------------------------------------------------------//

ChainedHashIterator::ChainedHashIterator(const ChainedHashBackend* backend) : backend(backend),
																		key(NULL),
																		data(NULL),
																		hash_link_pos(-1)
{
}

OP_STATUS ChainedHashIterator::First()
{
	return backend->First(this);
}

OP_STATUS ChainedHashIterator::Next()
{
	return backend->Next(this);
}

const void* ChainedHashIterator::GetKey() const
{
	OP_ASSERT(hash_link_pos >= 0);
	return key;
}

void* ChainedHashIterator::GetData() const
{
	OP_ASSERT(hash_link_pos >= 0);
	return data;
}

void ChainedHashIterator::SetKeyAndData(const void* key, void* data)
{
	this->key = key;
	this->data = data;
}

void ChainedHashIterator::SetHashLinkPos(INT32 pos)
{
	hash_link_pos = pos;
}

INT32 ChainedHashIterator::GetHashLinkPos() const
{
	return hash_link_pos;
}

//---------------------------------------------------------------------//
//----- ChainedHashBackend --------------------------------------------//
//---------------------------------------------------------------------//

ChainedHashBackend::ChainedHashBackend(OpHashFunctions* hashFunctions) : HashBackend(hashFunctions),
																		 table_size(0),
																		 nr_of_hash_links(0),
																		 hash_array(NULL),
																		 hash_links(NULL),
																		 first_free_link(NULL)
{
}

ChainedHashBackend::~ChainedHashBackend()
{
#ifdef _DEBUG
	// analyze hashtable performance
	int max_length = 0;
	int used_slots = 0;
	for (unsigned int i = 0; i < table_size; i++)
	{
		int len = 0;
		ChainedHashLink* curr_link = hash_array[i];
		if (curr_link)
		{
			used_slots++;
		}
		while (curr_link != NULL)
		{
			len++;
			curr_link = curr_link->next;
		}
		max_length = MAX(max_length, len);
//		OP_ASSERT(len <= 7); // bad hash function
	}
#endif // _DEBUG
	OP_DELETEA(hash_array);
	OP_DELETEA(hash_links);
}

OP_STATUS ChainedHashBackend::Init(UINT32 array_size, UINT32 max_nr_of_elements)
{
	OP_ASSERT(max_nr_of_elements > 0 && array_size > 0);
	hash_array = OP_NEWA(ChainedHashLink*, array_size);
	if (hash_array == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	for (UINT32 i = 0; i < array_size; i++)
	{
		hash_array[i] = NULL;
	}
	hash_links = OP_NEWA(ChainedHashLink, max_nr_of_elements);
	if (hash_links == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	for (UINT32 j = 0; j < max_nr_of_elements; j++)
	{
		hash_links[j].key = NULL;
		hash_links[j].data = NULL;
		hash_links[j].next = &hash_links[j + 1];
		hash_links[j].used = FALSE;
	}
	hash_links[max_nr_of_elements - 1].next = NULL;
	first_free_link = &hash_links[0];

	table_size = array_size;
	nr_of_hash_links = max_nr_of_elements;

	return OpStatus::OK;
}

OP_STATUS ChainedHashBackend::Add(const void* key, void* data)
{
	UINT32 array_pos = Hash(key, table_size);
	ChainedHashLink* link;
	ChainedHashLink* prev_link;
	BOOL has_elm = FindElm(array_pos, key, &link, &prev_link);
	if (has_elm)
	{
		return OpStatus::ERR;
	}
	link = GetNewLink();
	link->key = key;
	link->data = data;
	if (prev_link != NULL)
	{
		prev_link->next = link;
	}
	else
	{
		hash_array[array_pos] = link;
	}
	return OpStatus::OK;
}

OP_STATUS ChainedHashBackend::GetData(const void* key, void** data) const
{
	UINT32 array_pos = Hash(key, table_size);
	ChainedHashLink* link;
	ChainedHashLink* prev_link;
	BOOL has_elm = FindElm(array_pos, key, &link, &prev_link);
	if (!has_elm)
	{
		return OpStatus::ERR;
	}
	OP_ASSERT(link != NULL);
	*data = link->data;
	return OpStatus::OK;
}

OP_STATUS ChainedHashBackend::Remove(const void* key, void** data)
{
	UINT32 array_pos = Hash(key, table_size);
	ChainedHashLink* link;
	ChainedHashLink* prev_link;
	BOOL has_elm = FindElm(array_pos, key, &link, &prev_link);
	if (!has_elm)
	{
		return OpStatus::ERR;
	}
	OP_ASSERT(link != NULL);
	*data = link->data;
	if (prev_link)
	{
		prev_link->next = link->next;
	}
	else
	{
		hash_array[array_pos] = link->next;
	}
	FreeLink(link);
	return OpStatus::OK;
}

OP_STATUS ChainedHashBackend::First(ChainedHashIterator* iterator) const
{
	return Iterate(iterator, 0);
}

OP_STATUS ChainedHashBackend::Next(ChainedHashIterator* iterator) const
{
	INT32 pos = iterator->GetHashLinkPos();
	if (pos < 0)
	{
		return OpStatus::ERR;
	}
	return Iterate(iterator, (UINT32)(pos + 1));
}

OP_STATUS ChainedHashBackend::Iterate(ChainedHashIterator* iterator, UINT32 start_pos) const
{
	for (UINT32 i = start_pos; i < nr_of_hash_links; i++)
	{
		if (hash_links[i].used)
		{
			iterator->SetKeyAndData(hash_links[i].key, hash_links[i].data);
			iterator->SetHashLinkPos(i);
			return OpStatus::OK;
		}
	}
	iterator->SetKeyAndData(NULL, NULL);
	iterator->SetHashLinkPos(-1);
	return OpStatus::ERR;
}


OpHashIterator* ChainedHashBackend::GetIterator() const
{
	return OP_NEW(ChainedHashIterator, (this));
}

void ChainedHashBackend::ForEach(void (*function)(const void *, void *))
{
	for (UINT32 i = 0; i < nr_of_hash_links; i++)
		if (hash_links[i].used)
			function(hash_links[i].key, hash_links[i].data);
}

void ChainedHashBackend::ForEach(void (*function)(const void *, void *, OpHashTable *), OpHashTable* table)
{
	for (UINT32 i = 0; i < nr_of_hash_links; i++)
		if (hash_links[i].used)
			function(hash_links[i].key, hash_links[i].data, table);
}

OP_STATUS ChainedHashBackend::CopyAllToVector(OpGenericVector& vector) const
{
	for (UINT32 i = 0; i < nr_of_hash_links; i++)
	{
		if (hash_links[i].used)
		{
			if (vector.Add(hash_links[i].data) != OpStatus::OK)
				return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}

OP_STATUS ChainedHashBackend::CopyAllToHashTable(OpHashTable& hash_table) const
{
	for (UINT32 i = 0; i < nr_of_hash_links; i++)
	{
		if (hash_links[i].used)
		{
			if (hash_table.Contains(hash_links[i].key, hash_links[i].data))
				continue;

			if (hash_table.Add(hash_links[i].key, hash_links[i].data) != OpStatus::OK)
				return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}

UINT32 ChainedHashBackend::Hash(const void* key, UINT32 tableSize) const
{
	UINT32 hashValue = hash_functions->Hash(key);
	return hashValue % tableSize;
}

ChainedHashLink* ChainedHashBackend::GetNewLink()
{
	OP_ASSERT(first_free_link != NULL);
	ChainedHashLink* link = first_free_link;
	first_free_link = first_free_link->next;
	link->used = TRUE;
	link->next = NULL;
	return link;
}

void ChainedHashBackend::FreeLink(ChainedHashLink* link)
{
	link->next = first_free_link;
	link->used = FALSE;
	link->key = NULL;
	link->data = NULL;
	first_free_link = link;
}

BOOL ChainedHashBackend::FindElm(UINT32 array_pos, const void* key, ChainedHashLink** link, ChainedHashLink** prev_link) const
{
	*link = NULL;
	*prev_link = NULL;
	ChainedHashLink* curr_link = hash_array[array_pos];
	while (curr_link != NULL)
	{
		if (hash_functions->KeysAreEqual(curr_link->key, key))
		{
			*link = curr_link;
			return TRUE;
		}
		*prev_link = curr_link;
		curr_link = curr_link->next;
	}
	return FALSE;
}

//---------------------------------------------------------------------//
//----- OpHashTable ---------------------------------------------------//
//---------------------------------------------------------------------//

OpHashTable::OpHashTable(OpHashFunctions* hashFunctions,
						 BOOL useChainedHashing) : hash_functions(hashFunctions),
												   hash_backend(NULL),
												   nr_of_elements(0),
												   minimum_nr_of_elements(0),
												   hash_size_index(0),
												   flags(0)
{
	SetChained(useChainedHashing);
}

OpHashTable::~OpHashTable()
{
	OP_DELETE(hash_backend);
}

HashBackend* OpHashTable::CreateBackend(UINT16 size_index)
{
	HashBackend* backend = NULL;
	if (IsChained())
	{
		backend = OP_NEW(ChainedHashBackend, (hash_functions));
	}
	else
	{
		// FIXME:HASHTABLE-KILSMO
		backend = OP_NEW(ChainedHashBackend, (hash_functions));
	}
	if (backend == NULL)
	{
		return NULL;
	}
	OP_STATUS ret_val = backend->Init(hashTableSizes[size_index], maximumNrOfElements[size_index]);
	if (OpStatus::IsError(ret_val))
	{
		OP_DELETE(backend);
		return NULL;
	}
	return backend;
}

OP_STATUS OpHashTable::Init()
{
	if (hash_functions == NULL)
	{
		hash_functions = &g_pointer_hash_functions;
	}
	hash_backend = CreateBackend(hash_size_index);
	if (hash_backend == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	SetInitialized(TRUE);
	return OpStatus::OK;
}

void OpHashTable::RemoveAll()
{
	if (IsInitialized())
	{
		OP_DELETE(hash_backend);
		hash_backend = NULL;
		nr_of_elements = 0;
		hash_size_index = 0;
		SetInitialized(FALSE);
	}
}

OP_STATUS OpHashTable::Add(const void* key, void* data)
{
	if (!IsInitialized())
	{
		RETURN_IF_ERROR(Init());
	}

	if (nr_of_elements >= maximumNrOfElements[hash_size_index])
	{
		if (hash_size_index + 1 >= NR_OF_HASH_TABLE_SIZES)
		{
			// Cannot grow the table more, it is huge already!
			return OpStatus::ERR_NO_MEMORY;
		}
		RETURN_IF_ERROR(Rehash(hash_size_index + 1));
		hash_size_index++;
	}

	RETURN_IF_ERROR(hash_backend->Add(key, data));
	nr_of_elements++;
	return OpStatus::OK;
}

void OpHashTable::AddL(const void* key, void* data)
{
	LEAVE_IF_ERROR(Add(key, data));
}

OP_STATUS OpHashTable::GetData(const void* key, void** data) const
{
	OP_ASSERT(data != NULL);
	*data = NULL;

	if (!IsInitialized())
	{
		return OpStatus::ERR;
	}
	return hash_backend->GetData(key, data); // Never returns ERR_NO_MEMORY.
}

OpHashIterator*	OpHashTable::GetIterator()
{
	if (!IsInitialized())
	{
		OP_STATUS ret_val = Init();
		if (OpStatus::IsError(ret_val))
		{
			return NULL;
		}
	}
	return hash_backend->GetIterator();
}

void OpHashTable::ForEach(void (*function)(const void *, void *))
{
	if (IsInitialized())
		hash_backend->ForEach(function);
}

void OpHashTable::ForEach(void (*function)(const void *, void *, OpHashTable *))
{
	if (IsInitialized())
		hash_backend->ForEach(function, this);
}

OP_STATUS OpHashTable::CopyAllToVector(OpGenericVector& vector) const
{
	if (IsInitialized())
		return hash_backend->CopyAllToVector(vector);

	return OpStatus::OK;
}

OP_STATUS OpHashTable::CopyAllToHashTable(OpHashTable& hash_table) const
{
	if (IsInitialized())
		return hash_backend->CopyAllToHashTable(hash_table);

	return OpStatus::OK;
}

void OpHashTable::DeleteFunc(const void* /*key*/, void *data, OpHashTable* table)
{
	table->Delete(data);
}

void OpHashTable::DeleteAll()
{
	ForEach(DeleteFunc);
	RemoveAll();
}

OP_STATUS OpHashTable::Remove(const void* key, void** data)
{
	OP_ASSERT(data != NULL);
	*data = NULL;

	if (!IsInitialized())
	{
		return OpStatus::ERR;
	}

	if (nr_of_elements <= minimumNrOfElements[hash_size_index] && hash_size_index > 0 && minimumNrOfElements[hash_size_index - 1] >= minimum_nr_of_elements)
	{
		OP_STATUS ret_val = Rehash(hash_size_index - 1);
		if (OpStatus::IsSuccess(ret_val))
		{
				hash_size_index--;
		}
	}

	RETURN_IF_ERROR(hash_backend->Remove(key, data)); // Never returns ERR_NO_MEMORY.
	nr_of_elements--;
	return OpStatus::OK;
}

void OpHashTable::SetMinimumCount(UINT32 minimum_count)
{
	minimum_nr_of_elements = minimum_count;

	// find desired hash_size_index

	INT32 i=0;

	for (; i < NR_OF_HASH_TABLE_SIZES; i++)
	{
		if (minimumNrOfElements[i] >= minimum_nr_of_elements)
			break;
	}

	// if hashtable is already as big or bigger, or if we cannot grow, return now

	if (i <= hash_size_index || Rehash(i) != OpStatus::OK)
		return;

	hash_size_index = i;
}

OP_STATUS OpHashTable::Rehash(UINT16 size_index)
{
	if (!IsInitialized())
		return OpStatus::OK;

	HashBackend* backend_copy = CreateBackend(size_index);
	if (backend_copy == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OpHashIterator* iterator = hash_backend->GetIterator();
	if (iterator == NULL)
	{
		OP_DELETE(backend_copy);
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS ret_val = iterator->First();
	while (OpStatus::IsSuccess(ret_val))
	{
		OP_STATUS dummy_ret = backend_copy->Add(iterator->GetKey(), iterator->GetData());
		if (dummy_ret == OpStatus::ERR_NO_MEMORY)
			{
				OP_DELETE(backend_copy);
				OP_DELETE(iterator);
				return OpStatus::ERR_NO_MEMORY;
			}
		OP_ASSERT(OpStatus::IsSuccess(dummy_ret));
		ret_val = iterator->Next();
	}

	OP_DELETE(iterator);

	OP_DELETE(hash_backend);
	hash_backend = backend_copy;

	return OpStatus::OK;
}

BOOL OpHashTable::IsInitialized() const
{
	return (flags & INITIALIZED_FLAG) != 0;
}

BOOL OpHashTable::IsChained() const
{
	return (flags & CHAINED_FLAG) != 0;
}

void OpHashTable::SetInitialized(BOOL initialized)
{
	if (initialized)
	{
		flags |= INITIALIZED_FLAG;
	}
	else
	{
		flags &= ~INITIALIZED_FLAG;
	}
}

void OpHashTable::SetChained(BOOL chained)
{
	if (chained)
	{
		flags |= CHAINED_FLAG;
	}
	else
	{
		flags &= ~CHAINED_FLAG;
	}
}

