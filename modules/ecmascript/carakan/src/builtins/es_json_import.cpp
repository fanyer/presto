/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_json_builtins.h"
#include "modules/ecmascript/carakan/src/compiler/es_lexer.h"


class JsonParser
    : public ES_StackGCRoot
{
    ES_Context *context;
    JString *last_string;
    /**< Contains the last parsed string/ident if it didn't fit
         in buffer _or_ the detailed error message
         constructed by errorL. */

    int buffer_length;
    uni_char buffer[256]; // ARRAY OK 2009-05-19 stanislavj

    const uni_char *source_begin;
    const uni_char *source_end;

    const uni_char *MakeErrorMessage(const char *message, const uni_char *error_begin, const uni_char *error_end = NULL);

    const uni_char *ParseValue(const uni_char *cur, ES_Value_Internal *pres);
    /** Parses a quoted string (which may happen to represent an identifier/index.)
        The parsed string (with escape sequences processed) is stored either in
        (a) 'buffer' if it is small enough to fit there;
        (b) in last_string otherwise
        In the (a) case, 'last_string' is set to NULL and 'buffer_length' is set appropriately;
        In the (b) case, 'last_string' is non NULL and 'buffer_length' is set to -1

        @return the source position right after the string (i.e. current parsing position) */

    const uni_char *ParseString(const uni_char *cur);

    const uni_char *ParseNumber(const uni_char *cur, ES_Value_Internal *pres);

    virtual void GCTrace()
    {
        GC_PUSH_BOXED(context->heap, last_string);
    }

public:
    JsonParser(ES_Context *context, const uni_char *src, unsigned src_len)
        : ES_StackGCRoot(context->GetExecutionContext())
        , context(context)
        , last_string(NULL)
        , buffer_length(-1)
        , source_begin(src)
        , source_end(src + src_len)
    {
    }

    BOOL  Parse(ES_Value_Internal *value);

    const uni_char *GetErrorMessage() const { return buffer; }
};


