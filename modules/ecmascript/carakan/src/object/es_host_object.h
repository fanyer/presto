/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_HOST_OBJECT_H
#define ES_HOST_OBJECT_H

#include "modules/ecmascript/carakan/src/object/es_function.h"

class ES_Host_Object : public ES_Object
{
public:
    typedef BOOL (HostFunction)(ES_Object *, ES_Value *, int, ES_Value *, ES_Runtime *);

    class HostFunctionArgs
    {
    public:
        HostFunctionArgs() {}
        HostFunctionArgs(ES_Object* this_native_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
            : this_native_object(this_native_object), argv(argv), argc(argc), return_value(return_value), origining_runtime(origining_runtime) {}

        ES_Object* this_native_object;
        ES_Value* argv;
        int argc;
        ES_Value* return_value;
        ES_Runtime* origining_runtime;
    };

    static ES_Host_Object *Make(ES_Context *context, EcmaScript_Object *host_object, ES_Object *prototype, const char *object_class);
    static ES_Host_Object *Make(ES_Context *context, void *&payload, unsigned payload_size, ES_Object *prototype, const char *object_class, BOOL need_destroy, BOOL is_singleton);

    static void Initialize(ES_Host_Object *object, BOOL need_destroy, ES_Class *klass = NULL);
    static void Destroy(ES_Host_Object *object);

    DeleteResult DeleteHostL(ES_Execution_Context *context, JString *name, unsigned &result);
    /**< Remove name from the object. If deleting name is a violation to security, DeleteHostL
         returns FALSE, TRUE otherwise. If there is no property name, or the property is read
         only, then FALSE is returned through result, TRUE otherwise.
    */
    DeleteResult DeleteHostL(ES_Execution_Context *context, unsigned index, unsigned &result);
    GetResult GetHostL(ES_Execution_Context *context, ES_Object *this_object, JString *name, ES_Value_Internal &value, ES_Object *&prototype_object, ES_PropertyIndex &index);
    /**< Get the specified property. Handles security violations and will calls ThrowReferenceError
         when they occur. Will suspend if the EcmaScript_Object requires it and restart
         when resumed. GetHostL takes a this_object parameter for when ES_Object::GetL calls GetHostL
         on a prototype.
    */
    GetResult GetHostL(ES_Execution_Context *context, ES_Object *this_object, unsigned index, ES_Value_Internal &value);
    PutResult PutHostL(ES_Execution_Context *context, JString *name, const ES_Value_Internal &value, BOOL in_class, ES_PropertyIndex &index);
    /**< Put the specified property. Handles security violations and will calls ThrowReferenceError
         when they occur. Will suspend if the EcmaScript_Object requires it and restart
         when resumed. Will handle handle conversions of the written value if the EcmaScript_Object
         requires it.
    */
    PutResult PutHostL(ES_Execution_Context *context, unsigned index, unsigned *attributes, const ES_Value_Internal &value);

    BOOL HasPropertyHost(ES_Execution_Context *context, JString *name);

    BOOL HasPropertyHost(ES_Execution_Context *context, unsigned index);

    void SetHostObject(EcmaScript_Object* object) { host_object = object; }

    void DecoupleHostObject() { host_object = NULL; }
    /**< Decouple this object from its host object */

    BOOL MustTraceForeignObject() const { return (hdr.header & ES_Header::MASK_NO_FOREIGN_TRACE) == 0; }
    /**< Returns TRUE if the foreign object's GCTrace function should be called during current GC */

    void SetMustTraceForeignObject() { hdr.header &= ~ES_Header::MASK_NO_FOREIGN_TRACE; }
    /**< Mark this object's host object to be traced during GC */

    void ClearMustTraceForeignObject() { hdr.header |= ES_Header::MASK_NO_FOREIGN_TRACE; }
    /**< Mark this object's host object not to be traced during GC */

    EcmaScript_Object *GetHostObject() const { return host_object; }

    static ES_Object *Identity(ES_Context *context, ES_Host_Object *target);

    static ES_Object *IdentityStandardStack(ES_Host_Object *target);

    BOOL HasOwnHostProperty(ES_Context *context, JString *name, ES_Property_Info &info, BOOL &is_secure);
    /**< Check if the host object instance has a property. Will return property attributes through info.
         Uses is_secure as described by ES_Object::HasPropertyWithInfo. HasOwnHostProperty doesn't
         set exceptions to throw if security checks doens't succeed.
    */

    BOOL HasOwnHostProperty(ES_Context *context, unsigned index, ES_Property_Info &info, BOOL &is_secure);

    void FetchProperties(ES_Context *context, ES_PropertyEnumerator *enumerator);

    unsigned GetIndexedPropertiesLength(ES_Context *context);

    BOOL AllowOperationOnProperty(ES_Execution_Context *context, JString *name, EcmaScript_Object::PropertyOperation op);

    inline BOOL AllowGetterSetterFor(ES_Execution_Context *context, JString *name) { return AllowOperationOnProperty(context, name, EcmaScript_Object::ALLOW_ACCESSORS); }

