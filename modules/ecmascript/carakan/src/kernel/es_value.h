/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

/**
 * ES_Value_Internal and operations on ES_Value.
 *
 * @section value_set             ECMAScript values
 *
 * @section values                Represented values
 *
 * A value in the engine is a tagged union 'ES_Value' of the following types:
 *
 * <ul>
 * <li>   null
 * <li>   undefined
 * <li>   number:   IEEE double values
 * <li>   boolean:  TRUE, FALSE, magic cookie values
 * <li>   string:   JString value
 * <li>   object:   ES_Object value
 * </ul>
 *
 * The ES_Value_Internal type is itself constructed from a value union field and a
 * type field; in attribute lists, these are stored separately in order to
 * conserve space.
 *
 * The internal 'Reference' type from the ECMAScript specification is not a
 * represented value: the compiler resolves all references at compile time
 * by generating different code for lvalues and rvalues.  This extends to
 * its use in E4X.
 *
 * Two new 'internal' types are introduced by E4X: 'AnyName' and 'AttributeName'.
 * Since the E4X support library is written in ECMAScript in this engine, these
 * types are represented as object types in the engine.
 *
 * The AnyName type is represented as a distinguished object, whose value can be
 * obtained with the instruction 'XMLOP1(E4X_ANYNAME)'.
 *
 * The AttributeName type is represented as a new object type; if x is an
 * AttributeName then typeof x => "attributename".  (As an optimization matter,
 * QName objects should probably carry a lazily constructed value to use when
 * the QName is converted to AttributeName, to avoid consing up AttributeNames
 * all the time.)
 *
 *
 * @subsection heap_storage       Heap allocated storage
 *
 * Objects allocated on the garbage-collected heap are all derived from the
 * type ES_Boxed.  This includes the value types ES_Object and JString, but
 * also numerous internal data types like string storage, identifiers (property
 * names), ribs (environment objects), property lists, and code vectors.
 *
 * Normally only the value types ES_Object (and its subtypes) and JString are
 * available through ES_Value.  Some private properties (not available to
 * code written in ECMAScript) in the objects may however contain boxed types
 * other than these two.
 *
 * See es_boxed.h for more information.
 */

#ifndef ES_VALUE_H
#define ES_VALUE_H

class ES_Execution_Context;

/*
 * Translation from futhark:
 * futark        carakan
 * as_*      ->  Get*    Value must be of right type
 * To*Now    ->  To*     Changes value inplace (uses As*)
 * To*Slow   ->  As*     Returns the value converted (not reference to JS_Value as futhark did)
 */

/**
 * ES_ValueType enumerates the ECMAScript types represented by a ES_Value.
 *
 * The type ordering in the enum is not arbitrary: Pointer objects follow
 * non-pointer objects, and ESTYPE_BOXED is known to be the first pointer
 * object.  The operation is_boxed() uses this fact.
 *
 * IMPORTANT: The native code generation depends on the exact order and values
 * in this enumeration, in particular if !ES_VALUES_AS_NANS.  For instance:
 * IsNullOrUndefined() is implemented by subtracting 2 from the type and
 * checking the carry flag, which works if only ESTYPE_NULL and ESTYPE_UNDEFINED
 * are below two.
 */