static inline
BOOL is_json_space(uni_char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

#define skip_space() while (is_json_space(*cur)) ++cur

const uni_char *
JsonParser::MakeErrorMessage(const char *message, const uni_char *error_begin, const uni_char *error_end)
{
    uni_char *pbuf = buffer, *pend = buffer + ARRAY_SIZE(buffer) - 1;
    const char* separator = ": ";

    do
    {
        while (*message && pbuf < pend)
            *pbuf++ = (uni_char)*message++;

    } while (message != separator + 2 && (message = separator));

    int nsample = 7;
    while (0 < nsample && pbuf < pend && *error_begin && (!error_end || error_begin < error_end) && !is_json_space(*error_begin))
    {
        *pbuf++ = *error_begin++;
        nsample--;
    }

    *pbuf = 0;

    /* MakeErrorMessage() "signals" error by returning NULL,
       with the error message stored in 'buffer'. Callers
       must propagate the NULL value outwards. */
    return NULL;
}

const uni_char *
JsonParser::ParseString(const uni_char *cur)
{
    OP_ASSERT(*cur == '"');

    const uni_char* str_beg = cur;

    uni_char* put = buffer;
    uni_char* put_limit = buffer + ARRAY_SIZE(buffer);
    last_string = NULL;

    for (;;)
    {
        uni_char cc = *++cur;
        if (cc == '"')
        {
            if (last_string)
            {
                Append(context, last_string, buffer, put - buffer);
                buffer_length = -1;
            }
            else
            {
                buffer_length = static_cast<int>(put - buffer);
                if (buffer_length == 0) // buffer[0] must always make sense ...
                    buffer[0] = 0;
            }

            return cur + 1;
        }
        else if (cc < 0x20)
        {
            if (cur == source_end)
                return MakeErrorMessage("Unterminated string", str_beg, cur);
            else
                return MakeErrorMessage("Unescaped control char in string", str_beg, cur);
        }
        else if (cc == '\\')
        {
            switch (*++cur)
            {
            case 'b': cc = '\b'; break;
            case 'f': cc = '\f'; break;
            case 'n': cc = '\n'; break;
            case 'r': cc = '\r'; break;
            case 't': cc = '\t'; break;
            case '"': cc = '\"'; break;
            case '\\':cc = '\\'; break;
            case '/' :cc = '/' ; break;
            case 'u':
                {
                    cc = 0;
                    for (int i = 0; i < 4; i++)
                        if (!ES_Lexer::IsHexDigit(*++cur))
                            return MakeErrorMessage("Invalid codepoint escape sequence", str_beg, cur - i - 1);
                        else
                            cc = cc * 16 + ES_Lexer::HexDigitValue(*cur);
                    break;
                }

            default: return MakeErrorMessage("Invalid escape char", str_beg, cur);
            }
            // fall through to append cc
        }

        // Append current char
        if (put >= put_limit)
        {
            OP_ASSERT(put == put_limit);
            if (last_string)
                Append(context, last_string, buffer, ARRAY_SIZE(buffer));
            else
                last_string = JString::Make(context, buffer, ARRAY_SIZE(buffer));

            put = buffer;
        }
        *put++ = cc;
    }
}

const uni_char*
JsonParser::ParseValue(const uni_char* cur, ES_Value_Internal* pres)
{
    enum
    {
        IDX_PREV_FRAME_ARR,
        IDX_NEXT_FRAME_ARR, //meant to optimize allocations
        IDX_SUBJECT_OBJ,
        IDX_ID_STR,
        NUSED,
        IDX_ARR_LEN=NUSED,
        IDX_IDX=IDX_ARR_LEN,
        NSLOTS
    };

    ES_Execution_Context *exec_context = context->GetExecutionContext();
    ES_Global_Object *global_object;
    if (exec_context)
        global_object = exec_context->GetGlobalObject();
    else
        global_object = static_cast<ES_Global_Object *>(context->runtime->GetGlobalObject());
    AutoRegisters auto_regs(exec_context, 2);
    ES_Value_Internal *regs = auto_regs.GetRegisters(), local_regs[2];
    if (!regs)
        regs = local_regs;
    ES_Value_Internal& result_value = regs[0];
    regs[1].SetUndefined();//regs[1] stores top_frame (when non-NULL).

    ES_Boxed_Array *top_frame = NULL;
parse_value:
    skip_space();

    switch (*cur)
    {
    case '{':
        {
            cur++;
            ES_JSON_PUSH_FRAME(context, top_frame, regs[1]);
            ES_Object* obj; obj = ES_Object::Make(context, global_object->GetObjectClass());
            top_frame->slots[IDX_SUBJECT_OBJ] = obj;

            skip_space();
            if (*cur == '}')
                goto end_of_object;

            for (;;)
            {
                if (*cur == '"')
                {
                    JString* id;
                    if ((cur = ParseString(cur)) == NULL)
                        return NULL;
                    if (buffer_length >= 0)
                    {
                        UINT32 idx = 0;
                        if (op_isdigit(buffer[0]) && convertindex(buffer, buffer_length, idx))
                        {
                            top_frame->uints[IDX_IDX] = idx;
                            id = NULL;
                        }
                        else
                            id = JString::Make(context, buffer, buffer_length);
                    }
                    else
                        id = last_string;

                    top_frame->slots[IDX_ID_STR] = id;
                    skip_space();

                    if (*cur != ':')
                        return MakeErrorMessage("Colon expected", cur);

                    ++cur;
                    goto parse_value;
resume_parse_object:
                    obj = static_cast<ES_Object *>(top_frame->slots[IDX_SUBJECT_OBJ]);
                    id  = static_cast<JString *>(top_frame->slots[IDX_ID_STR]);

                    { // Scope for |info| so that no goto statement misses its init.
                        ES_Property_Info info;
                        if (id != NULL)
                        {
                            info.Set(CP);
                            obj->PutNativeL(context, id, info, result_value);
                        }
                        else
                        {
                            UINT32 idx = top_frame->uints[IDX_IDX];
                            obj->PutNativeL(context, idx, info, result_value);
                        }
                    }

                    skip_space();

                    if (*cur == ',')
                    {
                        cur++;
                        skip_space();
                        continue;
                    }
                    else if (*cur == '}')
                    {
end_of_object:
                        ++cur;
                        result_value.SetObject(obj);
                        top_frame->slots[IDX_SUBJECT_OBJ] = NULL; // so it can be GC-ed
                        top_frame->slots[IDX_ID_STR] = NULL; // so it can be GC-ed
                        top_frame = static_cast<ES_Boxed_Array *>(top_frame->slots[IDX_PREV_FRAME_ARR]);
                        goto local_return;
                    }
                    else
                        return MakeErrorMessage("'}' expected", cur);
                }
                else
                    return MakeErrorMessage("Property name (in double quotes) expected", cur);
            }

        }

    case '[':
        {
            cur++;
            ES_JSON_PUSH_FRAME(context, top_frame, regs[1]);
            ES_Object*  arr; arr = ES_Array::Make(context, global_object);
            top_frame->slots[IDX_SUBJECT_OBJ] = arr;
            UINT32      arr_len; arr_len = 0;
            bool        just_added; just_added = false;
            for (;;)
            {
                skip_space();

                if (*cur == ']')
                {
                    if (0 < arr_len)
                    {
                        if (!just_added)
                            return MakeErrorMessage("Superfluous trailing comma in array literal", cur);
                        ES_Array::SetLength(context, arr, arr_len);
                    }
                    ++cur;
                    result_value.SetObject(arr);
                    top_frame->slots[IDX_SUBJECT_OBJ] = NULL; // so it can be GC-ed
                    top_frame = static_cast<ES_Boxed_Array *>(top_frame->slots[IDX_PREV_FRAME_ARR]);
                    //goto local_return;
local_return:
                    if (top_frame != NULL)
                        if (static_cast<ES_Object *>(top_frame->slots[IDX_SUBJECT_OBJ])->IsArrayObject())
                            goto resume_parse_array;
                        else
                            goto resume_parse_object;
                    else
                    {
                        *pres = result_value;
                        return cur;
                    }
                }

                if (*cur == ',')
                {
                    if (just_added)
                        just_added = false;
                    else
                        //replace next line with "arr_len++;" if we are to be lenient:
                        return MakeErrorMessage("Superfluous comma in array literal", cur);
                    cur++;
                    continue;
                }
                else if (just_added)
                    return MakeErrorMessage("Missing comma between elements in array literal", cur);

                top_frame->slots[IDX_ID_STR]  = NULL;
                top_frame->uints[IDX_ARR_LEN] = arr_len;
                goto parse_value;
resume_parse_array:
                arr = static_cast<ES_Object *>(top_frame->slots[IDX_SUBJECT_OBJ]);
                arr_len = top_frame->uints[IDX_ARR_LEN];

                if (exec_context)
                    arr->PutL(exec_context, arr_len, result_value);
                else
                    arr->PutNativeL(context, arr_len, 0, result_value);
                ++arr_len;

                just_added = true;
            }
        }

    case '"':
        if ((cur = ParseString(cur)) == NULL)
            return NULL;
        if (!last_string)
            result_value.SetString(JString::Make(context, buffer, buffer_length));
        else
        {
            result_value.SetString(last_string);
            last_string = NULL;
        }
        goto local_return;

    case 't':
        if (cur[1] == 'r' && cur[2] == 'u' && cur[3] == 'e' && !uni_isalnum(cur[4]))
        {
            result_value.SetBoolean(TRUE);
            cur += 4;
            goto local_return;
        }
        break;

    case 'f':
        if (cur[1] == 'a' && cur[2] == 'l' && cur[3] == 's' && cur[4] == 'e' && !uni_isalnum(cur[5]))
        {
            result_value.SetBoolean(FALSE);
            cur += 5;
            goto local_return;
        }
        break;

    case 'n':
        if (cur[1] == 'u' && cur[2] == 'l' && cur[3] == 'l' && !uni_isalnum(cur[4]))
        {
            result_value.SetNull();
            cur += 4;
            goto local_return;
        }
        break;

    case '-':
        if (                 cur[1] >= '0' && cur[1] <= '9' ||
            cur[1] == '.' && cur[2] >= '0' && cur[2] <= '9')
        {
            if (cur[1] == '0' && uni_isdigit(cur[2]))
                goto error_leading_0;
            if (cur[1] == '.')
                goto error_leading_dot;

            if ((cur = ParseNumber(cur, &result_value)) == NULL)
                return NULL;
            goto local_return;
        }
        else
            break;

    case '.':
        if (cur[1] >= '0' && cur[1] <= '9')
        error_leading_dot:
            return MakeErrorMessage("Illegal number format (leading decimal dot)", cur);
        else
            break;

    case '0':
        if (uni_isdigit(cur[1]))
    error_leading_0:
            return MakeErrorMessage("Illegal number format (excessive leading 0)", cur);
        //else - fall through

    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        if ((cur = ParseNumber(cur, &result_value)) == NULL)
            return NULL;
        goto local_return;
    }

    return MakeErrorMessage("Unable to parse value", cur);
}

BOOL
JsonParser::Parse(ES_Value_Internal* pres)
{
    const uni_char* cur = ParseValue(source_begin, pres);
    if (cur != NULL)
    {
        //check that source contains only white space after end of parse (current pos):
        skip_space();
        if (cur == source_end)
            return TRUE;
        {
            MakeErrorMessage("Parsed string contains more than single value", cur);
            return FALSE;
        }
    }
    else
        return FALSE;
}

static void
ReleaseBuffer(ES_Context *context, char *buffer)
{
    if (ES_Execution_Context *exec_context = context->GetExecutionContext())
    {
        ES_Suspended_DELETEA<char> suspended(buffer);
        exec_context->SuspendedCall(&suspended);
    }
    else
        OP_DELETEA(buffer);
}

class ES_Suspended_strtod
    : public ES_SuspendedCall
{
public:
    ES_Suspended_strtod(ES_Execution_Context *context, char *buffer)
        : buffer(buffer)
        , result(0.0)
    {
        context->SuspendedCall(this);
    }

    char *buffer;
    double result;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        result = op_strtod(buffer, NULL);
    }
};

