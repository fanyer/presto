/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/object/es_property_table.h"

/* ES_Property_Table */

/* static */ ES_Property_Table *
ES_Property_Table::Make(ES_Context *context, unsigned size)
{
    ES_Property_Table *table;
    GC_ALLOCATE(context, table, ES_Property_Table, (table, size));
    ES_CollectorLock gclock(context);

    table->properties    = ES_Identifier_List::Make(context, size);
    table->property_info = ES_Box::Make(context, size * sizeof(ES_Property_Info));
    table->layout_info   = ES_Box::Make(context, size * sizeof(ES_Layout_Info));

    ES_Property_Info *info = table->GetPropertyInfo();
    ES_Layout_Info *linfo = table->GetLayoutInfo();
    for (unsigned i = table->used; i < table->capacity ; ++i)
    {
        info[i].Reset();
        linfo[i].Reset();
    }

    return table;
}

/* static */ void
ES_Property_Table::Initialize(ES_Property_Table *self, unsigned size)
{
    self->InitGCTag(GCTAG_ES_Property_Table);

    self->capacity      = size;
    self->used          = 0;
    self->properties    = NULL;
    self->property_info = NULL;
    self->layout_info   = NULL;
}

unsigned
ES_Property_Table::CalculateAlignedOffset(ES_StorageType type, unsigned index) const
{
    unsigned offset = 0;

    if (index > 0)
    {
        ES_Layout_Info layout = GetLayoutInfoAtIndex(ES_LayoutIndex(index - 1));
        offset = layout.GetNextOffset();
    }

    unsigned size = ES_Layout_Info::AlignmentOf(type);
    if (size > 0)
    {
        unsigned align = size - 1;
        if (((offset + 4) & align) != 0)
            offset += 4;
    }

    return offset;
}

BOOL
ES_Property_Table::AppendL(ES_Context *context, JString *name, unsigned attributes, ES_StorageType type, BOOL hide_existing)
{
    unsigned index;
    if (!properties->AppendL(context, name, index, hide_existing))
        return FALSE;

    unsigned offset = CalculateAlignedOffset(type, used);

    ES_Layout_Info layout(offset, type);

    ES_Property_Info info(attributes, ES_LayoutIndex(used));

    if (used == capacity)
        GrowToL(context, (capacity * 3) / 2 + 1);

    OP_ASSERT(used < capacity && index <= used);

    ES_LayoutIndex layout_index(used++);
    GetLayoutInfoAtIndex_ref(layout_index)   = layout;
    GetPropertyInfoAtIndex_ref(ES_PropertyIndex(index)) = info;

    return TRUE;
}

BOOL
ES_Property_Table::AppendL(ES_Context *context, unsigned &index, ES_StorageType type)
{
    unsigned offset = 0;

    offset = CalculateAlignedOffset(type, used);

    ES_Layout_Info layout(offset, type);

    if (used == capacity)
        GrowToL(context, (capacity * 3) / 2 + 1);

    index = used++;
    GetLayoutInfoAtIndex_ref(ES_LayoutIndex(index)) = layout;

    return TRUE;
}

ES_Property_Table *
ES_Property_Table::CopyL(ES_Context *context, unsigned count, unsigned property_count)
{
    ES_Property_Table *table;
    GC_ALLOCATE(context, table, ES_Property_Table, (table, capacity));
    ES_CollectorLock gclock(context);

    table->used          = count > used ? used : count;
    table->properties    = properties->CopyL(context, property_count);
    table->property_info = ES_Box::Make(context, capacity * sizeof(ES_Property_Info));
    table->layout_info   = ES_Box::Make(context, capacity * sizeof(ES_Layout_Info));

    op_memcpy(table->property_info->Unbox(), property_info->Unbox(), table->used * sizeof(ES_Property_Info));
    op_memcpy(table->layout_info->Unbox(), layout_info->Unbox(), table->used * sizeof(ES_Layout_Info));

    ES_Property_Info *info = table->GetPropertyInfo();
    ES_Layout_Info *linfo = table->GetLayoutInfo();
    for (unsigned i = table->used; i < table->capacity ; ++i)
    {
        info[i].Reset();
        linfo[i].Reset();
    }

    return table;
}

void
ES_Property_Table::AdjustOffset(ES_LayoutIndex from)
{
    for (unsigned i = from; i < used; ++i)
    {
        unsigned offset = CalculateAlignedOffset(GetLayoutInfoAtIndex(ES_LayoutIndex(i)).GetStorageType(), i);
        GetLayoutInfoAtIndex_ref(ES_LayoutIndex(i)).SetOffset(offset);
    }
}

/* ES_Property_Mutable_Table */

