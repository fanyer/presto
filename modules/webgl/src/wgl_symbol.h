/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 */
#ifndef WGL_SYMBOL_H
#define WGL_SYMBOL_H

#include "modules/webgl/src/wgl_base.h"
#include "modules/util/simset.h"

class WGL_Identifier_Array
{
public:
    static WGL_Identifier_Array *Make(WGL_Context *context, unsigned size, unsigned nused = 0);

    unsigned nused;
    WGL_String *identifiers[1];

private:
    static void Initialize(WGL_Identifier_Array *array, unsigned nused);
};


class WGL_IdentifierCell_Array
{
public:
    static WGL_IdentifierCell_Array *Make(WGL_Context *context, unsigned size);

    unsigned size;

    class Cell
    {
    public:
        /** First part of the hash table key. */
        WGL_String *key;
        /** The hash-table value + extra key bits. [0-7] extra key bits, [8-31] value */
        unsigned value_with_extra;
    };

    Cell cells[1];

private:
    static void Initialize(WGL_IdentifierCell_Array *array, unsigned size);
};

class WGL_Identifier_List
    : public ListElement<WGL_Identifier_List>
{
public:
    static WGL_Identifier_List *Make(WGL_Context *context, unsigned size);
    /**< Create a hashed list of identifiers. */

    BOOL AppendL(WGL_Context *context, WGL_String *element, unsigned &position, BOOL hide_existing = FALSE);
    /**< Append identifier to the list, write the index to position. If
         the list already contains the identifier AppendL returns FALSE; */

    BOOL IndexOf(WGL_String *element, unsigned &index);
    /**< Writes the index of the element to index if it exists. Returns FALSE
         if the list does not contain the identifier. */

    BOOL IndexOf(const uni_char *string, unsigned length, unsigned &index);
    /**< Writes the index of the element to index if it exists. Returns FALSE
         if the list does not contain the identifier. */

    BOOL Lookup(unsigned index, WGL_String *&element);
    /**< Looks up which element reside at the given index. Returns FALSE
         if the index is out of bounds or the element is hidden. The latter
         only happens if the list is a WGL_Identifier_Mutable_List. */

    WGL_String *GetIdentifierAtIndex(unsigned index)
    {
        return identifiers->identifiers[index];
    }

    unsigned GetCount() { return Used(); }
    /**< @return the number of elements in the list */

    WGL_String **GetIdentifiers() { return identifiers->identifiers; }

    void Rehash();

protected:
    static void Initialize(WGL_Identifier_List *list);

    void ResizeL(WGL_Context *context);

    inline unsigned &Used() { return identifiers->nused; }
    inline unsigned &Allocated() { return nallocated; }

    unsigned nallocated;
    unsigned *indices;
    WGL_Identifier_Array *identifiers;
};

class WGL_Identifier_Mutable_List
    : public WGL_Identifier_List
{
public:
    void Remove(unsigned index);
    /**< Remove an element from the hash list. Space will be reclaimed when re-sizing/hashing */
};

class WGL_Identifier_Hash_Table
{
public:
    static WGL_Identifier_Hash_Table *Make(WGL_Context *context, unsigned size);
    /**< Create a hash table of specified size. */

    BOOL AddL(WGL_Context *context, WGL_String *key, unsigned value, unsigned key_extra_bits = 0);
    /**< Add the key with value to the hash table unless the key already
         exists in the table. Leaves on OOM. */

    BOOL Find(WGL_String *key, unsigned &value, unsigned key_extra_bits = 0);
    /**< Find the value associated with key. Returns true if found, false
         otherwise. Writes the result to value. */

    BOOL Remove(WGL_String *key, unsigned &value, unsigned key_extra_bits = 0);
    /**< Removes key from hash table, returns true if successful and writes
         the value associated with the key to value. */

    BOOL Remove(WGL_String *key, unsigned key_extra_bits = 0);
    /**< Removes key from hash table, returns true if successful. */

    unsigned GetCount() { return nused; }

protected:
    static void Initialize(WGL_Identifier_Hash_Table *table);

    void ResizeL(WGL_Context *context);

    unsigned nused, ndeleted;
    WGL_IdentifierCell_Array *cells;
};

#endif // WGL_SYMBOL_H