enum ES_ValueType
{
#if defined(ES_VALUES_AS_NANS)
    ESTYPE_DOUBLE          = 0x7ffffff7, // Must be first. Never stored, just for return from Type().
    ESTYPE_INT32_OR_DOUBLE = 0x7ffffff8, // For type analysis only; never used on actual values.
    ESTYPE_INT32           = 0x7ffffff9,
    ESTYPE_UNDEFINED       = 0x7ffffffa,
    ESTYPE_NULL            = 0x7ffffffb,
    ESTYPE_BOOLEAN         = 0x7ffffffc,
    ESTYPE_BOXED           = 0x7ffffffd, // Some traced type without further type information; any subtype of ES_Boxed must have a higher value.
    ESTYPE_STRING          = 0x7ffffffe,
    ESTYPE_OBJECT          = 0x7fffffff
#else
    // These have "bit-field" style values to enable simple multiple
    // type identification in machine code using bitwise and.
    ESTYPE_UNDEFINED       = 0,   // Useful for this to be zero, for initialization purposes
    ESTYPE_NULL            = 1,
    ESTYPE_DOUBLE          = 2,
    ESTYPE_INT32           = 4,
    ESTYPE_INT32_OR_DOUBLE = 6,  // For type analysis only; never used on actual values.
    ESTYPE_BOOLEAN         = 8,
    ESTYPE_BOXED           = 16,  // Some traced type without further type information; any subtype of ES_Boxed must have a higher value.
    ESTYPE_STRING          = 32,
    ESTYPE_OBJECT          = 64
#endif
};

enum ES_ValueTypeBits
{
#ifdef ES_VALUES_AS_NANS
    ESTYPE_BITS_OBJECT          = 1,
    ESTYPE_BITS_STRING          = 2,
    ESTYPE_BITS_BOXED           = 4,
    ESTYPE_BITS_BOOLEAN         = 8,
    ESTYPE_BITS_NULL            = 16,
    ESTYPE_BITS_UNDEFINED       = 32,
    ESTYPE_BITS_INT32           = 64,
    ESTYPE_BITS_DOUBLE          = 128,
#else // ES_VALUES_AS_NANS
    ESTYPE_BITS_UNDEFINED       = 1,
    ESTYPE_BITS_NULL            = 2,
    ESTYPE_BITS_DOUBLE          = 4,
    ESTYPE_BITS_INT32           = 8,
    ESTYPE_BITS_BOOLEAN         = 16,
    ESTYPE_BITS_BOXED           = 32,
    ESTYPE_BITS_STRING          = 64,
    ESTYPE_BITS_OBJECT          = 128,
#endif // ES_VALUES_AS_NANS
    ESTYPE_BITS_ALL_TYPES       = 255
};


/**
 * ES_StorageType enumarates types used when dealing with the char *
 * representaion for ECMAScript types.
 */
enum ES_StorageType
{
    ES_STORAGE_WHATEVER = 0,
    ES_STORAGE_BOOLEAN,
    ES_STORAGE_INT32,
    ES_STORAGE_UNDEFINED,
    ES_STORAGE_NULL,
    ES_STORAGE_DOUBLE,

    ES_STORAGE_INT32_OR_DOUBLE,

    ES_STORAGE_STRING,
    ES_STORAGE_STRING_OR_NULL,
    ES_STORAGE_OBJECT,
    ES_STORAGE_OBJECT_OR_NULL,
    ES_STORAGE_BOXED,

    ES_STORAGE_SPECIAL_PRIVATE,

    FIRST_VALUE_TYPE = ES_STORAGE_BOOLEAN,
    LAST_UNBOXED_TYPE = ES_STORAGE_DOUBLE,
    FIRST_SPECIAL_TYPE = ES_STORAGE_SPECIAL_PRIVATE,
    LAST_BOXED_TYPE = ES_STORAGE_SPECIAL_PRIVATE,

    LAST_SIZE_4 = ES_STORAGE_INT32
};

enum ES_StorageTypeBits
{
    ES_STORAGE_BITS_WHATEVER = 0,
    ES_STORAGE_BITS_BOOLEAN = 1,
    ES_STORAGE_BITS_INT32 = 2,
    ES_STORAGE_BITS_UNDEFINED = 4,
    ES_STORAGE_BITS_NULL = 8,
    ES_STORAGE_BITS_DOUBLE = 16,

    ES_STORAGE_BITS_STRING = 32,
    ES_STORAGE_BITS_STRING_OR_NULL = 32,
    ES_STORAGE_BITS_OBJECT = 64,
    ES_STORAGE_BITS_OBJECT_OR_NULL = 64,
    ES_STORAGE_BITS_BOXED = 128
};

