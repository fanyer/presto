/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_PROPERTY_TABLE
#define ES_PROPERTY_TABLE

#include "modules/ecmascript/carakan/src/util/es_hash_table.h"
#include "modules/ecmascript/carakan/src/kernel/es_box.h"

#ifdef DEBUG_ENABLE_OPASSERT
struct ES_LayoutIndex
{
public:
    explicit ES_LayoutIndex(unsigned index) : index(index) {}
    operator unsigned() const { return index; }
private:
    unsigned index;
};

struct ES_PropertyIndex
{
public:
    ES_PropertyIndex() : index(UINT_MAX) {}
    explicit ES_PropertyIndex(unsigned index) : index(index) {}
    operator unsigned() const { return index; }
private:
    unsigned index;
};
#else
typedef unsigned ES_LayoutIndex;
typedef unsigned ES_PropertyIndex;
#endif

enum PropertyAttribute
{
    RO = 1,  /**< Read only. */
    DE = 2,  /**< Don't enumerate. */
    DD = 4,  /**< Don't delete. */
    SP = 8,  /**< Special property. */
    CP = 16, /**< Class property. */
    FN = 32, /**< Function. */
    HP = 64, /**< Host property. */

    PROPERTY_ATTRIBUTE_MASK = 0x7f
};

class ES_Property_Info
{
public:
    union
    {
        struct
        {
            unsigned int read_only      : 1;
            unsigned int dont_enum      : 1;
            unsigned int dont_delete    : 1;
            unsigned int special        : 1;
            unsigned int class_property : 1;
            unsigned int function       : 1;
            unsigned int dont_put_native: 1;
            unsigned int index : 25;
        } data;

        unsigned init;
    };

    ES_Property_Info() { Reset(); }
    ES_Property_Info(unsigned attributes) { Set(attributes); }
    ES_Property_Info(unsigned attributes, ES_LayoutIndex index)
    {
        Set(attributes);
        if (index != UINT_MAX)
            SetIndex(index);
    }

    inline void Set(unsigned attributes)
    {
        data.read_only = (attributes & RO);
        data.dont_enum = (attributes & DE) >> 1;
        data.dont_delete = (attributes & DD) >> 2;
        data.special = (attributes & SP) >> 3;
        data.class_property = (attributes & CP) >> 4;
        data.function = (attributes & FN) >> 5;
        data.dont_put_native = (attributes & HP) >> 6;
        data.index = 0;
    }

    inline void Set(unsigned attributes, unsigned index)
    {
        data.read_only = (attributes & RO);
        data.dont_enum = (attributes & DE) >> 1;
        data.dont_delete = (attributes & DD) >> 2;
        data.special = (attributes & SP) >> 3;
        data.class_property = (attributes & CP) >> 4;
        data.function = (attributes & FN) >> 5;
        data.dont_put_native = (attributes & HP) >> 6;
        data.index = index;
    }

    inline void SetIndex(ES_LayoutIndex index)
    {
        data.index = index;
    }

    inline void DecIndex()
    {
        --data.index;
    }

    inline void ChangeAttributes(unsigned set, unsigned clear)
    {
        OP_ASSERT((set & clear) == 0);
        OP_ASSERT((set & PROPERTY_ATTRIBUTE_MASK) == set);
        OP_ASSERT((clear & PROPERTY_ATTRIBUTE_MASK) == clear);

        unsigned attributes = (Attributes() | set) & ~clear;
        Set(attributes, ES_PropertyIndex(data.index));
    }

    inline unsigned Attributes() const { return (data.dont_put_native << 6) | (data.function << 5) | (data.class_property << 4) | (data.special << 3) | (data.dont_delete << 2) | (data.dont_enum << 1) | data.read_only; }

#ifdef DEBUG_ENABLE_OPASSERT
    inline BOOL Check(unsigned attributes)
    {
        return data.read_only == (attributes & RO) &&
               data.dont_enum == (attributes & DE) >> 1 &&
               data.dont_delete == (attributes & DD) >> 2 &&
               data.special == (attributes & SP) >> 3 &&
               data.class_property == (attributes & CP) >> 4 &&
               data.function == (attributes & FN) >> 5 &&
               data.dont_put_native == (attributes & HP) >> 6;
    }
    inline BOOL Check(const ES_Property_Info &info)
    {
        return data.read_only == info.data.read_only &&
               data.dont_enum == info.data.dont_enum &&
               data.dont_delete == info.data.dont_delete &&
               data.special == info.data.special &&
               data.function == info.data.function &&
               data.dont_put_native == info.data.dont_put_native;
    }
#endif // DEBUG_ENABLE_OPASSERT

