/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- symbol table.
 *
 */
#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_string.h"
#include "modules/webgl/src/wgl_symbol.h"
#include "modules/webgl/src/wgl_context.h"


#define WGL_ALLOCATE_WITH_EXTRA(context, object_ptr, size_in_bytes, TYPE, args) \
  do { \
    unsigned char *block = OP_NEWGROA_L(unsigned char, sizeof(TYPE) + size_in_bytes, context->Arena()); \
    object_ptr = (TYPE *)block; \
    Initialize args; \
  } while (0)

#define WGL_ALLOCATE(context, object_ptr, TYPE, args) \
  do { \
    object_ptr = (TYPE *)OP_NEWGROA_L(unsigned char, sizeof(TYPE), context->Arena()); \
    Initialize args; \
  } while (0)

#define WGL_NEWA_L(context, TYPE, size) OP_NEWGROA_L(TYPE, size, context->Arena());

static inline unsigned doubleHash (unsigned key)
{
  key = ~key + (key >> 23);
  key ^= (key << 12);
  key ^= (key >> 7);
  key ^= (key << 2);
  key ^= (key >> 20);
  return key;
}

#define DECLARE() unsigned index = hash, jump = 1 | doubleHash (hash);
#define NEXT() index += jump;
#define TOMB_MARKER 0xFFFFFFFF
#define FREE_MARKER 0xFFFFFFFE

static unsigned
ComputeSize(unsigned size)
{
    unsigned candidate_size = 4;
    while (candidate_size < size)
        candidate_size *= 2;
    return candidate_size;
}

/* static */ WGL_Identifier_Array *
WGL_Identifier_Array::Make(WGL_Context *context, unsigned size, unsigned nused)
{
    WGL_Identifier_Array *array;

    WGL_ALLOCATE_WITH_EXTRA(context, array, (size - 1) * sizeof(WGL_String *), WGL_Identifier_Array, (array, nused));
    return array;
}

/* static */ void
WGL_Identifier_Array::Initialize(WGL_Identifier_Array *array, unsigned nused)
{
    array->nused = nused;
}

/* static */ WGL_IdentifierCell_Array *
WGL_IdentifierCell_Array::Make(WGL_Context *context, unsigned size)
{
    WGL_IdentifierCell_Array *array;
    WGL_ALLOCATE_WITH_EXTRA(context, array, (size - 1) * sizeof(Cell), WGL_IdentifierCell_Array, (array, size));
    return array;
}

/* static */ void
WGL_IdentifierCell_Array::Initialize(WGL_IdentifierCell_Array *array, unsigned size)
{
    array->size = size;
}

/* static */ WGL_Identifier_List *
WGL_Identifier_List::Make(WGL_Context *context, unsigned size)
{
    WGL_Identifier_List *list;
    WGL_ALLOCATE(context, list, WGL_Identifier_List, (list));

    size = ComputeSize(size);

    list->indices = WGL_NEWA_L(context, unsigned, size);
    list->identifiers = WGL_Identifier_Array::Make(context, size);
    list->nallocated  = size;

    unsigned *hashed = list->indices;
    WGL_String **identifiers = list->GetIdentifiers();
    for (unsigned i = 0; i < list->Allocated(); ++i)
    {
        hashed[i]      = FREE_MARKER;
        identifiers[i] = NULL;
    }

    return list;
}

BOOL
WGL_Identifier_List::AppendL(WGL_Context *context, WGL_String *element, unsigned &position, BOOL hide_existing)
{
    OP_ASSERT(element != NULL);
restart:
    unsigned hash = element->Hash(), mask = Allocated() - 1;
    DECLARE();
    unsigned *candidate = NULL;
    unsigned *hashed = indices;
    WGL_String **keys = GetIdentifiers();

    while (TRUE)
    {
        unsigned &entry = hashed[index & mask];

        if (entry == FREE_MARKER)
        {
        add:
            if (4 * Used() >= 3 * Allocated())
            {
                ResizeL(context);
                goto restart;
            }

            if (!candidate)
                candidate = &entry;

            *candidate   = position = Used();
            keys[Used()] = element;
            ++Used();
            return TRUE;
        }
        else if (!keys[entry])
            candidate = &entry;
        else if (keys[entry] == element || keys[entry]->Equals(element))
        {
            if (hide_existing)
            {
                /* Use the already existing identifier to ensure that a pointer
                   comparison is enough to compare them.  ResizeL depend on this
                   fact later. */
                element = keys[entry];

                candidate = &entry;
                goto add;
            }
            else
            {
                position = entry;
                return FALSE;
            }
        }
        NEXT();
    }
}

