/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_CLASS_H
#define ES_CLASS_H

#include "modules/ecmascript/carakan/src/object/es_property_table.h"
#include "modules/ecmascript/carakan/src/util/es_util.h"
#include "modules/ecmascript/carakan/src/util/es_hash_table.h"

enum ES_SubClassType
{
    ES_SubClassTypeInstance          = 1,
    ES_SubClassTypeSingletonInstance = 2,
    ES_SubClassTypeProxy             = 3
};

class ES_Class_Data : public ES_Boxed
{
public:
    static ES_Class_Data *Make(ES_Context *context, const char *object_name, JString *object_name_string, ES_Object *prototype, ES_Class *root);
    static void Initialize(ES_Class_Data *self, const char *object_name, JString *object_name_string, ES_Object *prototype, ES_Class *root);

    inline const char *ObjectName() const { return object_name; }
    inline JString *ObjectNameString() const { return object_name_string; }
    inline ES_Class *RootClass() const { return root; }
    inline void SetRootNode(ES_Class *new_root) { root = new_root; }
    inline ES_Class *MainClass() const { return main; }
    inline ES_Object *Prototype() const { return prototype; }
    inline ES_Object *GetInstance() const;
    inline BOOL HasInstances() const { return instance != NULL; }

    inline void SetMainClass(ES_Class *new_main);

    inline void SetPrototype(ES_Object *new_prototype) { prototype = new_prototype; }

    inline void SetObjectNameString(JString *s) { object_name_string = s; }
    inline void InvalidateSubClasses();

    inline void AddInstance(ES_Context *context, ES_Boxed *new_instance);
    inline void RemoveInstance(ES_Boxed *old_instance);

    inline void SetIsCompact() { compact = TRUE; }
    inline BOOL IsCompact() const { return compact; }

private:
    friend class ESMM;

    const char *object_name;
    JString *object_name_string;
    ES_Class *root;
    ES_Class *main;
    ES_Boxed *instance;
#ifdef ES_NATIVE_SUPPORT
public:
#endif // ES_NATIVE_SUPPORT
    ES_Object *prototype;
    BOOL compact;
};

class ES_Class_Node;
class ES_Class_Compact_Node;
class ES_Class_Hash_Base;
class ES_Class_Singleton;
class ES_Class_Hash;
class ES_Class_Extra;

class ES_Class : public ES_Boxed
{
public:
    static ES_Class_Node *MakeRoot(ES_Context *context, ES_Object *prototype, const char *object_name = NULL, BOOL force_new_root = FALSE, unsigned size = UINT_MAX);
    static ES_Class_Node *MakeRoot(ES_Context *context, ES_Object *prototype, const char *object_name , JString *object_name_string, BOOL force_new_root = FALSE, unsigned size = UINT_MAX);
    /**< Make an empty class with the given prototype and object name. If force_new_root is
         FALSE then MakeRoot will try to find a class root with the name 'object_name' in
         the instances from 'prototype'. To make this possible it is necessary to add new
         roots to 'prototype' using ES_Object::AddInstance.
     */
    static ES_Class_Compact_Node *MakeCompactRoot(ES_Context *context, ES_Object *prototype, const char *object_name = NULL, BOOL force_new_root = FALSE, unsigned size = UINT_MAX);
    static ES_Class_Compact_Node *MakeCompactRoot(ES_Context *context, ES_Object *prototype, const char *object_name, JString *object_name_string, BOOL force_new_root = FALSE, unsigned size = UINT_MAX);
    static ES_Class_Singleton *MakeSingleton(ES_Context *context, ES_Object *prototype, const char *object_name = NULL, unsigned size = UINT_MAX);
    static ES_Class_Singleton *MakeSingleton(ES_Context *context, ES_Object *prototype, const char *object_name, JString *object_name_string, unsigned size = UINT_MAX);
    static ES_Class *MakeHash(ES_Context *context, ES_Class *klass, unsigned count);

    static void Initialize(ES_Class *self);

