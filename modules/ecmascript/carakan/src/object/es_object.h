/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_OBJECT_H
#define ES_OBJECT_H

#ifdef ES_DEBUG_COMPACT_OBJECTS
#define OP_ASSERT_PROPERTY_COUNT(o) ((o->Count() <= (o->Class()->GetPropertyTable() ? o->Class()->GetPropertyTable()->Count() : 0)) && o->Count() <= o->Class()->Count())
#else
#define OP_ASSERT_PROPERTY_COUNT(o)
#endif

#include "modules/ecmascript/carakan/src/kernel/es_heap_object.h"
#include "modules/ecmascript/carakan/src/kernel/es_boxed.h"
#include "modules/ecmascript/carakan/src/object/es_class.h"
#include "modules/ecmascript/carakan/src/object/es_property_table.h"
#include "modules/ecmascript/carakan/src/object/es_property.h"
#include "modules/ecmascript/carakan/src/kernel/es_string.h"

enum PutResult
{
    PROP_PUT_FAILED           = 0,
    PROP_PUT_OK               = 1,
    PROP_PUT_READ_ONLY        = 2,
    PROP_PUT_OK_CAN_CACHE     = 5,
    PROP_PUT_OK_CAN_CACHE_NEW = 13
};

enum GetResult
{
    PROP_GET_FAILED                 = 0,
    PROP_GET_OK                     = 1,
    PROP_GET_NOT_FOUND              = 2,
    PROP_GET_OK_CAN_CACHE           = 5,
    PROP_GET_NOT_FOUND_CAN_CACHE    = 6
};

typedef BOOL DeleteResult;

enum InstanceOfResult
{
    INSTANCE_NO = 0,
    INSTANCE_YES,
    INSTANCE_BAD
};

#define GET_CANNOT_CACHE(res) (GetResult)((res) & ~4)
#define PUT_CANNOT_CACHE(res) (PutResult)((res) & ~12)
#define GET_OK(res) (((res) & 1) != 0)
#define PUT_OK(res) (((res) & 1) != 0)
#define GET_NOT_FOUND(res) ((res) & 2)
#define GET_FOUND(res) (((res) & 2) ^ 2)

#ifdef _DEBUG
# define inline
#endif // _DEBUG

class ES_Indexed_Properties;
class ES_Special_Mutable_Access;

class ES_Object : public ES_Boxed
{
public:

    static ES_Object *Make(ES_Context *context, ES_Class *klass, unsigned size = 0);
    static ES_Object *MakeArray(ES_Context *context, ES_Class *klass, unsigned indexed_capacity);

    static ES_Object *MakePrototypeObject(ES_Context *context, ES_Class *&instance, ES_Object *prototype, const char *class_name, unsigned size);
    /**< Create an object prepared for use as a prototype (that is, on which
         IsPrototype() returns true.) */

    static ES_Object *MakePrototypeObject(ES_Context *context, ES_Class *&instance);
    /**< Create the Object prototype (initial value of 'Object.prototype'.) */

    void InitPropertyL(ES_Context *context, JString *name, const ES_Value_Internal &value, unsigned attributes = 0, ES_StorageType type = ES_STORAGE_WHATEVER, BOOL is_new = FALSE);
    /**< Initialise a property that is known not to exist in the object or has
         never been set, i.e. IsUndefined() would return TRUE. InitPropertyL
         will adhere to location by offset known by class, and will not change
         already present property info, making it suitable for the
         ES_*Builtins-classes when populating prototypes.

         InitPropertyL performs no security checks and neither calls getters nor
         setters.
    */

    void InitPropertyL(ES_Context *context, unsigned index, const ES_Value_Internal &value, unsigned attributes = 0);

    void UnsafePutL(ES_Context *context, JString *name, const ES_Value_Internal &value);
    /**< Write to a property that is known to exist, i.e. GetOwnLocation returns
         TRUE UnsafePutL is meant to be used when pre-constructing objects and
         the property needs to be altered. If the property is known to be
         undefined consider using InitPropertyL for clarity.

         UnsafePutL performs no security checks and neither calls getters nor
         setters.
    */

    PutResult PutL(ES_Execution_Context *context, const ES_Value_Internal &this_value, JString *name, const ES_Value_Internal &value, ES_PropertyIndex &index);

    inline PutResult PutL(ES_Execution_Context *context, JString *name, const ES_Value_Internal &value);

    PutResult PutNonClassL(ES_Execution_Context *context, JString *name, const ES_Value_Internal &value);

    PutResult PutL(ES_Execution_Context *context, unsigned index, const ES_Value_Internal &value);

    inline PutResult PutCachedAtOffset(unsigned offset, unsigned type, const ES_Value_Internal &value);
    inline PutResult PutCachedAtIndex(ES_PropertyIndex index, const ES_Value_Internal &value);
    inline PutResult PutCached(ES_Layout_Info layout, const ES_Value_Internal &value);

    inline PutResult PutNoLockL(ES_Execution_Context *context, unsigned index, const ES_Value_Internal &value);
    inline PutResult PutNoLockL(ES_Execution_Context *context, unsigned index, unsigned *attributes, const ES_Value_Internal &value);