void
ES_Property_Mutable_Table::Delete(unsigned index)
{
    unsigned layout_index = GetPropertyInfoAtIndex(ES_PropertyIndex(index)).Index();
    static_cast<ES_Identifier_Mutable_List *>(properties)->Remove(index);
    properties->Rehash();
    used--;
    op_memmove(GetPropertyInfo() + index, GetPropertyInfo() + index + 1, (used - index) * sizeof(ES_Property_Info));
    op_memmove(GetLayoutInfo() + layout_index, GetLayoutInfo() + layout_index + 1, (used - layout_index) * sizeof(ES_Layout_Info));
}

/* ES_Property_Data_Table */

/* static */ ES_Property_Value_Table *
ES_Property_Value_Table::Make(ES_Context *context, unsigned size)
{
    ES_Property_Value_Table *table;
    GC_ALLOCATE_WITH_EXTRA(context, table, size * sizeof(Property_Data_Cell), ES_Property_Value_Table, (table, size));
    ES_CollectorLock gclock(context);

    for (unsigned i = 0; i < size; ++i)
    {
        Property_Data_Cell &cell = table->property_value[i];
        cell.value.SetUndefined();
        cell.value.SetFalse();
        cell.info.Reset();
    }
    table->properties = static_cast<ES_Identifier_Mutable_List *>(ES_Identifier_List::Make(context, size));
    return table;
}

/* static */ void
ES_Property_Value_Table::Initialize(ES_Property_Value_Table *self, unsigned size)
{
    self->InitGCTag(GCTAG_ES_Property_Value_Table);

    self->capacity   = size;
    self->used       = 0;
    self->deleted    = 0;
    self->properties = NULL;
}

ES_Property_Value_Table *
ES_Property_Value_Table::CopyL(ES_Context *context)
{
    ES_Property_Value_Table *table;
    GC_ALLOCATE_WITH_EXTRA(context, table, capacity * sizeof(Property_Data_Cell), ES_Property_Value_Table, (table, capacity));
    ES_CollectorLock gclock(context);

    table->used = used;
    table->properties = static_cast<ES_Identifier_Mutable_List *>(properties->CopyL(context));

    op_memcpy(table->property_value, property_value, capacity * sizeof(Property_Data_Cell));

    return table;
}

void
ES_Property_Value_Table::InsertL(ES_Context *context, ES_Property_Value_Table *&table, JString *name, const ES_Value_Internal &value, ES_Property_Info info, unsigned serial)
{
    unsigned index;

    OP_ASSERT(!info.IsClassProperty());

    if (properties->AppendL(context, name, index))
    {
        OP_ASSERT(index == used);
        if (used < capacity)
        {
            table = this;
        }
        else
        {
            unsigned size = (capacity * 3) / 2 + 1;
            GC_ALLOCATE_WITH_EXTRA(context, table, size * sizeof(Property_Data_Cell), ES_Property_Value_Table, (table, size));

            table->used       = used;
            table->properties = properties;

            op_memcpy(table->property_value, property_value, used * sizeof(Property_Data_Cell));
        }
    }

    info.SetIndex(ES_LayoutIndex(index));

    Property_Data_Cell &cell = table->property_value[table->used++];
    cell.value = value;
    cell.info = info;
#ifdef ES_VALUES_AS_NANS
    cell.serial = serial;
#else
    cell.value.SetExtra(serial);
#endif
}

void
ES_Property_Value_Table::ReplaceL(ES_Context *context, ES_Property_Value_Table *&table, JString *name, const ES_Value_Internal *value, ES_Property_Info info)
{
    OP_ASSERT(!info.IsClassProperty());

    unsigned index;
#ifdef DEBUG_ENABLE_OPASSERT
    BOOL exists =
#endif // DEBUG_ENABLE_OPASSERT
        properties->IndexOf(name, index);
    OP_ASSERT(exists);

    info.SetIndex(ES_LayoutIndex(index));
    Property_Data_Cell &cell = table->property_value[index];
    if (value)
        cell.value = *value;
    cell.info = info;
}

void
ES_Property_Value_Table::Delete(JString *name)
{
    unsigned index;
    if (properties->IndexOf(name, index))
    {
        properties->Remove(index);
        deleted++;
        if (4 * deleted >= used)
        {
            JString *ident = NULL;
            unsigned index = 0;
            for (unsigned i = 0; i < used && properties->Lookup(i, ident); ++i)
            {
                if (ident != NULL)
                {
#ifndef ES_VALUES_AS_NANS
                    unsigned serial = property_value[i].value.GetExtra();
#endif // ES_VALUES_AS_NANS

                    unsigned new_index = index++;
                    property_value[new_index] = property_value[i];
                    property_value[new_index].info.SetIndex(ES_LayoutIndex(new_index));

#ifndef ES_VALUES_AS_NANS
                    property_value[new_index].value.SetExtra(serial);
#endif // ES_VALUES_AS_NANS


                    ident = NULL;
                }
            }
            for (unsigned j = index; j < used; ++j)
            {
                Property_Data_Cell &cell = property_value[j];
                cell.value.SetUndefined();
                cell.value.SetFalse();
                cell.info.Reset();
            }
            used = index;
            properties->Rehash();
            deleted = 0;
        }
    }
}
