/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_CLASS_INLINES_H
#define ES_CLASS_INLINES_H

inline void
ES_Class_Data::SetMainClass(ES_Class *new_main)
{
    main = new_main;
}

inline void
ES_Class_Data::InvalidateSubClasses()
{
    if (instance && instance->GCTag() >= GCTAG_ES_Object)
        static_cast<ES_Object *>(instance)->InvalidateInstances();
    else
        for (ES_Boxed_List *l = static_cast<ES_Boxed_List *>(instance); l; l = l->tail)
            if (l->head->GCTag() >= GCTAG_ES_Object)
            {
                OP_ASSERT(l->head->GCTag() < GCTAG_UNINITIALIZED);
                static_cast<ES_Object *>(l->head)->InvalidateInstances();
            }
            else
            {
                OP_ASSERT(l->head->IsClass());
                ES_Class::InvalidateTree(static_cast<ES_Class *>(l->head));
            }
}

inline void
ES_Class_Data::AddInstance(ES_Context *context, ES_Boxed *new_instance)
{
    ES_CollectorLock gclock(context);

    if (!instance)
    {
        instance = new_instance->GCTag() >= GCTAG_ES_Object ? new_instance : ES_Boxed_List::Make(context, new_instance);
        return;
    }
    else if (instance->GCTag() >= GCTAG_ES_Object)
        instance = ES_Boxed_List::Make(context, instance);
    else if (new_instance->IsClass())
    {
        for (ES_Boxed_List *l = static_cast<ES_Boxed_List *>(instance); l; l = l->tail)
            if (l->head == new_instance)
                return;
    }

    instance = ES_Boxed_List::Make(context, new_instance, static_cast<ES_Boxed_List *>(instance));
}

inline void
ES_Class_Data::RemoveInstance(ES_Boxed *old_instance)
{
    if (instance)
        if (instance == old_instance)
            instance = NULL;
        else if (instance->GCTag() == GCTAG_ES_Boxed_List)
        {
            ES_Boxed_List *first = static_cast<ES_Boxed_List *>(instance);

            for (ES_Boxed_List **ptr = &first; *ptr; ptr = &(*ptr)->tail)
                if ((*ptr)->head == old_instance)
                {
                    *ptr = (*ptr)->tail;
                    break;
                }

            instance = first;
        }
}

inline ES_Object *
ES_Class_Data::GetInstance() const
{
    if (instance && instance->GCTag() >= GCTAG_ES_Object)
        return static_cast<ES_Object *>(instance);
    else
        for (ES_Boxed_List *l = static_cast<ES_Boxed_List *>(instance); l; l = l->tail)
            if (l->head->GCTag() >= GCTAG_ES_Object)
            {
                return static_cast<ES_Object *>(l->head);
            }

    return NULL;
}

inline /* static */ BOOL
ES_Class::Find(ES_Class *this_class, JString *name, ES_Property_Info &info, unsigned count)
{
    return this_class->HasHashTableProperties() && static_cast<ES_Class_Hash_Base *>(this_class)->Find(name, info)
        || this_class->Find(name, info, count);
}

inline /* static */ ES_Class *
ES_Class::ChangeAttribute(ES_Context *context, ES_Class *this_class, JString *name, ES_Property_Info info, unsigned count, BOOL &needs_conversion)
{
    info.data.class_property = 1;
    ES_PropertyIndex index;
    if (this_class->property_table->IndexOf(name, index) && index < count)
        return ES_Class::ChangeAttributeFromTo(context, this_class, info.Attributes(), (~info.Attributes()) & PROPERTY_ATTRIBUTE_MASK, index, index, count, needs_conversion);
    else
        return this_class;
}

inline void
ES_Class::InvalidateCurrentClass()
{
    class_id = INVALID_CLASS_ID;
}

inline void
ES_Class::InvalidateInstances()
{
    static_data->InvalidateSubClasses();
}

inline void
ES_Class::Invalidate()
{
    InvalidateCurrentClass();
    InvalidateInstances();
}


inline unsigned
ES_Class::Id() const
{
    return class_id;
}

inline unsigned
ES_Class::GetId(ES_Context *context)
{
    if (class_id == INVALID_CLASS_ID)
    {
        class_id = context->NewUnique();
        if (HasHashTableProperties())
            class_id |= MASK_HAS_HASH_TABLE_PROPERTIES;
    }

    return class_id;
}

inline BOOL
ES_Class::IsInvalid() const
{
    return class_id == INVALID_CLASS_ID;
}

inline unsigned
ES_Class::Count() const
{
    return level;
}

inline unsigned
ES_Class::CountProperties() const
{
    return LayoutLevel() - CountExtra();
}