    static ES_Class *ExtendWithL(ES_Context *context, ES_Class *this_class, JString *name, ES_Property_Info info, ES_StorageType type, ES_LayoutIndex position = ES_LayoutIndex(UINT_MAX), BOOL hide_existing = FALSE);
    static ES_Class *ExtendWithL(ES_Context *context, ES_Class *this_class, ES_StorageType type, ES_LayoutIndex position = ES_LayoutIndex(UINT_MAX));
    static ES_Class *DeleteL(ES_Context *context, ES_Class *this_class, JString *name, unsigned length, BOOL &needs_conversion);
    inline static ES_Class *ChangeAttribute(ES_Context *context, ES_Class *this_class, JString *name, ES_Property_Info info, unsigned count, BOOL &needs_conversion);
    static ES_Class *ChangeAttributeFromTo(ES_Context *context, ES_Class *this_class, unsigned set_attributes, unsigned clear_attributes, ES_PropertyIndex from, ES_PropertyIndex to, unsigned count, BOOL &needs_conversion, ES_Class *onto = NULL, ES_LayoutIndex onto_position = ES_LayoutIndex(UINT_MAX));
    static ES_Class *ChangeType(ES_Context *context, ES_Class *this_class, ES_PropertyIndex index, ES_StorageType type, unsigned count, BOOL &needs_conversion);
    inline static BOOL Find(ES_Class *this_class, JString *name, ES_Property_Info &info, unsigned count);

    static unsigned ChildCount(ES_Class *this_class);

    static unsigned MaxSize(ES_Class *this_class);

    static ES_Class *Branch(ES_Context *context, ES_Class *this_class, ES_LayoutIndex position);

    static ES_Class *ActualClass(ES_Class *this_class);

    static ES_Class *SetPropertyCount(ES_Class *this_class, unsigned count);

    ES_Class *CopyProperties(ES_Context *context, ES_Class *from_class, ES_LayoutIndex first, ES_LayoutIndex last, ES_PropertyIndex index, ES_LayoutIndex to, BOOL &needs_conversion, BOOL hide_existing = FALSE);

    inline void InvalidateCurrentClass();
    static void InvalidateTree(ES_Class *tree);
    inline void InvalidateInstances();
    inline void Invalidate();

    inline unsigned Id() const;
    inline unsigned GetId(ES_Context *context);
    inline BOOL IsInvalid() const;

    inline unsigned Count() const;
    inline unsigned CountProperties() const;
    inline unsigned CountExtra() const;

    inline ES_PropertyIndex Level() const;
    inline ES_LayoutIndex LayoutLevel() const;

    inline void SetIsCompact() { static_data->SetIsCompact(); }

    inline BOOL HasEnumerableProperties() const;
    inline BOOL HasInstances() const;

    inline const char *ObjectName() const;
    inline JString *ObjectName(ES_Context *context) const;
    inline JString *ObjectNameString() const;

    inline BOOL HasPrototype() const;
    inline ES_Object *Prototype() const;
    inline void SetPrototype(ES_Object *object) const;

    inline ES_Property_Table *GetPropertyTable() const;

    inline ES_Class *GetRootClass() const;

    inline void AddInstance(ES_Context *context, ES_Boxed *instance);
    inline void RemoveInstance(ES_Boxed *instance);
    inline ES_Object *GetInstance() const;

    inline BOOL Find(JString *name, ES_Property_Info &info, unsigned limit);
    inline BOOL Find(JString *name, ES_PropertyIndex &index, unsigned limit);
    inline BOOL Lookup(ES_PropertyIndex index, JString *&name, ES_Property_Info &info);
    inline BOOL LookupExtra(ES_StorageType type, ES_Layout_Info &layout);
    inline JString *GetNameAtIndex(ES_PropertyIndex index);
    inline ES_PropertyIndex IndexOf(JString *name);

    inline ES_Class *GetParent() const;

    inline ES_Class *GetMainClass() const;

    inline BOOL HasHashTableProperties() const;
    inline BOOL IsSingleton() const;
    inline static BOOL HasHashTableProperties(unsigned id);
    inline static ES_Property_Value_Table *GetHashTable(ES_Class *this_class);

    inline BOOL IsNode() const;
    inline BOOL IsCompact() const;
    inline BOOL NeedLimitCheck() const;
    inline BOOL NeedLimitCheck(ES_LayoutIndex limit, BOOL negative) const;

    inline unsigned SizeOf(unsigned count) const;