enum ES_CachedTypeBits
{
    ES_VALUE_TYPE_MASK        = 0x000000ff,
#ifdef ES_VALUES_AS_NANS
    ES_VALUE_TYPE_INIT_MASK   = 0x7fffff00,
#else
    ES_VALUE_TYPE_INIT_MASK   = 0x00000000,
#endif
    ES_VALUE_TYPE_SHIFT       = 0,
    ES_STORAGE_TYPE_MASK      = 0x000fff00,
    ES_STORAGE_TYPE_SHIFT     = 8,
    ES_STORAGE_SIZE_MASK      = 0x0ff00000,
    ES_STORAGE_SIZE_SHIFT     = 20,
    ES_STORAGE_NULL_MASK      = 0x70000000,
    ES_STORAGE_NULL_SHIFT     = 28
};

/**
 * ES_ValueUnion is a union that represents the values of a ES_Value, but
 * without any type.
 */
union ES_ValueUnion
{
    INT32           i32;
    int             boolean;    // Really, not BOOL.  Other values are used for magic cookies (666, 667, ...)
    ES_Boxed*       boxed;    // For GC tracing: JString and ES_Object are subclasses of ES_Boxed
    JString*        string;
    ES_Object*      object;
#if !defined(ES_VALUES_AS_NANS)
    double          number;
#endif
};

/**
 * ES_Value_Internal is a type-plus-value container for ECMAScript values.
 *
 * Operations on ES_Value_Internal are inline functions that take a ES_Value_Internal as
 * the first parameter, not methods on ES_Value, because ES_Value_Internal may
 * have a representation that does not allow methods.
 */
class ES_Value_Internal
{
public:
#if !defined(ES_VALUES_AS_NANS)
    ES_ValueType private_type;
    unsigned private_extra;
    ES_ValueUnion private_value;
#else
    union
    {
        double double_value;
        struct {
#if IEEE_BITS_HI // the sign + exponent part are at higher address (as in 8087)
            ES_ValueUnion value;
            ES_ValueType   type;
#else            // the sign + exponent part are at lowerr address (as in Mac)
            ES_ValueType   type;
            ES_ValueUnion value;
#endif
        } other;
    };
#endif

    ES_Value_Internal() { SetUndefined(); }
    ES_Value_Internal(INT32 i) { SetInt32(i); }
    ES_Value_Internal(UINT32 u) { SetUInt32(u); }
    ES_Value_Internal(double d) { SetNumber(d); }
    ES_Value_Internal(JString *s) { SetString(s); }
    ES_Value_Internal(ES_Object *o) { SetObject(o); }
    ES_Value_Internal(ES_Boxed *o) { SetBoxed(o); }

    static ES_Value_Internal Boolean(BOOL b) { ES_Value_Internal value; value.SetBoolean(b); return value; }
    static ES_Value_Internal Object(ES_Object *o, ES_ValueType t) { ES_Value_Internal value; if (o) value.SetObject(o); else value.SetType(t); return value; }

    inline ES_ValueType Type() const;
    /**< @return the type of the value */

    inline ES_ValueType TypeTag() const;
    /**< @return the type tag of the value.
       Note: it will never return ESTYPE_DOUBLE if ES_VALUES_AS_NANS is defined. */

    static inline unsigned TypeBits(ES_ValueType type);
    static inline ES_ValueType TypeFromBits(unsigned bits);

    inline unsigned TypeBits() const;

    static inline BOOL IsMonoTypeBits(unsigned bits);

    JString* TypeString(ES_Context *context) const;
    /**< @return the type of the value as a string*/

    inline BOOL IsUndefined() const;
    /**< @return TRUE if value is of the undefined type */

    inline BOOL IsNull() const;
    /**< @return TRUE if value is of the null type */

    inline BOOL IsNullOrUndefined() const;
    /**< @return TRUE if value is of the null type or the undefined type */

    inline BOOL IsBoolean() const;
    /**< @return TRUE if value is of the boolean type (even if it represents a magic cookie) */

