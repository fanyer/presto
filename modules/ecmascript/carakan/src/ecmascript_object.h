/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2011
 *
 * class EcmaScript_Object
 */

#ifndef ECMASCRIPT_OBJECT_H
#define ECMASCRIPT_OBJECT_H

class ES_Runtime;
class ES_PropertyEnumerator;

/* Function parameter conversion strings
   =====================================

   The 'parameter_types' argument to SetFunctionRuntime() is a specially
   formatted string controlling how the engine converts function arguments
   before calling a host function's Call() method.

   If NULL, no argument conversion takes place.

   If non-NULL, there is one conversion specifier per argument.  If there are
   fewer arguments than conversion specifiers, excess conversion specifiers are
   ignored.  If there are more arguments than there are conversion specifiers,
   the last conversion specifier is used for all extra arguments.

   The following conversion specifiers are supported:

     '-': no conversion
     'b': ToBoolean() => VALUE_BOOLEAN
     'n': ToNumber() => VALUE_NUMBER
     's': ToString() => VALUE_STRING
     'S': null => VALUE_NULL,
          undefined => VALUE_UNDEFINED, else
          ToString() => VALUE_STRING
     'z': ToString() => VALUE_STRING_WITH_LENGTH
     'Z': null => VALUE_NULL,
          undefined => VALUE_UNDEFINED, else
          ToString() => VALUE_STRING_WITH_LENGTH
     'p':  ToPrimitive() => any type except VALUE_OBJECT
     'Ps': ToPrimitive(hint=String) => any type except VALUE_OBJECT
     'Pn': ToPrimitive(hint=Number) => any type except VALUE_OBJECT
     '{...}':  object => dictionary type,
               * => TypeError
     '?{...}': null => VALUE_NULL,
               undefined => VALUE_UNDEFINED,
               object => dictionary type,
               * => TypeError
     '[...]':  object => array type,
               * => TypeError
     '?[...]': null => VALUE_NULL,
               undefined => VALUE_UNDEFINED,
               object => array type,
               * => TypeError
     '(...)':  multiple conversion alternatives, see below

   Note that that there is no conversion specifier for conversion to object.

   Object Conversions
   ==================

   The object conversions are '{...}' for dictionary type objects and '[...]'
   for array type objects.  They perform similar conversions, and has some
   behavior in common.

   Both object conversions can be preceded by a question mark ('?') to signal
   that null or undefined are acceptable values in addition to object values.
   Values of boolean, number or string type are never acceptable.  When an
   unacceptable value is encountered, the conversion throws a TypeError
   exception, and the host function is not called.

   Both object conversions produce an object value as a result, unless null or
   undefined was acceptable and supplied by the caller.  The object produced is
   guaranteed to meet certain requirements, detailed below.  It will be a
   partial clone of the actual argument object if the actual argument object did
   not meet the requirements, but may be so even if the actual argument object
   did meet the requirements.  The host function should in no way depend on
   whether the argument object was cloned.

   Dictionary types
   ----------------

   See Web IDL:

     http://dev.w3.org/2006/webapi/WebIDL/#idl-dictionary
     http://dev.w3.org/2006/webapi/WebIDL/#es-dictionary

   A dictionary type conversion specifier is on the format

     {name1:conv1,name2:conv2,...}

   or

     ?{name1:conv1,name2:conv2,...}

   where 'nameN' is the dictionary member name and 'convN' is one of the
   conversion specifiers listed above, except a dictionary type.  (Nested
   dictionary types are not supported.)  Note that the conversion specifiers 'z'
   and 'Z' are not meaningful here since the converted value is never actually
   stored in an ES_Value at this point.

   The resulting value is of type VALUE_OBJECT and is an object from which every
   listed member can be read reliably using ES_Runtime::GetName() and has a
   value of the result type of the member's conversion specifier.  If property
   accesses or type conversions have observable side-effects, those side-effects
   are guaranteed to have happened already when the host function is called, and
   guaranteed to not happen again when ES_Runtime::GetName() is used to read the
   property values.

   Array types
   -----------

   See:
     http://dev.w3.org/2006/webapi/WebIDL/#es-sequence
     http://dev.w3.org/2006/webapi/WebIDL/#es-array

   An array type conversion specifier is on the format

     [conv]

   or

     ?[conv]

   where 'conv' is one of the conversion specifiers listed above, except an
   array type.  (Nested array types are not supported.)  Note that the
   conversion specifiers 'z' and 'Z' are not meaningful here since the converted
   value is never actually stored in an ES_Value at this point.

   The resulting value is of type VALUE_OBJECT and is an object from which the
   named property "length" can be read reliably using ES_Runtime::GetName() and,
   if available, has a value that is a number that is an integer in the range
   [0, UINT_MAX]. If the property is not available, then a value of zero should
   be assumed.

   Futhermore, for every index in the range [0, length), ES_Runtime::GetIndex()
   can reliably be used to read that element of the array, and if it exists, the
   element's value will be of the result type of the nested conversion
   specifier.

   Alternatives
   ============

   A conversion specifier on the format

     (x|y|z)

   signals that multiple types of values are possible.  There must be at least
   two.  Actual conversion will only be performed for the last specifier among
   the alternatives.

   For each conversion specifier in the list, other than the last, the supplied
   argument value is checked if it is already of the expected type.  If so, the
   argument is left unchanged, and the rest of the conversion specifiers in the
   list are ignored.

   If none of the non-last conversion specifiers in the list matched the
   supplied argument value, the last conversion specifier is used as if it was
   specified on its own.

   The last conversion specifier can be any kind of conversion specifier.  The
   non-last conversion specifiers are limited to the set 'b', 'n', 's', 'z' and
   the special conversion specifier '#Class' which matches an object whose
   [[Class]] attribute matches the name in the conversion specifier.  A class
   attribute conversion specifier can only be used in an alternatives conversion
   specifier, and can't be the last conversion specifier in the list. */

