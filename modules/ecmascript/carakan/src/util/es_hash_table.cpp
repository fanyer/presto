/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/kernel/es_box.h"
#include "modules/ecmascript/carakan/src/util/es_hash_table.h"

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

static unsigned ComputeSize(unsigned size)
{
    unsigned candidate_size = 4;
    while (candidate_size < size)
        candidate_size *= 2;
    return candidate_size;
}

/* static */
ES_Identifier_Array *ES_Identifier_Array::Make(ES_Context *context, unsigned size, unsigned nused)
{
    ES_Identifier_Array *array;
    GC_ALLOCATE_WITH_EXTRA(context, array, (size - 1) * sizeof(JString *), ES_Identifier_Array, (array, nused));
    return array;
}

/* static */
void ES_Identifier_Array::Initialize(ES_Identifier_Array *array, unsigned nused)
{
    array->InitGCTag(GCTAG_ES_Identifier_Array);
    array->nused = nused;
}

/* static */
ES_IdentifierCell_Array *ES_IdentifierCell_Array::Make(ES_Context *context, unsigned size)
{
    ES_IdentifierCell_Array *array;
    GC_ALLOCATE_WITH_EXTRA(context, array, (size - 1) * sizeof(Cell), ES_IdentifierCell_Array, (array, size));
    return array;
}

/* static */
void ES_IdentifierCell_Array::Initialize(ES_IdentifierCell_Array *array, unsigned size)
{
    array->InitGCTag(GCTAG_ES_IdentifierCell_Array);
    array->size = size;
}

/* JString_Hash_List */

/* static */
ES_Identifier_List *ES_Identifier_List::Make(ES_Context *context, unsigned size)
{
    ES_Identifier_List *list;
    GC_ALLOCATE(context, list, ES_Identifier_List, (list));
    ES_CollectorLock gclock(context);

    size = ComputeSize(size);

    list->indices     = ES_Box::Make(context, size * sizeof(unsigned));
    list->identifiers = ES_Identifier_Array::Make(context, size);
    list->nallocated  = size;

    unsigned *hashed            = reinterpret_cast<unsigned*>(list->indices->Unbox());
    JString **identifiers = list->GetIdentifiers();
    for (unsigned i = 0; i < list->Allocated(); ++i)
    {
        hashed[i]      = FREE_MARKER;
        identifiers[i] = NULL;
    }

    return list;
}

/* static */ void
ES_Identifier_List::Free(ES_Context *context, ES_Identifier_List *list)
{
    ES_Heap *heap = context->heap;

    heap->Free(list->indices);
    heap->Free(list->identifiers);
    heap->Free(list);
}

BOOL ES_Identifier_List::AppendL(ES_Context *context, JString *element, unsigned &position, BOOL hide_existing)
{
    OP_ASSERT(element != NULL);
restart:
    unsigned hash = element->Hash(), mask = Allocated() - 1;
    DECLARE();
    unsigned *candidate = NULL;
    unsigned *hashed = reinterpret_cast<unsigned*>(indices->Unbox());
    JString **keys = GetIdentifiers();

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


        NEXT();
    }
}

void ES_Identifier_List::AppendAtIndexL(ES_Context *context, JString *element, unsigned index, unsigned &position)
{
    unsigned *hashed = reinterpret_cast<unsigned *>(indices->Unbox());
    JString **keys = GetIdentifiers();

    OP_ASSERT(hashed[index] == FREE_MARKER);

    if (4 * Used() < 3 * Allocated())
    {
        hashed[index] = Used();
        keys[Used()] = element;

        position = Used()++;
    }
    else
        AppendL(context, element, position);
}

