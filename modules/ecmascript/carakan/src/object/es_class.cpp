/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/object/es_class.h"

#ifdef ES_DEBUG_COMPACT_OBJECTS

void
ES_PrintAttributes(unsigned attributes)
{
    if (attributes)
    {
        printf(", flags = ");
        const char attrs[7][3] = { "RO", "DE", "DD", "SP", "CP", "FN", "HP" };

        for (unsigned bit = 0; bit < 7; ++bit)
        {
            if (((1 << bit) & attributes) != 0)
                printf("%s ", attrs[bit]);
        }
    }
}

const char *
StorageTypeString(ES_Class *klass, unsigned index, ES_StorageType storage_type)
{
    const char *type;
    switch (storage_type)
    {
        case ES_STORAGE_WHATEVER: type = "value"; break;
        case ES_STORAGE_NULL: type = "null"; break;
        case ES_STORAGE_UNDEFINED: type = "undefined"; break;
        case ES_STORAGE_BOOLEAN: type = "bool"; break;
        case ES_STORAGE_INT32: type = "int"; break;
        case ES_STORAGE_DOUBLE: type = "double"; break;
        case ES_STORAGE_STRING: type = "string"; break;
        case ES_STORAGE_STRING_OR_NULL: type = "string?"; break;
        case ES_STORAGE_OBJECT: type = "object"; break;
        case ES_STORAGE_OBJECT_OR_NULL: type = "object?"; break;
        case ES_STORAGE_BOXED: type = "boxed"; break;
        case ES_STORAGE_SPECIAL_PRIVATE: type = "private"; break;
        default: type = "ERROR: bad type"; break;
    }
    return type;
}

void
ES_PrintClassNonSuspended(ES_Context *context, ES_Class *klass, unsigned level)
{
    const char *class_type;
    if (klass->IsSingleton())
        class_type = "Singleton";
    else if (klass->IsCompact())
        class_type = "Compact";
    else if (klass->HasHashTableProperties())
        class_type = "Hash";
    else
        class_type = "";

    printf("%*s%sClass[%d] %p, id=0x%x:\n", level, "", class_type, unsigned(klass->LayoutLevel()), static_cast<void *>(klass), klass->Id());
    printf("%*s%ssibling=%p\n", level, "", "", static_cast<void *>(klass->sibling));
    printf("%*s%sproperties:", level, "", "");
    if (klass->property_table)
    {
        unsigned count = klass->property_table->Count();
        unsigned j = 0;
        for (unsigned i = 0; i < count; ++i)
        {
            printf("\n");
            ES_Layout_Info layout = klass->GetLayoutInfoAtIndex(ES_LayoutIndex(i));

            const char *type = StorageTypeString(klass, i, layout.GetStorageType());
            OpString8 string8;

            if (!layout.IsSpecialType())
            {
                JString *p = klass->property_table->properties->GetIdentifierAtIndex(j);
                string8.Set(Storage(context, p), Length(p));
            }
            else
                string8.Set("[[internal]]");


            printf("%*s[%d]@0x%04x", level, "", i, layout.GetOffset());

            printf(" %s%s : %s", (i < klass->LayoutLevel()) ? "" : "| ", string8.CStr(), type);

            if (!layout.IsSpecialType())
            {
                ES_PrintAttributes(klass->GetPropertyInfoAtIndex(ES_PropertyIndex(j)).Attributes());
                ++j;
            }
        }
    }
    printf("\n");

    if (klass->IsNode())
    {
        ES_Boxed *children;

        if (klass->IsCompact())
            children = static_cast<ES_Class_Compact_Node *>(klass)->children;
        else
            children = static_cast<ES_Class_Node *>(klass)->children;

        printf("%*s%schildren:", level, "", "");
        if (children)
        {
            if (children->GCTag() == GCTAG_ES_Identifier_Boxed_Hash_Table)
            {
                ES_Identifier_Boxed_Hash_Table *table = static_cast<ES_Identifier_Boxed_Hash_Table*>(children);
                for (unsigned i = 0; i < table->cells->size; ++i)
                {
                    const ES_IdentifierCell_Array::Cell &cell = table->cells->cells[i];
                    if (cell.key)
                    {
                        printf("\n");
                        OpString8 string8;
                        string8.Set(Storage(context, cell.key), Length(cell.key));
                        printf("%*s%s", level, "", string8.CStr());
                        ES_PrintAttributes(cell.key_fragment);
                        printf("\n");

                        ES_PrintClassNonSuspended(context, static_cast<ES_Class *>(table->array->slots[cell.value]), level + 4);
                    }
                }
            }
            else
            {
                printf("\n");
                ES_Class_Node_Base *node = static_cast<ES_Class_Node_Base *>(children);
                ES_PropertyIndex i = ES_PropertyIndex(node->Level() - 1);

                JString *key = node->GetNameAtIndex(i);

                OpString8 string8;
                string8.Set(Storage(context, key), Length(key));
                printf("%*s%s", level, "", string8.CStr());
                ES_PrintAttributes(node->GetPropertyInfoAtIndex(i).Attributes());
                printf("\n");

                ES_PrintClassNonSuspended(context, node, level + 4);
            }
        }
        else
            printf("\n");
    }
}


class ES_SuspendedPrintClass
    : public ES_SuspendedCall
{
public:
    ES_SuspendedPrintClass(ES_Execution_Context *context, ES_Class *klass)
        : klass(klass)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        ES_PrintClassNonSuspended(context, klass, 0);
    }

    ES_Class *klass;
};

void
ES_PrintClass(ES_Execution_Context *context, ES_Class *klass)
{
    if (context)
        ES_SuspendedPrintClass(context, klass);
    else
        ES_PrintClassNonSuspended(context, klass, 0);
}

#endif // ES_DEBUG_COMPACT_OBJECTS

/* static */ ES_Class_Data *
ES_Class_Data::Make(ES_Context *context, const char *object_name, JString *object_name_string, ES_Object *prototype, ES_Class *root)
{
    ES_Class_Data *data;
    GC_ALLOCATE(context, data, ES_Class_Data, (data, object_name, object_name_string, prototype, root));
    return data;
}

/* static */ void
ES_Class_Data::Initialize(ES_Class_Data *self, const char *object_name, JString *object_name_string, ES_Object *prototype, ES_Class *root)
{
    self->InitGCTag(GCTAG_ES_Class_Data);

    self->object_name = object_name;
    self->object_name_string = object_name_string;
    self->prototype = prototype;
    self->root = self->main = root;
    self->instance = NULL;
    self->compact = FALSE;
}

/* static */ ES_Class_Node *
ES_Class::MakeRoot(ES_Context *context, ES_Object *prototype, const char *object_name, BOOL force_new_root, unsigned size)
{
    GC_STACK_ANCHOR(context, prototype);
    if (!object_name)
        object_name = "Object";

    OP_ASSERT(op_strlen(object_name) < 64);

    uni_char name[64], *p = name;
    const char *p8 = object_name;
    unsigned length = 0;

    while ((*p++ = *p8++) != 0) ++length;

    JString *object_name_string = context->rt_data->GetSharedString(name, length);

    return ES_Class::MakeRoot(context, prototype, object_name, object_name_string, force_new_root, size);
}

/* static */ ES_Class_Node *
ES_Class::MakeRoot(ES_Context *context, ES_Object *prototype, const char *object_name, JString *object_name_string, BOOL force_new_root, unsigned size)
{
    GC_STACK_ANCHOR(context, prototype);
    OP_ASSERT(object_name_string);

    OP_ASSERT(op_strlen(object_name) < 64);

    ES_Class *node;
    if (!force_new_root && prototype && (node = prototype->FindInstance(context, object_name_string)))
    {
        if (node->IsNode() && !node->IsCompact())
            return static_cast<ES_Class_Node *>(node->GetRootClass());
    }

    ES_CollectorLock gclock(context);

    object_name_string = context->rt_data->GetSharedString(object_name_string);

    ES_Class_Node *klass = ES_Class_Node::Make(context, NULL);

    klass->static_data = ES_Class_Data::Make(context, object_name, object_name_string, prototype, klass);

    if (size != UINT_MAX)
        klass->property_table = ES_Property_Table::Make(context, size);

    return klass;
}