inline unsigned
ES_Class::CountExtra() const
{
    ES_Class_Extra *extra_data = extra;

    while (extra_data)
    {
        if (extra_data->LayoutLevel() > LayoutLevel())
            extra_data = extra_data->next;
        else
            return extra_data->length;
    }

    return 0;
}

inline ES_PropertyIndex
ES_Class::Level() const
{
    return ES_PropertyIndex(LayoutLevel() - CountExtra());
}

inline ES_LayoutIndex
ES_Class::LayoutLevel() const
{
    return ES_LayoutIndex(level);
}


inline BOOL
ES_Class::HasEnumerableProperties() const
{
    return has_enumerable_properties != 0;
}

inline BOOL
ES_Class::HasInstances() const
{
    return static_data->HasInstances();
}

inline const char *
ES_Class::ObjectName() const
{
    return static_data->ObjectName();
}

inline JString *
ES_Class::ObjectName(ES_Context *context) const
{
    if (!static_data->ObjectNameString())
        static_data->SetObjectNameString(context->rt_data->GetSharedString(ObjectName()));
    return static_data->ObjectNameString();
}

inline JString *
ES_Class::ObjectNameString() const
{
    return static_data->ObjectNameString();
}


inline BOOL
ES_Class::HasPrototype() const
{
    return static_data->Prototype() != NULL;
}

inline ES_Object *
ES_Class::Prototype() const
{
    return static_data->Prototype();
}

inline void
ES_Class::SetPrototype(ES_Object *object) const
{
    static_data->SetPrototype(object);
}

inline ES_Property_Table *
ES_Class::GetPropertyTable() const
{
    return property_table;
}

inline ES_Class *
ES_Class::GetRootClass() const
{
    return static_data->RootClass();
}

inline void
ES_Class::AddInstance(ES_Context *context, ES_Boxed *instance)
{
    static_data->AddInstance(context, instance);
}

inline void
ES_Class::RemoveInstance(ES_Boxed *instance)
{
    static_data->RemoveInstance(instance);
}

inline ES_Object *
ES_Class::GetInstance() const
{
    return static_data->GetInstance();
}

inline BOOL
ES_Class::Find(JString *name, ES_Property_Info &info, unsigned limit)
{
    ES_PropertyIndex position;
    if (property_table && property_table->Find(name, info, position))
    {
        if (info.Index() >= limit)
            return FALSE;

        return TRUE;
    }

    return FALSE;
}

inline BOOL
ES_Class::Find(JString *name, ES_PropertyIndex &index, unsigned limit)
{
    ES_Property_Info info;
    if (property_table && property_table->Find(name, info, index))
    {
        if (info.Index() >= limit)
            return FALSE;

        return TRUE;
    }

    return FALSE;
}

inline BOOL
ES_Class::Lookup(ES_PropertyIndex index, JString *&name, ES_Property_Info &info)
{
    OP_ASSERT(index < CountProperties());
    return property_table->Lookup(index, name, info);
}

inline BOOL
ES_Class::LookupExtra(ES_StorageType type, ES_Layout_Info &layout)
{
    ES_Class_Extra *ptr = extra;

    while (ptr && ptr->LayoutLevel() > LayoutLevel())
        ptr = ptr->next;

    while (ptr)
    {
        ES_Layout_Info extra_layout = extra->klass->GetLayoutInfoAtIndex(ES_LayoutIndex(ptr->LayoutLevel() - 1));

        if (type == extra_layout.GetStorageType())
        {
            layout = extra_layout;
            return TRUE;
        }
        ptr = ptr->next;
    }

    return FALSE;
}

inline JString *
ES_Class::GetNameAtIndex(ES_PropertyIndex index)
{
    OP_ASSERT(index < CountProperties());
    return property_table->properties->GetIdentifierAtIndex(index);
}

inline ES_PropertyIndex
ES_Class::IndexOf(JString *name)
{
    ES_PropertyIndex index;
#ifdef DEBUG_ENABLE_OPASSERT
    OP_ASSERT(property_table->IndexOf(name, index));
#else // DEBUG_ENABLE_OPASSERT
    property_table->IndexOf(name, index);
#endif // DEBUG_ENABLE_OPASSERT
    return index;
}

inline ES_Class *
ES_Class::GetParent() const
{
    OP_ASSERT(IsNode());
    return static_cast<const ES_Class_Node *const>(this)->parent;
}

inline ES_Class *
ES_Class::GetMainClass() const
{
    return static_data->MainClass();
}

inline BOOL
ES_Class::HasHashTableProperties() const
{
    return GCTag() == GCTAG_ES_Class_Hash || (GCTag() == GCTAG_ES_Class_Singleton && static_cast<const ES_Class_Hash_Base *const>(this)->data != NULL);
}