    inline PutResult PutManyL(ES_Execution_Context *context, unsigned &args_put, unsigned index, unsigned argc, ES_Value_Internal *argv);

    PutResult PutCachedNew(ES_Context *context, unsigned offset, unsigned type, const ES_Value_Internal &value, ES_Class *new_class);
    PutResult PutCachedNew(ES_Context *context, const ES_Layout_Info &layout, const ES_Value_Internal &value, ES_Class *new_class);

    PutResult PutLength(ES_Execution_Context *context, unsigned length, BOOL throw_on_read_only);
    /**< Put property named "length" (fast if this is an array object). */

    GetResult GetL(ES_Execution_Context *context, const ES_Value_Internal &this_value, JString *name, ES_Value_Internal &value, ES_Object *&prototype, ES_PropertyIndex &index);

    inline GetResult GetL(ES_Execution_Context *context, JString *name, ES_Value_Internal &value);

    GetResult GetL(ES_Execution_Context *context, const ES_Value_Internal &this_value, unsigned index, ES_Value_Internal &value);

    inline GetResult GetL(ES_Execution_Context *context, unsigned index, ES_Value_Internal &value);

    GetResult GetNonClassL(ES_Execution_Context *context, JString *name, ES_Value_Internal &value);

    inline GetResult GetCachedAtIndex(ES_PropertyIndex index, ES_Value_Internal &value) const;
    inline GetResult GetCached(unsigned offset, unsigned type, ES_Value_Internal &value) const;
    inline GetResult GetCached(ES_Layout_Info layout, ES_Value_Internal &value) const;

    inline GetResult GetLength(ES_Execution_Context *context, unsigned &length, BOOL must_exist = FALSE);
    /**< Get property named "length" (fast if this is an array object) and
         convert its value to an Uint32 into 'length'. If 'must_exist' is TRUE, the property must be present. */
    GetResult GetLengthSlow(ES_Execution_Context *context, unsigned &length, BOOL must_exist = FALSE);

    GetResult GetPrototypeL(ES_Execution_Context *context, ES_Value_Internal &value);

    PutResult PutPrototypeL(ES_Execution_Context *context, const ES_Value_Internal &value);

    PutResult DefineGetterL(ES_Execution_Context *context, JString *name, ES_Function *function);
    /**< Define name to be a getter with the accessor method 'function'. If
         there is already is a value with the property name that value will be
         completely overwritten if it isn't of type ES_Special_Mutable_Access.
         If it is of type ES_Special_Mutable_Access only the getter field of
         ES_Special_Mutable_Access will be overwritten.
    */

    PutResult DefineSetterL(ES_Execution_Context *context, JString *name, ES_Function *function);
    /**< Define name to be a setter with the mutator method 'function'. If
         there is already is a value with the property name that value will be
         completely overwritten if it isn't of type ES_Special_Mutable_Access.
         If it is of type ES_Special_Mutable_Access only the setter field of
         ES_Special_Mutable_Access will be overwritten.
    */

    GetResult GetOwnPropertyL(ES_Execution_Context *context, ES_Object *this_object, const ES_Value_Internal &this_value, JString *name, ES_Value_Internal &value, ES_PropertyIndex &index);
    inline GetResult GetOwnPropertyL(ES_Execution_Context *context, ES_Object *this_object, const ES_Value_Internal &this_value, unsigned index, ES_Value_Internal &value);

    BOOL HasProperty(ES_Context *context, JString *name);

    BOOL HasProperty(ES_Context *context, unsigned index);

    BOOL HasProperty(ES_Execution_Context *context, JString *name, BOOL &is_secure);
    /**< Check if property is present on an object or in its prototype chain.
         Triggers security checks for any host object that is encountered; */

    BOOL HasProperty(ES_Execution_Context *context, unsigned index, BOOL &is_secure);
    /**< Check if an indexed property is present on an object or in its prototype
         chain. Triggers security checks for any host object that is encountered. */

    BOOL HasPropertyWithInfo(ES_Execution_Context *context, JString *name, ES_Property_Info &info, ES_Object *&owner, BOOL &is_secure, BOOL &can_cache);
    /**< Check if a property is present on an object or in its prototype chain.
         HasPropertyWithInfo will trigger security checks for any host object that
         it passes.

         The is_secure argument indicates the security status of the operation. If
         HasPropertyWithInfo is called with is_secure equal to TRUE, the operation
         is asserted to be secure, ignoring all security checks. If is_secure is
         equal to FALSE the operation is not known to be secure and security will
         be checked for all host objects in the prototype chain.

         Upon return the result of HasPropertyWithInfo indicates if a property was
         found, and if that is the case then:

         * info will contain the attributes of the property.
         * owner will contain a pointer to the object the property was found in.
         * can_cache will be TRUE if the property could be cached.

         Regardless of what HasPropertyWithInfo returns:

         * is_secure will be TRUE if all security checks succeeded, FALSE otherwise.

         Notice that HasPropertyWithInfo doesn't throw an exception if a security check
         fail, that is the responsibility of the caller.
    */

