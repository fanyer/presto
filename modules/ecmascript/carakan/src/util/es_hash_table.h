/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_HASH_TABLE_H
#define ES_HASH_TABLE_H

#include "modules/ecmascript/carakan/src/util/es_util.h"

class ES_Box;

class ES_Identifier_Array : public ES_Boxed
{
public:
    static ES_Identifier_Array *Make(ES_Context *context, unsigned size, unsigned nused = 0);

    unsigned nused;

    JString *identifiers[1];

private:
    static void Initialize(ES_Identifier_Array *array, unsigned nused);
};

class ES_IdentifierCell_Array : public ES_Boxed
{
public:
    static ES_IdentifierCell_Array *Make(ES_Context *context, unsigned size);

    unsigned size;

    class Cell
    {
    public:
        JString *key;
        unsigned key_fragment;
        unsigned value;
    };

    Cell cells[1];

private:
    static void Initialize(ES_IdentifierCell_Array *array, unsigned size);
};

class ES_Identifier_List : public ES_Boxed
{
public:
    static ES_Identifier_List *Make(ES_Context *context, unsigned size);
    /**< Create a hashed list of identifiers.
    */

    static void Free(ES_Context *context, ES_Identifier_List *list);
    /**< Release the memory allocated by this list (but not the identifiers
         referenced by it.)  Use with care, obviously.  Used to release
         temporary hash tables allocated by the compiler. */

    BOOL AppendL(ES_Context *context, JString *element, unsigned &position, BOOL hide_existing = FALSE);
    /**< Append identifier to the list, write the index to position. If
         the list already contains the identifier AppendL returns FALSE;
    */

    void AppendAtIndexL(ES_Context *context, JString *element, unsigned index, unsigned &position);

    BOOL IndexOf(JString *element, unsigned &index);
    /**< Writes either the index of the element or the index of a free position it
         would have had if it had existed to |index|. Returns FALSE if the list
         does not contain the identifier.
    */

    BOOL Lookup(unsigned index, JString *&element);
    /**< Looks up which element reside at the given index. Returns FALSE
         if the index is out of bounds or the element is hidden. The latter
         only happens if the list is a ES_Identifier_Mutable_List.
    */

    JString *FindOrFindPositionFor(JString *element, unsigned& index);
    /**< Returns the JString in the list that matches element or NULL if none was found.
         The out variable |index| is set to either the element's index position or
         the position where the element could be inserted in case there
         was no match. See AppendAtIndexL().
    */

    JString *GetIdentifierAtIndex(unsigned index) { return identifiers->identifiers[index]; }

    ES_Identifier_List *CopyL(ES_Context *context, unsigned count);
    ES_Identifier_List *CopyL(ES_Context *context);
    /**< Return a newly allocated copy of the list.
    */

    unsigned GetCount() { return Used(); }
    /**< @return the number of elements in the list */

    JString **GetIdentifiers() { return identifiers->identifiers; }

    void Rehash();

protected:
    static void Initialize(ES_Identifier_List *list);

    void ResizeL(ES_Context *context);

    inline unsigned &Used() { return identifiers->nused; }
    inline unsigned &Allocated() { return nallocated; }
    friend class ESMM;

    unsigned nallocated;
    ES_Box *indices;
    ES_Identifier_Array *identifiers;
};

class ES_Identifier_Mutable_List : public ES_Identifier_List
{
public:
    void Remove(unsigned index);
    /**< Remove an element from the hash list. Space will be reclaimed when re-sizing/hashing
    */

    void RemoveLast(unsigned remove_index);
    /**< Drop last element appended to the hash list. List is contracted and updated in-place.
         Caller is responsible for ensuring that no intervening mutations have occurred.
    */
};

class ES_Identifier_Hash_Table : public ES_Boxed
{
public:
    static ES_Identifier_Hash_Table *Make(ES_Context *context, unsigned size);
    /**< Create a hash table of specified size.
    */

    static void Free(ES_Context *context, ES_Identifier_Hash_Table *table);
    /**< Release the memory allocated by this hash table (but not the strings
         referenced by it.) Used to carefully release temporary tables.
    */

    BOOL AddL(ES_Context *context, JString *key, unsigned value, unsigned key_extra_bits = 0);
    /**< Add the key with value to the hash table unless the key already
         exists in the table. Leaves on oom.
    */

    BOOL Contains(JString *key, unsigned key_extra_bits = 0);
    /**< Check if hash table contains key.
    */

    BOOL Find(JString *key, unsigned &value, unsigned key_extra_bits = 0);
    /**< Find the value associated with key. Returns true if found, false
         otherwise. Writes the result to value.
    */

    BOOL Remove(JString *key, unsigned &value, unsigned key_extra_bits = 0);
    /**< Removes key from hash table, returns true if successful and writes
         the value associated with the key to value.
    */

    BOOL Remove(JString *key, unsigned key_extra_bits = 0);
    /**< Removes key from hash table, returns true if successful.
    */

    ES_Identifier_Hash_Table *CopyL(ES_Context *context);
    /**< Return a newly allocated copy of the hash table.
    */

    unsigned GetCount() { return nused; }

#ifndef ES_DEBUG_COMPACT_OBJECTS
protected:
#endif // ES_DEBUG_COMPACT_OBJECTS
    static void Initialize(ES_Identifier_Hash_Table *table);

    void ResizeL(ES_Context *context);

    friend class ESMM;

    /*
    struct Cell
    {
        JString *key;
        unsigned value;
    };
    */

    unsigned nused, ndeleted;
    ES_IdentifierCell_Array *cells;
};

class ES_Property_Info;

class ES_Identifier_Boxed_Hash_Table : public ES_Identifier_Hash_Table
{
public:
    static ES_Identifier_Boxed_Hash_Table *Make(ES_Context *context, unsigned size);

    BOOL AddL(ES_Context *context, JString *key, unsigned key_extra_bits, ES_Boxed *value);
    inline BOOL Contains(JString *key, unsigned key_extra_bits) { return ES_Identifier_Hash_Table::Contains(key, key_extra_bits); }
    BOOL Find(JString *key, unsigned key_extra_bits, ES_Boxed *&value);
    BOOL FindLocation(JString *key, unsigned key_extra_bits, ES_Boxed **&value);

    inline ES_Boxed_Array *GetValues() const { return array; }
    unsigned GetCount() { return nused; }

#if !defined(ES_DEBUG_COMPACT_OBJECTS) && !defined(DEBUG_ENABLE_OPASSERT)
private:
#endif
    static void Initialize(ES_Identifier_Boxed_Hash_Table *table);

    friend class ESMM;

    ES_Boxed_Array *array;
};

#endif // ES_HASH_TABLE_H