/** @short Base class for host objects. */
class EcmaScript_Object
{
friend class ES_Runtime;
friend OP_STATUS InitObject(EcmaScript_Object* object);
friend void Destruct(EcmaScript_Object* object);

private:
    ES_Object *native_object;
    /**< The native object that shadows this object, or NULL. */

    ES_Runtime *runtime;
    /**< The ECMAScript runtime associated with this object. */

public:
    EcmaScript_Object()
        : native_object(NULL),
          runtime(NULL)
    {
    }

    operator ES_Object *() { return native_object; }
    /**< Return the native object associated with this host object (may be NULL). */

    ES_Object* GetNativeObject() const { return native_object; }
    /**< Return the native object associated with this host object (may be NULL). */

    void SetNativeObject(ES_Object* object) { native_object = object; }
    /**< Set the native object associated with this host object. */

    ES_Runtime* GetRuntime() const { return runtime; }
    /**< Return a pointer to the ES_Runtime associated with this EcmaScript_Object. */

    virtual BOOL IsA(int tag);
    /**< @return TRUE iff this object is of a class that is characterized by the
         value of 'tag'.  Tags for object types are derived from the enumerated
         type EcmaScript_HostObjectType. */

    void ConnectObject(ES_Runtime* runtime, ES_Object* native_object);

    OP_STATUS SetObjectRuntime(ES_Runtime* es_runtime, ES_Object* prototype, const char* object_class, BOOL phase2=FALSE);
    /**< Initialize this object by giving it a shadow native object, if it does not
         have one.

         @param es_runtime (in) The runtime to which this object is tied: the runtime will
                not be deleted until this object has been deleted also.
         @param prototype (in) If this object already has a native shadow object, then this
                parameter value replaces the prototype object of that shadow object.
                Otherwise, this value is used as the prototype of the new shadow object.
         @param object_class (in) If not NULL, use this string for the class name of the object.
                The string must be a constant, the engine will not copy it.
         @param phase2 (in) If TRUE, then this object will be deleted during the second sweep
                phase of garbage collection.  (Don't use this argument if you don't know
                what that means.) */

    OP_STATUS SetFunctionRuntime(ES_Runtime* es_runtime, ES_Object* prototype, const char* object_class, const char* parameter_types);
    /**< Initialize this object as a host function object: make this object reference the
         runtime and give it a native function object that will forward any calls to the
         call-out interface of this object.

         @param es_runtime (in) the runtime object to attach this object to
         @param prototype (in) the prototype object to use for the native function
         @param object_class (in) the class name for the function
         @param parameter_types (in) a specification (see comment block at the top of this
                file) for parameter type conversion on call */

    OP_STATUS SetFunctionRuntime(ES_Runtime* es_runtime, const uni_char* function_name, const char* object_class, const char* parameter_types);
    /**< As SetFunctionRuntime with the other signature, see above.

         @param es_runtime (in) the runtime object to attach this object to
         @param function_name (in) the name with which this function will identify itself
         @param object_class (in) the class name for the function
         @param parameter_types (in) a specification (see comment block at the top of this
                file) for parameter type conversion on call */

    OP_STATUS SetFunctionRuntime(ES_Runtime* es_runtime, ES_Object *prototype, const uni_char* function_name, const char* object_class, const char* parameter_types);
    /**< As SetFunctionRuntime with the other signatures, see above. */

    void SetFunctionRuntimeL(ES_Runtime* es_runtime, const uni_char* function_name, const char* object_class, const char* parameter_types);
    /**< As SetFunctionRuntime with the same parameters, but leaves on OOM */