    inline ES_Layout_Info GetLayoutInfo(const ES_Property_Info &info) const;
    inline ES_Layout_Info GetLayoutInfoAtInfoIndex(ES_PropertyIndex index) const;
    inline ES_Layout_Info &GetLayoutInfoAtInfoIndex_ref(ES_PropertyIndex index) const;
    inline ES_Layout_Info GetLayoutInfoAtIndex(ES_LayoutIndex index) const;
    inline ES_Layout_Info &GetLayoutInfoAtIndex_ref(ES_LayoutIndex index) const;
    inline ES_Property_Info GetPropertyInfoAtIndex(ES_PropertyIndex index) const;
    inline ES_Property_Info &GetPropertyInfoAtIndex_ref(ES_PropertyIndex index) const;

    unsigned GetNextOffset(ES_LayoutIndex index) const { return property_table->GetNextOffset(index); }

    void AdjustOffsetAtIndex(ES_LayoutIndex index);
    void AdjustOffset(ES_LayoutIndex index);

    inline ES_Class *Next(ES_Class *start = NULL);
    inline ES_Class *Parent();
    inline ES_Class *LastChild();

    enum
    {
        MASK_HAS_HASH_TABLE_PROPERTIES = 0x80000000,

        NOT_CACHED_CLASS_ID = 0,
        INVALID_CLASS_ID = 1,
        GLOBAL_IMMEDIATE_CLASS_ID = 0x7fffffff
    };

    unsigned level:30;
    unsigned need_limit_check:1;
    unsigned has_enumerable_properties:1;

    ES_Property_Table *property_table;
    ES_Class_Data *static_data;
    unsigned class_id;

    ES_Class *sibling;
    ES_Class_Extra *extra;
};

class ES_Class_Extra : public ES_Boxed
{
public:
    static ES_Class_Extra *Make(ES_Context *context, unsigned index, ES_Class *klass, ES_Class_Extra *next);
    static void Initialize(ES_Class_Extra *self, unsigned index, ES_Class *klass, ES_Class_Extra *next);

    ES_LayoutIndex LayoutLevel() const { return ES_LayoutIndex(level); }
    void DecLevel() { level--; }

    unsigned length;
    ES_Class *klass;
    ES_Class_Extra *next;
private:
    unsigned level;
};

class ES_Class_Node_Base : public ES_Class
{
public:
    static void Initialize(ES_Class_Node_Base *self, ES_Class_Node_Base *parent);
    inline void InvalidateBranch();

    void AddChild(ES_Context *context, ES_Identifier_Boxed_Hash_Table *children, JString *name, ES_Property_Info info, ES_Class_Node_Base *new_klass, ES_Class_Node_Base *old = NULL);
    ES_Class_Node_Base *UpdateTypeInplace(ES_LayoutIndex index, ES_StorageType type);
    void ClearMainClass(ES_Context *context);
    void AddSibling(ES_Class *new_sibling);

    void AddExtra(ES_Class_Extra *data);

    unsigned MaxSize();

    ES_Class_Node_Base *ChangeAttributeFromTo(ES_Context *context, unsigned set_attributes, unsigned clear_attributes, ES_PropertyIndex from, ES_PropertyIndex to, unsigned count, BOOL &needs_conversion, ES_Class *onto, ES_LayoutIndex onto_position);

    ES_Class_Node_Base *parent;
};

class ES_Class_Node : public ES_Class_Node_Base
{
public:
    static ES_Class_Node *Make(ES_Context *context, ES_Class_Node *parent);
    static void Initialize(ES_Class_Node *self, ES_Class_Node *parent);

    ES_Class_Node *ExtendWithL(ES_Context *context, JString *name, ES_Property_Info info, ES_StorageType type, BOOL hide_existing);
    ES_Class_Node *ExtendWithL(ES_Context *context, ES_StorageType type);
    ES_Class_Node *DeleteL(ES_Context *context, JString *name, BOOL &needs_conversion);
    ES_Class_Node *ChangeType(ES_Context *context, ES_PropertyIndex index, ES_StorageType type, BOOL &needs_conversion);

    inline unsigned ChildCount() const;

public:
    ES_Boxed *children;
};

class ES_Class_Compact_Node : public ES_Class_Node_Base
{
public:
    static ES_Class_Compact_Node *Make(ES_Context *context, ES_Class_Compact_Node *parent);
    static void Initialize(ES_Class_Compact_Node *self, ES_Class_Compact_Node *parent);