    inline BOOL IsReadOnly() const { return data.read_only; }
    inline BOOL IsWritable() const { return !IsReadOnly(); }

    inline BOOL IsDontDelete() const { return data.dont_delete; }
    inline BOOL IsConfigurable() const { return !IsDontDelete(); }

    inline BOOL IsDontEnum() const { return data.dont_enum; }
    inline BOOL IsEnumerable() const { return !IsDontEnum(); }

    inline BOOL IsSpecial() const { return data.special; }
    inline BOOL IsClassProperty() const { return data.class_property; }
    inline BOOL IsFunction() const { return data.function; }

    inline void SetIsFunction() { data.function = 1; }
    inline void ClearIsFunction() { data.function = 0; }

    inline void SetIsSpecial() { data.special = 1; }
    inline void ClearIsSpecial() { data.special = 0; }

    inline ES_LayoutIndex Index() const { return ES_LayoutIndex(data.index); }

    inline void SetIsNotWriteable() { data.dont_put_native = 1; }
    inline BOOL IsNativeWriteable() { return data.dont_put_native == 0; }

    inline void SetConfigurable(BOOL value) { data.dont_delete = !value; }
    inline void SetEnumerable(BOOL value) { data.dont_enum = !value; }
    inline void SetWritable(BOOL value) { data.read_only = !value; }

    inline BOOL CanCacheGet() { return data.special == 0 && data.class_property == 1; }
    inline BOOL CanCachePut() { return data.read_only == 0 && data.special == 0 && data.class_property == 1 && data.function == 0; }

    void Reset() { init = 0; }
};

class ES_Layout_Info
{
public:
    union
    {
        struct
        {
            /* ES_StorageType. Enum bitfields aren't portable (signedness), hence unsigned. */
            unsigned type : 4;
            unsigned offset : 28;
        } data;

        unsigned init;
    };

    ES_Layout_Info() { Reset(); }
    ES_Layout_Info(unsigned offset) { Set(offset, ES_STORAGE_WHATEVER); }
    ES_Layout_Info(unsigned offset, ES_StorageType type) { Set(offset, type); }

    void Set(unsigned offset, ES_StorageType type) { data.offset = static_cast<unsigned>(offset); data.type = type; }

    inline void SetStorageType(ES_StorageType type) { data.type = type; }
    inline ES_StorageType GetStorageType() const { return static_cast<ES_StorageType>(data.type); }

    inline void SetOffset(unsigned offset) { data.offset = offset; }
    inline unsigned GetOffset() const { return data.offset; }

    inline unsigned GetNextOffset() const { return data.offset + ES_Layout_Info::SizeOf(static_cast<ES_StorageType>(data.type)); }

    inline int Diff(ES_Layout_Info layout) const;

    inline static unsigned SizeOf(ES_StorageType type);
    inline static unsigned AlignmentOf(ES_StorageType type);

    inline BOOL IsBoxedType() const { return data.type > LAST_UNBOXED_TYPE && data.type <= LAST_BOXED_TYPE; }
    inline BOOL IsUntyped() const { return data.type == ES_STORAGE_WHATEVER; }
    inline BOOL IsSpecialType() const { return data.type >= FIRST_SPECIAL_TYPE; }

    inline BOOL IsAligned() const { return ES_Layout_Info::SizeOf(GetStorageType()) % 8 == 0; }

    inline BOOL CheckType(ES_StorageType type) const;

    inline static BOOL CheckType(ES_StorageType type0, ES_StorageType type1);
    inline static ES_StorageType SuperType(ES_StorageType type);
    inline static BOOL IsSubType(ES_StorageType type0, ES_StorageType type1);
    inline static BOOL IsNullable(ES_StorageType type);
    inline static BOOL IsSpecialType(ES_StorageType type) { return type >= FIRST_SPECIAL_TYPE; }

