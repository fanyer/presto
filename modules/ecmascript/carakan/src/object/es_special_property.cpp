/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/object/es_special_property.h"
#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/ecmascript/carakan/src/object/es_arguments.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/ecmascript/carakan/src/vm/es_execution_context.h"

PutResult ES_Special_Property::SpecialPutL(ES_Context *context, ES_Object *this_object, const ES_Value_Internal &value, const ES_Value_Internal &this_value)
{
    switch (GCTag())
    {
    case GCTAG_ES_Special_Global_Variable:
    {
        OP_ASSERT(this_object->IsGlobalObject());
        ES_Global_Object *global_object = static_cast<ES_Global_Object *>(this_object);
        unsigned index = static_cast<ES_Special_Global_Variable *>(this)->index;
#ifdef ES_NATIVE_SUPPORT
        global_object->InvalidateInlineFunction(context, index);
#endif // ES_NATIVE_SUPPORT
        global_object->GetVariableValue(index) = value;
        return PROP_PUT_OK;
    }

    case GCTAG_ES_Special_Aliased:
        *static_cast<ES_Special_Aliased *>(this)->property = value;
        return PROP_PUT_OK;

    case GCTAG_ES_Special_Mutable_Access:
    {
        if (!context)
            return PROP_PUT_FAILED;

        ES_Execution_Context *exec_context = context->GetExecutionContext();
        if (!exec_context)
            return PROP_PUT_FAILED;

        ES_Object *setter = static_cast<ES_Special_Mutable_Access *>(this)->setter;

        if (setter == NULL)
            return PROP_PUT_READ_ONLY;

        /* Save scratch registers since we will now execute more code. */
        ES_Value_Internal *register_storage = exec_context->SaveScratchRegisters();

        ES_Value_Internal *registers = exec_context->SetupFunctionCall(setter, /* argc = */ 1);

        registers[0] = this_value;
        registers[1].SetObject(setter);
        registers[2] = value;

        if (!exec_context->CallFunction(registers, /* argc = */ 1))
        {
            exec_context->RestoreScratchRegisters(register_storage);
            return PROP_PUT_FAILED;
        }
        exec_context->RestoreScratchRegisters(register_storage);

        return PROP_PUT_OK;
    }

    case GCTAG_ES_Special_Function_Caller:
        return PROP_PUT_READ_ONLY;

    case GCTAG_ES_Special_Function_Length:
        return PROP_PUT_READ_ONLY;

    case GCTAG_ES_Special_Function_Prototype:
    {
        OP_ASSERT(this_object->IsFunctionObject());

        ES_Function *function = static_cast<ES_Function *>(this_object);
        ES_Box *properties = function->GetProperties();

        ES_Property_Info info(DE|DD);
        ES_Layout_Info old_layout = function->Class()->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::PROTOTYPE));
        ES_Class *old_klass = ES_Class::ActualClass(function->Class());
        BOOL needs_conversion = FALSE;

        ES_Class *new_klass = ES_Class::ChangeAttribute(context, function->Class(), context->rt_data->idents[ESID_prototype], info, function->Count(), needs_conversion);

        ES_CollectorLock gclock(context);

        if (function->GetGlobalObject()->IsDefaultFunctionProperties(properties))
        {
            properties = ES_Box::Make(context, new_klass->SizeOf(function->Count()));

            op_memcpy(properties->Unbox(), function->GetProperties()->Unbox(), function->GetProperties()->Size());

            this_object->SetProperties(properties);
        }

        if (needs_conversion)
            function->ConvertObject(context, old_klass, new_klass);
        else
        {
            ES_Property_Info prototype_info = old_klass->GetPropertyInfoAtIndex(ES_PropertyIndex(ES_Function::PROTOTYPE));
            int diff = new_klass->GetLayoutInfoAtIndex(prototype_info.Index()).Diff(old_layout);
            function->ConvertObjectSimple(context, new_klass, ES_LayoutIndex(prototype_info.Index() + 1), diff, old_klass->SizeOf(function->Count()), function->Count());
        }

        this_object->PutCachedAtIndex(ES_PropertyIndex(ES_Function::PROTOTYPE), value);

        OP_ASSERT_PROPERTY_COUNT(this_object);

        return PROP_PUT_OK;
    }

    case GCTAG_ES_Special_Error_StackTrace:
    {
        OP_ASSERT(this_object->IsErrorObject());

        ES_Error::StackTraceFormat format = static_cast<ES_Special_Error_StackTrace *>(this)->format;

        ES_Error::ErrorSpecialProperties property;
        JString *name = NULL;

        switch (format)
        {
        default:
            OP_ASSERT(!"I don't know nothing about that format.");
            /* fall through */
        case ES_Error::FORMAT_READABLE:
            property = ES_Error::PROP_stacktrace;
            name = context->rt_data->idents[ESID_stacktrace];
            break;
        case ES_Error::FORMAT_MOZILLA:
            property = ES_Error::PROP_stack;
            name = context->rt_data->idents[ESID_stack];
            break;
        }

        BOOL needs_conversion = FALSE;
        ES_Class *old_klass = ES_Class::ActualClass(this_object->Class());

        ES_Class *new_klass = ES_Class::ChangeAttribute(context, this_object->Class(), context->rt_data->idents[ESID_stacktrace], ES_Property_Info(DE, ES_LayoutIndex(format)), this_object->Count(), needs_conversion);
        ES_CollectorLock gclock(context);
        new_klass = ES_Class::ChangeAttribute(context, this_object->Class(), name, ES_Property_Info(DE, ES_LayoutIndex(format)), this_object->Count(), needs_conversion);

        this_object->ConvertObject(context, old_klass, new_klass);

        return this_object->PutCachedAtIndex(ES_PropertyIndex(property), value);
    }

    default:
        return PROP_PUT_OK;

    case GCTAG_ES_Special_RegExp_Capture:
        ES_RegExp_Constructor *ctor = static_cast<ES_RegExp_Constructor *>(this_object);
        unsigned index = static_cast<ES_Special_RegExp_Capture *>(this)->index;

        if (index == ES_RegExp_Constructor::INDEX_INPUT)
            ctor->PutCachedAtIndex(ES_PropertyIndex(ES_RegExp_Constructor::INPUT), value);
        else if (index == ES_RegExp_Constructor::INDEX_MULTILINE)
            ctor->SetMultiline(value.AsBoolean().GetBoolean());

        return PROP_PUT_OK;
    }
}