/* static */ ES_Class_Compact_Node *
ES_Class::MakeCompactRoot(ES_Context *context, ES_Object *prototype, const char *object_name, BOOL force_new_root, unsigned size)
{
    GC_STACK_ANCHOR(context, prototype);
    if (!object_name)
        object_name = "Object";

    OP_ASSERT(op_strlen(object_name) < 64);

    uni_char name[64], *p = name;
    const char *p8 = object_name;
    unsigned length = 0;

    while ((*p++ = *p8++) != 0) ++length;

    JString *object_name_string = context->rt_data->GetSharedString(name, length);

    return ES_Class::MakeCompactRoot(context, prototype, object_name, object_name_string, force_new_root, size);
}

/* static */ ES_Class_Compact_Node *
ES_Class::MakeCompactRoot(ES_Context *context, ES_Object *prototype, const char *object_name, JString *object_name_string, BOOL force_new_root, unsigned size)
{
    GC_STACK_ANCHOR(context, prototype);
    OP_ASSERT(object_name_string);

    OP_ASSERT(op_strlen(object_name) < 64);

    ES_Class *node;
    if (!force_new_root && prototype && (node = prototype->FindInstance(context, object_name_string)))
    {
        if (node->IsCompact())
            return static_cast<ES_Class_Compact_Node *>(node->GetRootClass());
    }

    ES_CollectorLock gclock(context);

    object_name_string = context->rt_data->GetSharedString(object_name_string);

    ES_Class_Compact_Node *klass = ES_Class_Compact_Node::Make(context, NULL);

    klass->static_data = ES_Class_Data::Make(context, object_name, object_name_string, prototype, klass);
    klass->children = ES_Identifier_Boxed_Hash_Table::Make(context, 4);

    if (size != UINT_MAX)
        klass->property_table = ES_Property_Table::Make(context, size);

    return klass;
}

/* static */ ES_Class_Singleton *
ES_Class::MakeSingleton(ES_Context *context, ES_Object *prototype, const char *object_name, unsigned size)
{
    GC_STACK_ANCHOR(context, prototype);
    if (!object_name)
        object_name = "Object";

    JString *object_name_string = context->rt_data->GetSharedString(object_name);

    return ES_Class::MakeSingleton(context, prototype, object_name, object_name_string, size);
}

/* static */ ES_Class_Singleton *
ES_Class::MakeSingleton(ES_Context *context, ES_Object *prototype, const char *object_name, JString *object_name_string, unsigned size)
{
    GC_STACK_ANCHOR(context, prototype);
    OP_ASSERT(object_name_string);

    ES_Class_Singleton *klass = ES_Class_Singleton::Make(context);
    ES_CollectorLock gclock(context);

    klass->static_data = ES_Class_Data::Make(context, object_name, object_name_string, prototype, klass);

    if (size != UINT_MAX)
        klass->property_table = ES_Property_Table::Make(context, size);

    return klass;
}

/* static */ ES_Class *
ES_Class::MakeHash(ES_Context *context, ES_Class *klass, unsigned count)
{
    if (klass->IsSingleton())
        return klass;

    OP_ASSERT(klass->GCTag() != GCTAG_ES_Class_Hash);

    ES_Class_Hash *hash_klass = ES_Class_Hash::Make(context, klass, count);
    ES_CollectorLock gclock(context);

    hash_klass->static_data = ES_Class_Data::Make(context, klass->ObjectName(), klass->ObjectName(context), klass->Prototype(), hash_klass);

    return hash_klass;
}

/* static */ void
ES_Class::Initialize(ES_Class *self)
{
    /* There are no instances of ES_Class */
    // self->InitGCTag(GCTAG_ES_Class);

    self->level = 0;
    self->need_limit_check = 0;
    self->has_enumerable_properties = 0;
    self->class_id = INVALID_CLASS_ID;

    self->property_table = NULL;
    self->static_data = NULL;
    self->sibling = NULL;
    self->extra = NULL;
}

/* static */ ES_Class *
ES_Class::ExtendWithL(ES_Context *context, ES_Class *this_class, JString *name, ES_Property_Info info, ES_StorageType type, ES_LayoutIndex position, BOOL hide_existing)
{
    switch (this_class->GCTag())
    {
    case GCTAG_ES_Class_Node:
        this_class = static_cast<ES_Class_Node *>(this_class)->ExtendWithL(context, name, info, type, hide_existing);
        break;
    case GCTAG_ES_Class_Compact_Node:
    case GCTAG_ES_Class_Compact_Node_Frozen:
        while (static_cast<ES_Class_Compact_Node *>(this_class)->parent && static_cast<ES_Class_Compact_Node *>(this_class)->parent->LayoutLevel() >= position)
            this_class = static_cast<ES_Class_Compact_Node *>(this_class)->parent;

        this_class = static_cast<ES_Class_Compact_Node *>(this_class)->ExtendWithL(context, name, info, type, position, hide_existing);
        break;
    case GCTAG_ES_Class_Singleton:
        static_cast<ES_Class_Singleton *>(this_class)->AddL(context, name, info, type, hide_existing);
        break;
    case GCTAG_ES_Class_Hash:
        static_cast<ES_Class_Hash*>(this_class)->ExtendWithL(context, name, info, type, position, hide_existing);
        break;
    default:
        OP_ASSERT(!"Bad GCTAG");
    }

    OP_ASSERT(position == UINT_MAX || hide_existing || this_class->Find(name, info, position + 1));

    return this_class;
}

/* static */ ES_Class *
ES_Class::ExtendWithL(ES_Context *context, ES_Class *this_class, ES_StorageType type, ES_LayoutIndex position)
{
    switch (this_class->GCTag())
    {
    case GCTAG_ES_Class_Node:
        this_class = static_cast<ES_Class_Node *>(this_class)->ExtendWithL(context, type);
        break;
    case GCTAG_ES_Class_Compact_Node:
    case GCTAG_ES_Class_Compact_Node_Frozen:
        while (static_cast<ES_Class_Compact_Node *>(this_class)->parent && static_cast<ES_Class_Compact_Node *>(this_class)->parent->LayoutLevel() >= position)
            this_class = static_cast<ES_Class_Compact_Node *>(this_class)->parent;

        this_class = static_cast<ES_Class_Compact_Node *>(this_class)->ExtendWithL(context, type, position);
        break;
    case GCTAG_ES_Class_Singleton:
        static_cast<ES_Class_Singleton *>(this_class)->AddL(context, type);
        break;
    case GCTAG_ES_Class_Hash:
        static_cast<ES_Class_Hash*>(this_class)->ExtendWithL(context, type, position);
        break;
    default:
        OP_ASSERT(!"Bad GCTAG");
    }

    return this_class;
}

/* static */ ES_Class *
ES_Class::DeleteL(ES_Context *context, ES_Class *this_class, JString *name, unsigned length, BOOL &needs_conversion)
{
    switch (this_class->GCTag())
    {
    case GCTAG_ES_Class_Node:
        this_class = static_cast<ES_Class_Node *>(this_class)->DeleteL(context, name, needs_conversion);
        break;
    case GCTAG_ES_Class_Compact_Node:
    case GCTAG_ES_Class_Compact_Node_Frozen:
        this_class = static_cast<ES_Class_Compact_Node *>(this_class)->DeleteL(context, name, length, needs_conversion);
        break;
    case GCTAG_ES_Class_Singleton:
        static_cast<ES_Class_Singleton *>(this_class)->Remove(context, name);
        needs_conversion = FALSE;
        break;
    case GCTAG_ES_Class_Hash:
        static_cast<ES_Class_Hash *>(this_class)->DeleteL(context, name, length, needs_conversion);
        break;
    default:
        OP_ASSERT(!"Bad GCTAG");
    }

    return this_class;
}