BOOL ES_Identifier_List::IndexOf(JString *element, unsigned &i)
{
    OP_ASSERT(element != NULL);
    unsigned hash = element->Hash(), mask = Allocated() - 1;
    DECLARE();
    unsigned *hashed = reinterpret_cast<unsigned *>(indices->Unbox());
    JString **keys = GetIdentifiers();

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

BOOL ES_Identifier_List::Lookup(unsigned index, JString *&element)
{
    if (index >= Used())
        return FALSE;

    element = GetIdentifiers()[index];
    return TRUE;
}

JString *ES_Identifier_List::FindOrFindPositionFor(JString *element, unsigned &index)
{
    if (IndexOf(element, index))
        return GetIdentifiers()[index];
    return NULL;
}

static void CopyIdentifiers(JString **target, JString **source, unsigned count)
{
    op_memcpy(target, source, count * sizeof(*target));
}

ES_Identifier_List *ES_Identifier_List::CopyL(ES_Context *context)
{
    ES_Identifier_List *new_copy = ES_Identifier_List::Make(context, Allocated());
    op_memcpy(new_copy->indices->Unbox(), indices->Unbox(), Allocated() * sizeof(unsigned));

    CopyIdentifiers(new_copy->GetIdentifiers(), GetIdentifiers(), Used());

    new_copy->Used() = Used();
    return new_copy;
}

ES_Identifier_List *ES_Identifier_List::CopyL(ES_Context *context, unsigned count)
{
    OP_ASSERT(count <= Used());
    ES_Identifier_List *new_copy = ES_Identifier_List::Make(context, Allocated());
    CopyIdentifiers(new_copy->GetIdentifiers(), GetIdentifiers(), count);
    new_copy->Used() = count;
    new_copy->Rehash();

    return new_copy;
}

/* static */
void ES_Identifier_List::Initialize(ES_Identifier_List *list)
{
    list->InitGCTag(GCTAG_ES_Identifier_List);

    list->indices     = NULL;
    list->identifiers = NULL;
}

void ES_Identifier_List::ResizeL(ES_Context *context)
{
    unsigned nused = Used();
    unsigned new_nallocated = nallocated;

    while (4 * nused >= 3 * new_nallocated)
        new_nallocated *= 2;
    while (4 * nused < new_nallocated)
        new_nallocated /= 2;

    ES_Box *new_indices                  = ES_Box::Make(context, new_nallocated * sizeof(unsigned));
    ES_CollectorLock gclock(context);
    ES_Identifier_Array *new_identifiers = ES_Identifier_Array::Make(context, new_nallocated, Used());
    JString **new_idents           = new_identifiers->identifiers;

    CopyIdentifiers(new_idents, GetIdentifiers(), Used());

    unsigned *new_hashed_indices = reinterpret_cast<unsigned *>(new_indices->Unbox());
    for (unsigned i = 0; i < new_nallocated; ++i)
        new_hashed_indices[i] = FREE_MARKER;

    JString **item = new_idents;

    for (unsigned added = 0; added < nused; ++added, ++item)
    {
        // *item == NULL will allow us to handle Remove() in a mutable version
        if (*item == NULL)
            continue;

        unsigned hash = (*item)->Hash(), mask = new_nallocated - 1;
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

    ES_Heap *heap = context->heap;

    heap->Free(indices);
    heap->Free(identifiers);

    indices     = new_indices;
    identifiers = new_identifiers;
    nallocated  = new_nallocated;
    return;
}


void ES_Identifier_List::Rehash()
{
    JString **identifiers = GetIdentifiers(), **item = identifiers;

    unsigned nused = Used();
    unsigned nallocated = Allocated();
    Used() = 0;

    unsigned *hashed_indices = reinterpret_cast<unsigned *>(indices->Unbox());
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

/* ES_Identifier_Mutable_List */

void ES_Identifier_Mutable_List::Remove(unsigned index)
{
    OP_ASSERT(index < Used());
    JString **identifiers = GetIdentifiers();
    identifiers[index] = NULL;
}

void ES_Identifier_Mutable_List::RemoveLast(unsigned remove_index)
{
    OP_ASSERT(remove_index == (Used() - 1));

    JString **identifiers = GetIdentifiers();
    unsigned nallocated = Allocated();
    unsigned *hashed_indices = reinterpret_cast<unsigned *>(indices->Unbox());

    unsigned hash = identifiers[remove_index]->Hash(), mask = nallocated - 1;
    DECLARE();
    while (TRUE)
    {
        unsigned &entry = hashed_indices[index & mask];

        if (entry == remove_index)
        {
            entry = FREE_MARKER;
            --Used();
            break;
        }
        NEXT();
    }
    identifiers[remove_index] = NULL;
}

/* ES_Identifier_Hash_Table */

ES_Identifier_Hash_Table *ES_Identifier_Hash_Table::Make(ES_Context *context, unsigned size)
{
    ES_Identifier_Hash_Table *table;
    GC_ALLOCATE(context, table, ES_Identifier_Hash_Table, (table));

    ES_CollectorLock gclock(context);

    size = ComputeSize(size);

    table->cells = ES_IdentifierCell_Array::Make(context, size);
    op_memset(table->cells->cells, 0, size * sizeof(ES_IdentifierCell_Array::Cell));

#ifdef DEBUG_ENABLE_OPASSERT
    for (unsigned i = 0; i < size; ++i)
    {
        OP_ASSERT(table->cells->cells[i].key == NULL);
    }
#endif
    return table;
}

void ES_Identifier_Hash_Table::Initialize(ES_Identifier_Hash_Table *table)
{
    table->InitGCTag(GCTAG_ES_Identifier_Hash_Table);

    table->nused = 0;
    table->ndeleted = 0;
    table->cells = NULL;
}

/* static */ void
ES_Identifier_Hash_Table::Free(ES_Context *context, ES_Identifier_Hash_Table *table)
{
    ES_Heap *heap = context->heap;

    heap->Free(table->cells);
    heap->Free(table);
}

BOOL ES_Identifier_Hash_Table::AddL(ES_Context *context, JString *key, unsigned value, unsigned key_extra_bits)
{
restart:
    unsigned hash = key->Hash(), nallocated = cells->size, mask = nallocated - 1;
    ES_HASH_UPDATE(hash, key_extra_bits);

    DECLARE();
    ES_IdentifierCell_Array::Cell *candidate = 0;
    ES_IdentifierCell_Array::Cell *hashed = (ES_IdentifierCell_Array::Cell *) cells->cells;

    while (TRUE)
    {
        ES_IdentifierCell_Array::Cell &entry = hashed[index & mask];

        if (entry.key == NULL && entry.value != TOMB_MARKER)
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
            candidate->value = value;
            candidate->key_fragment = key_extra_bits;

            return TRUE;
        }
        else if (entry.key == NULL && entry.value == TOMB_MARKER  && !candidate)
            candidate = &entry;
        else if ((entry.key == key || entry.key->Equals(key)) && entry.key_fragment == key_extra_bits)
            return FALSE;

        NEXT();
    }
}

BOOL ES_Identifier_Hash_Table::Contains(JString *key, unsigned extra_bits)
{
    unsigned value;
    return Find(key, value, extra_bits);
}

BOOL ES_Identifier_Hash_Table::Find(JString *key, unsigned &value, unsigned key_extra_bits)
{
    unsigned hash = key->Hash(), mask = cells->size - 1;
    ES_HASH_UPDATE(hash, key_extra_bits);
    DECLARE();
    ES_IdentifierCell_Array::Cell *hashed = (ES_IdentifierCell_Array::Cell *) cells->cells;

    while (TRUE)
    {
        ES_IdentifierCell_Array::Cell &entry = hashed[index & mask];
        if (entry.key == NULL && entry.value != TOMB_MARKER)
            return FALSE;

        if ((entry.key == key || entry.key->Equals(key)) && entry.key_fragment == key_extra_bits)
        {
            value = entry.value;
            return TRUE;
        }
        NEXT();
    }

    return FALSE;
}

BOOL ES_Identifier_Hash_Table::Remove(JString *key, unsigned &value, unsigned key_extra_bits)
{
    unsigned hash = key->Hash(), mask = cells->size - 1;
    ES_HASH_UPDATE(hash, key_extra_bits);
    DECLARE();
    ES_IdentifierCell_Array::Cell *hashed = (ES_IdentifierCell_Array::Cell *) cells->cells;

    while (TRUE)
    {
        ES_IdentifierCell_Array::Cell &entry = hashed[index & mask];

        if (entry.key == NULL && entry.value != TOMB_MARKER)
            return FALSE;
        else if ((entry.key == key || entry.key->Equals(key)) && entry.key_fragment == key_extra_bits)
        {
            value = entry.value;
            entry.key = NULL;
            entry.value = TOMB_MARKER;
            entry.value = 0;
            --nused;
            ++ndeleted;
            return TRUE;
        }
        NEXT();
    }
}

BOOL ES_Identifier_Hash_Table::Remove(JString *key, unsigned key_extra_bits)
{
    unsigned value;
    return Remove(key, value, key_extra_bits);
}

void ES_Identifier_Hash_Table::ResizeL(ES_Context *context)
{
    unsigned nallocated = cells->size;

    while (4 * nused >= 3 * nallocated)
        nallocated *= 2;
    while (4 * nused < nallocated)
        nallocated /= 2;

    ES_IdentifierCell_Array *new_cells = ES_IdentifierCell_Array::Make(context, nallocated);
    ES_IdentifierCell_Array::Cell *new_hashed = new_cells->cells;

    for (unsigned i = 0; i < nallocated; ++i)
    {
        new_hashed[i].key = NULL;
        new_hashed[i].value = 0;
        new_hashed[i].key_fragment = 0;
    }

    ES_IdentifierCell_Array::Cell *item = cells->cells;
    for (unsigned added = 0; added < nused; ++added)
    {
        while (item->key == NULL)
            ++item;

        unsigned hash = item->key->Hash(), mask = nallocated - 1;
        if (item->key_fragment)
            ES_HASH_UPDATE(hash, item->key_fragment);

        DECLARE();

        while (TRUE)
        {
            ES_IdentifierCell_Array::Cell &entry = new_hashed[index & mask];

            if (entry.key == NULL)
            {
                entry.key = item->key;
                entry.value = item->value;
                entry.key_fragment = item->key_fragment;
                item++;
                break;
            }

            NEXT();
        }
    }
    op_memset(cells->cells, 0, cells->size * sizeof(ES_IdentifierCell_Array::Cell));
    cells = new_cells;
    ndeleted = 0;
    return;
}

ES_Identifier_Hash_Table *ES_Identifier_Hash_Table::CopyL(ES_Context *context)
{
    ES_Identifier_Hash_Table *table = ES_Identifier_Hash_Table::Make(context, cells->size);
    table->nused    = nused;
    table->ndeleted = ndeleted;
    op_memcpy(table->cells->cells, cells->cells, cells->size * sizeof(ES_IdentifierCell_Array::Cell));
    return table;
}

/* ES_Identifier_Boxed_Hash_Table */

ES_Identifier_Boxed_Hash_Table *ES_Identifier_Boxed_Hash_Table::Make(ES_Context *context, unsigned size)
{
    ES_Identifier_Boxed_Hash_Table *self;
    GC_ALLOCATE(context, self, ES_Identifier_Boxed_Hash_Table, (self));

    ES_CollectorLock gclock(context);

    size = ComputeSize(size);

    self->array = ES_Boxed_Array::Make(context, size, 0);

    self->cells = ES_IdentifierCell_Array::Make(context, size);
    op_memset(self->cells->cells, 0, size * sizeof(ES_IdentifierCell_Array::Cell));

#ifdef DEBUG_ENABLE_OPASSERT
    for (unsigned i = 0; i < size; ++i)
    {
        OP_ASSERT(self->cells->cells[i].key == NULL);
    }
#endif

    return self;
}

void ES_Identifier_Boxed_Hash_Table::Initialize(ES_Identifier_Boxed_Hash_Table *table)
{
    ES_Identifier_Hash_Table::Initialize(table);
    table->ChangeGCTag(GCTAG_ES_Identifier_Boxed_Hash_Table);
}

BOOL ES_Identifier_Boxed_Hash_Table::AddL(ES_Context *context, JString *key, unsigned key_extra_bits, ES_Boxed *value)
{
    if (ES_Identifier_Hash_Table::AddL(context, key, array->nused, key_extra_bits))
    {
        if (!(array->nused < array->nslots))
            array = ES_Boxed_Array::Grow(context, array);

        ES_Boxed *&storage = array->slots[array->nused++];
        storage = value;
        return TRUE;
    }
    return FALSE;
}

BOOL ES_Identifier_Boxed_Hash_Table::Find(JString *key, unsigned key_extra_bits, ES_Boxed *&value)
{
    unsigned index;
    if (ES_Identifier_Hash_Table::Find(key, index, key_extra_bits))
    {
        value = array->slots[index];
        return TRUE;
    }
    return FALSE;
}

BOOL ES_Identifier_Boxed_Hash_Table::FindLocation(JString *key, unsigned key_extra_bits, ES_Boxed **&value)
{
    unsigned index;
    if (ES_Identifier_Hash_Table::Find(key, index, key_extra_bits))
    {
        value = &array->slots[index];
        return TRUE;
    }

    return FALSE;
}

#undef DECLARE
#undef NEXT
#undef TOMB_MARKER
#undef FREE_MARKER
