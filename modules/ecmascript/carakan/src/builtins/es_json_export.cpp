/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_json_builtins.h"
#include "modules/ecmascript/carakan/src/kernel/es_string_appender.h"

class JSON_Appender
    : public ES_JString_Appender
{
public:
    JSON_Appender(ES_Context *context, JString *target)
        : ES_JString_Appender(context, target)
    {
    }

    /* Outputting JSON specific values and fragments */
    void AppendStringEscaped(const uni_char* s, unsigned len, uni_char delim = 0, uni_char post = 0);

    void AppendKey(JString* js)
    {
        AppendStringEscaped(Storage(context, js), Length(js), '"', ':');
    }

    void AppendKey(unsigned idx)
    {
        uni_char buf[16]; // ARRAY OK 2010-01-15 stanislavj
        int sz = uni_sprintf(buf, UNI_L("\"%u\":"), idx);
        this->Append(buf, sz);
    }

    void AppendStrVal(JString* jstr)
    {
        AppendStringEscaped(Storage(context, jstr), Length(jstr), '"');
    }

    void AppendDblVal(double dbl)
    {
        char buffer[32]; // ARRAY OK 2012-04-21 sof
        tostring(context, dbl, buffer);
        this->Append(buffer);
    }

    void AppendIntVal(INT32 v)
    {
        if (v >= 0 && v <= 9)
            this->Append('0' + v);
        else
        {
            char buf[16]; // ARRAY OK 2011-12-04 sof
            op_itoa(v, buf, 10);
            this->Append(buf);
        }
    }
};

void
JSON_Appender::AppendStringEscaped(const uni_char* s, unsigned len, uni_char delim, uni_char post)
{
    if (delim != 0)
        this->Append(delim);

    /* Common case: It fits into the local buffer _and_ without requiring escapes */
    if (this->BufferSpaceLeft() >= len)
    {
        uni_char *orig_put = this->put;
        for (const uni_char* p = s ; p < (s+len) ; ++p)
        {
            const uni_char ch = *p;
            if ( ch > 0x22 && ch != '\\' && ch != 0x7f)
                *this->put++ = ch;
            else
            {
                this->put = orig_put; /* reset pointer and do it slowly. */
                goto slow;
            }
        }
    }
    else
    {   /* check for non-escaped strings..and insert them straight if found */
        for (const uni_char* p = s ; p < (s+len) ; ++p)
        {
            const uni_char ch = *p;
            if ( ch < 0x23 || ch == '\\' || ch > 0x7e)
                goto slow;
        }
        this->Append(s, len);
    }
    goto output_post_chars;

slow:
    uni_char subst_buff[8]; // ARRAY OK 2009-07-06 stanislavj
    const uni_char *subst;
    unsigned subst_len;
    for (const uni_char *ends = s + len; s < ends; ++s)
    {
        switch (*s)
        {
        case '\b': subst = UNI_L("\\b");  subst_len = 2; break;
        case '\t': subst = UNI_L("\\t");  subst_len = 2; break;
        case '\n': subst = UNI_L("\\n");  subst_len = 2; break;
        case '\f': subst = UNI_L("\\f");  subst_len = 2; break;
        case '\r': subst = UNI_L("\\r");  subst_len = 2; break;
        case '\\': subst = UNI_L("\\\\"); subst_len = 2; break;
        case '"' : subst = UNI_L("\\\""); subst_len = 2; break;
        case '\x7f': subst = UNI_L("\\u007f"); subst_len = 6; break;
        default:
             /* Section 15.12.3's definition of Quote(value), bullet 2.c:
                 "character having a code unit value C less than the space character, ... escape it" */
            if (*s < 0x20)
            {
                uni_strncpy(subst_buff, UNI_L("\\u00"), 4);
                subst_buff[4] = *s >= 0x10 ? '1' : '0';
                unsigned digit = (*s % 0x10);
                subst_buff[5] = digit >= 10 ? ('a' + (digit - 10)) : ('0' + digit);
                subst = subst_buff;
                subst_len = 6;
            }
            else
            {
                this->Append(*s);
                continue;
            }
        }
        this->Append(subst, subst_len);
    }

output_post_chars:
    if (delim != 0)
        this->Append(delim);
    if (post  != 0)
        this->Append(post);
    return;
}