// ParseNumber is closely based on Lexer::LexDecimalNumber():
const uni_char *
JsonParser::ParseNumber(const uni_char* cur, ES_Value_Internal* pres)
{
    const uni_char* str_beg = cur;

    BOOL  negate;
    if (*cur == '-')
    {
        cur++;
        negate = TRUE;
    }
    else
        negate = FALSE;

    // Common case
    const uni_char *inputstart = cur;
    int i = 0;
    int k = 9;        // Any 9-digit decimal number can be represented as a 32-bit (signed) int

    while (k > 0 && *cur >= '0' && *cur <= '9')
    {
        --k;
        i = i*10 + (*cur++ - '0');
    }

    if (*cur != '.' && !uni_isalnum(*cur))
    {
        pres->SetInt32(negate ? -i : i);
        return cur;
    }

    // Hard case
    //
    // ({dec_number}\.{dec_number}?([eE][-+]?{dec_number})?)|({dec_number}?\.{dec_number}([eE][-+]?{dec_number})?)
    //
    // States:
    //   0  before anything
    //   1  initial digit string, before .
    //   2  after . but before any exponent
    //   3  after e but before + or -
    //   4  after + or -

    char shortbuf[60]; // ARRAY OK 2008-08-08 stanislavj
    char *numbuf = shortbuf;
    char *nump = shortbuf;
    char *numlim = shortbuf + ARRAY_SIZE(shortbuf) - 1;        // Leave space for a terminator
    int digits_in_fraction = 0;
    int digits_in_exponent = 0;
    int state;

    cur = inputstart;
    state = 0;
    for (;;)
    {
        if (*cur >= '0' && *cur <= '9')
        {
            if (state == 0) state = 1;
            else if (state == 3) state = 4;

            if (state == 1 || state == 2) digits_in_fraction++;
            else if (state == 4) digits_in_exponent++;
        }
        else if (*cur == '.')
        {
            if (state >= 2) goto done;
            state = 2;
            if (!(cur[1] >= '0' && cur[1] <= '9')) // see CORE-23837
                return MakeErrorMessage("Illegal number format (trailing decimal dot)", str_beg, cur);
        }
        else if (*cur == 'e' || *cur == 'E')
        {
            if (state != 1 && state != 2) goto done;
            state = 3;
        }
        else if (*cur == '+' || *cur == '-')
        {
            if (state != 3) goto done;
            state = 4;
        }
        else
            goto done;

        /* Extra long literal, expand buffer. */
        if (nump == numlim)
        {
            unsigned current_length = nump - numbuf;
            unsigned newbuf_length = (current_length + 1) * 2;
            char *newnumbuf = NULL;
            if (ES_Execution_Context *exec_context = context->GetExecutionContext())
            {
                ES_Suspended_NEWA<char> suspended(newbuf_length);
                exec_context->SuspendedCall(&suspended);
                if (!suspended.storage)
                {
                    if (numbuf != shortbuf)
                        ReleaseBuffer(context, numbuf);
                    context->AbortOutOfMemory();
                }
                newnumbuf = suspended.storage;
                op_memcpy(newnumbuf, numbuf, current_length);
            }
            else if ((newnumbuf = OP_NEWA(char, newbuf_length)) == NULL)
            {
                if (numbuf != shortbuf)
                    ReleaseBuffer(context, numbuf);
                context->AbortOutOfMemory();
            }

            numbuf = newnumbuf;
            nump = numbuf + current_length;
            numlim = numbuf + newbuf_length - 1;        // Leave space for a terminator
        }
        *nump++ = (char)(*cur++);
    }

done:
    if (state == 0 || state == 3 || digits_in_fraction == 0 || state == 4 && digits_in_exponent == 0 || uni_isalnum(*cur))
    {
        if (numbuf != shortbuf)
            ReleaseBuffer(context, numbuf);

        return MakeErrorMessage("Illegal number format", str_beg);
    }

    *nump = 0;
    double result;
    if (ES_Execution_Context *exec_context = context->GetExecutionContext())
        result = ES_Suspended_strtod(exec_context, numbuf).result;
    else
        result = op_strtod(numbuf, NULL);

    pres->SetNumber(negate ? -result : result);
    if (numbuf != shortbuf)
        ReleaseBuffer(context, numbuf);

    return cur;
}