    inline BOOL IsNumber() const;
    /**< @return TRUE if value is of the number type */

    inline BOOL IsDouble() const;
    /**< @return TRUE if value is a 'double' number type */

    inline BOOL IsInt32() const;
    /**< @return TRUE if value is a 'int32' number type */

    inline BOOL IsUInt32() const;
    /**< @return TRUE if value is an 'int32' or 'double' number type in the range of an UINT32.
       Returns TRUE if the value of value is -0.0  */

    inline BOOL IsString() const;
    /**< @return TRUE if value is of the string type */

    inline BOOL IsObject() const;
    /**< @return TRUE if value is of the object type */

    inline BOOL IsHiddenObject(ES_Execution_Context *context) const;
    /**< @return TRUE if value is of the object type */

    inline BOOL IsCallable(ES_Execution_Context *context) const;
    /**< @return TRUE if value is of the object type, and the object implements [[Call]] */

    inline BOOL IsBoxed() const;
    /**< @return TRUE if value is of the boxed type */

    inline BOOL IsDecodedBoxed() const;
    /**< @return TRUE if value is of some boxed type (boxed, object or string) */

    inline BOOL IsArgumentsCookie() const;
    /**< @return TRUE if value is an arguments cookie type */

    inline BOOL IsNoExceptionCookie() const;
    /**< @return TRUE if value is a no exception cookie type */

    inline BOOL IsGetterSetterCookie() const;
    /**< @return TRUE if value is a getter/setter cookie type */

    inline BOOL IsSpecial() const;
    /**< @return TRUE if value is a special property */

    inline BOOL IsGetterOrSetter() const;
    /**< @return TRUE if value is a getter or setter */

    inline void    SetType(ES_ValueType type);
    /**< Set this value's type while leaving the value undefined */

    inline void    SetUndefined();
    /**< Set this value to the undefined value */

    inline void    SetUndefined(BOOL hidden_boolean);
    /**< Set this value to the undefined value, storing 'hidden_boolean' where a
         boolean's value would have been stored.  'Hidden_boolean' can be read
         using GetHiddenBoolean(). */

    inline void    SetNull();
    /**< Set this value to the null value */

    inline void    SetBoolean(BOOL boolean);
    /**< Set this value to the given boolean value */

    inline void SetTrue();
    /**< Set this value to the boolean value TRUE */

    inline void SetFalse();
    /**< Set this value to the boolean value FALSE */

    inline void SetNumber(double number);
    /**< Set this value to the given numeric value with type DOUBLE or INT32 as appropriate */

    inline void    SetDouble(double number);
    /**< Set this value to the given double value */

    inline void SetUInt32(UINT32 number);
    /**< Set this value to the given unsigned integer value
       DO NOT CALL THIS.  It is here just to catch errors. */

    inline void SetInt32(INT32 number);
    /**< Set this value to the given integer value */

    inline void SetNan();
    /**< Set this value to the NaN number value */

    inline void    SetString(JString* string);
    /**< Set this value to the given string value */

    inline void    SetObject(ES_Object* object);
    /**< Set this value to the given object value */

    inline void SetArgumentsCookie();
    /**< Set this value to the magic arguments cookie value */

    inline void SetNoExceptionCookie();
    /**< Set this value to the magic no exception cookie value */

    inline void SetBoxed(ES_Boxed* boxed);
    /**< Set this value to the given boxed (ES_Object or JString) value */

    inline BOOL GetBoolean() const;
    /**< Value must be an ESTYPE_BOOLEAN
       @return this value's boolean value
    */

    inline BOOL GetHiddenBoolean() const;
    /**< Read hidden boolean value set by SetUndefined(BOOL).
       @return this value's hidden boolean value
    */

    inline double GetNumAsDouble() const;
    /**< Value must be an ESTYPE_DOUBLE or ESTYPE_INT32
       @return this value's number value as a double
    */

    inline INT32 GetNumAsInt32() const;
    /**< Value must be an ESTYPE_DOUBLE or ESTYPE_INT32
       @return this value's number value as an INT32
    */