    inline BOOL HasPropertyWithInfo(ES_Execution_Context *context, JString *name, ES_Property_Info &info, ES_Object *&owner, BOOL &is_secure);

    BOOL HasIndexedProperties();
    /**< Returns TRUE if this object or any object in its prototype
         chain has any indexed properties. */
    BOOL HasIndexedGettersOrSetters();
    /**< Returns TRUE if this object or any object in its prototype
         chain has any indexed getters or setters. */

    InstanceOfResult InstanceOf(ES_Execution_Context *context, ES_Object *constructor);

    DeleteResult DeleteL(ES_Execution_Context *context, JString *name, unsigned &result);
    /**< Remove name from the object. Returns FALSE to indicate that an exception has been
         set in the context, TRUE otherwise. This happens when this is a host object and
         deleting name would be a security violation. The value returned by reference is
         the actual result of the operation. TRUE if deletion was successful or if there
         is no such property in the object, FALSE otherwise.
    */

    DeleteResult DeleteL(ES_Execution_Context *context, unsigned index, unsigned &result);

    inline ES_Class *Class() const;
    inline void SetClass(ES_Class *klass);

    inline BOOL IsArrayObject() const;

    inline BOOL IsBooleanObject() const;

    inline BOOL IsDateObject() const;

    inline BOOL IsErrorObject() const;

    inline BOOL IsFunctionObject() const;
    inline BOOL IsNativeFunctionObject();
    /**< TRUE for script-defined functions (functions that aren't builtin or
         host functions.) */
    inline BOOL IsNativeFunctionObject(BOOL &is_strict_mode);
    /**< TRUE for script-defined functions (functions that aren't builtin or
         host functions.)  Sets 'is_strict_mode' if the object is a native
         function, otherwise leaves untouched. */

    inline BOOL IsGlobalObject() const;

    inline BOOL IsHostObject() const;
    inline void SetIsHostObject();

    enum IncludeInactiveTag { IncludeInactive };
    inline BOOL IsHostObject(IncludeInactiveTag include_inactive) const;

    inline BOOL IsHostFunctionObject() const;

    inline BOOL IsHiddenObject() const;
    inline void SetIsHiddenObject();

    inline BOOL HasMultipleIdentities() const;
    void SetHasMultipleIdentities(ES_Context *context);

    inline BOOL IsProxyInstanceAdded() const;
    inline void SetProxyInstanceAdded();
    inline void ClearProxyInstanceAdded();

    inline BOOL IsProxyCachingDisabled() const;
    inline void SetProxyCachingDisabled();

    inline BOOL HasVolatilePropertySet() const;
    inline void SetHasVolatilePropertySet();

    inline BOOL HasUnorderedProperties() const;
    inline void SetHasUnorderedProperties();

    inline BOOL HasBeenInlined() const;
    inline void SetHasBeenInlined();
    inline void ClearHasBeenInlined();

    inline BOOL IsCrossDomainAccessible() const;
    inline void SetIsCrossDomainAccessible();

    inline BOOL IsCrossDomainHostAccessible() const;
    inline void SetIsCrossDomainHostAccessible();

    inline BOOL HasGetterOrSetter() const;
    inline void SetHasGetterOrSetter();

    inline void SignalPropertySetChanged();

    inline BOOL IsNumberObject() const;

    inline BOOL IsRegExpObject() const;

    inline BOOL IsRegExpConstructor() const;

    inline BOOL IsStringObject() const;

    inline BOOL IsArgumentsObject() const;

    inline BOOL IsVariablesObject() const;

    inline BOOL IsArrayBufferObject() const;

    inline BOOL IsTypedArrayObject() const;

    inline BOOL IsScopeObject() const;
    inline void SetIsScopeObject();

    inline BOOL IsInToString() const;
    inline void SetInToString();
    inline void ResetInToString();

    inline BOOL IsBuiltInFunction() const;

#ifdef ES_NATIVE_SUPPORT
    inline ES_BuiltInFunction GetFunctionID() const;
    inline void SetFunctionID(ES_BuiltInFunction id);
#endif // ES_NATIVE_SUPPORT

    inline BOOL IsSingleton() const;
    inline BOOL HasHashTableProperties() const;

    inline BOOL IsExtensible() const;
    inline void SetIsNotExtensible();

    inline const char *ObjectName() const;

    inline unsigned Count() const { return property_count; }
    inline unsigned CountProperties() const { return property_count - klass->CountExtra(); };

    inline unsigned PropertyCount() const { return property_count; }

    void SetPropertyCount(unsigned count);

    BOOL RedefinePropertyL(ES_Execution_Context *context, JString *name, const ES_Property_Info &info, const ES_Value_Internal *value);
    BOOL RedefinePropertyL(ES_Execution_Context *context, unsigned index, const ES_Property_Info &info, const ES_Value_Internal *value);

    inline unsigned Capacity() const { OP_ASSERT(!IsVariablesObject()); return reinterpret_cast<ES_Box *>(properties - sizeof(ES_Box))->Size(); }
    inline unsigned RemainingCapacity() const { return reinterpret_cast<ES_Box *>(properties - sizeof(ES_Box))->Size() - SizeOf(); }
    inline unsigned SizeOf() const { return klass->SizeOf(property_count); }