BOOL
ES_JsonBuiltins::parse(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc == 0)
    {
        context->ThrowTypeError("JSON.parse: parse requires at least 1 argument");
        return FALSE;
    }

    ES_Value_Internal* text_val    = &argv[0];
    ES_Value_Internal* reviver_val = (argc >= 2 ? &argv[1] : NULL);

    text_val->ToString(context);

    if (!text_val->IsString())
    {
        context->ThrowTypeError("JSON.parse: argument `text' should be convertible to string");
        return FALSE;
    }

    {
        JString *src = text_val->GetString();
        JsonParser parser(context, StorageZ(context, src), Length(src));
        if (!parser.Parse(return_value))
        {
            ES_CollectorLock gclock(context);
            JString* err_msg = JString::Make(context, "JSON.parse: ");
            Append(context, err_msg, parser.GetErrorMessage());
            context->ThrowSyntaxError(err_msg);
            return FALSE;
        }
    }

    if (reviver_val != NULL)
    {
        if (reviver_val->IsObject() && reviver_val->GetObject()->IsFunctionObject())
        {
            AutoRegisters reg(context, 1);
            ES_Object *holder_obj = ES_Object::Make(context, context->GetGlobalObject()->GetObjectClass());
            reg[0].SetObject(holder_obj);
            holder_obj->PutL(context, context->rt_data->strings[STRING_empty], *return_value);
            ES_Value_Internal key_val(context->rt_data->strings[STRING_empty]);
            if (!WalkObject(context,
                       holder_obj,
                       key_val,
                       static_cast<ES_Function *>(reviver_val->GetObject()),
                       return_value))
                return FALSE;
        }
        /* Per spec, silently ignore non-function values. */
    }

    return TRUE;
}