BOOL
WGL_Identifier_List::IndexOf(WGL_String *element, unsigned &i)
{
    OP_ASSERT(element != NULL);
    unsigned hash = element->Hash(), mask = Allocated() - 1;
    DECLARE();
    unsigned *hashed = indices;
    WGL_String **keys = GetIdentifiers();

    while (TRUE)
    {
        unsigned &entry = hashed[index & mask];

        if (entry == FREE_MARKER)
        {
            i = index & mask;
            return FALSE;
        }
        else if (keys[entry] == element || keys[entry] && keys[entry]->hash == hash && keys[entry]->Equals(element))
        {
            i = entry;
            return TRUE;
        }
        NEXT();
    }

    return FALSE;
}

BOOL
WGL_Identifier_List::IndexOf(const uni_char *string, unsigned length, unsigned &i)
{
    unsigned hash = WGL_String::CalculateHash(string, length);
    unsigned mask = Allocated() - 1;
    DECLARE();
    unsigned *hashed = indices;
    WGL_String **keys = GetIdentifiers();

    while (TRUE)
    {
        unsigned &entry = hashed[index & mask];

        if (entry == FREE_MARKER)
        {
            i = index & mask;
            return FALSE;
        }
        else if (keys[entry] && keys[entry]->hash == hash && keys[entry]->Equals(string, length))
        {
            i = entry;
            return TRUE;
        }
        NEXT();
    }

    return FALSE;
}

BOOL
WGL_Identifier_List::Lookup(unsigned index, WGL_String *&element)
{
    if (index >= Used())
        return FALSE;

    element = GetIdentifiers()[index];
    return TRUE;
}

static void
CopyIdentifiers(WGL_String **target, WGL_String **source, unsigned count)
{
    op_memcpy(target, source, count * sizeof(*target));
}

/* static */ void
WGL_Identifier_List::Initialize(WGL_Identifier_List *list)
{
    list->indices     = NULL;
    list->identifiers = NULL;
}

void
WGL_Identifier_List::ResizeL(WGL_Context *context)
{
    unsigned nused = Used();
    while (4 * nused >= 3 * nallocated)
        nallocated *= 2;
    while (4 * nused < nallocated)
        nallocated /= 2;

    unsigned *new_indices = WGL_NEWA_L(context, unsigned, nallocated * sizeof(unsigned));
    WGL_Identifier_Array *new_identifiers = WGL_Identifier_Array::Make(context, nallocated, Used());
    WGL_String **new_idents = new_identifiers->identifiers;

    CopyIdentifiers(new_idents, GetIdentifiers(), Used());

    unsigned *new_hashed_indices = new_indices;
    for (unsigned i = 0; i < nallocated; ++i)
        new_hashed_indices[i] = FREE_MARKER;

    WGL_String **item = new_idents;

    for (unsigned added = 0; added < nused; ++added, ++item)
    {
        // *item == NULL will allow us to handle Remove() in a mutable version
        if (*item == NULL)
            continue;

        unsigned hash = (*item)->Hash(), mask = nallocated - 1;
        DECLARE();

        while (TRUE)
        {
            unsigned &entry = new_hashed_indices[index & mask];

            if (entry == FREE_MARKER || new_idents[entry] == *item)
            {
                entry = added;
                break;
            }
            NEXT();
        }
    }

    indices = new_indices;
    identifiers = new_identifiers;
}

void
WGL_Identifier_List::Rehash()
{
    WGL_String **identifiers = GetIdentifiers(), **item = identifiers;

    unsigned nused = Used();
    unsigned nallocated = Allocated();
    Used() = 0;

    unsigned *hashed_indices = indices;
    for (unsigned i = 0; i < nallocated; ++i)
        hashed_indices[i] = FREE_MARKER;

    for (unsigned added = 0; added < nused; ++added, ++item)
    {
        if (*item == NULL)
            continue;

        unsigned hash = (*item)->Hash(), mask = nallocated - 1;
        DECLARE();

        while (TRUE)
        {
            unsigned &entry = hashed_indices[index & mask];

            if (entry == FREE_MARKER)
            {
                unsigned new_index = Used()++;
                entry = new_index;
                identifiers[new_index] = *item;
                break;
            }
            OP_ASSERT(identifiers[entry] != *item);
            NEXT();
        }
    }
}

void
WGL_Identifier_Mutable_List::Remove(unsigned index)
{
    OP_ASSERT(index < Used());
    WGL_String **identifiers = GetIdentifiers();
    identifiers[index] = NULL;
}

WGL_Identifier_Hash_Table *
WGL_Identifier_Hash_Table::Make(WGL_Context *context, unsigned size)
{
    WGL_Identifier_Hash_Table *table;
    WGL_ALLOCATE(context, table, WGL_Identifier_Hash_Table, (table));

    size = ComputeSize(size);

    table->cells = WGL_IdentifierCell_Array::Make(context, size);
    op_memset(table->cells->cells, 0, size * sizeof(WGL_IdentifierCell_Array::Cell));

    return table;
}

void
WGL_Identifier_Hash_Table::Initialize(WGL_Identifier_Hash_Table *table)
{
    table->nused = 0;
    table->ndeleted = 0;
    table->cells = NULL;
}