/* static */ ES_Class *
ES_Class::ChangeAttributeFromTo(ES_Context *context, ES_Class *this_class, unsigned set_attributes, unsigned clear_attributes, ES_PropertyIndex from, ES_PropertyIndex to, unsigned count, BOOL &needs_conversion, ES_Class *onto, ES_LayoutIndex onto_position)
{
    switch (this_class->GCTag())
    {
    case GCTAG_ES_Class_Node:
    case GCTAG_ES_Class_Compact_Node:
    case GCTAG_ES_Class_Compact_Node_Frozen:
        this_class = static_cast<ES_Class_Node_Base *>(this_class)->ChangeAttributeFromTo(context, set_attributes, clear_attributes, from, to, count, needs_conversion, onto, onto_position);
        break;
    case GCTAG_ES_Class_Singleton:
        static_cast<ES_Class_Singleton *>(this_class)->ChangeAttributeFromTo(context, set_attributes, clear_attributes, from, to);
        needs_conversion = FALSE;
        break;
    case GCTAG_ES_Class_Hash:
        static_cast<ES_Class_Hash *>(this_class)->ChangeAttributeFromTo(context, set_attributes, clear_attributes, from, to, count, needs_conversion, onto, onto_position);
        break;
    default:
        OP_ASSERT(!"Bad GCTAG");
    }

    return this_class;
}

/* static */ ES_Class *
ES_Class::ChangeType(ES_Context *context, ES_Class *this_class, ES_PropertyIndex index, ES_StorageType type, unsigned count, BOOL &needs_conversion)
{
    switch (this_class->GCTag())
    {
    case GCTAG_ES_Class_Node:
        this_class = static_cast<ES_Class_Node *>(this_class)->ChangeType(context, index, type, needs_conversion);
        break;
    case GCTAG_ES_Class_Compact_Node:
    case GCTAG_ES_Class_Compact_Node_Frozen:
        this_class = static_cast<ES_Class_Compact_Node *>(this_class)->ChangeType(context, index, type, count, needs_conversion);
        break;
    case GCTAG_ES_Class_Singleton:
        static_cast<ES_Class_Singleton *>(this_class)->ChangeType(context, index, type);
        needs_conversion = FALSE;
        break;
    case GCTAG_ES_Class_Hash:
        static_cast<ES_Class_Hash *>(this_class)->ChangeType(context, index, type, count, needs_conversion);
        break;
    default:
        OP_ASSERT(!"Bad GCTAG");
    }

    return this_class;
}

/* static */ unsigned
ES_Class::ChildCount(ES_Class *this_class)
{
    switch (this_class->GCTag())
    {
    case GCTAG_ES_Class_Node: return static_cast<ES_Class_Node *>(this_class)->ChildCount();
    case GCTAG_ES_Class_Compact_Node:
    case GCTAG_ES_Class_Compact_Node_Frozen:
        return static_cast<ES_Class_Compact_Node *>(this_class)->ChildCount();
    case GCTAG_ES_Class_Singleton: return 0;
    case GCTAG_ES_Class_Hash: return ES_Class::ChildCount(static_cast<ES_Class_Hash *>(this_class)->klass);
    default:
        OP_ASSERT(!"Bad GCTAG");
        return 0;
    }
}

/* static */ unsigned
ES_Class::MaxSize(ES_Class *this_class)
{
    switch (this_class->GCTag())
    {
    case GCTAG_ES_Class_Node:
    case GCTAG_ES_Class_Compact_Node:
    case GCTAG_ES_Class_Compact_Node_Frozen:
        return static_cast<ES_Class_Node_Base *>(this_class)->MaxSize();
    case GCTAG_ES_Class_Singleton: return this_class->SizeOf(this_class->LayoutLevel());
    case GCTAG_ES_Class_Hash: return ES_Class::MaxSize(static_cast<ES_Class_Hash *>(this_class)->klass);
    default:
        OP_ASSERT(!"Bad GCTAG");
        return 0;
    }
}

/* static */ ES_Class *
ES_Class::Branch(ES_Context *context, ES_Class *this_class, ES_LayoutIndex position)
{
    switch (this_class->GCTag())
    {
    case GCTAG_ES_Class_Compact_Node:
    case GCTAG_ES_Class_Compact_Node_Frozen:
        while (static_cast<ES_Class_Compact_Node *>(this_class)->parent && static_cast<ES_Class_Compact_Node *>(this_class)->parent->LayoutLevel() >= position)
            this_class = static_cast<ES_Class_Compact_Node *>(this_class)->parent;

        this_class = static_cast<ES_Class_Compact_Node *>(this_class)->Branch(context, position);
        break;
    default:
        break;
    }

    return this_class;
}

/* static */ ES_Class *
ES_Class::ActualClass(ES_Class *this_class)
{
    while (TRUE)
        switch (this_class->GCTag())
        {
        case GCTAG_ES_Class_Node:
        case GCTAG_ES_Class_Compact_Node:
        case GCTAG_ES_Class_Compact_Node_Frozen:
        case GCTAG_ES_Class_Singleton:
            return this_class;
        case GCTAG_ES_Class_Hash:
            this_class = static_cast<ES_Class_Hash *>(this_class)->klass;
            break;
        default:
            OP_ASSERT(!"Bad GCTAG");
            return NULL;
        }
}

/* static */ ES_Class *
ES_Class::SetPropertyCount(ES_Class *this_class, unsigned count)
{
    switch (this_class->GCTag())
    {
    case GCTAG_ES_Class_Node:
    case GCTAG_ES_Class_Compact_Node:
    case GCTAG_ES_Class_Compact_Node_Frozen:
        {
            ES_Class_Node_Base *node = static_cast<ES_Class_Node_Base *>(this_class);
            while (node->parent && node->parent->LayoutLevel() >= count)
                node = node->parent;
            this_class = node;
            break;
        }
    default:
        OP_ASSERT(!"Bad GCTAG, expected node based class");
        return NULL;
    }
    return this_class;
}

/* static */ void
ES_Class::InvalidateTree(ES_Class *tree)
{
    if (tree)
    {
        switch (tree->GCTag())
        {
        case GCTAG_ES_Class_Node:
        case GCTAG_ES_Class_Compact_Node:
        case GCTAG_ES_Class_Compact_Node_Frozen:
            for (ES_Class *invalid = tree->static_data->RootClass(); invalid; invalid = invalid->Next())
                invalid->class_id = INVALID_CLASS_ID;
            break;
        case GCTAG_ES_Class_Singleton:
            tree->class_id = INVALID_CLASS_ID;
            break;
        case GCTAG_ES_Class_Hash:
            tree->class_id = INVALID_CLASS_ID;
            ES_Class::InvalidateTree(static_cast<ES_Class_Hash*>(tree)->klass);
            return;
        default:
            OP_ASSERT(!"Bad GCTAG");
        }

        tree->static_data->InvalidateSubClasses();
    }
}

ES_Class *
ES_Class::CopyProperties(ES_Context *context, ES_Class *from_class, ES_LayoutIndex first, ES_LayoutIndex last, ES_PropertyIndex index, ES_LayoutIndex to, BOOL &needs_conversion, BOOL hide_existing)
{
    OP_ASSERT(last == 0 || last <= from_class->property_table->Count());

    ES_Class *node = this;

    for (unsigned i = first, j = index, k = to; i < last; ++i, ++k)
    {
        ES_Layout_Info layout = from_class->GetLayoutInfoAtIndex(ES_LayoutIndex(i));

        if (layout.IsSpecialType())
            node = ES_Class::ExtendWithL(context, node, layout.GetStorageType());
        else
        {
            ES_Property_Info info = from_class->GetPropertyInfoAtIndex(ES_PropertyIndex(j));
            OP_ASSERT(ES_PropertyIndex(j) <= from_class->Level());
            node = ES_Class::ExtendWithL(context, node, from_class->GetNameAtIndex(ES_PropertyIndex(j)), info, layout.GetStorageType(), ES_LayoutIndex(k), hide_existing);
            ++j;
        }

        if (!needs_conversion)
        {
            ES_Layout_Info new_layout = node->GetLayoutInfoAtIndex(ES_LayoutIndex(k));
            needs_conversion = new_layout.GetStorageType() != layout.GetStorageType() || new_layout.GetOffset() != layout.GetOffset();
        }
    }

    return node;
}

void
ES_Class::AdjustOffsetAtIndex(ES_LayoutIndex index)
{
    OP_ASSERT(IsSingleton());
    unsigned offset = property_table->CalculateAlignedOffset(GetLayoutInfoAtIndex(index).GetStorageType(), index);
    GetLayoutInfoAtIndex_ref(index).SetOffset(offset);
}