    ES_Indexed_Properties *GetIndexedProperties() { return indexed_properties; }
    ES_Indexed_Properties *&GetIndexedPropertiesRef() { return indexed_properties; }

    inline void SetIndexedProperties(ES_Indexed_Properties *properties);
    inline void SetPlainCompactIndexedProperties(ES_Indexed_Properties *properties);

    ES_Property_Value_Table *GetHashTable() { return ES_Class::GetHashTable(klass); }

    class SimpleCachedPropertyAccess
    {
    public:
        void Initialize(ES_Object *object, JString *name);

        GetResult GetL(ES_Execution_Context *context, ES_Value_Internal &value);
        //PutResult PutL(const ES_Value_Internal &value);

    private:
        friend class ESMM;

        ES_Object *object;
        JString *name;
        unsigned class_id;
        ES_PropertyIndex index;
    };

    class CachedPropertyAccess
    {
    public:
        void Initialize(JString *name);

        GetResult GetL(ES_Execution_Context *context, ES_Object *object, ES_Value_Internal &value);

    private:
        friend class ESMM;

        JString *name;
        unsigned class_id;
        ES_PropertyIndex cached_index;
        ES_Object *prototype;
    };

    ES_Class *GetSubObjectClass() const;
    ES_Class *SetSubObjectClass(ES_Execution_Context *context);
    void SetSubObjectClass(ES_Context *context, ES_Class *sub_object_class);

    void ChangeClass(ES_Context *context, ES_Class *klass, BOOL needs_conversion);

    void ChangeAttribute(ES_Context *context, JString *name, const ES_Property_Info &info);
    void ChangeAttribute(ES_Context *context, unsigned index, const ES_Property_Info &info);

    ES_Class *ChangePrototype(ES_Context *context, ES_Object *prototype);

    ES_Box *GetProperties() { return reinterpret_cast<ES_Box *>(properties - sizeof(ES_Box)); }

    void SetProperties(ES_Value_Internal *props) { properties = reinterpret_cast<char *>(props) - 4; }
    void SetProperties(ES_Box *props) { properties = props->Unbox(); }

    inline void UpdateProperties(ES_Context *context, ES_Box *props);

    inline BOOL HasOwnNativeProperty(JString *name);
    inline BOOL HasOwnNativeProperty(JString *name, ES_Property_Info &info);

    inline BOOL HasOwnNativeProperty(unsigned index);
    inline BOOL HasOwnNativeProperty(unsigned index, ES_Property_Info &info);

    class ES_Value_Internal_Ref
    {
    public:
        ES_Value_Internal_Ref() : location(NULL), type(ES_STORAGE_WHATEVER), index(UINT_MAX) {}
        ES_Value_Internal_Ref(char *location, ES_StorageType type, ES_PropertyIndex index) { Set(location, type, index); }
        ES_Value_Internal_Ref(char *location, ES_Layout_Info layout, ES_PropertyIndex index) { Set(location + layout.GetOffset(), layout.GetStorageType(), index); }

        inline BOOL IsNull() const
        {
            return location == NULL;
        }

        inline void Set(char *location, ES_StorageType type, ES_PropertyIndex index)
        {
            this->location = location;
            this->type = type;
            this->index = index;
        }

        inline void Set(char *location, ES_Layout_Info layout, ES_PropertyIndex index)
        {
            Set(location + layout.GetOffset(), layout.GetStorageType(), index);
        }

        inline void Write(const ES_Value_Internal &value)
        {
            ES_Value_Internal::Memcpy(location, value, type);
        }

        inline void Read(ES_Value_Internal &value) const
        {
            ES_Value_Internal::Memcpy(value, location, type);
        }

        inline BOOL IsWhatever() const
        {
            return type == ES_STORAGE_WHATEVER;
        }

        inline BOOL IsBoxed() const
        {
            return type == ES_STORAGE_BOXED ||
                (type == ES_STORAGE_WHATEVER && reinterpret_cast<ES_Value_Internal *>(location)->IsBoxed());
        }

        inline BOOL IsGetterOrSetter() const
        {
            return (type == ES_STORAGE_BOXED && (*reinterpret_cast<ES_Boxed **>(location))->GCTag() == GCTAG_ES_Special_Mutable_Access) ||
                (type == ES_STORAGE_WHATEVER && reinterpret_cast<ES_Value_Internal *>(location)->IsGetterOrSetter());
        }

        inline BOOL IsObject() const
        {
            return type == ES_STORAGE_OBJECT || (type == ES_STORAGE_WHATEVER && reinterpret_cast<ES_Value_Internal *>(location)->IsObject());
        }

        inline ES_Value_Internal *GetValue_loc()
        {
            OP_ASSERT(type == ES_STORAGE_WHATEVER);
            return reinterpret_cast<ES_Value_Internal *>(location);
        }