    ES_Class_Compact_Node *ExtendWithL(ES_Context *context, JString *name, ES_Property_Info info, ES_StorageType type, ES_LayoutIndex position, BOOL hide_existing);
    ES_Class_Compact_Node *ExtendWithL(ES_Context *context, ES_StorageType type, ES_LayoutIndex position);
    ES_Class_Compact_Node *DeleteL(ES_Context *context, JString *name, unsigned position, BOOL &needs_conversion);
    ES_Class_Compact_Node *ChangeType(ES_Context *context, ES_PropertyIndex index, ES_StorageType type, unsigned length, BOOL &needs_conversion);

    inline unsigned ChildCount() const;
    inline BOOL ShouldBranch() const;

    ES_Class_Compact_Node *Branch(ES_Context *context, ES_LayoutIndex position);

public:
    ES_Identifier_Boxed_Hash_Table *children;
};

class ES_Class_Hash_Base : public ES_Class
{
public:
    void MakeData(ES_Context *context, unsigned count);
    static void Initialize(ES_Class_Hash_Base *klass);

    void AppendSerialNr(ES_Context *context, unsigned serial);

    inline unsigned *GetSerials();
    inline ES_Box *GetSerialsBox();
    inline ES_Property_Value_Table *GetHashTable();

    inline BOOL Find(JString *name, ES_Property_Info &info);
    inline BOOL FindLocation(JString *name, ES_Property_Info &info, ES_Value_Internal *&value);
    inline void InsertL(ES_Context *context, JString *name, const ES_Value_Internal &value, ES_Property_Info info, unsigned count);
    inline void RedefinePropertyL(ES_Context *context, JString *name, const ES_Value_Internal *value, ES_Property_Info info);
    inline void Delete(JString *name);
    inline void GetSerialNr(const ES_Property_Info &info, unsigned &serial);
    inline unsigned GetSerialNrAtIndex(ES_PropertyIndex index);
    inline void SetSerialNrAtIndex(ES_PropertyIndex index, unsigned serial);

    inline void CopyHashTableFrom(ES_Class_Hash_Base *klass);

protected:
    struct Data
    {
        ES_Property_Value_Table *values;
        ES_Box *class_serials;
        unsigned serial;
        unsigned count;
    };

    inline Data *GetData() { return data ? reinterpret_cast<Data*>(data->Unbox()) : NULL; }

public:
    ES_Box *data;
};

class ES_Class_Singleton : public ES_Class_Hash_Base
{
public:
    static ES_Class_Singleton *Make(ES_Context *context);
    static void Initialize(ES_Class_Singleton *self);

    void AddL(ES_Context *context, JString *name, ES_Property_Info info, ES_StorageType type, BOOL hide_existing);
    void AddL(ES_Context *context, ES_StorageType type);
    void Remove(ES_Context *context, JString *name);
    void ChangeAttributeFromTo(ES_Context *context, unsigned set_attributes, unsigned clear_attributes, ES_PropertyIndex from, ES_PropertyIndex to);
    void ChangeType(ES_Context *context, ES_PropertyIndex index, ES_StorageType type);

    BOOL HasInstances() const { return instances != NULL; }
    ES_Boxed *GetInstances() const { return instances; }
    void SetInstances(ES_Boxed *b) { instances = b; }

    ES_Boxed *instances;
    ES_Class_Singleton *next;
};

class ES_Class_Hash : public ES_Class_Hash_Base
{
public:
    static ES_Class_Hash *Make(ES_Context *context, ES_Class *proper_klass, unsigned count);
    static void Initialize(ES_Class_Hash *klass, ES_Class *proper_klass, unsigned count);

    void ExtendWithL(ES_Context *context, JString *name, ES_Property_Info info, ES_StorageType type, ES_LayoutIndex position, BOOL hide_existing);
    void ExtendWithL(ES_Context *context, ES_StorageType type, ES_LayoutIndex position);
    void DeleteL(ES_Context *context, JString *name, unsigned length, BOOL &needs_conversion);
    void ChangeAttributeFromTo(ES_Context *context, unsigned set_attributes, unsigned clear_attributes, ES_PropertyIndex from, ES_PropertyIndex to, unsigned count, BOOL &needs_conversion, ES_Class *onto, ES_LayoutIndex onto_position);
    void ChangeType(ES_Context *context, ES_PropertyIndex index, ES_StorageType type, unsigned length, BOOL &needs_conversion);

public:
    ES_Class *klass;
};

#endif // ES_CLASS_H