static BOOL
INVOKE_toJSON_IF_APPLICABLE(ES_Context *context, ES_Value_Internal *value_val, ES_Value_Internal &key_val)
{
    OP_ASSERT(value_val != NULL && value_val->IsObject());

    if (ES_Execution_Context* exec_context = context->GetExecutionContext())
    {
        ES_Value_Internal toJSON_val;
        GetResult res = value_val->GetObject(context)->GetL(exec_context, context->rt_data->strings[STRING_toJSON], toJSON_val);
        if (!GET_OK(res) || !toJSON_val.IsObject() || !toJSON_val.GetObject()->IsFunctionObject())
            return TRUE; // there was no 'toJSON' property or it was not a function

        ES_Value_Internal *registers = exec_context->SetupFunctionCall(static_cast<ES_Function *>(toJSON_val.GetObject()), 1);

        registers[0] = *value_val;
        registers[1] = toJSON_val;
        registers[2] = key_val;

        return exec_context->CallFunction(registers, 1, value_val);
    }
    else
        return TRUE;
}

static inline BOOL
INVOKE_REPLACER_FUNCTION(ES_Execution_Context *context, ES_Object *subject_obj, ES_Function *replacer_fn, ES_Value_Internal &key_val, ES_Value_Internal *value)
{
    ES_Value_Internal *registers = context->SetupFunctionCall(replacer_fn, 2);

    registers[0].SetObject(subject_obj);
    registers[1].SetObject(replacer_fn);
    registers[2] = key_val;
    registers[3] = *value;

    return context->CallFunction(registers, 2, value);
}

/** The stringify method takes a value and an optional replacer, and an optional
    space parameter, and returns a JSON text. The replacer can be a function
    that can replace values, or an array of strings that will select the keys.
    A default replacer method can be provided. Use of the space parameter can
    produce text that is more easily readable. */
BOOL
ES_JsonBuiltins::stringify(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc == 0)
    {
        /* The (non-normative) ES5.1 spec text for JSON.stringify() states that
           'value' is required, but this has since been clarified as an
           unintended requirement (CORE-48161.) */
        return_value->SetUndefined();
        return TRUE;
    }

    ES_Value_Internal *value_val = &argv[0];

    OP_ASSERT(argc == 1 || context->GetExecutionContext());

    ES_Value_Internal *replacer_val = argc >= 2 ? &argv[1] : NULL;
    ES_Value_Internal *space_val = argc >= 3 ? &argv[2] : NULL;

    if (space_val)
    {
        if (space_val->IsObject())
        {
            switch (space_val->GetObject()->GCTag())
            {
            case GCTAG_ES_Object_Number:
            case GCTAG_ES_Object_String:
                space_val->ToPrimitive(context->GetExecutionContext(), ES_Value_Internal::HintNone);
            }
        }

        if (space_val->IsNumber())
        {
            // If the space parameter is a number, replace it with a string containing that many spaces.
            double d = space_val->GetNumAsDouble();
            if (!op_isfinite(d) || d < 1)
                space_val->SetNull();
            else
            {
                unsigned u = d > 10 ? 10 : op_double2uint32(d);

                space_val->SetString(JString::Make(context, u));
                for (uni_char* s = Storage(context, space_val->GetString()); 0 < u--; )
                    *s++ = ' ';
            }
        }
        else if (space_val->IsString())
        {
            unsigned length = Length(space_val->GetString());
            if (length > 10)
                space_val->SetString( JString::Make(context, space_val->GetString(), 0, 10) );
            else if (length == 0)
                space_val->SetNull();
        }
        else
        {
            // null is for empty string:
            space_val->SetNull();
        }
    }

    ES_Object *replacer_obj = NULL;
    if (replacer_val)
    {
        // If replacer is not a function or an array it should be ignored:
        if (replacer_val->IsObject()
            && (replacer_val->GetObject()->IsFunctionObject()
                || replacer_val->GetObject()->IsArrayObject()))
            replacer_obj = replacer_val->GetObject();
        else
            replacer_val->SetUndefined();
    }

    // If applicable, invoke subject's toJSON method:
    JString *nul_str = context->rt_data->strings[STRING_empty];
    ES_Value_Internal  nul_str_val(nul_str);

    if (value_val != NULL && value_val->IsObject() && !INVOKE_toJSON_IF_APPLICABLE(context, value_val, nul_str_val))
        return FALSE;

    AutoRegisters reg(context, 1);
    // Apply the (optional) replacer function
    if (replacer_obj != NULL && replacer_obj->IsFunctionObject())
    {
        ES_Object* subject_obj = ES_Object::Make(context, context->GetGlobalObject()->GetObjectClass());
        reg[0].SetObject(subject_obj);
        subject_obj->PutL(context, nul_str, *value_val);
        if (!INVOKE_REPLACER_FUNCTION(context, subject_obj, static_cast<ES_Function *>(replacer_obj), nul_str_val, value_val))
            return FALSE;
    }

    if (value_val->IsUndefined() || value_val->IsObject() && value_val->GetObject()->IsFunctionObject())
    {
        return_value->SetUndefined();
        return TRUE;
    }

    reg[0].SetString(JString::Make(context));
    JSON_Appender result(context, reg[0].GetString());

    BOOL ok = ExportValue(context,
                      value_val,
                      replacer_obj,
                      space_val && space_val->IsString() ? space_val->GetString() : NULL,
                      result);
    if (ok)
        return_value->SetString(result.GetString());

    return ok;
}