        inline ES_Boxed *GetBoxed()
        {
            OP_ASSERT(type == ES_STORAGE_BOXED || type == ES_STORAGE_WHATEVER);
            if (type == ES_STORAGE_BOXED)
                return *reinterpret_cast<ES_Boxed **>(location);
            else
                return reinterpret_cast<ES_Value_Internal *>(location)->GetDecodedBoxed();
        }

        inline ES_Object *GetObject()
        {
            OP_ASSERT(type == ES_STORAGE_OBJECT || type == ES_STORAGE_WHATEVER);
            if (type == ES_STORAGE_OBJECT)
                return *reinterpret_cast<ES_Object **>(location);
            else
                return reinterpret_cast<ES_Value_Internal *>(location)->GetObject();
        }

        inline ES_StorageType GetType() const
        {
            return type;
        }

        inline void SetType(ES_StorageType type)
        {
            this->type = type;
        }

        inline BOOL CheckType(ES_ValueType value_type) const
        {
            return ES_Layout_Info::CheckType(ES_Value_Internal::ConvertToStorageType(value_type), type);
        }

        inline BOOL CheckType(ES_StorageType storage_type) const
        {
            return ES_Layout_Info::CheckType(storage_type, type);
        }

        inline char *GetLocation() const
        {
            return location;
        }

        inline ES_PropertyIndex Index() const
        {
            OP_ASSERT(unsigned(index) != UINT_MAX);
            return index;
        }

        inline void Update(char *properties, ES_Class *klass)
        {
            Set(properties, klass->GetLayoutInfoAtInfoIndex(index), index);
        }
    private:
        char *location;
        ES_StorageType type;
        ES_PropertyIndex index;
    };

    BOOL GetOwnLocation(JString *name, ES_Property_Info &info, ES_Value_Internal_Ref &value_loc);
    BOOL GetOwnLocationL(ES_Context *context, unsigned index, ES_Property_Info &info, ES_Value_Internal &value);

    GetResult GetWithLocationL(ES_Execution_Context *context, ES_Object *this_object, ES_Property_Info info, ES_Value_Internal_Ref &value_ref, ES_Value_Internal &value);

    PutResult PutWithLocation(ES_Execution_Context *context, ES_Object *this_object, ES_Property_Info info, ES_Value_Internal_Ref &value_loc, const ES_Value_Internal &value);

    PutResult ResetOwnFunction(ES_Execution_Context *context, JString *name, ES_Property_Info info, ES_Value_Internal_Ref &value_ref, const ES_Value_Internal &value);

    BOOL DeleteOwnPropertyL(ES_Context *context, JString *name);
    /**< Remove name from the object. Returns FALSE if there is no such property
         or if it is marked as read only. True otherwise.
    */

    BOOL DeleteOwnPropertyL(ES_Context *context, unsigned index);

    inline void PutPrivateL(ES_Context *context, unsigned private_name, const ES_Value_Internal &value, ES_Class *private_klass);
    inline BOOL GetPrivateL(ES_Context *context, unsigned private_name, ES_Value_Internal &value);
    inline BOOL DeletePrivateL(ES_Context *context, unsigned private_name);

    void PutNativeL(ES_Context *context, JString *name, const ES_Property_Info &new_info, const ES_Value_Internal &new_value);
    inline void PutNativeL(ES_Context *context, unsigned index, const ES_Property_Info &new_info, const ES_Value_Internal &new_value);

    BOOL GetNativeL(ES_Context *context, JString *name, ES_Value_Internal &value, ES_Property_Info *info = NULL, GetNativeStatus *status = NULL);
    inline BOOL GetNativeL(ES_Context *context, unsigned index, ES_Value_Internal &value);
    inline BOOL DeleteNativeL(ES_Context *context, JString *name);

    inline BOOL HasEnumerableProperties() const;

    ES_Value_Internal GetPropertyValueAtIndex(ES_PropertyIndex index) const;

    inline BOOL HasOwnProperty(ES_Context *context, unsigned index);

    inline BOOL HasOwnProperty(ES_Context *context, JString *name);

    inline BOOL HasOwnProperty(ES_Context *context, JString *name, ES_Property_Info &info, BOOL &is_secure);
    inline BOOL HasOwnProperty(ES_Context *context, unsigned index, ES_Property_Info &info, BOOL &is_secure);

    inline BOOL IsShadowed(ES_Context *context, ES_Object *object, JString *name);

    inline BOOL IsShadowed(ES_Context *context, ES_Object *object, unsigned index);

    JString *GetTypeOf(ES_Context *context);
    /**< Return string that the typeof operator should return for this object.
         Rule is that all objects yield "object" except functions, which return
         "function", except some callable host objects that return "object". */

    inline void Grow(ES_Context *context, ES_Class *new_klass, unsigned capacity, unsigned minimum_capacity);

    inline ES_Box *AllocateExtraPropertyStorage(ES_Context *context, ES_Class *klass, unsigned capacity, unsigned minimum_capacity);
    /**< Allocate a larger properties table. Used when pre-allocating property storage
         prior to extending the object's class. */

    inline void ReplaceProperties(ES_Context *context, ES_Box *new_properties, unsigned capacity);

