/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_PROPERTY_TABLE_INLINES_H
#define ES_PROPERTY_TABLE_INLINES_H

inline int
ES_Layout_Info::Diff(ES_Layout_Info layout) const
{
    return data.offset + ES_Layout_Info::SizeOf(GetStorageType()) - layout.GetOffset() - ES_Layout_Info::SizeOf(layout.GetStorageType());
}

inline /* static */ unsigned
ES_Layout_Info::SizeOf(ES_StorageType type)
{
    unsigned size = 0;
    switch(type)
    {
    case ES_STORAGE_WHATEVER:
        size = sizeof(ES_Value_Internal);
        break;
    case ES_STORAGE_BOOLEAN:
    case ES_STORAGE_INT32:
        size = sizeof(unsigned int);
        break;
    case ES_STORAGE_DOUBLE:
    case ES_STORAGE_INT32_OR_DOUBLE:
        size = sizeof(double);
        break;
    case ES_STORAGE_UNDEFINED:
    case ES_STORAGE_NULL:
    case ES_STORAGE_STRING:
    case ES_STORAGE_STRING_OR_NULL:
        size = sizeof(JString *);
        break;
    case ES_STORAGE_OBJECT:
    case ES_STORAGE_OBJECT_OR_NULL:
        size = sizeof(ES_Object *);
        break;
    case ES_STORAGE_BOXED:
        size = sizeof(ES_Boxed *);
        break;
    case ES_STORAGE_SPECIAL_PRIVATE:
        size = sizeof(ES_Boxed*);
        break;
    }

    OP_ASSERT(size <= sizeof(ES_Value_Internal));
    return size;
}

inline /* static */ unsigned
ES_Layout_Info::AlignmentOf(ES_StorageType type)
{
    unsigned size = 0;
    switch(type)
    {
    case ES_STORAGE_WHATEVER:
        size = sizeof(double);
        break;
    case ES_STORAGE_BOOLEAN:
    case ES_STORAGE_INT32:
        size = sizeof(unsigned int);
        break;
    case ES_STORAGE_DOUBLE:
    case ES_STORAGE_INT32_OR_DOUBLE:
        size = sizeof(double);
        break;
    case ES_STORAGE_UNDEFINED:
    case ES_STORAGE_NULL:
    case ES_STORAGE_STRING:
    case ES_STORAGE_STRING_OR_NULL:
        size = sizeof(JString *);
        break;
    case ES_STORAGE_OBJECT:
    case ES_STORAGE_OBJECT_OR_NULL:
        size = sizeof(ES_Object *);
        break;
    case ES_STORAGE_BOXED:
        size = sizeof(ES_Boxed *);
        break;
    case ES_STORAGE_SPECIAL_PRIVATE:
        size = sizeof(ES_Boxed*);
        break;
    }

    OP_ASSERT(size <= sizeof(double));
    return size;
}

inline BOOL
ES_Layout_Info::CheckType(ES_StorageType type) const
{
    return CheckType(type, static_cast<ES_StorageType>(data.type));
}

inline /* static */ BOOL
ES_Layout_Info::CheckType(ES_StorageType type0, ES_StorageType type1)
{
    return type0 == type1 || type0 == ES_STORAGE_NULL && ES_Layout_Info::IsNullable(type1) || ES_Layout_Info::IsSubType(type0, type1);
}

inline /* static */ ES_StorageType
ES_Layout_Info::SuperType(ES_StorageType type)
{
    switch (type)
    {
    case ES_STORAGE_INT32:
        type = ES_STORAGE_DOUBLE;
        break;
    case ES_STORAGE_STRING:
        type = ES_STORAGE_STRING_OR_NULL;
        break;
    case ES_STORAGE_OBJECT:
        type = ES_STORAGE_OBJECT_OR_NULL;
        break;
    default:
        type = ES_STORAGE_WHATEVER;
        break;
    }

    return type;
}

inline /* static */ BOOL
ES_Layout_Info::IsSubType(ES_StorageType type0, ES_StorageType type1)
{
    do
    {
        type0 = ES_Layout_Info::SuperType(type0);
        if (type0 == type1)
            return TRUE;

    } while (type0 != ES_STORAGE_WHATEVER);

    return FALSE;
}

/* static */ inline BOOL
ES_Layout_Info::IsNullable(ES_StorageType type)
{
    switch (type)
    {
    case ES_STORAGE_STRING_OR_NULL:
    case ES_STORAGE_OBJECT_OR_NULL:
        return TRUE;
    default:
        return FALSE;
    }
}

/* static */ inline ES_StorageType
ES_Layout_Info::NonNullableType(ES_StorageType type)
{
    switch (type)
    {
    case ES_STORAGE_STRING_OR_NULL:
        return ES_STORAGE_STRING;
    case ES_STORAGE_OBJECT_OR_NULL:
        return ES_STORAGE_OBJECT;
    default:
        return type;
    }
}

/* static */ inline ES_StorageType
ES_Layout_Info::ToNullableType(ES_StorageType type)
{
    switch (type)
    {
    case ES_STORAGE_STRING:
        return ES_STORAGE_STRING_OR_NULL;
    case ES_STORAGE_OBJECT:
        return ES_STORAGE_OBJECT_OR_NULL;
    default:
        return type;
    }
}