BOOL
WGL_Identifier_Hash_Table::AddL(WGL_Context *context, WGL_String *key, unsigned value, unsigned key_extra_bits)
{
restart:
    unsigned hash = key->Hash(), nallocated = cells->size, mask = nallocated - 1;
    DECLARE();
    WGL_IdentifierCell_Array::Cell *candidate = 0;
    WGL_IdentifierCell_Array::Cell *hashed = static_cast<WGL_IdentifierCell_Array::Cell *>(cells->cells);

    while (TRUE)
    {
        WGL_IdentifierCell_Array::Cell &entry = hashed[index & mask];

        if (entry.key == NULL && entry.value_with_extra != TOMB_MARKER)
        {
            if (!candidate)
            {
                if (4 * (nused + ndeleted) >= 3 * nallocated)
                {
                    ResizeL(context);
                    goto restart;
                }

                candidate = &entry;
            }
            else
            {
                --ndeleted;
            }
            ++nused;
            candidate->key = key;
            candidate->value_with_extra = (value << 8) | key_extra_bits;

            return TRUE;
        }
        else if (entry.key == NULL && entry.value_with_extra == TOMB_MARKER  && !candidate)
            candidate = &entry;
        else if ((entry.key == key || entry.key->Equals(key)) && (entry.value_with_extra & 0xFF) == key_extra_bits)
            return FALSE;

        NEXT();
    }
}

BOOL
WGL_Identifier_Hash_Table::Find(WGL_String *key, unsigned &value, unsigned key_extra_bits)
{
    unsigned hash = key->Hash(), mask = cells->size - 1;
    DECLARE();
    WGL_IdentifierCell_Array::Cell *hashed = static_cast<WGL_IdentifierCell_Array::Cell *>(cells->cells);

    while (TRUE)
    {
        WGL_IdentifierCell_Array::Cell &entry = hashed[index & mask];
        if (entry.key == NULL && entry.value_with_extra != TOMB_MARKER)
            return FALSE;

        if ((entry.key == key || entry.key->Equals(key)) && (entry.value_with_extra & 0xFF) == key_extra_bits)
        {
            value = entry.value_with_extra >> 8;
            return TRUE;
        }
        NEXT();
    }

    return FALSE;
}

BOOL
WGL_Identifier_Hash_Table::Remove(WGL_String *key, unsigned &value, unsigned key_extra_bits)
{
    unsigned hash = key->Hash(), mask = cells->size - 1;
    DECLARE();
    WGL_IdentifierCell_Array::Cell *hashed = static_cast<WGL_IdentifierCell_Array::Cell *>(cells->cells);

    while (TRUE)
    {
        WGL_IdentifierCell_Array::Cell &entry = hashed[index & mask];

        if (entry.key == NULL && entry.value_with_extra != TOMB_MARKER)
            return FALSE;
        else if ((entry.key == key || entry.key->Equals(key)) && (entry.value_with_extra & 0xFF) == key_extra_bits)
        {
            value = entry.value_with_extra >> 8;
            entry.key = NULL;
            entry.value_with_extra = TOMB_MARKER;
            --nused;
            ++ndeleted;
            return TRUE;
        }
        NEXT();
    }
}

BOOL
WGL_Identifier_Hash_Table::Remove(WGL_String *key, unsigned key_extra_bits)
{
    unsigned value;
    return Remove(key, value, key_extra_bits);
}

void
WGL_Identifier_Hash_Table::ResizeL(WGL_Context *context)
{
    unsigned nallocated = cells->size;

    while (4 * nused >= 3 * nallocated)
        nallocated *= 2;
    while (4 * nused < nallocated)
        nallocated /= 2;

    WGL_IdentifierCell_Array *new_cells = WGL_IdentifierCell_Array::Make(context, nallocated);
    WGL_IdentifierCell_Array::Cell *new_hashed = new_cells->cells;

    for (unsigned i = 0; i < nallocated; ++i)
    {
        new_hashed[i].key = NULL;
        new_hashed[i].value_with_extra = 0;
    }

    WGL_IdentifierCell_Array::Cell *item = cells->cells;
    for (unsigned added = 0; added < nused; ++added)
    {
        while (item->key == NULL)
            ++item;

        unsigned hash = item->key->Hash(), mask = nallocated - 1;
        DECLARE();

        while (TRUE)
        {
            WGL_IdentifierCell_Array::Cell &entry = new_hashed[index & mask];

            if (entry.key == NULL)
            {
                entry.key = item->key;
                entry.value_with_extra = item->value_with_extra;
                item++;
                break;
            }
            NEXT();
        }
    }
    op_memset(cells->cells, 0, cells->size * sizeof(WGL_IdentifierCell_Array::Cell));
    cells = new_cells;
    ndeleted = 0;
}

#undef DECLARE
#undef NEXT
#undef TOMB_MARKER
#undef FREE_MARKER

#endif // CANVAS3D_SUPPORT