    inline void AppendValueL(ES_Context *context, JString *name, ES_PropertyIndex &index, const ES_Value_Internal &value, unsigned attributes, ES_StorageType type);

    void ConvertToSingletonObject(ES_Context *context);
    /**< Convert object to a singleton object. */

    void ConvertToPrototypeObject(ES_Context *context, ES_Class *sub_object_class = NULL);
    /**< Convert object to a prototype object. */

    void ConvertObject(ES_Context *context, ES_Class *old_klass, ES_Class *new_klass);
    void ConvertObject(ES_Context *context, ES_Class *old_klass, ES_Class *new_klass, ES_Box *new_properties, unsigned from, unsigned to, unsigned start);

    void ConvertObjectSimple(ES_Context *context, ES_Class *new_klass, ES_LayoutIndex index, int diff, unsigned size, unsigned count);

    void ConvertProperty(ES_Context *context, ES_Value_Internal_Ref &value_ref, ES_StorageType new_type);

    inline BOOL IsPrototypeObject() const;

    inline void Invalidate();
    void InvalidateInstances();

    void AddInstance(ES_Context *context, ES_Class *sub_object_class, BOOL is_singleton);
    /**< AddInstance assumes that this object is a prototype and adds 'sub_object_class' to its
         list of instance objects. Instances are added with the ES_Class::static_data::object_name
         as key, and if two instances have the same name, then their roots must be the same.
         This to allow sharing of roots in ES_Class::MakeRoot (and using ES_Object::FindInstance).

         On the other hand, if the class added denotes a singleton class it must be possible to
         have several instances with the same name. In this case AddInstance should be called
         with 'is_singleton' set to TRUE. This only adds the class to the invalidation chain
         and not to the shared root classes. */

    ES_Class *FindInstance(ES_Context *context, JString *name) const;
    /**< Possibly find a root class with 'name', already added as an instance to the current
         object. */

    void RemoveSingletonInstance(ES_Heap *heap, ES_Class *sub_object_class);
    /**< The dual to AddInstance() over singletons, removing 'sub_object_class' as an instance of
         this prototype object. The instance sought removed may have been added as a singleton
         instance, and is a required step when replacing prototypes, maintaining the invariant
         that prototype chains together with their classes are acyclic. */

    ES_Boxed *GetPropertyNamesL(ES_Context *context, BOOL use_special_cases, BOOL enum_dont_enum);
    /**< Return all property names in the object and its prototype chain. If enum_dont_enum
         is TRUE, properties that aren't enumerable are also included. If use_special_cases
         is FALSE the returned box is always an ES_Boxed_List with values which are always
         ES_Indexed_Properties (compact or sparse). If use_special_cases is TRUE the
         returned box is different things depending on the state of the object:

         * ES_Class if there are only class properties and there are no properties added
           from the prototype chain. This will include non-enumerable properties
           regardless of enum_dont_enum.

         * ES_String_Object if the object is a string object and there are no properties
           added from the prototype chain.

         * ES_Indexed_Properties if there are no properties added from the prototype chain.
           The ES_Value_Internals will contain values of type JString* for named
           properties and unsigned for indexed properties.

         * ES_Boxed_List if there are objects added from the prototype chain, where
           the elements of the list are one of the above.

         If there are no properties, NULL is returned.

         A note about insertion order. Properties are enumerated starting from the instance
         and moving up the prototoype chain.
    */

    ES_Boxed *GetOwnPropertyNamesL(ES_Context *context, ES_Object *shadow, BOOL use_special_cases, BOOL enum_dont_enum);
    /**< Return all property names in the object. If enum_dont_enum is TRUE, properties
         that aren't enumerable are also included. If use_special_cases is FALSE the
         returned box is always an ES_Indexed_Properties (compact or sparse). If
         use_special_cases is TRUE the returned box is different things depending on the
         state of the object:

         * ES_Class if there are only class properties. This will include non-enumerable
           properties regardless of enum_dont_enum.

         * ES_String_Object if the object is a string object.

         * ES_Indexed_Properties otherwise. The ES_Value_Internals will contain values
           of type JString* for named properties and unsigned for indexed properties.

         If shadow is null, all properties of the object are included, else only properties
         not shadowed by shadow are included.

         If there are no properties, NULL is returned.

         A note about insertion order. Properties are enumerated starting with indexed
         properties in index order followed by named properties in insertion order.
    */

    inline ES_Boxed_List *GetPropertyNamesSafeL(ES_Context *context, BOOL enum_dont_enum);
    /**< Safe version of GetPropertyNamesL. Always returns an ES_Boxed_List of
         ES_Indexed_Properties.

         If there are no properties, NULL is returned.
    */

    inline ES_Indexed_Properties *GetOwnPropertyNamesSafeL(ES_Context *context, ES_Object *shadow, BOOL enum_dont_enum);
    /**< Safe version of GetOwnPropertyNamesL. Always returns an ES_Indexed_Properties.

         If there are no properties, NULL is returned.
    */

#ifdef ECMASCRIPT_DEBUGGER
    void TrackedByDebugger(BOOL tracked);
    /**< Set whether this object is tracked by debugger. If so we must inform
       the debugger when the object is destroyed. */