void
ES_Class::AdjustOffset(ES_LayoutIndex index)
{
    OP_ASSERT(IsSingleton());
    property_table->AdjustOffset(index);
}

static BOOL
CanUpdateTypeInplace(ES_StorageType type)
{
    return ES_Layout_Info::IsNullable(type);
}

static ES_StorageType
ResolveConflictingType(ES_StorageType old_type, ES_StorageType new_type)
{
    if (old_type == ES_STORAGE_NULL || new_type == ES_STORAGE_NULL)
    {
        if (old_type == ES_STORAGE_OBJECT || new_type == ES_STORAGE_OBJECT)
            return ES_STORAGE_OBJECT_OR_NULL;
        else if (old_type == ES_STORAGE_STRING || new_type == ES_STORAGE_STRING)
            return ES_STORAGE_STRING_OR_NULL;
    }
    else if (old_type == ES_STORAGE_INT32 && new_type == ES_STORAGE_DOUBLE)
        return ES_STORAGE_DOUBLE;

    return ES_STORAGE_WHATEVER;
}

static BOOL
FindChild(ES_Identifier_Boxed_Hash_Table *children, JString *name, ES_Property_Info info, ES_StorageType &type, ES_PropertyIndex &index, ES_Class_Node_Base *&klass)
{
    ES_Boxed *found_class = klass = NULL;

    if (static_cast<ES_Identifier_Boxed_Hash_Table *>(children)->Find(name, info.Attributes(), found_class))
    {
        klass = static_cast<ES_Class_Node_Base *>(found_class);
        index = klass->IndexOf(name);
        ES_Layout_Info layout = klass->GetLayoutInfoAtInfoIndex(index);
        if (!layout.CheckType(type))
        {
            type = ResolveConflictingType(layout.GetStorageType(), type);
            return FALSE;
        }
        else
            return TRUE;
    }

    return FALSE;
}

/* ES_Class_Extra */

/* static */ ES_Class_Extra *
ES_Class_Extra::Make(ES_Context *context, unsigned index, ES_Class *klass, ES_Class_Extra *next)
{
    ES_Class_Extra *self;
    GC_ALLOCATE(context, self, ES_Class_Extra, (self, index, klass, next));
    return self;
}

/* static */ void
ES_Class_Extra::Initialize(ES_Class_Extra  *self, unsigned index, ES_Class *klass, ES_Class_Extra *next)
{
    self->InitGCTag(GCTAG_ES_Class_Extra);

    self->level = index;

    if (next)
    {
        OP_ASSERT(next->LayoutLevel() <= self->LayoutLevel());
        self->length = next->LayoutLevel() < self->LayoutLevel() ? next->length + 1 : next->length;
    }
    else
        self->length = 1;

    self->klass = klass;
    self->next  = next;
}

/* ES_Class_Node_Base */

/* static */ void
ES_Class_Node_Base::Initialize(ES_Class_Node_Base *self, ES_Class_Node_Base *parent)
{
    /* Not a heap object on its own; common initialisation for Node_Base-based classes. */
    if (parent)
    {
        self->level = parent->level + 1;
        self->has_enumerable_properties = parent->has_enumerable_properties;
        self->property_table = parent->property_table;
        self->static_data = parent->static_data;
    }
    else
    {
        self->level = 0;
        self->has_enumerable_properties = 0;
        self->property_table = NULL;
        self->static_data = NULL;
    }

    self->need_limit_check = 0;
    self->class_id = INVALID_CLASS_ID;
    self->sibling = NULL;
    self->extra = NULL;
    self->parent = parent;
}

void
ES_Class_Node_Base::AddChild(ES_Context *context, ES_Identifier_Boxed_Hash_Table *children, JString *name, ES_Property_Info info, ES_Class_Node_Base *new_klass, ES_Class_Node_Base *old)
{
    OP_ASSERT(new_klass->sibling == NULL || new_klass->sibling == LastChild() && children->GetCount() == 0 || new_klass->sibling != NULL && new_klass->GCTag() == GCTAG_ES_Class_Node);
    ES_Boxed **box;
    if (children->FindLocation(name, info.Attributes(), box))
    {
        OP_ASSERT(new_klass->sibling == NULL);
        ES_Class_Node_Base *old_klass = static_cast<ES_Class_Node_Base *>(*box);

        if (GetMainClass() != NULL)
            static_data->SetMainClass(this);

        if (old_klass != LastChild())
        {
            new_klass->sibling = old_klass->sibling;
            old_klass->sibling = new_klass;
        }
        else
            new_klass->sibling = old_klass;

        *box = new_klass;

        old_klass->InvalidateBranch();
    }
    else
    {
        if (new_klass->sibling == NULL)
            new_klass->sibling = LastChild();
        children->AddL(context, name, info.Attributes(), new_klass);
    }

    if (old)
    {
        ES_Class *ptr = LastChild();

        while (ptr->sibling != old)
            ptr = ptr->sibling;

        OP_ASSERT(ptr != NULL);

        ptr->sibling = old->sibling;
        old->sibling = NULL;
    }

    new_klass->AddExtra(new_klass->parent->extra);
}

ES_Class_Node_Base *
ES_Class_Node_Base::UpdateTypeInplace(ES_LayoutIndex index, ES_StorageType type)
{
#ifdef ES_DEBUG_COMPACT_OBJECTS
#ifdef _STANDALONE
    extern unsigned g_total_type_changes;
    g_total_type_changes++;

    extern BOOL g_debug_type_changes;
    if (g_debug_type_changes)
    {
        JString *name = GetNameAtIndex(ES_PropertyIndex(index - CountExtra()));
        ES_StorageType type0 = GetLayoutInfoAtIndex(index).GetStorageType();
        OpString8 string8;
        string8.Set(Storage(NULL, name), Length(name));

        printf("%s ", string8.CStr());

        printf("id: %d -> ?, ", Id());
        printf("type: %s -> %s (%s)\n", StorageTypeString(this, index, type0),
            StorageTypeString(this, index, type), StorageTypeString(this, index, type));
    }
#endif // _STANDALONE
#endif // ES_DEBUG_COMPACT_OBJECTS

    OP_ASSERT(ES_Layout_Info::IsNullable(type));

    ES_Class *node = this;

    while (node->Parent() && node->Parent()->LayoutLevel() > index)
        node = node->Parent();

    ES_Class *break_node = node;

    OP_ASSERT(node->Parent() == NULL || node->Parent()->LayoutLevel() <= index);

    while (node)
    {
        node->GetLayoutInfoAtIndex_ref(index).SetStorageType(type);
        node->class_id = INVALID_CLASS_ID;
        node = node->Next(break_node);
    }

    return this;
}

void
ES_Class_Node_Base::ClearMainClass(ES_Context *context)
{
    if (!static_data->MainClass())
        return;

    ES_Class_Node_Base *ptr0 = parent, *ptr1 = this;

    while (ptr0 != NULL)
    {
        if (ES_Class::ChildCount(ptr0) == 1 && ptr0->LastChild() != ptr1)
            return;

        ptr1 = ptr0;
        ptr0 = ptr0->parent;
    }

    static_data->SetMainClass(NULL);
}

void
ES_Class_Node_Base::AddSibling(ES_Class *new_sibling)
{
    OP_ASSERT(!new_sibling->sibling);
    new_sibling->sibling = sibling;
    sibling = new_sibling;
}

void
ES_Class_Node_Base::AddExtra(ES_Class_Extra *data)
{
    while (data && data->LayoutLevel() >= LayoutLevel())
        data = data->next;
    if (data)
        extra = data;
}

unsigned
ES_Class_Node_Base::MaxSize()
{
    unsigned size = 0;

    for (ES_Class *node = this; node; node = node->Next(this))
    {
        if (ES_Class::ChildCount(node) == 0)
            size = es_maxu(size, node->SizeOf(node->LayoutLevel()));
    }

    return size;
}