    BOOL GetOwnHostPropertyL(ES_Execution_Context *context, PropertyDescriptor &desc, JString *name, unsigned index, BOOL fetch_value, BOOL fetch_accessors);

    BOOL SecurityCheck(ES_Context *context);

    inline ES_Runtime *GetRuntime() const { return host_object->GetRuntime(); }

    void SignalIdentityChange(ES_Object *previous_identity);
    void SignalDisableCaches(ES_Object *current_identity);

protected:

    static BOOL ES_CALLING_CONVENTION HostGetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION HostSetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    enum FailedReason
    {
        NoReason,
        NotFound,
        Exception,
        SecurityViolation,
        ReadOnly,
        NeedsBoolean,
        NeedsNumber,
        NeedsString,
        NeedsStringWithLength
    };

    static BOOL ConvertL(ES_Execution_Context *context, ES_PutState reason, const ES_Value_Internal &from, ES_Value_Internal &to);
    /**< Convert a value as specified by 'reason', putting the result in 'to'.
         The result of converting null to string is "".
    */
    static BOOL ConvertL(ES_Execution_Context *context, ES_ArgumentConversion reason, const ES_Value_Internal &from, ES_Value_Internal &to);
    /**< Convert a value as specified by 'reason', putting the result in 'to'.
         The result of converting null to string is "null".
    */
    static BOOL FinishPut(ES_Execution_Context *context, ES_PutState state, FailedReason &reason, BOOL &can_cache, const ES_Value &external_value);
    /**< Map ES_PutState to FailedReason to handle conversion and setup the appropriate exceptions if applicaple.
    */
    static GetResult FinishGet(ES_Execution_Context *context, ES_GetState state, const ES_Value &external_value);
    /**< Setup the appropriate exceptions if applicaple.
    */
    static void SuspendL(ES_Execution_Context *context, ES_Value_Internal *restart_object, const ES_Value &external_value);
    /**< Setup a restart object and yield.
    */

    GetResult GetOwnHostPropertyL(ES_Execution_Context *context, ES_Object *this_object, JString *name, ES_Value_Internal &value, ES_PropertyIndex &index);
    GetResult GetOwnHostPropertyL(ES_Execution_Context *context, ES_Object *this_object, unsigned index, ES_Value_Internal &value);

    BOOL PutInHostL(ES_Execution_Context *context, JString *name, const ES_Value_Internal &value, FailedReason &reason, BOOL &can_cache, ES_PropertyIndex &index);
    BOOL PutInHostL(ES_Execution_Context *context, unsigned index, const ES_Value_Internal &value, FailedReason &reason);

    GetResult GetInOwnHostPropertyL(ES_Execution_Context *context, JString *name, ES_Value_Internal &value);

    EcmaScript_Object *host_object;
};

class ES_Host_Function : public ES_Host_Object
{
public:
    static ES_Host_Function *Make(ES_Context *context, ES_Global_Object *global_object, EcmaScript_Object* host_object, ES_Object* prototype, const uni_char *function_name, const char* object_class, const char *parameter_types);
    static ES_Host_Function *Make(ES_Context *context, ES_Global_Object *global_object, void *&payload, unsigned payload_size, ES_Object* prototype, const uni_char *function_name, const char* object_class, const char *parameter_types, BOOL need_destroy, BOOL is_singleton);

    static void Initialize(ES_Host_Function *object, BOOL need_destroy, ES_Class *klass, const char *parameter_types);

    static BOOL ES_CALLING_CONVENTION Call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< Invoke Call in the host function object. The host function object is implicitly passed in argv[-1] making Call look and behave as a BuiltIn.
         Call will suspend if EcmaScript_Object::Call requires it to.
    */
    static BOOL ES_CALLING_CONVENTION Construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< Invoke Construct in the host function object. The host function object is implicitly passed in argv[-1] making Construct look and behave as a BuiltIn.
         Construct will suspend if EcmaScript_Object::Construct requires it to.
    */

    JString *GetHostFunctionName() { return function_name; }

    unsigned GetParameterTypesLength() { return parameter_types_length; }

protected:
    friend class ESMM;

    static BOOL FormatDictionary(ES_Execution_Context *context, const char *&format, ES_Value_Internal &source);
    static BOOL FormatArray(ES_Execution_Context *context, const char *&format, ES_Value_Internal &source);
    static BOOL FormatAlternatives(ES_Execution_Context *context, const char *&format, ES_Value_Internal &source, char &export_conversion);

    static BOOL FormatValue(ES_Execution_Context *context, const char *&format, ES_Value_Internal &source, ES_Value *destination = NULL, ES_ValueString *destination_string = NULL);

    static BOOL FormatArguments(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value *out_argv, ES_ValueString *out_argv_strings, const char *format);
    /**< @return FALSE if thrown exectption. */

    static BOOL FinishL(ES_Execution_Context *context, unsigned result, const ES_Value &external_value, ES_Value_Internal *return_value, BOOL constructing);
    JString *function_name;
    const char *parameter_types;
    unsigned parameter_types_length;
};

#endif // ES_HOST_OBJECT_H