    inline static ES_StorageType NonNullableType(ES_StorageType type);
    inline static ES_StorageType ToNullableType(ES_StorageType type);

    inline void AdjustOffset(int diff) { data.offset = static_cast<unsigned>(data.offset + diff); }

    void Reset() { init = 0; }
};


class ES_Property_Table : public ES_Boxed
{
private:
    static void Initialize(ES_Property_Table *self, unsigned size);

public:
    static ES_Property_Table *Make(ES_Context *context, unsigned size);

    unsigned CalculateAlignedOffset(ES_StorageType type, unsigned index) const;

    BOOL AppendL(ES_Context *context, JString *name, unsigned attributes, ES_StorageType type, BOOL hide_existing = FALSE);
    BOOL AppendL(ES_Context *context, unsigned &index, ES_StorageType type);

    inline void GrowToL(ES_Context *context, unsigned size);

    ES_Property_Table *CopyL(ES_Context *context, unsigned count, unsigned property_count);

    inline BOOL Find(JString *name, ES_Property_Info &info, ES_PropertyIndex &position);
    inline BOOL IndexOf(JString *name, ES_PropertyIndex &index) const;

    BOOL Lookup(ES_PropertyIndex index, JString *&name, ES_Property_Info &info);
    BOOL Lookup(ES_PropertyIndex index, JString *&name, ES_Property_Info &info, ES_Layout_Info &layout);

    inline unsigned Count() const { return used; }
    inline unsigned CountProperties() const { return properties->GetCount(); }

    inline unsigned Capacity() const { return capacity; }

    inline ES_Property_Info GetPropertyInfoAtIndex(ES_PropertyIndex index) const;
    inline ES_Property_Info &GetPropertyInfoAtIndex_ref(ES_PropertyIndex index);
    inline ES_Property_Info *GetPropertyInfo();

    inline ES_Layout_Info GetLayoutInfoAtIndex(ES_LayoutIndex index) const;
    inline ES_Layout_Info &GetLayoutInfoAtIndex_ref(ES_LayoutIndex index);
    inline ES_Layout_Info *GetLayoutInfo();

    void AdjustOffset(ES_LayoutIndex from);

    inline unsigned GetNextOffset(ES_LayoutIndex index) const;

    ES_Identifier_List *properties;

    unsigned capacity;
    unsigned used;

    ES_Box *property_info;
    ES_Box *layout_info;
};

class ES_Property_Mutable_Table : public ES_Property_Table
{
public:
    void Delete(unsigned index);
};

class ES_Property_Value_Table : public ES_Boxed
{
public:
    static ES_Property_Value_Table *Make(ES_Context *context, unsigned size);

    static void Initialize(ES_Property_Value_Table *, unsigned size);

    ES_Property_Value_Table *CopyL(ES_Context *context);

    BOOL FindLocation(JString *name, ES_Property_Info &info, ES_Value_Internal *&value);

    inline BOOL Find(JString *name, ES_Property_Info &info)
    {
        ES_Value_Internal *dummy;
        return FindLocation(name, info, dummy);
    }

    void Lookup(unsigned index, JString *&name, ES_Property_Info &info, ES_Value_Internal &value);

    void GetSerialNr(unsigned index, unsigned &serial_nr);

    void SetSerialNr(unsigned index, unsigned serial_nr);

    ES_Property_Info GetInfo(unsigned index) const { return property_value[index].info; }

    BOOL Find(JString *name, ES_Value_Internal &value);

    void InsertL(ES_Context *context, ES_Property_Value_Table *&table, JString *name, const ES_Value_Internal &value, ES_Property_Info info, unsigned serial);

    void ReplaceL(ES_Context *context, ES_Property_Value_Table *&table, JString *name, const ES_Value_Internal *value, ES_Property_Info info);

    void Delete(JString *name);

    inline unsigned Count() const { return used; }

    struct Property_Data_Cell
    {
        ES_Value_Internal value;
        ES_Property_Info info;
#ifdef ES_VALUES_AS_NANS
        unsigned serial;
#endif
    };
    unsigned capacity, used, deleted;
    ES_Identifier_Mutable_List *properties;
    Property_Data_Cell property_value[1];
};

#endif // ES_PROPERTY_TABLE