ES_Class_Node_Base *
ES_Class_Node_Base::ChangeAttributeFromTo(ES_Context *context, unsigned set_attributes, unsigned clear_attributes, ES_PropertyIndex from, ES_PropertyIndex to, unsigned count, BOOL &needs_conversion, ES_Class *onto, ES_LayoutIndex onto_position)
{
    ES_Class *node = NULL;
    ES_LayoutIndex layout_index = GetPropertyInfoAtIndex(from).Index();
    ES_PropertyIndex name_index(from);

    if (onto)
        node = onto;
    else
    {
        node = this;
        onto_position = ES_LayoutIndex(layout_index);
    }

    while (node->Parent() && node->LayoutLevel() > layout_index)
        node = node->Parent();

    while (name_index > 0 && GetPropertyInfoAtIndex(name_index).Index() > node->LayoutLevel())
        name_index = ES_PropertyIndex(name_index - 1);

    ES_CollectorLock gclock(context);

    if (onto == NULL)
        node = node->CopyProperties(context, this, node->LayoutLevel(), GetPropertyInfoAtIndex(from).Index(), name_index, node->LayoutLevel(), needs_conversion);

    BOOL has_changed = FALSE;
    ES_LayoutIndex last = GetPropertyInfoAtIndex(to).Index();
    for (unsigned i = layout_index, j = from, k = onto_position; i <= last; ++i, ++k)
    {
        ES_Layout_Info layout = GetLayoutInfoAtIndex(ES_LayoutIndex(i));

        if (layout.IsSpecialType())
            node = ES_Class::ExtendWithL(context, node, layout.GetStorageType(), ES_LayoutIndex(k));
        else
        {
            ES_StorageType type = GetLayoutInfoAtIndex(ES_LayoutIndex(i)).GetStorageType();
            JString *name = GetNameAtIndex(ES_PropertyIndex(j));
            ES_Property_Info info = GetPropertyInfoAtIndex(ES_PropertyIndex(j));
            info.ChangeAttributes(set_attributes, clear_attributes);
            node = ES_Class::ExtendWithL(context, node, name, info, type, ES_LayoutIndex(k));
            has_changed = has_changed || GetPropertyInfoAtIndex(ES_PropertyIndex(j)).Attributes() != node->GetPropertyInfoAtIndex(ES_PropertyIndex(j)).Attributes();
            ++j;
        }

        needs_conversion = needs_conversion || node->GetLayoutInfoAtIndex(ES_LayoutIndex(k)).GetStorageType() != layout.GetStorageType();
    }

    if (last < count)
        node = node->CopyProperties(context, this, ES_LayoutIndex(last + 1), LayoutLevel(), ES_PropertyIndex(to + 1), ES_LayoutIndex(last + 1), needs_conversion);

    if (has_changed)
        InvalidateInstances();

    return static_cast<ES_Class_Node_Base *>(node);
}

/* ES_Class_Node */

/* static */ ES_Class_Node *
ES_Class_Node::Make(ES_Context *context, ES_Class_Node *parent)
{
    ES_Class_Node *klass;
    GC_ALLOCATE(context, klass, ES_Class_Node, (klass, parent));
    return klass;
}

/* static */ void
ES_Class_Node::Initialize(ES_Class_Node *self, ES_Class_Node *parent)
{
    self->InitGCTag(GCTAG_ES_Class_Node);
    ES_Class_Node_Base::Initialize(self, parent);
    self->children = NULL;
}

ES_Class_Node *
ES_Class_Node::ExtendWithL(ES_Context *context, JString *name, ES_Property_Info info, ES_StorageType type, BOOL hide_existing)
{
    OP_ASSERT(name != NULL);
    ES_Class_Node_Base *klass = NULL;
    ES_Class_Node *new_class = NULL;

    info.data.class_property = 1;

    unsigned attributes = info.Attributes();

    ES_CollectorLock gclock(context);

    if (children)
    {
        if (children->GCTag() == GCTAG_ES_Identifier_Boxed_Hash_Table)
        {
            ES_PropertyIndex index;

            /* FindChild will resolve a conflicting type if a child with the correct name and attributes
               is found but with the wrong type. */
            if (FindChild(static_cast<ES_Identifier_Boxed_Hash_Table *>(children), name, info, type, index, klass))
                return static_cast<ES_Class_Node *>(klass);

            if (klass && CanUpdateTypeInplace(type))
            {
                ES_Class_Node *node = static_cast<ES_Class_Node *>(klass);
                return static_cast<ES_Class_Node *>(node->UpdateTypeInplace(node->GetPropertyInfoAtIndex(index).Index(), type));
            }
        }
        else
        {
            ES_Class_Node *child_class = static_cast<ES_Class_Node *>(children);
            ES_Property_Info child_info;
            JString *child_name;

            if (child_class->Lookup(Level(), child_name, child_info) && child_info.Index() == LayoutLevel() && attributes == child_info.Attributes() && child_name->Equals(name))
            {
                ES_Layout_Info layout = child_class->GetLayoutInfoAtIndex(child_info.Index());
                if (layout.CheckType(type))
                    return child_class;

                type = ResolveConflictingType(layout.GetStorageType(), type);

                if (CanUpdateTypeInplace(type))
                    return static_cast<ES_Class_Node *>(child_class->UpdateTypeInplace(LayoutLevel(), type));

                new_class = ES_Class_Node::Make(context, this);
                new_class->sibling = child_class;
                children = new_class;

                /* Usually the 'extra' is added through AddChild, but since we skip
                   AddChild we need to add the 'extra' manually. */
                new_class->AddExtra(extra);
            }
            else
            {
                children = ES_Identifier_Boxed_Hash_Table::Make(context, 4);
                JString *child_name = child_class->GetNameAtIndex(Level());
                child_info = child_class->GetPropertyInfoAtIndex(Level());
                AddChild(context, static_cast<ES_Identifier_Boxed_Hash_Table *>(children), child_name, child_info, child_class);
            }
        }

        if (children->GCTag() == GCTAG_ES_Identifier_Boxed_Hash_Table)
        {
            new_class = ES_Class_Node::Make(context, this);
            AddChild(context, static_cast<ES_Identifier_Boxed_Hash_Table *>(children), name, info, new_class);
        }

        if (property_table)
            new_class->property_table = property_table->CopyL(context, LayoutLevel(), Level());
        else
            new_class->property_table = ES_Property_Table::Make(context, 4);
    }
    else
    {
        ES_Class *last_child = LastChild();

        children = new_class = ES_Class_Node::Make(context, this);

        new_class->sibling = last_child;

        if (!property_table)
            new_class->property_table = ES_Property_Table::Make(context, 4);

        /* Usually the 'extra' is added through AddChild, but since we skip
           AddChild we need to add the 'extra' manually. */
        new_class->AddExtra(extra);

        OP_ASSERT(new_class->property_table->Count() == LayoutLevel());
    }

    if (!info.IsDontEnum())
        new_class->has_enumerable_properties = 1;

#ifdef DEBUG_ENABLE_OPASSERT
    BOOL added = new_class->property_table->AppendL(context, name, info.Attributes(), type, hide_existing);
    OP_ASSERT(added);
#else // DEBUG_ENABLE_OPASSERT
    new_class->property_table->AppendL(context, name, info.Attributes(), type, hide_existing);
#endif // DEBUG_ENABLE_OPASSERT

    if (static_data->MainClass() == this)
        static_data->SetMainClass(new_class);
    else
        ClearMainClass(context);

    OP_ASSERT(!extra || new_class->extra || extra->LayoutLevel() >= new_class->LayoutLevel());

    return new_class;
}

ES_Class_Node *
ES_Class_Node::ExtendWithL(ES_Context *context, ES_StorageType type)
{
    ES_Class_Extra *ptr = extra;

    ES_LayoutIndex layout_index = LayoutLevel();

    while (ptr && ptr->LayoutLevel() > layout_index)
    {
        OP_ASSERT(ptr->LayoutLevel() == layout_index + 1);

        ES_Layout_Info layout = ptr->klass->GetLayoutInfoAtIndex(layout_index);

        if (type == layout.GetStorageType())
            return static_cast<ES_Class_Node *>(ptr->klass);

        ptr = ptr->next;
    }

    ES_Class_Node *new_class = ES_Class_Node::Make(context, this);
    ES_CollectorLock gclock(context);

    if (property_table)
    {
        OP_ASSERT(LayoutLevel() <= property_table->Count());
        new_class->property_table = property_table->CopyL(context, LayoutLevel(), Level());
    }
    else
        new_class->property_table = ES_Property_Table::Make(context, 4);

    unsigned index;

#ifdef DEBUG_ENABLE_OPASSERT
    BOOL added = new_class->property_table->AppendL(context, index, type);
    OP_ASSERT(added);
#else // DEBUG_ENABLE_OPASSERT
    new_class->property_table->AppendL(context, index, type);
#endif // DEBUG_ENABLE_OPASSERT

    ES_Class_Node_Base *last_child = static_cast<ES_Class_Node_Base *>(LastChild());

    new_class->extra = extra = ES_Class_Extra::Make(context, index + 1, new_class, extra);
    if (last_child)
        last_child->AddSibling(new_class);

    return new_class;
}