inline BOOL
ES_Class::IsSingleton() const
{
    return GCTag() == GCTAG_ES_Class_Singleton;
}

inline /* static */ BOOL
ES_Class::HasHashTableProperties(unsigned id)
{
    return (id & MASK_HAS_HASH_TABLE_PROPERTIES) != 0;
}

inline /* static */ ES_Property_Value_Table *
ES_Class::GetHashTable(ES_Class *this_class)
{
    if (this_class->HasHashTableProperties())
        return static_cast<ES_Class_Hash_Base *>(this_class)->GetHashTable();
    else
        return NULL;
}

inline BOOL
ES_Class::IsNode() const
{
    return GCTag() == GCTAG_ES_Class_Node || GCTag() == GCTAG_ES_Class_Compact_Node || GCTag() == GCTAG_ES_Class_Compact_Node_Frozen;
}

inline BOOL
ES_Class::IsCompact() const
{
    return GCTag() == GCTAG_ES_Class_Compact_Node || GCTag() == GCTAG_ES_Class_Compact_Node_Frozen;
}

inline BOOL
ES_Class::NeedLimitCheck() const
{
    return GCTag() == GCTAG_ES_Class_Compact_Node || GCTag() == GCTAG_ES_Class_Compact_Node_Frozen;
}

inline BOOL
ES_Class::NeedLimitCheck(ES_LayoutIndex limit, BOOL negative) const
{
    OP_ASSERT(!IsCompact() || LayoutLevel() == 0 || GetParent()->LayoutLevel() < LayoutLevel());
    if (!IsCompact())
        return FALSE;

    if (need_limit_check != 0)
        return TRUE;

    if (negative)
        return static_cast<const ES_Class_Compact_Node * const>(this)->children == NULL && GCTag() != GCTAG_ES_Class_Compact_Node_Frozen || limit <= LayoutLevel();
    else
        return LayoutLevel() > 0 && GetParent()->LayoutLevel() < limit;
}

inline unsigned
ES_Class::SizeOf(unsigned count) const
{
    OP_ASSERT(count == 0 || count <= property_table->Count());
    return count != 0 ? property_table->GetLayoutInfoAtIndex(ES_LayoutIndex(count - 1)).GetNextOffset() : 0;
}

inline ES_Layout_Info
ES_Class::GetLayoutInfo(const ES_Property_Info &info) const
{
    return property_table->GetLayoutInfoAtIndex(info.Index());
}

inline ES_Layout_Info
ES_Class::GetLayoutInfoAtInfoIndex(ES_PropertyIndex index) const
{
    return GetLayoutInfo(GetPropertyInfoAtIndex(index));
}

inline ES_Layout_Info &
ES_Class::GetLayoutInfoAtInfoIndex_ref(ES_PropertyIndex index) const
{
    return GetLayoutInfoAtIndex_ref(GetPropertyInfoAtIndex(index).Index());
}

inline ES_Layout_Info
ES_Class::GetLayoutInfoAtIndex(ES_LayoutIndex index) const
{
    return property_table->GetLayoutInfoAtIndex(index);
}

inline ES_Layout_Info &
ES_Class::GetLayoutInfoAtIndex_ref(ES_LayoutIndex index) const
{
    return property_table->GetLayoutInfoAtIndex_ref(index);
}

inline ES_Property_Info
ES_Class::GetPropertyInfoAtIndex(ES_PropertyIndex index) const
{
    return property_table->GetPropertyInfoAtIndex(index);
}

inline ES_Property_Info &
ES_Class::GetPropertyInfoAtIndex_ref(ES_PropertyIndex index) const
{
    return property_table->GetPropertyInfoAtIndex_ref(index);
}

inline ES_Class *
ES_Class::Next(ES_Class *start)
{
    ES_Class *next = LastChild();

    if (!next)
    {
        ES_Class *node = this;

        while (!next && node && node != start)
        {
            next = node->sibling;
            node = node->Parent();
        }
    }

    return next;
}

inline ES_Class *
ES_Class::Parent()
{
    return IsNode() ? static_cast<ES_Class_Node_Base *>(this)->parent : NULL;
}