    BOOL IsTrackedByDebugger() const;
#endif // ECMASCRIPT_DEBUGGER

    inline ES_Boxed *GetBoxed(ES_Layout_Info layout) const;

    inline ES_Value_Internal *GetValue_loc(ES_Layout_Info layout) const;
    inline ES_Value_Internal *GetValueAtIndex_loc(ES_PropertyIndex index) const;

    inline static unsigned AllocationSize(ES_Class *klass, unsigned used, unsigned size);

    inline BOOL HasStorage() const;

    inline BOOL HasIntrinsicLength() const;

    inline static void Initialize(ES_Box *box) { ES_Box::Initialize(box); }

    BOOL HasCycle(ES_Object *maybe_cycle_start);

    static void Initialize(ES_Object *self, ES_Class *klass);

    inline void AllocateProperties(ES_Context *context);
    inline void AllocatePropertyStorage(ES_Context *context, unsigned size);

    void AppendOwnProperty(ES_Context *context, JString *name, ES_PropertyIndex &index, const ES_Value_Internal &value);

    ES_Boxed *GetInstances() const;
    void SetInstances(ES_Boxed *instance);

    inline BOOL SecurityCheck(ES_Execution_Context *context);
    /**< If not 'IsHostObject()', return TRUE, otherwise perform security check,
         and if that fails return FALSE, otherwise return TRUE. */

    BOOL SecurityCheck(ES_Execution_Context *context, const char *message);
    /**< Returns the same as SecurityCheck(ES_Execution_Context*), and if that returns
         FALSE throw a ReferenceError with the supplied message.

         The message should typically have the prefix "Security error: ".  Such
         a prefix is not added by this function. */


    friend class ESMM;

    class PropertyDescriptor
    {
    public:
        PropertyDescriptor()
        {
            Reset();
        }

        void Reset()
        {
            value.SetUndefined();
            accessor = NULL;
            getter = NULL;
            setter = NULL;
            info.Reset();
            is_undefined = 0;
            is_host_property = 0;
            has_enumerable = 0;
            has_configurable = 0;
            has_value = 0;
            has_writable = 0;
            has_getter = 0;
            has_setter = 0;
        }

        ES_Value_Internal value;
        ES_Special_Mutable_Access *accessor;
        ES_Object *getter;
        ES_Object *setter;
        ES_Property_Info info;

        unsigned is_undefined:1;
        unsigned is_host_property:1;
        unsigned has_enumerable:1;
        unsigned has_configurable:1;
        unsigned has_value:1;
        unsigned has_writable:1;
        unsigned has_getter:1;
        unsigned has_setter:1;

        void SetEnumerable(BOOL value) { has_enumerable = 1; info.SetEnumerable(value); }
        void SetConfigurable(BOOL value) { has_configurable = 1; info.SetConfigurable(value); }
        void SetValue(const ES_Value_Internal &value0) { has_value = 1; value = value0; }
        void SetWritable(BOOL value) { has_writable = 1; info.SetWritable(value); }
        void SetGetter(ES_Object *value) { has_getter = 1; getter = value; }
        void SetSetter(ES_Object *value) { has_setter = 1; setter = value; }
    };

    static BOOL IsAccessorDescriptor(const PropertyDescriptor &desc) { return desc.has_getter || desc.has_setter; }
    static BOOL IsDataDescriptor(const PropertyDescriptor &desc) { return desc.has_value || desc.has_writable; }
    static BOOL IsGenericDescriptor(const PropertyDescriptor &desc) { return !IsAccessorDescriptor(desc) && !IsDataDescriptor(desc); }

    BOOL GetOwnPropertyL(ES_Execution_Context *context, PropertyDescriptor &desc, JString *name, unsigned index, BOOL fetch_value, BOOL fetch_accessors);
    /**< 8.12.1 [[GetOwnProperty]]

         @param fetch_value If TRUE, the 'value' field is set in 'desc'.  If
                            FALSE, 'has_value' is set, but 'value' is left
                            undefined.

         @param fetch_accessors If TRUE, the 'getter' and 'setter' fields are
                                set in 'desc', possibly first creating a host
                                getter object.  If FALSE, 'has_getter' and
                                'has_setter' are set, but 'getter' and 'setter'
                                are NULL. */

    BOOL GetPropertyL(ES_Execution_Context *context, PropertyDescriptor &desc, JString *name, unsigned index, BOOL fetch_value, BOOL fetch_accessors);
    /**< 8.12.2 [[GetProperty]]

         @param fetch_value If TRUE, the 'value' field is set in 'desc'.  If
                            FALSE, 'has_value' is set, but 'value' is left
                            undefined.

         @param fetch_accessors If TRUE, the 'getter' and 'setter' fields are
                                set in 'desc', possibly first creating a host
                                getter object.  If FALSE, 'has_getter' and
                                'has_setter' are set, but 'getter' and 'setter'
                                are NULL. */