    void ChangeRuntime(ES_Runtime* es_runtime);
    /**< Change the runtime pointer.  Can only be called on objects previously initialized
         through a call to SetObjectRuntime or SetFunctionRuntime.

         @param es_runtime (in) the new runtime object to attach this object to. */

    void SetHasMultipleIdentities();
    /**< Call to indicate that this object has an implementation of the
         Identity() callback that can return another object; that is, that
         this object ever acts as a proxy object for other objects. */

    void SignalIdentityChange(ES_Object *previous_identity);
    /**< Signal that the identity of a "multiple identity" object has
         changed. */

    void SignalDisableCaches(ES_Object *current_identity);
    /**< Signal that caches over this "multiple identity" object should be
         disabled.  The object should be removed from the invalidation chain
         of its current identity (if registered) and never be added again. */

    void SetHasVolatilePropertySet();
    /**< Call to mark this object as having a volatile set of properties.
         Meaning that the object may add new properties (or remove existing),
         but without being able to detect when this happens. If it can
         detect when this happens, it does not need to mark itself as
         being volatile but call SignalPropertySetChanged() instead. */

    BOOL HasVolatilePropertySet();
    /**< Returns TRUE if this is a host object with a volatile property
         set. See SetHasVolatilePropertySet(). */

    void SignalPropertySetChanged();
    /**< Notify that a new host object property has been added or deleted.
         Must be called whenever the property set changes, but isn't neeeded
         when changing an existing property. */

    /* Call-in protocol */

    OP_STATUS Put(const uni_char* property_name, ES_Object* object, BOOL read_only = FALSE);
    void PutL(const uni_char* property_name, ES_Object* object, BOOL read_only = FALSE);
    OP_STATUS Put(const uni_char* property_name, const ES_Value &value, BOOL read_only = FALSE);
    OP_BOOLEAN Get(const uni_char* property_name, ES_Value* value);
    OP_STATUS Delete(const uni_char* property_name);
    OP_BOOLEAN GetPrivate(int private_name, ES_Value* value);
    OP_STATUS PutPrivate(int private_name, ES_Object* object);
    OP_STATUS PutPrivate(int private_name, ES_Value& object);
    OP_STATUS DeletePrivate(int private_name);

    /* Internal call-out protocol */

    ES_GetState HasPropertyIndex(unsigned property_index, BOOL is_restarted, ES_Runtime* origining_runtime);
    /**< Perform [[HasProperty]] with an indexed property on a host object.

         The operation proceeds by invoking GetIndex() with 'value' as NULL to
         determine if the index is supported and accessible on the host object.
         If that operation cannot be completed synchronously, the later restarted
         attempt will use GetIndexRestart() instead.

         @param property_index The index of the property.
         @param is_restarted The flag is set to TRUE if this is a restarted
                attempt to determine status for a property.
         @param origining_runtime the caller's runtime performing the operation.
         @return One of the following:

            GET_SUCCESS The indexed property is present and accessible.

            GET_FAILED The property is not present.

            GET_SECURITY_VIOLATION The property is not accessible.

            GET_SUSPEND The status of the indexed property could not be
                        determined synchronously, but will require
                        suspension and subsequent restart.

            GET_NO_MEMORY The operation signalled OOM. */

    ES_GetState HasPropertyName(const uni_char* property_name, int property_code, BOOL is_restarted, ES_Runtime* origining_runtime);
    /**< Perform [[HasProperty]] with a named property on a host object.

         The operation proceeds by invoking GetName() with 'value' as NULL
         to determine if the named property is supported and accessible
         on the host object. If that operation cannot be completed
         synchronously, the later restarted attempt will use GetNameRestart()
         instead.

         @param property_name The named property to query for.
         @param property_code Its corresponding numeric code, if assigned.
         @param is_restarted The flag is set to TRUE if this is a restarted
                attempt to determine status for a property.
         @param origining_runtime the caller's runtime performing the operation.
         @return One of the following:

            GET_SUCCESS The named property is present and accessible.

            GET_FAILED The property is not present.

            GET_SECURITY_VIOLATION The property is not accessible.

            GET_SUSPEND The [[HasProperty]] operation could not be completed
                        synchronously, but will require suspension and
                        subsequent restart.

            GET_NO_MEMORY The operation signalled OOM. */

    /* Public call-out protocol. */

    virtual ~EcmaScript_Object();

    virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_GetState GetIndexRestart(int property_index, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

    virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_GetState GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

    virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_PutState PutIndexRestart(int property_index, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

    virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_PutState PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

    virtual ES_DeleteStatus DeleteIndex(int property_index, ES_Runtime* origining_runtime);
    /**< Perform an indexed delete operation on the host object. The implementation
         should follow the WebIDL behaviour for [[Delete]] on platform objects; specifically
         that it uses DELETE_REJECT to signal that the indexed delete is not permitted.
         For everything else (except OOM conditions), the host object should return DELETE_OK.

         @param property_index index of property to delete.
         @param origining_runtime the caller's runtime performing the operation.
         @return DELETE_REJECT if the object did not permit the indexed property
                 to be deleted; DELETE_OK otherwise. OOM signalled using DELETE_NO_MEMORY. */

    virtual ES_DeleteStatus DeleteName(const uni_char* property_name, ES_Runtime* origining_runtime);
    /**< Perform a named property delete operation on the host object. The implementation
         should follow the WebIDL behaviour for [[Delete]] on platform objects; specifically
         that it uses DELETE_REJECT to signal that the named property could not be deleted.
         For everything else (except OOM conditions), the host object should return DELETE_OK.

         @param property_name name of property to delete.
         @param origining_runtime the caller's runtime performing the operation.
         @return DELETE_REJECT if the object did not permit the named property
                 to be deleted; DELETE_OK otherwise. OOM signalled using DELETE_NO_MEMORY. */

    enum PropertyOperation
    {
        ALLOW_ACCESSORS,
        /**< Does the host object allow definition of accessor properties (getter/setters)
             for a host property? */

        ALLOW_NATIVE_OVERRIDE,
        /**< Does the host object allow a property to be overridden by a native property,
             including adding accessor methods? */

        ALLOW_NATIVE_DECLARED_OVERRIDE
        /**< Does the host object allow a property to be overridden if declared? (using
             variable or function declarations.) A host object may allow this, but not
             the more general ALLOW_NATIVE_OVERRIDE for certain properties. From the
             host object's side, a request for this property operation serves the dual
             purpose of allowing it (the host object) to distinguish between host
             properties attempted declared as overridden and run-time attempts to update
             a property's value (using Object.defineProperty().) It can disallow
             both, but take the use of ALLOW_NATIVE_DECLARED_OVERRIDE into account
             when deciding if a [[Put]] should have an effect or not. */
    };

    virtual BOOL AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op);
    /**< @return TRUE iff the host object allows given property operation 'op'; encompasses definition of
         getter/setter for the specified property and permitting overriding via own native property.
         The default EcmaScript_Object implementation always returns TRUE. */

#ifdef _DEBUG
    // Make subclasses trying to override the old API fail to compile rather than cause runtime errors.
    // IF YOU GET AN ERROR RELATED TO THIS YOU ARE OVERRIDING THE WRONG Construct FUNCTION
    virtual void Construct(ES_Object* removed_this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime) { OP_ASSERT(!"DO NOT IMPLEMENT THIS ONE"); }
#endif // _DEBUG

    virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
    virtual int Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

    virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
    /**< Generate the set of enumerable properties that this object
         supports. This includes both named and indexed properties.

         The properties are communicated back to the engine through
         the ES_PropertyEnumerator argument.

         @param enumerator The object for registering properties.
         @return One of the following:

            GET_SUCCESS The operation completed successfully.
            GET_SUSPEND The operation had to suspend and must be restarted.
            GET_NO_MEMORY OOM condition encountered.

            No other values of ES_GetState must be returned by an implementation
            of this method.

            As the ES_PropertyEnumerator methods will leave upon encountering
            OOM, an implementation must expect this and handle the condition
            correctly. An implementation is currently allowed to report
            OOM either by leaving or a GET_NO_MEMORY return	value. */

    virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);
    /**< Returns the number of indexed properties supported by this
         host object.

         If non-zero, the length is assumed to be one larger than the
         maximum index.

         @param [out]count The resulting count, 0 if the object doesn't
                support indexed properties.
         @param origining_runtime The runtime of the caller.
         @return One of the following:

           GET_SUCCESS Length successfully computed.
           GET_NO_MEMORY OOM condition encountered.
           GET_SUSPEND The operation had to suspend and must be restarted.

           No other ES_GetState values are recognized by the engine. */

    virtual OP_STATUS Identity(ES_Object** loc);
    /**< If the native object of this object is a proxy for another object, then return
         the object it is the proxy for in *loc.  Otherwise return the native object of
         this object. */

    virtual void GCTrace();
    /**< For each ES_Object reachable from this object that is not reachable from
         data structures inside the engine and is not protected, call ES_Runtime::GCMark.

         This method MUST NOT make changes to data structures inside the engine. */

    virtual BOOL SecurityCheck(ES_Runtime* origining_runtime);
    /**< @return TRUE if code running in the domain of origining_runtime is allowed to
         access this object, otherwise FALSE.  The default check is that
         origining_runtime must be the same object as the runtime of this object. */

    virtual BOOL TypeofYieldsObject();
    /**< @return TRUE if this object should answer "object" in response to "typeof".
         Will only be called on objects that also represent functions.  */
};

#endif // ECMASCRIPT_OBJECT_H