GetResult ES_Special_Property::SpecialGetL(ES_Context *context, ES_Object *this_object, ES_Value_Internal &value, const ES_Value_Internal &this_value)
{
    switch (GCTag())
    {
    case GCTAG_ES_Special_Global_Variable:
    {
        OP_ASSERT(this_object->IsGlobalObject());
        ES_Global_Object *global_object = static_cast<ES_Global_Object *>(this_object);
        unsigned index = static_cast<ES_Special_Global_Variable *>(this)->index;
        value = global_object->GetVariableValue(index);
        return PROP_GET_OK;
    }

    case GCTAG_ES_Special_Aliased:
        value = *static_cast<ES_Special_Aliased *>(this)->property;
        return PROP_GET_OK;

    case GCTAG_ES_Special_Mutable_Access:
    {
        if (!context)
            return PROP_GET_FAILED;

        ES_Execution_Context *exec_context = context->GetExecutionContext();
        if (!exec_context || !exec_context->IsActive())
            return PROP_GET_FAILED;

        ES_Object *getter = static_cast<ES_Special_Mutable_Access *>(this)->getter;

        if (getter == NULL)
        {
            value.SetUndefined();
            return PROP_GET_OK;
        }

        ES_Value_Internal result;
        ES_Value_Internal *registers = exec_context->SetupFunctionCall(getter, /* argc = */ 0);

        registers[0] = this_value;
        registers[1].SetObject(getter);

        if (!exec_context->CallFunction(registers, /* argc = */ 0, &result))
            return PROP_GET_FAILED;

        value = result;
        return PROP_GET_OK;
    }

    case GCTAG_ES_Special_Function_Name:
    {
        OP_ASSERT(this_object->IsFunctionObject());

        JString *name = static_cast<ES_Function *>(this_object)->GetName(context);

        value.SetString(name);

        return PROP_GET_OK;
    }

    case GCTAG_ES_Special_Function_Length:
    {
        OP_ASSERT(this_object->IsFunctionObject());

        ES_Function *function = static_cast<ES_Function *>(this_object);

        value.SetUInt32(function->GetLength());

        return PROP_GET_OK;
    }

    case GCTAG_ES_Special_Function_Prototype:
    {
        OP_ASSERT(this_object->IsFunctionObject());

        ES_Function *function = static_cast<ES_Function *>(this_object);
        ES_FunctionCode *fncode = function->GetFunctionCode();

        if (fncode)
        {
            ES_Object *prototype = ES_Object::Make(context, fncode->global_object->GetFunctionPrototypeClass());
            ES_CollectorLock gclock(context);

            prototype->PutCachedAtIndex(ES_PropertyIndex(0), this_object);
            SpecialPutL(context, this_object, prototype, this_value);

            value.SetObject(prototype);
        }
        else
            value.SetUndefined();

        return PROP_GET_OK;
    }

    case GCTAG_ES_Special_Function_Arguments:
    {
        OP_ASSERT(this_object->IsFunctionObject());

        ES_Execution_Context *exec_context = context->GetExecutionContext();
        if (exec_context)
        {
            ES_Function *function = static_cast<ES_Function *>(this_object);
            ES_FunctionCode *fncode = function->GetFunctionCode();

            if (fncode)
            {
                ES_FrameStackIterator frames(exec_context);

                while (frames.Next())
                    if (frames.GetCode() == fncode)
                    {
#ifdef ES_NATIVE_SUPPORT
                        if (frames.IsFollowedByNative())
                        {
                            frames.Next();

                            OP_ASSERT(frames.GetCode() == fncode);
                        }
#endif // ES_NATIVE_SUPPORT

                        ES_Value_Internal *registers = frames.GetRegisterFrame();
                        ES_Arguments_Object *arguments = frames.GetArgumentsObject();

                        if (arguments ? arguments->GetFunction() == function : registers[1].GetObject() == function)
                        {
                            ES_FunctionCodeStatic *data = fncode->GetData();

                            if (data->arguments_index != ES_FunctionCodeStatic::ARGUMENTS_NOT_USED)
                            {
                                value = registers[data->arguments_index];
                                return PROP_GET_OK;
                            }

                            if (!arguments)
                            {
#ifdef ES_NATIVE_SUPPORT
                                function->SetHasCreatedArgumentsArray(context);
#endif // ES_NATIVE_SUPPORT

                                arguments = ES_Arguments_Object::Make(context, function, registers, frames.GetArgumentsCount());
                                frames.SetArgumentsObject(arguments);
                            }

                            arguments->SetUsed();

                            value.SetObject(arguments);
                            return PROP_GET_OK;
                        }
                    }
            }
        }

        value.SetNull();
        return PROP_GET_OK;
    }

    case GCTAG_ES_Special_Function_Caller:
    {
        OP_ASSERT(this_object->IsFunctionObject());

        ES_Execution_Context *exec_context = context->GetExecutionContext();
        if (exec_context)
        {
            ES_Function *function = static_cast<ES_Function *>(this_object);

            if (ES_FunctionCode *fncode = function->GetFunctionCode())
            {
                ES_FrameStackIterator frames(exec_context);

                while (frames.Next())
                    if (frames.GetCode() == fncode)
                    {
#ifdef ES_NATIVE_SUPPORT
                        if (frames.IsFollowedByNative())
                        {
                            frames.Next();

                            OP_ASSERT(frames.GetCode() == fncode);
                        }
#endif // ES_NATIVE_SUPPORT

                        ES_Arguments_Object *arguments = frames.GetArgumentsObject();
                        if (arguments ? arguments->GetFunction() == function : frames.GetRegisterFrame()[1].GetObject() == function)
                        {
                            while (frames.Next())
                                if (ES_Code *code = frames.GetCode())
                                    if (code->type == ES_Code::TYPE_FUNCTION && !code->is_strict_eval)
                                    {
                                        if (code->data->is_strict_mode)
                                        {
                                            exec_context->ThrowTypeError("Illegal property access");
                                            return PROP_GET_FAILED;
                                        }
                                        value.SetObject(frames.GetRegisterFrame()[1].GetObject());
                                        return PROP_GET_OK;
                                    }

                            break;
                        }
                    }
            }
        }

        value.SetNull();
        return PROP_GET_OK;
    }

    case GCTAG_ES_Special_RegExp_Capture:
    {
        ES_RegExp_Constructor *ctor = static_cast<ES_RegExp_Constructor *>(this_object);
        unsigned index = static_cast<ES_Special_RegExp_Capture *>(this)->index;

        switch (index)
        {
        case ES_RegExp_Constructor::INDEX_INPUT:
            ctor->GetCachedAtIndex(ES_PropertyIndex(ES_RegExp_Constructor::INPUT), value);
            break;

        case ES_RegExp_Constructor::INDEX_LASTMATCH:
            ctor->GetLastMatch(context, value);
            break;

        case ES_RegExp_Constructor::INDEX_LASTPAREN:
            ctor->GetLastParen(context, value);
            break;

        case ES_RegExp_Constructor::INDEX_LEFTCONTEXT:
            ctor->GetLeftContext(context, value);
            break;

        case ES_RegExp_Constructor::INDEX_RIGHTCONTEXT:
            ctor->GetRightContext(context, value);
            break;

        case ES_RegExp_Constructor::INDEX_MULTILINE:
            value.SetBoolean(ctor->GetMultiline());
            break;

        default:
            ctor->GetCapture(context, value, index);
        }

        return PROP_GET_OK;
    }

    case GCTAG_ES_Special_Error_StackTrace:
    {
        ES_Error *error = static_cast<ES_Error *>(this_object);

        if (ES_Execution_Context *exec_context = context->GetExecutionContext())
            value.SetString(error->GetStackTraceString(exec_context, NULL, static_cast<ES_Special_Error_StackTrace *>(this)->format, 0));
        else
            value.SetNull();

        return PROP_GET_OK;
    }

    default:
        OP_ASSERT(FALSE);

        value.SetUndefined();
        return PROP_GET_OK;
    }
}