/* static */ void
ES_JsonBuiltins::StringifyL(ES_Context *context, const uni_char *&string, const ES_Value_Internal &value0)
{
    JString *result_string = JString::Make(context);
    JSON_Appender result(context, result_string);
    ES_Value_Internal value(value0);

    if (ExportValue(context, &value, NULL, NULL, result))
        string = StorageZ(context, result.GetString());
    else
        LEAVE(OpStatus::ERR);
}

#define PUTC(C)   do result.Append(C); while (0)
#define PUTS(S)   do result.Append(S); while (0)

static inline BOOL
is_json_exportable(const ES_Value_Internal &val)
{
    if (val.IsUndefined() || val.IsObject() && val.GetObject()->IsFunctionObject())
        return false;
    else
        return true;
}


BOOL
ES_JsonBuiltins::ExportValue(ES_Context *context, ES_Value_Internal *subject_val, ES_Object *replacer_obj, JString *space_str, JSON_Appender &result)
{
    enum
    {
        IDX_PREV_FRAME_ARR,
        IDX_NEXT_FRAME_ARR, //meant to optimize allocations
        IDX_SUBJECT_OBJ,
        IDX_ALLPROPS_ARR,
        NUSED,
        IDX_NEXPORTED=NUSED,
        IDX_LENGTH,
        IDX_IPROP,
        NSLOTS
    };

    ES_Execution_Context *exec_context = context->GetExecutionContext();
    ES_Global_Object *global_object;
    if (exec_context)
        global_object = exec_context->GetGlobalObject();
    else
        global_object = static_cast<ES_Global_Object *>(context->runtime->GetGlobalObject());
    ES_Boxed_Array *top_frame  = NULL;
    unsigned indent_lvl = 0;

    AutoRegisters auto_regs(exec_context, 5);
    ES_Value_Internal *regs = auto_regs.GetRegisters(), local_regs[5];
    if (!regs)
        regs = local_regs;

    ES_Value_Internal &prop_val = regs[0];
    regs[1].SetUndefined();
    //reg[1] stores top_frame (when non-NULL).
    //reg[2] stores allprops
    //reg[3] stores prop_list
    //reg[4] stores props

    OP_ASSERT(!replacer_obj || exec_context);

recurse:
    if (subject_val->IsObject())
    {
        ES_Object* subject; subject = subject_val->GetObject(context); // to supress GCC warning
        // check for cycle first:
        for (ES_Boxed_Array *cur = top_frame; cur != NULL; cur = static_cast<ES_Boxed_Array *>(cur->slots[IDX_PREV_FRAME_ARR]))
            if (cur->slots[IDX_SUBJECT_OBJ] == subject)
            {
                if (exec_context)
                    exec_context->ThrowTypeError("JSON.stringify: Not an acyclic Object");

                return FALSE;
            }

        ES_Object *allprops; allprops = NULL; // to supress GCC warning
        unsigned nexported, length, iprop;

        switch (subject->GCTag())
        {
        case GCTAG_ES_Object_Array:
        //export_array:
        {
            subject->GetCachedAtIndex(ES_PropertyIndex(0), prop_val);
            length = prop_val.GetNumAsUInt32();

            PUTC('[');
            nexported = 0;
            for (iprop = 0; iprop < length; iprop++)
            {
                if (exec_context)
                {
                    if (!GET_OK(subject->GetOwnPropertyL(exec_context, NULL/*this*/, ES_Value_Internal(), iprop, prop_val)))
                        prop_val.SetNull();
                }
                else if (!subject->GetNativeL(context, iprop, prop_val))
                    prop_val.SetNull();

                {//block to limit idx_val's scope
                ES_Value_Internal idx_val(iprop);

                if (prop_val.IsObject() && !INVOKE_toJSON_IF_APPLICABLE(context, &prop_val, idx_val))
                    return FALSE;

                if (replacer_obj != NULL && replacer_obj->IsFunctionObject())
                {
                    idx_val.SetUInt32(iprop); // not a redundant set!
                    INVOKE_REPLACER_FUNCTION(exec_context, subject, static_cast<ES_Function *>(replacer_obj), idx_val, &prop_val);
                }
                }//block to limit idx_val's scope

                if (!is_json_exportable(prop_val))
                    prop_val.SetNull();

                if (nexported++ != 0)
                    PUTC(',');

                if (space_str != NULL)
                {
                    PUTC('\n');
                    for (unsigned j = indent_lvl+1; 0 < j; --j)
                        PUTS(space_str);
                }

            push_frame_and_recurse:

                ES_JSON_PUSH_FRAME(context, top_frame, regs[1]);
                top_frame->slots[IDX_SUBJECT_OBJ] = subject; subject = NULL;
                top_frame->slots[IDX_ALLPROPS_ARR]= allprops; allprops = NULL;
                top_frame->uints[IDX_NEXPORTED] = nexported;
                top_frame->uints[IDX_LENGTH] = length;
                top_frame->uints[IDX_IPROP] = iprop;
                *subject_val = prop_val;
                indent_lvl++;
                goto recurse;

            resume_export_array:;
            }

            if (length != 0 && space_str != NULL)
            {
                PUTC('\n');
                int j = indent_lvl;
                while (0 < j--)
                    PUTS(space_str);
            }
            PUTC(']');
        }
        break;


        case GCTAG_ES_Object_Number:
        case GCTAG_ES_Object_String:
        case GCTAG_ES_Object_Boolean:
            if (exec_context)
            {
                if (subject_val->ToPrimitive(exec_context, ES_Value_Internal::HintNone))
                    goto export_value;
            }
            else
            {
                switch (subject->GCTag())
                {
                case GCTAG_ES_Object_Number:
                    subject_val->SetNumber(static_cast<ES_Number_Object *>(subject)->GetValue());
                    break;
                case GCTAG_ES_Object_String:
                    subject_val->SetString(static_cast<ES_String_Object *>(subject)->GetValue());
                    break;
                case GCTAG_ES_Object_Boolean:
                    subject_val->SetBoolean(static_cast<ES_Boolean_Object *>(subject)->GetValue());
                }
                goto export_value;
            }

            if (!subject_val->IsObject())
            {
                OP_ASSERT(0);
                goto exit;
            }
            // Still an Object, fallthrough:
        default:
        //export_object: ('arguments' object is also exported here)
        {
            length = 0;
            if (replacer_obj == NULL || replacer_obj->IsFunctionObject())
            {
                // Collect all property names:
                if (subject->IsArgumentsObject())
                {
                    PUTS("{}");
                    goto pop_frame_and_resume;
                }

                allprops = ES_Array::Make(context, global_object);
                regs[2].SetObject(allprops); //protect!

                if (!GetProperties(context, subject, allprops, &length))
                    return FALSE;
            }
            else
            {
                OP_ASSERT(replacer_obj->IsArrayObject());
                GetResult gres = replacer_obj->GetLength(exec_context, length);
                OP_ASSERT(GET_OK(gres));
                (void) gres; // Silence compiler warning.
            }

            PUTC('{');
            nexported = 0;
            for (iprop = 0; iprop < length; iprop++)
            {
                {//block to localize scope of prop_name
                // fetch exported property name:
                ES_Value_Internal prop_name; // may hold (at different stages string id or uint32 index
                if (replacer_obj == NULL || replacer_obj->IsFunctionObject())
                {
                    if (!allprops->GetNativeL(context, iprop, prop_name))
                    {
                        OP_ASSERT(0); // shouldn't happen if JSON_PropertyIterator behaves as expected ...
                        continue;
                    }
                }
                else
                {
                    OP_ASSERT(replacer_obj->IsArrayObject());

                    if (!GET_OK(replacer_obj->GetL(exec_context, iprop, prop_name)))
                        continue;

                    if (prop_name.IsObject())
                    {
                        switch (prop_name.GetObject()->GCTag())
                        {
                        case GCTAG_ES_Object_Number:
                        case GCTAG_ES_Object_String:
                            if (!prop_name.ToString(exec_context))
                                continue;
                        }
                    }
                    if (prop_name.IsString())
                    {
                        JString *str = prop_name.GetString();
                        unsigned index;
                        if (convertindex(Storage(context, str), Length(str), index))
                        {
                            prop_name.SetUInt32(index);
                            goto index_prop_name;
                        }
                        else
                            goto string_prop_name;
                    }
                }

                GetResult gs;
                if (prop_name.IsString())
                {
                string_prop_name:
                    if (exec_context)
                        gs = subject->GetL(exec_context, prop_name.GetString(), prop_val);
                    else
                        gs = subject->GetNativeL(context, prop_name.GetString(), prop_val) ? PROP_GET_OK : PROP_GET_FAILED;
                }
                else if (prop_name.IsNumber())
                {
                    if (!prop_name.IsUInt32())
                    {
                        if (prop_name.ToString(context))
                            goto string_prop_name;
                        else
                            continue;
                    }
                index_prop_name:
                    if (exec_context)
                        gs = subject->GetL(exec_context, prop_name.GetNumAsUInt32(), prop_val);
                    else
                        gs = subject->GetNativeL(context, prop_name.GetNumAsUInt32(), prop_val) ? PROP_GET_OK : PROP_GET_FAILED;
                }
                else // ignore everything that is not a string or a number
                    continue;

                if (!GET_OK(gs))
                    continue;

                if (prop_val.IsObject() && !INVOKE_toJSON_IF_APPLICABLE(context, &prop_val, prop_name))
                    return FALSE;

                if (replacer_obj != NULL && replacer_obj->IsFunctionObject())
                    INVOKE_REPLACER_FUNCTION(exec_context, subject, static_cast<ES_Function *>(replacer_obj), prop_name, &prop_val);

                if (!is_json_exportable(prop_val))
                    continue;

                // render the `<property-name>:' part
                {
                    if (nexported++ != 0)
                        PUTC(',');
                    if (space_str != NULL)
                    {
                        PUTC('\n');
                        int j = indent_lvl+1;
                        while (0 < j--)
                            PUTS(space_str);
                    }
                    if (prop_name.IsString())
                        result.AppendKey(prop_name.GetString()); // string should be escaped!
                    else
                        result.AppendKey(prop_name.GetNumAsUInt32());
                    if (space_str != NULL)
                        PUTC(' ');
                }
                }//block to localize scope of prop_name
                goto push_frame_and_recurse;
            resume_export_object:;
            } // for
            if (nexported != 0 && space_str != NULL)
            {
                PUTC('\n');
                int j = indent_lvl;
                while (0 < j--)
                    PUTS(space_str);
            }
            PUTC('}');
        }//case it is Object
        break;
        }//swicth

    pop_frame_and_resume:
        if (top_frame != NULL)
        {
            // pop live local vars:
            subject_val->SetObject(subject = static_cast<ES_Object *>(top_frame->slots[IDX_SUBJECT_OBJ])); // may be not needed
            allprops  = static_cast<ES_Object *>(top_frame->slots[IDX_ALLPROPS_ARR]);
            nexported = top_frame->uints[IDX_NEXPORTED];
            length    = top_frame->uints[IDX_LENGTH];
            iprop     = top_frame->uints[IDX_IPROP];
            indent_lvl--;
            top_frame = static_cast<ES_Boxed_Array *>(top_frame->slots[IDX_PREV_FRAME_ARR]);
            if (subject->GCTag() == GCTAG_ES_Object_Array)
                goto resume_export_array;
            else
                goto resume_export_object;
        }
        else
            goto exit;
    }
    else // not an Object:
export_value:
    if (subject_val->IsNumber())
        if (subject_val->IsInt32())
            result.AppendIntVal(subject_val->GetInt32());
        else if (op_isfinite(subject_val->GetNumAsDouble()))
            result.AppendDblVal(subject_val->GetNumAsDouble());
        else
            goto export_null;
    else if (subject_val->IsString())
        result.AppendStrVal(subject_val->GetString()); // string should be escaped!
    else if (subject_val->IsBoolean())
    {
        OP_ASSERT(subject_val->GetBoolean() == TRUE || subject_val->GetBoolean() == FALSE);
        PUTS(subject_val->GetBoolean() ? "true" : "false");
    }
    else if (subject_val->IsNull())
    {
    export_null:
        PUTS("null");
    }
    else
    {
        //TODO it is not an (object | array | number | string | bool | null | undefined)
        OP_ASSERT(!subject_val->IsUndefined());
        OP_ASSERT(!"implemented yet");
    }

    goto pop_frame_and_resume;

exit:
    return TRUE;
}


/* static */ void
ES_JsonBuiltins::PopulateJson(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_JsonBuiltins);

    APPEND_PROPERTY(ES_JsonBuiltins, stringify, MAKE_BUILTIN_NO_PROTOTYPE(3, stringify));
    APPEND_PROPERTY(ES_JsonBuiltins, parse, MAKE_BUILTIN_NO_PROTOTYPE(2, parse));

    ASSERT_OBJECT_COUNT(ES_JsonBuiltins);
}

/* static */ void
ES_JsonBuiltins::PopulateJsonClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_JsonBuiltinsCount);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_JsonBuiltins, stringify, DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_JsonBuiltins, parse, DE, ES_STORAGE_OBJECT);
}