    BOOL DefineOwnPropertyL(ES_Execution_Context *context, JString *name, unsigned index, const PropertyDescriptor &desc, const char *&message);
    /**< 8.12.9 [[DefineOwnProperty]] */

public:
    union
    {
        unsigned object_bits;
        /**< Additional header bits needed for objects (to free up space in
           ES_Header). See struct below. */
#ifdef _DEBUG
        struct
        {
            unsigned is_host_object           : 1;
            unsigned is_global_object         : 1;
            unsigned is_dispatched_ctor       : 1;
            unsigned is_native_fn             : 1;
            unsigned is_builtin_fn            : 1;
            unsigned is_eval_fn               : 1;
            unsigned is_apply_fn              : 1;
            unsigned is_in_tostring           : 1;
            unsigned hidden_object            : 1;
            unsigned multiple_identities      : 1;
            unsigned is_scope_object          : 1;
            unsigned has_unknown_properties   : 1;
            unsigned has_unordered_properties : 1;
            unsigned has_been_inlined         : 1;
            unsigned tracked_by_debugger      : 1;
            unsigned has_relaxed_security     : 1;
            unsigned has_getter_or_setter     : 1;
            unsigned simple_compact_indexed   : 1;
            unsigned mutable_compact_indexed  : 1;
            unsigned static_byte_array        : 1;
            unsigned proxy_instance_added     : 1;
            unsigned proxy_caching_disabled   : 1;
            unsigned function_arguments_read  : 1;
            unsigned function_id              : 9;
        } as_struct;
#endif // _DEBUG
    };

    enum
    {
        MASK_IS_HOST_OBJECT           = 1 << 0,
        MASK_IS_GLOBAL_OBJECT         = 1 << 1,
        MASK_IS_DISPATCHED_CTOR       = 1 << 2,
        MASK_IS_NATIVE_FN             = 1 << 3,
        MASK_IS_BUILTIN_FN            = 1 << 4,
        MASK_IS_EVAL_FN               = 1 << 5,
        MASK_IS_APPLY_FN              = 1 << 6,
        MASK_IS_IN_TOSTRING           = 1 << 7,
        MASK_HIDDEN_OBJECT            = 1 << 8,
        MASK_MULTIPLE_IDENTITIES      = 1 << 9,
        MASK_IS_SCOPE_OBJECT          = 1 << 10,
        MASK_HAS_VOLATILE_PROPERTIES  = 1 << 11,
        MASK_HAS_UNORDERED_PROPERTIES = 1 << 12,
        MASK_HAS_BEEN_INLINED         = 1 << 13,
        MASK_TRACKED_BY_DEBUGGER      = 1 << 14,
        MASK_ALLOW_CROSS_DOMAIN_ACCESS = 1 << 15,
        MASK_HAS_GETTER_OR_SETTER     = 1 << 16,
#ifdef ES_NATIVE_SUPPORT
        MASK_SIMPLE_COMPACT_INDEXED   = 1 << 17,
        MASK_MUTABLE_COMPACT_INDEXED  = 1 << 18,
        MASK_BYTE_ARRAY_INDEXED       = 1 << 19,
        MASK_TYPE_ARRAY_INDEXED       = 1 << 20,
        MASK_INDEXED                  = ( MASK_SIMPLE_COMPACT_INDEXED | MASK_MUTABLE_COMPACT_INDEXED | MASK_BYTE_ARRAY_INDEXED | MASK_TYPE_ARRAY_INDEXED ),
#endif // ES_NATIVE_SUPPORT
        MASK_PROXY_INSTANCE_ADDED     = 1 << 21,
        MASK_PROXY_CACHING_DISABLED   = 1 << 22,
        MASK_FUNCTION_ARGUMENTS_READ  = 1 << 23,
        MASK_IS_NOT_EXTENSIBLE        = 1 << 24,
        MASK_ALLOW_CROSS_DOMAIN_HOST_ACCESS = 1 << 25,
        FUNCTION_ID_SHIFT             = 26,
        MASK_FUNCTION_ID              = 0x3fu << FUNCTION_ID_SHIFT
    };

    unsigned property_count;

    ES_Class *klass;
    char *properties;

    ES_Indexed_Properties *indexed_properties;
};

/* A variable object keeps an unused property table while
   it is initially attached to the register frame.

   Upon being detached from the frame, the register properties are
   copied down into this table and the variable object turned
   into an ES_Object. To avoid allocating extra storage
   to hold a reference to the spare property table, the variable
   object reuses indexed_properties for the purpose -- it is
   otherwise unused. i.e., the shape of an ES_Variable_Object
   is identical to that of an ES_Object. */
class ES_Variable_Object : public ES_Object
{
public:
    static ES_Variable_Object *Make(ES_Context *context, ES_Class *klass, ES_Value_Internal *properties);

    static void Initialize(ES_Variable_Object *self, ES_Class *klass);

    inline static void Initialize(ES_Box *box) { ES_Box::Initialize(box); }

    void CopyPropertiesFrom(ES_Execution_Context *context, ES_Value_Internal *values);

    void CreateAliasedFrom(ES_Context *context, ES_Value_Internal *values);
};

#ifdef _DEBUG
# undef inline
#endif // _DEBUG

#endif // ES_OBJECT_H