/* static */ void
ES_JsonBuiltins::ParseL(ES_Context *context, ES_Value_Internal &value, const uni_char *string, unsigned length)
{
    if (length == UINT_MAX)
        length = uni_strlen(string);
    JsonParser parser(context, string, length);
    if (!parser.Parse(&value))
        LEAVE(OpStatus::ERR);
}

BOOL
ES_JsonBuiltins::GetProperties(ES_Context *context, ES_Object *subject, ES_Object *allprops, UINT32 *nallprops)
{
    AutoRegisters auto_regs(context->GetExecutionContext(), 1);
    ES_Value_Internal *regs = auto_regs.GetRegisters(), local_reg;
    if (!regs)
        regs = &local_reg;

    ES_Boxed *prop_names = subject->GetOwnPropertyNamesL(context, /*shadow*/NULL, TRUE, FALSE);
    regs[0].SetBoxed(prop_names);
    UINT32 length = 0;

    if (prop_names)
    {
        switch (prop_names->GCTag())
        {
        case GCTAG_ES_Compact_Indexed_Properties:
        case GCTAG_ES_Sparse_Indexed_Properties:
            {
                ES_Indexed_Properties *names = static_cast<ES_Indexed_Properties *>(prop_names);
                ES_Indexed_Property_Iterator iterator(context, NULL, names);
                unsigned index;
                if (iterator.LowerBound(index, 0))
                    do
                    {
                        ES_Value_Internal prop_name;
                        iterator.GetValue(prop_name); // should always succeed
                        allprops->PutNativeL(context, length++, 0, prop_name);
                    } while (iterator.Next(index));
            }
            break;
        case GCTAG_ES_Class_Compact_Node:
        case GCTAG_ES_Class_Compact_Node_Frozen:
        case GCTAG_ES_Class_Node:
        case GCTAG_ES_Class_Singleton:
            {
                ES_Class *klass = static_cast<ES_Class *>(prop_names);
                for (unsigned index = 0; index < klass->CountProperties(); ++index)
                {
                    ES_Property_Info info;
                    JString *prop_name;
                    klass->Lookup(ES_PropertyIndex(index), prop_name, info);

                    if (info.IsDontEnum())
                        continue;

                    allprops->PutNativeL(context, length++, 0, prop_name);
                }
            }
            break;
        case GCTAG_ES_Object_String:
            // Shouldn't happen
            break;
        }
    }

    *nallprops = length;
    return TRUE;
}