inline BOOL
ES_Property_Table::Find(JString *name, ES_Property_Info &info, ES_PropertyIndex &position)
{
    unsigned index;
    if (properties->IndexOf(name, index))
    {
        position = ES_PropertyIndex(index);
        info = GetPropertyInfoAtIndex(position);
        return TRUE;
    }

    return FALSE;
}

inline BOOL
ES_Property_Table::IndexOf(JString *name, ES_PropertyIndex &index) const
{
    unsigned i;
    BOOL res = properties->IndexOf(name, i);
    index = ES_PropertyIndex(i);
    return res;
}

inline BOOL
ES_Property_Table::Lookup(ES_PropertyIndex index, JString *&name, ES_Property_Info &info)
{
    ES_Layout_Info layout;
    return Lookup(index, name, info, layout);
}

inline BOOL
ES_Property_Table::Lookup(ES_PropertyIndex index, JString *&name, ES_Property_Info &info, ES_Layout_Info &layout)
{
    OP_ASSERT(index < properties->GetCount());
    if (properties->Lookup(index, name))
    {
        info = GetPropertyInfoAtIndex(index);
        layout = GetLayoutInfoAtIndex(info.Index());
        return TRUE;
    }

    return FALSE;
}

inline void
ES_Property_Value_Table::Lookup(unsigned index, JString *&name, ES_Property_Info &info, ES_Value_Internal &value)
{
    properties->Lookup(index, name);
    value = property_value[index].value;
    info = property_value[index].info;
}

inline BOOL
ES_Property_Value_Table::Find(JString *name, ES_Value_Internal &value)
{
    unsigned index;
    if (properties && properties->IndexOf(name, index))
    {
        value = property_value[index].value;
        return TRUE;
    }

    return FALSE;
}

inline BOOL
ES_Property_Value_Table::FindLocation(JString *name, ES_Property_Info &info, ES_Value_Internal *&value)
{
    unsigned index;
    if (properties && properties->IndexOf(name, index))
    {
        info  = property_value[index].info;
        value = &property_value[index].value;
        return TRUE;
    }

    return FALSE;
}

inline void
ES_Property_Value_Table::GetSerialNr(unsigned index, unsigned &serial_nr)
{
#ifdef ES_VALUES_AS_NANS
    serial_nr = property_value[index].serial;
#else
    serial_nr = property_value[index].value.GetExtra();
#endif
}

inline void
ES_Property_Value_Table::SetSerialNr(unsigned index, unsigned serial_nr)
{
#ifdef ES_VALUES_AS_NANS
    property_value[index].serial = serial_nr;
#else
    property_value[index].value.SetExtra(serial_nr);
#endif
}

inline void
ES_Property_Table::GrowToL(ES_Context *context, unsigned size)
{
    OP_ASSERT(size > capacity);

    ES_Box *new_property_info = ES_Box::Make(context, size * sizeof(ES_Property_Info));
    ES_CollectorLock gclock(context);
    op_memcpy(new_property_info->Unbox(), property_info->Unbox(), used * sizeof(ES_Property_Info));
    context->heap->Free(property_info);
    property_info = new_property_info;

    ES_Box *new_layout_info = ES_Box::Make(context, size * sizeof(ES_Layout_Info));
    op_memcpy(new_layout_info->Unbox(), layout_info->Unbox(), used * sizeof(ES_Layout_Info));
    context->heap->Free(layout_info);
    layout_info = new_layout_info;

    capacity = size;
}

inline ES_Property_Info
ES_Property_Table::GetPropertyInfoAtIndex(ES_PropertyIndex index) const
{
    OP_ASSERT(index < used);
    OP_ASSERT(reinterpret_cast<ES_Property_Info *>(property_info->Unbox())[index].Index() < used);
    return reinterpret_cast<ES_Property_Info *>(property_info->Unbox())[index];
}

inline ES_Property_Info &
ES_Property_Table::GetPropertyInfoAtIndex_ref(ES_PropertyIndex index)
{
    OP_ASSERT(index < used);
    return reinterpret_cast<ES_Property_Info *>(property_info->Unbox())[index];
}
inline ES_Property_Info *
ES_Property_Table::GetPropertyInfo()
{
    return reinterpret_cast<ES_Property_Info *>(property_info->Unbox());
}

inline ES_Layout_Info
ES_Property_Table::GetLayoutInfoAtIndex(ES_LayoutIndex index) const
{
    OP_ASSERT(index < used);
    return reinterpret_cast<ES_Layout_Info *>(layout_info->Unbox())[index];
}

inline ES_Layout_Info &
ES_Property_Table::GetLayoutInfoAtIndex_ref(ES_LayoutIndex index)
{
    OP_ASSERT(index < used);
    return reinterpret_cast<ES_Layout_Info *>(layout_info->Unbox())[index];
}

inline ES_Layout_Info *
ES_Property_Table::GetLayoutInfo()
{
    return reinterpret_cast<ES_Layout_Info *>(layout_info->Unbox());
}

inline unsigned
ES_Property_Table::GetNextOffset(ES_LayoutIndex index) const
{
    OP_ASSERT(index <= used);
    return index + 1 >= used ? GetLayoutInfoAtIndex(index).GetNextOffset() : GetLayoutInfoAtIndex(ES_LayoutIndex(index + 1)).GetOffset();
}

#endif // ES_PROPERTY_TABLE_INLINES_H