ES_Class_Node *
ES_Class_Node::DeleteL(ES_Context *context, JString *name, BOOL &needs_conversion)
{
    unsigned index;
    ES_Class *node = this;

    ES_CollectorLock gclock(context);
    if (property_table && property_table->properties->IndexOf(name, index))
    {
        ES_LayoutIndex layout_index = GetPropertyInfoAtIndex(ES_PropertyIndex(index)).Index();
        while (node->Parent() && node->LayoutLevel() > layout_index)
            node = node->Parent();

        node = node->CopyProperties(context, this, ES_LayoutIndex(layout_index + 1), LayoutLevel(), ES_PropertyIndex(index + 1), ES_LayoutIndex(layout_index), needs_conversion);

        InvalidateInstances();
    }

    ClearMainClass(context);

    return static_cast<ES_Class_Node *>(node);
}

ES_Class_Node *
ES_Class_Node::ChangeType(ES_Context *context, ES_PropertyIndex index, ES_StorageType type, BOOL &needs_conversion)
{
    ES_Property_Info info = GetPropertyInfoAtIndex(index);
    ES_Layout_Info layout = property_table->GetLayoutInfoAtIndex(info.Index());

    OP_ASSERT(!layout.IsSpecialType());

    if (layout.GetStorageType() == type)
        return this;

    type = ResolveConflictingType(layout.GetStorageType(), type);

    if (CanUpdateTypeInplace(type))
    {
        InvalidateInstances();
        return static_cast<ES_Class_Node *>(UpdateTypeInplace(info.Index(), type));
    }

    ES_Class *node = this;

    while (node->LayoutLevel() > info.Index())
        node = node->Parent();

    ES_CollectorLock gclock(context);

    node = ES_Class::ExtendWithL(context, node, GetNameAtIndex(index), info, type);

    needs_conversion = needs_conversion || node->GetLayoutInfo(info).GetStorageType() != layout.GetStorageType();

    if (info.Index() < LayoutLevel())
        node = node->CopyProperties(context, this, ES_LayoutIndex(info.Index() + 1), LayoutLevel(), ES_PropertyIndex(index + 1), ES_LayoutIndex(info.Index() + 1), needs_conversion);

    InvalidateInstances();
    return static_cast<ES_Class_Node *>(node);
}

/* ES_Class_Compact_Node */

/* static */ ES_Class_Compact_Node *
ES_Class_Compact_Node::Make(ES_Context *context, ES_Class_Compact_Node *parent)
{
    ES_Class_Compact_Node *klass;
    GC_ALLOCATE(context, klass, ES_Class_Compact_Node, (klass, parent));
    return klass;
}

/* static */ void
ES_Class_Compact_Node::Initialize(ES_Class_Compact_Node *self, ES_Class_Compact_Node *parent)
{
    self->InitGCTag(GCTAG_ES_Class_Compact_Node);
    ES_Class_Node_Base::Initialize(self, parent);
    self->children = NULL;
}

ES_Class_Compact_Node *
ES_Class_Compact_Node::ExtendWithL(ES_Context *context, JString *name, ES_Property_Info info, ES_StorageType type, ES_LayoutIndex position, BOOL hide_existing)
{
    OP_ASSERT(position != UINT_MAX);
    OP_ASSERT(name);

    ES_Class_Node_Base *klass = NULL;
    ES_Class_Compact_Node *new_class = NULL;

    info.data.class_property = 1;

    ES_CollectorLock gclock(context);

    if (position < LayoutLevel())
    {
        OP_ASSERT(property_table);
        OP_ASSERT(parent);
        OP_ASSERT(property_table->Count() > position);
        ES_Property_Info info0;
        ES_StorageType type0;
        JString *name0;

        ES_PropertyIndex index(position - CountExtra());
        property_table->Lookup(index, name0, info0);
        type0 = property_table->GetLayoutInfoAtIndex(info0.Index()).GetStorageType();

        need_limit_check = 1;

        if (name->Equals(name0) && info.Attributes() == info0.Attributes())
        {
            if (ES_Layout_Info::CheckType(type, type0))
                return this;

            type = ResolveConflictingType(type0, type);

            if (CanUpdateTypeInplace(type))
                return static_cast<ES_Class_Compact_Node *>(UpdateTypeInplace(info0.Index(), type));

            JString *branch_name;
            ES_Property_Info branch_info;
            property_table->Lookup(parent->Level(), branch_name, branch_info);

            new_class = ES_Class_Compact_Node::Make(context, static_cast<ES_Class_Compact_Node *>(Parent()));

            parent->AddChild(context, static_cast<ES_Class_Compact_Node *>(parent)->children, branch_name, branch_info, new_class);

            new_class->level = position + 1;
            new_class->property_table = property_table->CopyL(context, position, position);
        }
        else
        {
            JString *branch_name;
            ES_Property_Info branch_info;
            property_table->Lookup(parent->Level(), branch_name, branch_info);

            ES_Class_Compact_Node *branch_node = ES_Class_Compact_Node::Make(context, static_cast<ES_Class_Compact_Node *>(Parent()));

            parent->AddChild(context, static_cast<ES_Class_Compact_Node *>(parent)->children, branch_name, branch_info, branch_node, this);

            OP_ASSERT(sibling == NULL);

            branch_node->children = ES_Identifier_Boxed_Hash_Table::Make(context, 4);
            branch_node->level = position;
            branch_node->has_enumerable_properties = has_enumerable_properties;
            branch_node->property_table = property_table;

            parent = branch_node;
            new_class = ES_Class_Compact_Node::Make(context, branch_node);
            new_class->property_table = property_table->CopyL(context, position, position);
            new_class->has_enumerable_properties = has_enumerable_properties;

            branch_node->AddChild(context, branch_node->children, name0, info0, this);
            branch_node->AddChild(context, branch_node->children, name, info, new_class);

            OP_ASSERT(!name->Equals(name0) || info.Attributes() != info0.Attributes());
            OP_ASSERT(new_class->Level() > branch_node->Level());
            OP_ASSERT(Level() > branch_node->Level());
            OP_ASSERT(branch_node->Level() > branch_node->parent->Level());
        }
    }
    else
    {
        ES_PropertyIndex index(0);

        /* FindChild will resolve a conflicting type if a child with the correct name and attributes
           is found but with the wrong type. */
        if (children && FindChild(children, name, info, type, index, klass))
            return static_cast<ES_Class_Compact_Node *>(klass);

        if (klass && CanUpdateTypeInplace(type))
        {
            ES_Class_Compact_Node *node = static_cast<ES_Class_Compact_Node *>(klass);
            return static_cast<ES_Class_Compact_Node *>(node->UpdateTypeInplace(node->GetPropertyInfoAtIndex(index).Index(), type));
        }

        if (children)
        {
            new_class = ES_Class_Compact_Node::Make(context, this);

            if (property_table)
                new_class->property_table = property_table->CopyL(context, position, position);

            AddChild(context, children, name, info, new_class);
        }
        else if (ShouldBranch())
        {
            children = ES_Identifier_Boxed_Hash_Table::Make(context, 4);
            new_class = ES_Class_Compact_Node::Make(context, this);

            new_class->property_table = property_table;

            AddChild(context, children, name, info, new_class);
        }
        else if (GCTag() == GCTAG_ES_Class_Compact_Node_Frozen)
        {
            OP_ASSERT(!children);
            children = ES_Identifier_Boxed_Hash_Table::Make(context, 4);

            ChangeGCTag(GCTAG_ES_Class_Compact_Node);

            new_class = ES_Class_Compact_Node::Make(context, this);
            new_class->property_table = property_table;

            AddChild(context, children, name, info, new_class);
        }
        else
        {
            OP_ASSERT(property_table && position == property_table->Count());
            new_class = this;
            level++;
       }
    }

    OP_ASSERT(new_class);

    if (!new_class->property_table)
        new_class->property_table = ES_Property_Table::Make(context, 4);

    if (!info.IsDontEnum())
        new_class->has_enumerable_properties = 1;

#ifdef DEBUG_ENABLE_OPASSERT
    BOOL added = new_class->property_table->AppendL(context, name, info.Attributes(), type, hide_existing);
    OP_ASSERT(added);
#else // DEBUG_ENABLE_OPASSERT
    new_class->property_table->AppendL(context, name, info.Attributes(), type, hide_existing);
#endif // DEBUG_ENABLE_OPASSERT

    if (static_data->MainClass() == new_class->Parent() || static_data->MainClass() == this)
        static_data->SetMainClass(new_class);
    else
        ClearMainClass(context);

#ifdef DEBUG_ENABLE_OPASSERT
    {
        JString *name0;
        ES_Property_Info info0;
        OP_ASSERT(new_class->property_table->Lookup(new_class->IndexOf(name), name0, info0));
        OP_ASSERT(name->Equals(name0) && info.Attributes() == info0.Attributes());
        OP_ASSERT(new_class->children || !new_class->property_table || new_class->property_table->Count() == new_class->LayoutLevel());
        OP_ASSERT(new_class->Level() > new_class->parent->Level());
    }
#endif

    return new_class;
}