    inline UINT32 GetNumAsUInt32() const;
    /**< Value must be an ESTYPE_DOUBLE or ESTYPE_INT32
       @return this value's number value as an UINT32
    */

    inline double GetNumAsInteger() const;
    /**< Value must be an ESTYPE_DOUBLE or ESTYPE_INT32
       @return this value's number value as a double but truncated to integer
    */

    inline INT32 GetNumAsBoundedInt32(INT32 nan_value = 0) const;
    /**< Value must be an ESTYPE_DOUBLE or ESTYPE_INT32
       @return this value's number value as an INT32, with special case that
               +Infinity is INT_MAX and -Infinity is INT_MIN, and NaN will
               become nan_value.
               Not part of specification, but useful in some places when
               implementing things. Please use this with care.
    */

    double GetNumAsIntegerRound() const;
    /**< Value must be an ESTYPE_DOUBLE or ESTYPE_INT32
       @return this value's number value as a double but rounded to nearest integer,
               rounding to even on a tie.
    */

    INT32 GetNumAsBoundedInt32Slow(INT32 nan_value) const;
    /**< Slow path of GetNumAsBoundedInt32 */

    unsigned GetNumAsUint8Clamped() const;
    /**< Value must be an ESTYPE_DOUBLE or ESTYPE_INT32.
       @return this value's number value, but clamped to the
               [0, 255] range. That is, negative values will
               be clamped to zero, and all values larger than
               255 will be 255. NaN is mapped to 0.
    */

    inline double GetDouble() const;
    /**< Value must be an ESTYPE_DOUBLE
       @return this value's raw double number value
    */

    inline INT32 GetInt32() const;
    /**< Value must be an ESTYPE_INT32
       @return this value's raw INT32 number value
    */

    inline ES_Object* GetObject() const;
    /**< Value must be an ESTYPE_OBJECT
       @return this value's object pointer
    */

    inline ES_Object* GetObject(ES_Context *context) const;
    /**< Value must be an ESTYPE_OBJECT
       @return the computed identity of the value's object pointer
    */

    inline ES_Object*& GetObject_ref();
    /**< Value must be an ESTYPE_OBJECT
       @return this value's object pointer as a reference
    */


    inline JString* GetString() const;
    /**< Value must be an ESTYPE_STRING
       @return this value's string value
    */

    inline JString*& GetString_ref();
    /**< Value must be an ESTYPE_STRING
       @return this value's string value as a reference
    */

    inline ES_Boxed* GetBoxed() const;
    /**< Value must be an ESTYPE_BOXED
       @return this value's value as a markable object
    */

    inline ES_Boxed*& GetBoxed_ref();
    /**< Value must be an ESTYPE_BOXED
       @return this value's value as a markable object reference
    */


#ifndef ES_VALUES_AS_NANS
    inline unsigned GetExtra() const;
    /**< Return the extra 32-bit integer stored in what would otherwise have been padding.
         @return this value's extra storage */

    inline void SetExtra(unsigned extra);
    /**< Set the extra 32-bit integer stored in what would otherwise have been padding. */

    inline ES_Boxed* decode_boxed(ES_ValueUnion& v, int type);
    /**< @param value has a value for boxed, string, or object
       @param type is ESTYPE_BOXED, ESTYPE_STRING, or ESTYPE_OBJECT
       @return the contained value cast to ES_Boxed*
    */
#endif

    inline ES_Boxed* GetDecodedBoxed() const;
    /**< Value must be an ESTYPE_BOXED, ES_STRING or ES_OBJECT
       @return the contained value cast to ES_Boxed*
    */

#if 0
    inline ES_Boxed*& GetDecodedBoxed_ref();
    /**< Value must be an ESTYPE_BOXED, ES_STRING or ES_OBJECT
       @return the contained value cast to ES_Boxed* as a refernce
    */