BOOL
ES_JsonBuiltins::WalkObject(ES_Execution_Context *context, ES_Object *holder_obj, ES_Value_Internal &key_val, ES_Function *reviver, ES_Value_Internal *result)
{
    enum
    {
        IDX_PREV_FRAME_ARR,
        IDX_NEXT_FRAME_ARR, //meant to optimize allocations
        IDX_SUBJECT_OBJ,
        IDX_ALLPROPS_ARR,
        IDX_KEY_STR,
        NUSED,
        IDX_KEY_UINT = NUSED,
        IDX_IPROP,
        IDX_NALLPROPS,
        NSLOTS
    };

    AutoRegisters  regs(context, 2);
    ES_Value_Internal& value_val = regs[0];
    regs[1].SetUndefined(); // that one is for top_frame
    ES_Boxed_Array   *top_frame  = NULL;

recurse:

    OP_ASSERT(key_val.IsString() || key_val.IsUInt32());
    GetResult gr;
    if (key_val.IsString())
        gr = holder_obj->GetL(context, key_val.GetString(), value_val);
    else
        gr = holder_obj->GetL(context, key_val.GetNumAsUInt32(), value_val);

    if (!GET_OK(gr))
        value_val.SetUndefined();// probably deleted by previous call to reviver function.

    ES_Object *allprops;
    UINT32 iprop;

    if (value_val.IsObject())
    {
        ES_JSON_PUSH_FRAME(context, top_frame, regs[1]);
        allprops = ES_Object::Make(context, context->GetGlobalObject()->GetEmptyClass());
        top_frame->slots[IDX_SUBJECT_OBJ] = holder_obj;
        top_frame->slots[IDX_ALLPROPS_ARR]= allprops;
        if (key_val.IsUInt32())
        {
            top_frame->uints[IDX_KEY_UINT] = key_val.GetNumAsUInt32();
            top_frame->slots[IDX_KEY_STR]  = NULL;
        }
        else
            top_frame->slots[IDX_KEY_STR] = key_val.GetString();

        holder_obj = value_val.GetObject();
        UINT32 nallprops;
        if (!GetProperties(context, holder_obj, allprops, &nallprops))
            return FALSE;
        for (iprop = 0; iprop < nallprops; iprop++)
        {
            gr = allprops->GetL(context, iprop, key_val);
            if (!GET_OK(gr))
            {
                OP_ASSERT(FALSE);
                continue;
            }
            top_frame->uints[IDX_NALLPROPS] = nallprops;
            top_frame->uints[IDX_IPROP] = iprop;

            goto recurse;
resume:
            allprops  = static_cast<ES_Object *>(top_frame->slots[IDX_ALLPROPS_ARR]);
            nallprops = top_frame->uints[IDX_NALLPROPS];
            iprop     = top_frame->uints[IDX_IPROP];
            gr = allprops->GetL(context, iprop, key_val);
            if (!GET_OK(gr))
            {
                OP_ASSERT(FALSE);
                continue;
            }

            if (value_val.IsUndefined())
            {
                DeleteResult deleted;
                UINT32 result;
                if (key_val.IsUInt32())
                    deleted = holder_obj->DeleteL(context, key_val.GetNumAsUInt32(), result);
                else
                    deleted = holder_obj->DeleteL(context, key_val.GetString(), result);
                if (!deleted)
                    return FALSE;
            }
            else
            {
                PutResult pr;
                if (key_val.IsUInt32())
                    pr = holder_obj->PutL(context, key_val.GetNumAsUInt32(), value_val);
                else
                    pr = holder_obj->PutL(context, key_val.GetString(), value_val);
                if (!PUT_OK(pr))
                    return FALSE;
            }
        }

        value_val.SetObject(holder_obj);
        holder_obj = static_cast<ES_Object *>(top_frame->slots[IDX_SUBJECT_OBJ]);
        if (top_frame->slots[IDX_KEY_STR] == NULL)
            key_val.SetUInt32(top_frame->uints[IDX_KEY_UINT]);
        else
            key_val.SetString(static_cast<JString *>(top_frame->slots[IDX_KEY_STR]));
         // pop frame:
         top_frame = static_cast<ES_Boxed_Array *>(top_frame->slots[IDX_PREV_FRAME_ARR]);
    }

    ES_Value_Internal *registers = context->SetupFunctionCall(reviver, 2);

    registers[0].SetObject(holder_obj);
    registers[1].SetObject(reviver);
    registers[2] = key_val;
    registers[3] = value_val;

    if (!context->CallFunction(registers, 2, &value_val))
        return FALSE;

    if (top_frame)
        goto resume;
    else
    {
        *result = value_val;
        return TRUE;
    }
}