BOOL
ES_Special_Property::WillCreatePropertyOnGet(ES_Object *this_object) const
{
    return GCTag() == GCTAG_ES_Special_Function_Prototype && static_cast<ES_Function *>(this_object)->GetFunctionCode() != NULL;
}

/* static */
ES_Special_Mutable_Access *ES_Special_Mutable_Access::Make(ES_Context *context, ES_Function *ma_getter, ES_Function *ma_setter)
{
    ES_Special_Mutable_Access *self;
    GC_ALLOCATE(context, self, ES_Special_Mutable_Access, (self, ma_getter, ma_setter));
    return self;
}

/* static */
void ES_Special_Mutable_Access::Initialize(ES_Special_Mutable_Access *self, ES_Function *ma_getter, ES_Function *ma_setter)
{
    self->InitGCTag(GCTAG_ES_Special_Mutable_Access);
    self->getter = ma_getter;
    self->setter = ma_setter;
}

/* static */
ES_Special_Aliased *ES_Special_Aliased::Make(ES_Context *context, ES_Value_Internal *alias)
{
    ES_Special_Aliased *self;
    GC_ALLOCATE(context, self, ES_Special_Aliased, (self, alias));
    return self;
}

/* static */
void ES_Special_Aliased::Initialize(ES_Special_Aliased *self, ES_Value_Internal *alias)
{
    self->InitGCTag(GCTAG_ES_Special_Aliased);
    self->property = alias;
}