    inline ES_Boxed*& GetForcedDecodedBoxed_ref();
    /**< Value _does_not_ have to be an ESTYPE_BOXED, ES_STRING or ES_OBJECT
       @return the contained value cast to ES_Boxed* as a refernce
    */
#endif // 0

#ifdef ES_NATIVE_SUPPORT
    static int ES_CALLING_CONVENTION ReturnAsBoolean(ES_Value_Internal *value) REALIGN_STACK;
    /**< Return ToBoolean(value) without changing value. */
#endif // ES_NATIVE_SUPPORT

    inline BOOL ToNumber(ES_Execution_Context *context);
    /**< Convert the value in-place to ESTYPE_DOUBLE or ESTYPE_INT32.
       @return TRUE if the conversion succeeded, otherwise FALSE.
    */

    inline BOOL ToString(ES_Execution_Context *context);
    /**< Convert the value in-place to ESTYPE_STRING.
       @return TRUE if the conversion succeeded, otherwise FALSE.
    */

    inline BOOL ToString(ES_Context *context);
    /**< Convert the value in-place to ESTYPE_STRING.  Not supported on
         ESTYPE_OBJECT values!
       @return TRUE if the conversion succeeded, otherwise FALSE.
    */

    inline void ToBoolean();
    /**< Convert the value in-place to ESTYPE_BOOLEAN.
    */

    inline BOOL ToObject(ES_Execution_Context *context, BOOL throw_exception = TRUE);
    /**< Try to convert the value in-place to ESTYPE_OBJECT.
       @return TRUE if the conversion succeeded, otherwise FALSE.
    */

    inline BOOL SimulateToObject(ES_Execution_Context *context) const;
    /**< Simulate a call to ToObject().  Essentially: throw TypeError
         and return FALSE if value is null or undefined, otherwise do
         nothing and return TRUE. */

    enum HintType
    {
        HintNone = 0,
        HintNumber,
        HintString
    };
    /**< The translation of a value into its primitive representation
         is parameterised over a type hint. The hint
         encodes the representation preferred, e.g., (*) hints for
         numbers. No-hint is equal to a number hint, except for Date
         objects which interprets it as a string hint.

         Spec reference: 9.1.
    */

    BOOL ToPrimitive(ES_Execution_Context *context, HintType hint);
    /**< Convert the value in-place to its primitive, guided by the supplied type hint.
       @return TRUE if the conversion succeeded, otherwise FALSE.
    */

//BOOL Compare(const ES_Value& X, const ES_Value& Y);
    /**< @return TRUE if the two values are the same, otherwise FALSE.
       Strings are compared character-by-character; other values are
       compared by identity.

       OPTIMIZEME: only strings really need to go out-of-line, and
       only if they are not the same string object.  It should be
       straightforward to inline most of this at low cost.  (Note,
       we cannot rely on bit-by-bit comparison to work for the class
       representation of ES_Value, at least.)
    */

    ES_Value_Internal AsNumber(ES_Context *context) const;
    /**< Value is a non-number primitive value.
       @return a value that represents value converted to number using
       the ToNumber algorithm.
    */

    ES_Value_Internal AsBoolean() const;
    /**< Value is any non-boolean value (including object)
       @return a value that represents value converted to boolean using
       the ToBoolean algorithm.
    */

    inline ES_Value_Internal AsString(ES_Context *context) const;
    ES_Value_Internal AsStringSlow(ES_Context *context) const;
    /**< Value is a non-string primitive value.
       @return a value that represents value converted to string using
       the ToString algorithm.
    */

    ES_Value_Internal AsStringSpecial(ES_Execution_Context *context) const;
    /**< Value is a non-string primitive value.
     * It behaves the same as AsString with the difference
     * that null is converted to "" instead of "null" (this is the 'special' part is about).
     * This is needed to be compliant with Gecko and Webkit when it comes to passing null
     * to the host object functions/properties expecting a string.
     * @return a value that represents value converted to string using
     * the ToString algorithm.
     */