inline ES_Class *
ES_Class::LastChild()
{
    if (IsCompact() && static_cast<ES_Class_Compact_Node *>(this)->children && static_cast<ES_Class_Compact_Node *>(this)->children->GetCount() > 0)
    {
        ES_Identifier_Boxed_Hash_Table *table = static_cast<ES_Class_Compact_Node *>(this)->children;
        return static_cast<ES_Class *>(table->GetValues()->slots[table->GetCount() - 1]);
    }
    else if (IsNode() && static_cast<ES_Class_Node *>(this)->children)
    {
        ES_Boxed *child_or_table = static_cast<ES_Class_Node *>(this)->children;
        if (child_or_table->GCTag() != GCTAG_ES_Identifier_Boxed_Hash_Table)
            return static_cast<ES_Class *>(child_or_table);

        ES_Identifier_Boxed_Hash_Table *table = static_cast<ES_Identifier_Boxed_Hash_Table *>(child_or_table);

        if (table->GetCount() > 0)
            return static_cast<ES_Class *>(table->GetValues()->slots[table->GetCount() - 1]);
    }

    if ((IsCompact() || IsNode()) && extra && extra->LayoutLevel() == LayoutLevel() + 1)
        return extra->klass;

    return NULL;
}

inline void
ES_Class_Node_Base::InvalidateBranch()
{
    for (ES_Class *invalid = this; invalid; invalid = invalid->Parent())
        invalid->class_id = INVALID_CLASS_ID;

    ES_Class *invalid = Next(this);
    while (invalid)
    {
        invalid->class_id = INVALID_CLASS_ID;
        invalid = invalid->Next(this);
    }
}

inline unsigned
ES_Class_Node::ChildCount() const
{
    return children ? (children->GCTag() != GCTAG_ES_Class_Node ? static_cast<ES_Identifier_Boxed_Hash_Table *>(children)->GetCount() : 1) : 0;
}

inline unsigned
ES_Class_Compact_Node::ChildCount() const
{
    return children ? static_cast<ES_Identifier_Boxed_Hash_Table *>(children)->GetCount() : 0;
}

inline BOOL
ES_Class_Compact_Node::ShouldBranch() const
{
    return !static_data->IsCompact() && (CountProperties() < ES_CLASS_LINEAR_GROWTH_LIMIT || GetParent()->CountProperties() * ES_CLASS_GROWTH_RATE < CountProperties());
}

inline unsigned *
ES_Class_Hash_Base::GetSerials()
{
    return data ? reinterpret_cast<unsigned *>(GetData()->class_serials->Unbox()) : NULL;
}

inline ES_Box *
ES_Class_Hash_Base::GetSerialsBox()
{
    return data ? GetData()->class_serials : NULL;
}

inline ES_Property_Value_Table *
ES_Class_Hash_Base::GetHashTable()
{
    return data ? GetData()->values : NULL;
}

inline BOOL
ES_Class_Hash_Base::Find(JString *name, ES_Property_Info &info)
{
    return data && GetData()->values->Find(name, info);
}

inline BOOL
ES_Class_Hash_Base::FindLocation(JString *name, ES_Property_Info &info, ES_Value_Internal *&value)
{
    return data && GetData()->values->FindLocation(name, info, value);
}

inline void
ES_Class_Hash_Base::InsertL(ES_Context *context, JString *name, const ES_Value_Internal &value, ES_Property_Info info, unsigned count)
{
    if (!data)
        MakeData(context, count);

    GetData()->values->InsertL(context, GetData()->values, name, value, info, GetData()->serial++);
    InvalidateCurrentClass();
}

inline void
ES_Class_Hash_Base::RedefinePropertyL(ES_Context *context, JString *name, const ES_Value_Internal *value, ES_Property_Info info)
{
    OP_ASSERT(data);
    GetData()->values->ReplaceL(context, GetData()->values, name, value, info);
    InvalidateCurrentClass();
}

inline void
ES_Class_Hash_Base::Delete(JString *name)
{
    GetData()->values->Delete(name);
}

inline void
ES_Class_Hash_Base::GetSerialNr(const ES_Property_Info &info, unsigned &serial)
{
    OP_ASSERT(data);
    if (info.IsClassProperty())
    {
        OP_ASSERT(info.Index() < GetData()->count);
        serial = reinterpret_cast<unsigned *>(GetData()->class_serials->Unbox())[info.Index()];
    }
    else
    {
        GetData()->values->GetSerialNr(info.Index(), serial);
    }
}

inline unsigned
ES_Class_Hash_Base::GetSerialNrAtIndex(ES_PropertyIndex index)
{
    OP_ASSERT(index < GetData()->count);
    return reinterpret_cast<unsigned *>(GetData()->class_serials->Unbox())[index];
}

inline void
ES_Class_Hash_Base::SetSerialNrAtIndex(ES_PropertyIndex index, unsigned serial)
{
    OP_ASSERT(index < GetData()->count);
    reinterpret_cast<unsigned *>(GetData()->class_serials->Unbox())[index] = serial;
}

inline void
ES_Class_Hash_Base::CopyHashTableFrom(ES_Class_Hash_Base *klass)
{
    data = klass->data;
}

#endif // ES_CLASS_INLINES_H