/* static */
ES_Special_Global_Variable *ES_Special_Global_Variable::Make(ES_Context *context, unsigned index)
{
    ES_Special_Global_Variable *self;
    GC_ALLOCATE(context, self, ES_Special_Global_Variable, (self, index));
    return self;
}

/* static */
void ES_Special_Global_Variable::Initialize(ES_Special_Global_Variable *self, unsigned index)
{
    self->InitGCTag(GCTAG_ES_Special_Global_Variable);
    self->index = index;
}

/* static */
ES_Special_Function_Name *ES_Special_Function_Name::Make(ES_Context *context)
{
    ES_Special_Function_Name *self;
    GC_ALLOCATE(context, self, ES_Special_Function_Name, (self));
    return self;
}

/* static */
void ES_Special_Function_Name::Initialize(ES_Special_Function_Name *self)
{
    self->InitGCTag(GCTAG_ES_Special_Function_Name);
}

/* static */
ES_Special_Function_Length *ES_Special_Function_Length::Make(ES_Context *context)
{
    ES_Special_Function_Length *self;
    GC_ALLOCATE(context, self, ES_Special_Function_Length, (self));
    return self;
}

/* static */
void ES_Special_Function_Length::Initialize(ES_Special_Function_Length *self)
{
    self->InitGCTag(GCTAG_ES_Special_Function_Length);
}