    BOOL AsObject(ES_Execution_Context *context, ES_Value_Internal& dest, BOOL throw_exception) const;
    /**< Value is a non-object value.
       @param context (in) a context.
       @param dest (out) a location to receive a converted value
       @return TRUE if the conversion succeeded, FALSE otherwise.

       If this value is not undefined or null, stores an object representation
       of this value in dest, and returns TRUE.  Otherwise leaves dest alone
       and returns FALSE.

       Clients that call this must check the return value and throw an
       appropriate exception if it returns FALSE, as dest is garbage if
       the operation fails.
    */

    inline BOOL FastEqual(ES_Value_Internal& lhs);

    void ImportL(ES_Context *context, const ES_Value& src);
    /**< Import an ES_Value_Internal into a ES_Value.  String data will
       be copied into a local JString.
    */
    void ImportUnlockedL(ES_Context *context, const ES_Value& src);
    /**< Import an ES_Value_Internal into a ES_Value. If the operation will require object allocation, lock out garbage collection.
    */

    OP_STATUS Import(ES_Context *context, const ES_Value& src);
    /**< Like ImportL(), but does not leave */

    inline void ExportL(ES_Context *context, ES_Value& dst, char format = '-', ES_ValueString *dst_string = NULL) const;
    /**< Export a ES_Value_Internal to an ES_Value.  String data will *not*
       be copied but will be NUL-terminated in place and a pointer
       to the NUL-terminated character array will be exported.

       The "Identity" method will be called on object values.  On
       foreign objects this may entail creating an initializing a
       lazily created object.
    */

    OP_STATUS Export(ES_Context *context, ES_Value& dst) const;
    /**< Like ExportL(), but does not leave */

    inline BOOL ConvertL(ES_Execution_Context *context, char format);

# ifdef ECMASCRIPT_DECOMPILER

    void Decompile(JString* output);
    /**< @param output (out) is an output buffer
       Append a human-readable representation of value onto the output.
    */

# endif // ECMASCRIPT_DECOMPILER

#ifndef ES_VALUES_AS_NANS
    inline ES_Value_Internal& operator=(const ES_Value_Internal& rhs)
    {
        this->private_type = rhs.private_type;
        this->private_value = rhs.private_value;
        return *this;
    }
#endif // ES_VALUES_AS_NANS

    static inline void Memcpy(ES_Value_Internal &dst, char *src, ES_StorageType type);
    static inline void Memcpy(char *dst, const ES_Value_Internal &src, ES_StorageType type);

    static inline unsigned ConvertToValueTypeBits(ES_StorageType type);

    static inline ES_ValueType ConvertToValueType(ES_StorageType type);
    static inline ES_StorageType ConvertToStorageType(ES_ValueType type);

    static inline ES_ValueType ValueTypeFromCachedTypeBits(unsigned type);
    static inline ES_StorageType StorageTypeFromCachedTypeBits(unsigned type);
    static inline BOOL IsNullableFromCachedTypeBits(unsigned type);
    static inline unsigned SizeFromCachedTypeBits(unsigned type);
    static inline ES_StorageTypeBits StorageTypeBits(ES_StorageType type);
    static inline ES_StorageType StorageTypeFromBits(ES_StorageTypeBits bits, BOOL is_null);

    static inline unsigned CachedTypeBits(ES_StorageType type);

    inline ES_StorageType GetStorageType() const;
    inline BOOL CheckType(ES_StorageType type) const;

    static BOOL IsSameValue(ES_Context *context, const ES_Value_Internal &first, const ES_Value_Internal &second);
    /**< Implements "9.12 The Same Value Algorithm". */

private:
    BOOL ToNumberSlow(ES_Execution_Context *context);
    BOOL ToStringSlow(ES_Execution_Context *context);

    BOOL ToPrimitiveInternal(ES_Execution_Context *context, HintType hint);

    void SetInt32(unsigned number);
    /**< Private since the caller should probably call SetUInt32() instead.  If
         the value is known to be in int32 range, add an explicit cast. */
};

#endif // ES_VALUE_H