ES_Class_Compact_Node *
ES_Class_Compact_Node::ExtendWithL(ES_Context *context, ES_StorageType type, ES_LayoutIndex position)
{
    ES_Class_Extra *ptr = extra;

    while (ptr && ptr->LayoutLevel() > position)
    {
        OP_ASSERT(ptr->LayoutLevel() == position + 1);

        ES_Layout_Info layout = ptr->klass->GetLayoutInfoAtIndex(position);

        if (type == layout.GetStorageType())
            return static_cast<ES_Class_Compact_Node *>(ptr->klass);

        ptr = ptr->next;
    }

    ES_Class_Compact_Node *new_class = ES_Class_Compact_Node::Make(context, this);
    ES_CollectorLock gclock(context);

    if (property_table)
    {
        OP_ASSERT(LayoutLevel() <= property_table->Count());
        new_class->property_table = property_table->CopyL(context, LayoutLevel(), Level());
    }
    else
        new_class->property_table = ES_Property_Table::Make(context, 4);

    unsigned index;

#ifdef DEBUG_ENABLE_OPASSERT
    BOOL added = new_class->property_table->AppendL(context, index, type);
    OP_ASSERT(added);
#else // DEBUG_ENABLE_OPASSERT
    new_class->property_table->AppendL(context, index, type);
#endif // DEBUG_ENABLE_OPASSERT

    ES_Class_Node_Base *last_child = static_cast<ES_Class_Node_Base *>(LastChild());

    new_class->extra = extra = ES_Class_Extra::Make(context, index + 1, new_class, extra);
    if (last_child)
        last_child->AddSibling(new_class);

    return new_class;
}

ES_Class_Compact_Node *
ES_Class_Compact_Node::DeleteL(ES_Context *context, JString *name, unsigned length, BOOL &needs_conversion)
{
    ES_PropertyIndex index;
    ES_Class *node = this;

    ES_CollectorLock gclock(context);
    if (property_table && property_table->IndexOf(name, index))
    {
        ES_LayoutIndex layout_index = GetPropertyInfoAtIndex(index).Index();
        while (node->Parent() && node->LayoutLevel() > layout_index)
            node = node->Parent();

        ES_PropertyIndex name_index(index);
        while (GetPropertyInfoAtIndex(name_index).Index() > node->LayoutLevel())
            name_index = ES_PropertyIndex(name_index - 1);

        node = node->CopyProperties(context, this, node->LayoutLevel(), GetPropertyInfoAtIndex(index).Index(), name_index, node->LayoutLevel(), needs_conversion);

        node = node->CopyProperties(context, this, ES_LayoutIndex(GetPropertyInfoAtIndex(index).Index() + 1), ES_LayoutIndex(length), ES_PropertyIndex(index + 1), GetPropertyInfoAtIndex(index).Index(), needs_conversion);

        InvalidateInstances();
    }

    ClearMainClass(context);

    return static_cast<ES_Class_Compact_Node *>(node);
}

ES_Class_Compact_Node *
ES_Class_Compact_Node::ChangeType(ES_Context *context, ES_PropertyIndex index, ES_StorageType type, unsigned length, BOOL &needs_conversion)
{
    OP_ASSERT(property_table);

    ES_StorageType old_type = GetLayoutInfoAtInfoIndex(index).GetStorageType();

    if (old_type == type)
        return this;

    ES_LayoutIndex layout_index = GetPropertyInfoAtIndex(index).Index();

    type = ResolveConflictingType(old_type, type);

    if (CanUpdateTypeInplace(type))
    {
        InvalidateInstances();
        return static_cast<ES_Class_Compact_Node *>(UpdateTypeInplace(layout_index, type));
    }

    ES_Class *node = this;

    while (node->LayoutLevel() > layout_index)
        node = node->Parent();

    ES_PropertyIndex name_index(index);
    while (GetPropertyInfoAtIndex(name_index).Index() > node->LayoutLevel())
        name_index = ES_PropertyIndex(name_index - 1);

    ES_CollectorLock gclock(context);

    node = node->CopyProperties(context, this, node->LayoutLevel(), GetPropertyInfoAtIndex(index).Index(), name_index, node->LayoutLevel(), needs_conversion);

    node = ES_Class::ExtendWithL(context, node, GetNameAtIndex(index), GetPropertyInfoAtIndex(index), type, layout_index);

    ES_StorageType new_type = node->GetLayoutInfoAtIndex(layout_index).GetStorageType();

    needs_conversion = needs_conversion || (new_type != type && !ES_Layout_Info::IsSubType(type, new_type));

    if (layout_index < length)
        node = node->CopyProperties(context, this, ES_LayoutIndex(layout_index + 1), ES_LayoutIndex(length), ES_PropertyIndex(index + 1), ES_LayoutIndex(layout_index + 1), needs_conversion);

    InvalidateInstances();
    return static_cast<ES_Class_Compact_Node *>(node);
}

ES_Class_Compact_Node *
ES_Class_Compact_Node::Branch(ES_Context *context, ES_LayoutIndex position)
{
    OP_ASSERT(position != UINT_MAX);

    if (position < LayoutLevel())
    {
        need_limit_check = 1;

        OP_ASSERT(property_table);
        OP_ASSERT(parent);
        OP_ASSERT(property_table->Count() > position);
        ES_Property_Info info0;
        JString *name0;

        ES_PropertyIndex index(position - CountExtra());
        property_table->Lookup(index, name0, info0);

        JString *branch_name;
        ES_Property_Info branch_info;
        property_table->Lookup(parent->Level(), branch_name, branch_info);

        ES_Class_Compact_Node *branch_node = ES_Class_Compact_Node::Make(context, static_cast<ES_Class_Compact_Node *>(Parent()));
        ES_CollectorLock gclock(context);

        parent->AddChild(context, static_cast<ES_Class_Compact_Node *>(parent)->children, branch_name, branch_info, branch_node, this);

        OP_ASSERT(sibling == NULL);

        branch_node->children = ES_Identifier_Boxed_Hash_Table::Make(context, 4);
        branch_node->property_table = property_table;
        branch_node->level = position;
        branch_node->has_enumerable_properties = has_enumerable_properties;

        parent = branch_node;

        branch_node->AddChild(context, branch_node->children, name0, info0, this);

        OP_ASSERT(level > branch_node->Level());
        OP_ASSERT(branch_node->Level() > branch_node->parent->Level());

        return branch_node;
    }

    if (!children)
        ChangeGCTag(GCTAG_ES_Class_Compact_Node_Frozen);

    return this;
}