/* static */
ES_Special_Function_Prototype *ES_Special_Function_Prototype::Make(ES_Context *context)
{
    ES_Special_Function_Prototype *self;
    GC_ALLOCATE(context, self, ES_Special_Function_Prototype, (self));
    return self;
}

/* static */
void ES_Special_Function_Prototype::Initialize(ES_Special_Function_Prototype *self)
{
    self->InitGCTag(GCTAG_ES_Special_Function_Prototype);
}

/* static */
ES_Special_Function_Arguments *ES_Special_Function_Arguments::Make(ES_Context *context)
{
    ES_Special_Function_Arguments *self;
    GC_ALLOCATE(context, self, ES_Special_Function_Arguments, (self));
    return self;
}

/* static */
void ES_Special_Function_Arguments::Initialize(ES_Special_Function_Arguments *self)
{
    self->InitGCTag(GCTAG_ES_Special_Function_Arguments);
}

/* static */
ES_Special_Function_Caller *ES_Special_Function_Caller::Make(ES_Context *context)
{
    ES_Special_Function_Caller *self;
    GC_ALLOCATE(context, self, ES_Special_Function_Caller, (self));
    return self;
}

/* static */
void ES_Special_Function_Caller::Initialize(ES_Special_Function_Caller *self)
{
    self->InitGCTag(GCTAG_ES_Special_Function_Caller);
}

/* static */
ES_Special_RegExp_Capture *ES_Special_RegExp_Capture::Make(ES_Context *context, unsigned index)
{
    ES_Special_RegExp_Capture *self;
    GC_ALLOCATE(context, self, ES_Special_RegExp_Capture, (self, index));
    return self;
}

/* static */
void ES_Special_RegExp_Capture::Initialize(ES_Special_RegExp_Capture *self, unsigned index)
{
    self->InitGCTag(GCTAG_ES_Special_RegExp_Capture);
    self->index = index;
}

/* static */ ES_Special_Error_StackTrace *
ES_Special_Error_StackTrace::Make(ES_Context *context, ES_Error::StackTraceFormat format)
{
    ES_Special_Error_StackTrace *self;
    GC_ALLOCATE(context, self, ES_Special_Error_StackTrace, (self, format));
    return self;
}

/* static */ void
ES_Special_Error_StackTrace::Initialize(ES_Special_Error_StackTrace *self, ES_Error::StackTraceFormat format)
{
    self->InitGCTag(GCTAG_ES_Special_Error_StackTrace);
    self->format = format;
}