/* ES_Class_Hash_Base */

void
ES_Class_Hash_Base::MakeData(ES_Context *context, unsigned count)
{
    ES_Property_Value_Table *values = ES_Property_Value_Table::Make(context, 4);
    ES_Box *class_serials = ES_Box::Make(context, sizeof(unsigned) * es_maxu(count, 4));
    data = ES_Box::Make(context, sizeof(Data));
    Data *extra_data = GetData();
    extra_data->count = extra_data->serial = count;

    extra_data->values = values;
    extra_data->class_serials = class_serials;

    unsigned *serials = GetSerials();

    for (unsigned i = 0; i < count; i++)
        serials[i] = i;
}

/* static */ void
ES_Class_Hash_Base::Initialize(ES_Class_Hash_Base *klass)
{
    ES_Class::Initialize(klass);
    klass->data = NULL;
}

void
ES_Class_Hash_Base::AppendSerialNr(ES_Context *context, unsigned serial)
{
    Data *extra_data = GetData();
    if (extra_data->count >= extra_data->class_serials->Size() / 4)
    {
        ES_Box *new_serials = ES_Box::Make(context, extra_data->class_serials->Size() * 2);
        op_memcpy(new_serials->Unbox(), extra_data->class_serials->Unbox(), extra_data->class_serials->Size());
        extra_data->class_serials = new_serials;
    }

    GetSerials()[extra_data->count++] = serial;
}

/* ES_Class_Singleton */

/* static */ ES_Class_Singleton *
ES_Class_Singleton::Make(ES_Context *context)
{
    ES_Class_Singleton *klass;
    GC_ALLOCATE(context, klass, ES_Class_Singleton, (klass));

    return klass;
}

/* static */ void
ES_Class_Singleton::Initialize(ES_Class_Singleton *self)
{
    self->InitGCTag(GCTAG_ES_Class_Singleton);
    ES_Class_Hash_Base::Initialize(self);
    self->instances = NULL;
    self->next = NULL;
}

void
ES_Class_Singleton::AddL(ES_Context *context, JString *name, ES_Property_Info info, ES_StorageType type, BOOL hide_existing)
{
    if (!property_table)
        property_table = ES_Property_Table::Make(context, 4);

    if (!info.IsDontEnum())
        has_enumerable_properties = 1;

    info.data.class_property = 1;

    if (property_table->AppendL(context, name, info.Attributes(), type, hide_existing))
        ++level;

    if (data)
        AppendSerialNr(context, GetData()->serial++);

    class_id = INVALID_CLASS_ID;
}

void
ES_Class_Singleton::AddL(ES_Context *context, ES_StorageType type)
{
    if (!property_table)
        property_table = ES_Property_Table::Make(context, 4);

    unsigned index;
    if (property_table->AppendL(context, index, type))
        ++level;

    if (data)
        AppendSerialNr(context, GetData()->serial++);

    extra = ES_Class_Extra::Make(context, index + 1, this, extra);

    class_id = INVALID_CLASS_ID;
}

void
ES_Class_Singleton::Remove(ES_Context *context, JString *name)
{
    if (!property_table)
        return;

    ES_Property_Info info;
    ES_PropertyIndex index;

    BOOL found = FALSE;
    while (property_table->Find(name, info, index))
    {
        found = TRUE;

        /* To be able to do conversion of the singleton object we do NOT update offsets at this point */

        for (unsigned i = index + 1; i < Level(); ++i)
            GetPropertyInfoAtIndex_ref(ES_PropertyIndex(i)).DecIndex();

        static_cast<ES_Property_Mutable_Table *>(property_table)->Delete(index);
        --level;

        ES_Class_Extra *ptr = extra;
        while (ptr && info.Index() < ptr->LayoutLevel())
        {
            ptr->DecLevel();
            ptr = ptr->next;
        }
    }

    if (found)
        Invalidate();
}

void
ES_Class_Singleton::ChangeAttributeFromTo(ES_Context *context, unsigned set_attributes, unsigned clear_attributes, ES_PropertyIndex from, ES_PropertyIndex to)
{
    ES_Property_Info old_info;

    BOOL has_changed = FALSE;

    for (unsigned i = from; i <= to; ++i)
    {
        ES_Property_Info &info = GetPropertyInfoAtIndex_ref(ES_PropertyIndex(i));
        unsigned attributes = info.Attributes();
        info.ChangeAttributes(set_attributes, clear_attributes);
        has_changed = has_changed || attributes != info.Attributes();
    }

    if (has_changed)
        Invalidate();
}

void
ES_Class_Singleton::ChangeType(ES_Context *context, ES_PropertyIndex index, ES_StorageType type)
{
    OP_ASSERT(property_table);
    ES_Property_Info info = GetPropertyInfoAtIndex(index);
    ES_Layout_Info &old_layout = GetLayoutInfoAtIndex_ref(info.Index());

    type = ResolveConflictingType(old_layout.GetStorageType(), type);

    if (old_layout.GetStorageType() != type)
    {
        old_layout.SetStorageType(type);
        old_layout.SetOffset(property_table->CalculateAlignedOffset(type, info.Index()));
        class_id = INVALID_CLASS_ID;

        /* To be able to do conversion of the singleton object we do NOT update offsets at this point */
    }

    Invalidate();
}

/* ES_Class_Hash */

/* static */ ES_Class_Hash *
ES_Class_Hash::Make(ES_Context *context, ES_Class *proper_klass, unsigned count)
{
    ES_Class_Hash *klass;
    GC_ALLOCATE(context, klass, ES_Class_Hash, (klass, proper_klass, count));
    ES_CollectorLock gclock(context);

    klass->MakeData(context, count);

    return klass;
}

/* static */ void
ES_Class_Hash::Initialize(ES_Class_Hash *self, ES_Class *proper_klass, unsigned count)
{
    ES_Class_Hash_Base::Initialize(self);
    self->InitGCTag(GCTAG_ES_Class_Hash);

    self->klass = proper_klass;
    self->property_table = proper_klass->property_table;
    self->level = count;
    self->need_limit_check = 0;
    self->has_enumerable_properties = proper_klass->has_enumerable_properties;
    self->extra = proper_klass->extra;
}

static void
SynchClass(ES_Class_Hash *klass)
{
    klass->property_table = klass->klass->property_table;
    klass->has_enumerable_properties = klass->klass->has_enumerable_properties;
    klass->class_id = ES_Class::INVALID_CLASS_ID;
    klass->extra = klass->klass->extra;
}

void
ES_Class_Hash::ExtendWithL(ES_Context *context, JString *name, ES_Property_Info info, ES_StorageType type, ES_LayoutIndex position, BOOL hide_existing)
{
    klass = ES_Class::ExtendWithL(context, klass, name, info, type, ES_LayoutIndex(position), hide_existing);

    SynchClass(this);

    level++;

    if (data)
        AppendSerialNr(context, GetData()->serial++);
}

void
ES_Class_Hash::ExtendWithL(ES_Context *context, ES_StorageType type, ES_LayoutIndex position)
{
    klass = ES_Class::ExtendWithL(context, klass, type, position);

    SynchClass(this);

    level++;
}

void
ES_Class_Hash::DeleteL(ES_Context *context, JString *name, unsigned length, BOOL &needs_conversion)
{
    klass = ES_Class::DeleteL(context, klass, name, length, needs_conversion);

    SynchClass(this);

    level--;
}

void
ES_Class_Hash::ChangeAttributeFromTo(ES_Context *context, unsigned set_attributes, unsigned clear_attributes, ES_PropertyIndex from, ES_PropertyIndex to, unsigned count, BOOL &needs_conversion, ES_Class *onto, ES_LayoutIndex onto_position)
{
    klass = ES_Class::ChangeAttributeFromTo(context, klass, set_attributes, clear_attributes, from, to, count, needs_conversion, onto, onto_position);

    SynchClass(this);
}

void
ES_Class_Hash::ChangeType(ES_Context *context, ES_PropertyIndex index, ES_StorageType type, unsigned length, BOOL &needs_conversion)
{
    klass = ES_Class::ChangeType(context, klass, index, type, length, needs_conversion);

    SynchClass(this);
    InvalidateInstances();
}
