/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/kernel/es_string.h"
#include "modules/ecmascript/carakan/src/kernel/es_string_inlines.h"
#include "modules/ecmascript/carakan/src/builtins/es_string_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_string_object.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/ecmascript/carakan/src/object/es_array_object.h"
#include "modules/ecmascript/carakan/src/builtins/es_regexp_builtins.h"
#include "modules/ecmascript/carakan/src/compiler/es_lexer.h"
#include "modules/regexp/include/regexp_advanced_api.h"

#ifndef _STANDALONE
# include "modules/pi/OpLocale.h"
# include "modules/pi/pi_module.h"
#endif

#define ES_THIS_STRING() if (!ProcessThis(context, argv[-2])) return FALSE; JString *this_string = argv[-2].GetString();
#define ES_CHECK_CALLABLE(index, msg) do { if (index >= argc || !argv[index].IsObject() || !argv[index].GetObject(context)->IsFunctionObject()) context->ThrowTypeError(msg); } while (0)

#define TYPE_ERROR_TO_STRING "String.prototype.toString: this is not a String object"
#define TYPE_ERROR_VALUE_OF "String.prototype.valueOf: this is not a String object"

/* static */ BOOL
ES_StringBuiltins::constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *result_string;

    if (argc == 0)
        result_string = context->rt_data->strings[STRING_empty];
    else
    {
        if (!argv[0].ToString(context))
            return FALSE;
        result_string = argv[0].GetString();
    }

    return_value->SetString(result_string);
    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *value_string;

    if (argc == 0)
        value_string = context->rt_data->strings[STRING_empty];
    else
    {
        if (!argv[0].ToString(context))
            return FALSE;
        value_string = argv[0].GetString();
    }

    return_value->SetObject(ES_String_Object::Make(context, ES_GET_GLOBAL_OBJECT(), value_string));
    return TRUE;
}

static BOOL
ToStringType(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, BOOL to_string)
{
    JString *this_string;

    ES_Value_Internal this_arg = argv[-2];

    if (this_arg.IsString())
        this_string = this_arg.GetString();
    else if (this_arg.IsObject() && this_arg.GetObject(context)->IsStringObject())
        this_string = static_cast<ES_String_Object *>(this_arg.GetObject(context))->GetValue();
    else
    {
        context->ThrowTypeError(to_string ? TYPE_ERROR_TO_STRING : TYPE_ERROR_VALUE_OF);
        return FALSE;
    }

    return_value->SetString(this_string);
    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ToStringType(context, argc, argv, return_value, TRUE);
}

/* static */ BOOL
ES_StringBuiltins::valueOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ToStringType(context, argc, argv, return_value, FALSE);
}

/* static */ BOOL
ES_StringBuiltins::toLowerCase(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    return_value->SetString(ConvertCase(context, this_string, TRUE));

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::toLocaleLowerCase(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    // TODO: implement this function correctly

    ES_THIS_STRING();

    return_value->SetString(ConvertCase(context, this_string, TRUE));

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::toUpperCase(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    return_value->SetString(ConvertCase(context, this_string, FALSE));

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::toLocaleUpperCase(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    // TODO: implement this function correctly

    ES_THIS_STRING();

    return_value->SetString(ConvertCase(context, this_string, FALSE));

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::charCodeAt(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    int idx = 0;
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;
        idx = argv[0].GetNumAsBoundedInt32();
    }

    if (idx >= 0 && idx < (int)Length(this_string))
    {
        uni_char uc = Element(this_string, (int)idx);
        return_value->SetUInt32((UINT32)uc);
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::charAt(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    int idx = 0;
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;
        idx = argv[0].GetNumAsBoundedInt32();
    }
    if (idx >= 0 && idx < (int)Length(this_string))
    {
        uni_char uc = Element(this_string, (int)idx);
        return_value->SetString(JString::Make(context, &uc, 1));
    }
    else
        return_value->SetString(JString::Make(context, ""));

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::substring(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    size_t length = Length(this_string);

    int startidx = 0;
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;
        startidx = argv[0].GetNumAsBoundedInt32();
    }
    int endidx = length;
    if (argc >= 2 && !argv[1].IsUndefined())
    {
        if (!argv[1].ToNumber(context))
            return FALSE;
        endidx = argv[1].GetNumAsBoundedInt32();
    }
    startidx = es_mini(es_maxi(startidx, 0), length);
    endidx = es_mini(es_maxi(endidx, 0), length);
    int from = es_mini(startidx, endidx);
    int to = es_maxi(startidx, endidx);
    return_value->SetString(JString::Make(context, this_string, from, to - from));

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::substr(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    size_t length = Length(this_string);

    int startidx = 0;
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;
        startidx = argv[0].GetNumAsBoundedInt32();
        if (startidx < 0)
            startidx = es_maxi(0, startidx + length);
    }
    int slen = length - startidx;
    if (argc >= 2 && !argv[1].IsUndefined())
    {
        if (!argv[1].ToNumber(context))
            return FALSE;
        slen = es_mini(slen, es_maxi(0, argv[1].GetNumAsBoundedInt32()));
    }
    if (slen <= 0)
        return_value->SetString(context->rt_data->strings[STRING_empty]);
    else
        return_value->SetString(JString::Make(context, this_string, startidx, slen));

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::slice(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    size_t length = Length(this_string);

    int startidx = 0;
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;
        startidx = argv[0].GetNumAsBoundedInt32();
    }
    int endidx = length;
    if (argc >= 2 && !argv[1].IsUndefined())
    {
        if (!argv[1].ToNumber(context))
            return FALSE;
        endidx = argv[1].GetNumAsBoundedInt32();
    }
    startidx = startidx < 0 ? es_maxi((int)length + startidx, 0) : es_mini(startidx, length);
    endidx = endidx < 0 ? es_maxi((int)length + endidx, 0) : es_mini(endidx, length);
    int slicelen = es_maxi(endidx - startidx, 0);
    return_value->SetString(JString::Make(context, this_string, startidx, slicelen));

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::concat(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();
    GC_STACK_ANCHOR(context, this_string);
    JString *ret = Share(context, this_string);
    ES_CollectorLock gclock(context);
    for (unsigned i = 0; i < argc; i++)
    {
        if (!argv[i].ToString(context))
            return FALSE;
        Append(context, ret, argv[i].GetString());
    }
    return_value->SetString(Finalize(context, ret));
    return TRUE;
}

static int
ES_IndexOf(const uni_char *haystack, unsigned haystack_length, const uni_char *needle, unsigned needle_length, unsigned offset)
{
    haystack_length -= needle_length - 1;

    for (unsigned i = 0, j; i < haystack_length; ++i)
    {
        for (j = 0; j < needle_length; ++j)
             if (haystack[i + j] != needle[j])
                goto next;

        return (i + offset);
next:
        ;
    }

    return -1;
}

static BOOL
ES_MatchInSegmented(JSegmentIterator iter, unsigned index, const uni_char *needle, unsigned needle_length)
{
    const uni_char *haystack = iter.GetBase()->storage + iter.GetOffset();
    unsigned haystack_length = iter.GetLength();

    for (unsigned i = 0, j = index; i < needle_length; ++i)
    {
        if (haystack[j] != needle[i])
            return FALSE;

        if (++j == haystack_length)
        {
            iter.Next();

            haystack = iter.GetBase()->storage + iter.GetOffset();
            haystack_length = iter.GetLength();

            j = 0;
        }
    }

    return TRUE;
}

static int
ES_IndexOfInSegmented(JString *haystack, unsigned index, const uni_char *needle, unsigned needle_length, unsigned offset)
{
    JSegmentIterator iter(haystack);

    unsigned haystack_offset = index;
    unsigned haystack_length = Length(haystack) - index - (needle_length - 1);

    while (iter.Next() && haystack_offset >= iter.GetLength())
        haystack_offset -= iter.GetLength();

    for (unsigned i = 0, j = haystack_offset; i < haystack_length; ++i)
    {
        if (ES_MatchInSegmented(iter, j, needle, needle_length))
            return (i + offset);

        if (++j == iter.GetLength())
        {
            iter.Next();
            j = haystack_offset = 0;
        }
    }

    return -1;
}

static int
ES_IndexOf(JString *haystack, unsigned index, const uni_char *needle, unsigned needle_length)
{
    unsigned haystack_length = Length(haystack) - index;

    if (haystack_length >= needle_length)
        if (!IsSegmented(haystack))
            return ES_IndexOf(Storage(NULL, haystack) + index, haystack_length, needle, needle_length, index);
        else
            return ES_IndexOfInSegmented(haystack, index, needle, needle_length, index);
    else
        return -1;
}

/* static */ BOOL
ES_StringBuiltins::indexOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    int length = Length(this_string);

    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;

        JString *search_string = argv[0].GetString();

        int pos = 0;
        if (argc >= 2)
        {
            if (!argv[1].ToNumber(context))
                return FALSE;
            pos = argv[1].GetNumAsBoundedInt32();
            pos = es_mini(es_maxi(pos, 0), length);
        }

        return_value->SetInt32(ES_IndexOf(this_string, pos, Storage(context, search_string), Length(search_string)));
    }
    else
        return_value->SetInt32(-1);

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::lastIndexOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    int res = -1;
    int length = Length(this_string);

    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;

        JString* search_string = argv[0].GetString();
        int searchlen = Length(search_string);

        int pos = INT_MAX;
        if (argc >= 2)
        {
            if (!argv[1].ToNumber(context))
                return FALSE;
            pos = argv[1].GetNumAsBoundedInt32(INT_MAX);
        }
        pos = es_mini(es_maxi(pos, 0), length - searchlen);

        uni_char* src = Storage(context, this_string);
        uni_char* needle = Storage(context, search_string);

        for (int i = pos; i >= 0; i--)
        {
            int curr_pos = 0;
            while (curr_pos < searchlen)
            {
                OP_ASSERT(i + curr_pos < length);
                if (src[i + curr_pos] != needle[curr_pos])
                    break;
                curr_pos++;
            }
            if (curr_pos == searchlen)
            {
                res = i;
                break;
            }
        }
    }

    return_value->SetInt32(res);

    return TRUE;
}

#ifndef _STANDALONE
class ES_SuspendedLocaleCompare
    : public ES_SuspendedCall
{
public:
    ES_SuspendedLocaleCompare(ES_Execution_Context *context, JString *str1, JString *str2)
        : result(-1),
          status(OpStatus::OK),
          str1(str1),
          str2(str2)
    {
        context->SuspendedCall(this);
    }

    int result;
    OP_STATUS status;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        TRAP(status, result = g_oplocale->CompareStringsL(StorageZ(context, str1), StorageZ(context, str2)));
    }

    JString *str1;
    JString *str2;
};
#endif // _STANDALONE

/* static */ BOOL
ES_StringBuiltins::localeCompare(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    int res = 0;

    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;
        JString* compare_string = argv[0].GetString();
#ifndef _STANDALONE
        ES_SuspendedLocaleCompare lc_compare(context, this_string, compare_string);
        LEAVE_IF_ERROR(lc_compare.status);
        res = lc_compare.result;
#else
        res = Compare(context, this_string, compare_string);
#endif // _STANDALONE
    }

    return_value->SetInt32(res);

    return TRUE;
}

typedef ES_Boxed_Array ReplacementItem;
/** ReplacementItem is a struct with 3 members:
  { JStringStorage *base; unsigned offset, length; }
    We always allocate an array of these, but since we want to allocate them on the ES heap,
    things get a bit hairy, due to the first member being a ES_Boxed while second and third are scalar.
    ES_Boxed_Array is used with (nslots : nused) ratio 3:1; The first third of the slots stores the base(s)
    second 2/3 stores offset(s) and length(s). So i-th items's base is stored at items->slots[i] while its
    offset & length are stored at items->uints[items->nused + 2*i] and items->uints[items->nused + 2*i+1]
    That's the sophisticated part of it.
 */

void SetFinalItem(ReplacementItem *items, unsigned item_idx, JStringStorage *new_base, unsigned new_offset, unsigned new_length)
{
    OP_ASSERT(!IsSegmented(new_base));
    OP_ASSERT(new_offset < INT_MAX);
    OP_ASSERT(item_idx < items->nused);
    OP_ASSERT(new_length);

    items->slots[item_idx] = new_base;
    items->uints[items->nused + 2*item_idx] = new_offset;
    items->uints[items->nused + 2*item_idx+1] = new_length;
}

void SetFinalItem(ReplacementItem *items, unsigned item_idx, ES_Context *context, JString *new_base, unsigned new_offset, unsigned new_length)
{
    OP_ASSERT(new_offset < INT_MAX);
    OP_ASSERT(item_idx < items->nused);
    OP_ASSERT(new_length);

    items->slots[item_idx] = StorageSegment(context, new_base, new_offset, new_length);
    items->uints[items->nused + 2*item_idx] = new_offset;
    items->uints[items->nused + 2*item_idx+1] = new_length;
}


static void
AllocateItemsSlow(ES_Execution_Context *context, ReplacementItem *&items, unsigned &used, unsigned &allocated, unsigned additional, ES_Value_Internal& save_reg)
{
    unsigned new_allocated = allocated < 8 ? 8 : allocated << 2;
    while (!(used + additional < new_allocated))
        new_allocated <<= 1;
    ReplacementItem *new_items = ReplacementItem::Make(context, 3 * new_allocated, new_allocated);

    if (items)
    {
        op_memcpy(new_items->slots, items->slots, used * sizeof(items->slots[0]));
        op_memcpy(new_items->uints + new_items->nused, items->uints + items->nused, 2 * used * sizeof(items->slots[0]));
    }

    save_reg.SetBoxed(items = new_items);
    allocated = new_allocated;
}

static inline void
AllocateItems(ES_Execution_Context *context, ReplacementItem *&items, unsigned &used, unsigned &allocated, unsigned additional, ES_Value_Internal& save_reg)
{
    if (used + additional > allocated)
        AllocateItemsSlow(context, items, used, allocated, additional, save_reg);
}

/** Class ReplaceValueGenerator is a no-additional-member-vars subclass of ES_Boxed_Array
    which is there only to add functions to operate on the data kept in the boxed array.
    Later when we need a generaor, we actually allocate a boxed array and cast-to/view-it-as a
    generator in order to get to this generator defined functionallity.
    All this magic is in order to have ReplaceValueGenerator es-heap allocated and GC-proof.
 */
class ReplaceValueGenerator : public ES_Boxed_Array
{
public:
    struct Part
    {
        unsigned literal_offset;
        unsigned literal_length_or_match_index;
    };

    enum { IDX_parts, IDX_replacement, NUSED, IDX_parts_count = NUSED, NSLOTS };

    Part* GetParts() const
    {
        if (slots[IDX_parts] != NULL)
            return reinterpret_cast<Part*>( reinterpret_cast<ES_Box*>(slots[IDX_parts])->Unbox() );
        return NULL;
    }

    unsigned  PartsCount() const { return uints[IDX_parts_count]; }
    void      IncPartsCount() { uints[IDX_parts_count]++; }
    void      SetPartsCount(unsigned n) { uints[IDX_parts_count] = 0; }

    JString* replacement() const { return reinterpret_cast<JString*>(slots[IDX_replacement]); }

    void AddPart(unsigned literal_offset, unsigned literal_length_or_match_index);
    void AllocateParts(ES_Context *context);

    static ReplaceValueGenerator *Make(ES_Execution_Context *context, JString *replacement, unsigned ncaptures);

    void Measure(unsigned &length, unsigned &items, JString *source, const RegExpMatch *matches);
    void Generate(ES_Context *context, ReplacementItem *items, unsigned item_idx, JString *source, const RegExpMatch *matches);

    static inline void CreateGeneratorObject(ES_Execution_Context *context, JString *replacement, BOOL &created_boxed_array, ES_Boxed *&boxed_array);
};

void
ReplaceValueGenerator::AddPart(unsigned literal_offset, unsigned literal_length_or_match_index)
{
    OP_ASSERT(literal_offset == UINT_MAX || literal_offset < INT_MAX);

    if (GetParts())
    {
        Part &part = GetParts()[PartsCount()];
        part.literal_offset = literal_offset;
        part.literal_length_or_match_index = literal_length_or_match_index;
    }
    IncPartsCount();
}

void
ReplaceValueGenerator::AllocateParts(ES_Context *context)
{
    slots[IDX_parts] = ES_Box::Make(context, sizeof(Part)*PartsCount());
    SetPartsCount(0);
}

/*static*/ inline void
ReplaceValueGenerator::CreateGeneratorObject(ES_Execution_Context *context, JString *replacement, BOOL &created_boxed_array, ES_Boxed *&boxed_array)
{
    if (!created_boxed_array)
    {
        boxed_array = ES_Boxed_Array::Make(context, NSLOTS, NUSED);
        static_cast<ES_Boxed_Array*>(boxed_array)->uints[IDX_parts_count] = 0;
        static_cast<ES_Boxed_Array*>(boxed_array)->slots[IDX_replacement] = replacement;
        created_boxed_array = TRUE;
    }
}

/* static */ ReplaceValueGenerator *
ReplaceValueGenerator::Make(ES_Execution_Context *context, JString *replacement, unsigned ncaptures)
{
    AutoRegisters regs(context, 1);

    regs[0].SetBoxed(NULL);
    ES_Boxed *&barr = regs[0].GetBoxed_ref();
    BOOL created_boxed_array = FALSE;

    // Note the trick on next line: we're casting the boxed array to something it isn't
    // in order to get to the methods defined in its subclass ReplaceValueIterator;
    // As long as the subclass does not define any member vars, this will work fine:
#define generator static_cast<ReplaceValueGenerator*>(static_cast<ES_Boxed_Array*>( barr ))

    const uni_char *string = Storage(context, replacement);
    unsigned string_length = Length(replacement);

    while (TRUE)
    {
        const uni_char *ptr = string, *ptr_end = ptr + string_length, *literal_start = string;

        while (ptr != ptr_end)
            if (*ptr++ == '$')
                if (ptr != ptr_end)
                {
                    switch (*ptr)
                    {
                    case '$':
                        CreateGeneratorObject(context, replacement, created_boxed_array, barr);
                        generator->AddPart(literal_start - string, ptr - literal_start);
                        literal_start = ++ptr;
                        break;

                    case '&':
                    case '`':
                    case '\'':
                        CreateGeneratorObject(context, replacement, created_boxed_array, barr);
                        if (literal_start != ptr - 1)
                            generator->AddPart(literal_start - string, ptr - literal_start - 1);
                        generator->AddPart(~0u, *ptr == '&' ? 0 : *ptr == '`' ? UINT_MAX - 1 : UINT_MAX);
                        literal_start = ++ptr;
                        break;

                    default:
                        const uni_char *stored_ptr = ptr;
                        if (*ptr >= '0' && *ptr <= '9')
                        {
                            unsigned capture_index = *ptr - '0';
                            if (capture_index <= ncaptures)
                            {
                                ++ptr;
                                if (ptr != ptr_end && *ptr >= '0' && *ptr <= '9' && capture_index * 10 + *ptr - '0' <= ncaptures)
                                {
                                    capture_index = capture_index * 10 + *ptr - '0';
                                    ++ptr;
                                }

                                if (capture_index > 0)
                                {
                                    CreateGeneratorObject(context, replacement, created_boxed_array, barr);
                                    if (literal_start != stored_ptr - 1)
                                        generator->AddPart(literal_start - string, (stored_ptr - 1) - literal_start);
                                    generator->AddPart(~0u, capture_index);

                                    literal_start = ptr;
                                }
                                else
                                    ptr = stored_ptr;
                            }
                        }
                        else
                            ptr = stored_ptr;
                    }
                }

        if (literal_start != ptr)
        {
            CreateGeneratorObject(context, replacement, created_boxed_array, barr);
            generator->AddPart(literal_start - string, ptr - literal_start);
        }

        if (created_boxed_array && generator->GetParts())
            return generator;
        else if (literal_start == string)
            return NULL;
        else
        {
            CreateGeneratorObject(context, replacement, created_boxed_array, barr);
            generator->AllocateParts(context);
        }
    }

#undef generator
}

void
ReplaceValueGenerator::Measure(unsigned &length, unsigned &items, JString *source, const RegExpMatch *matches)
{
    Part *part = GetParts();
    for (unsigned index = 0; index < PartsCount(); ++index, ++part)
        if (part->literal_offset + 1 != 0)
        {
            length += part->literal_length_or_match_index;
            ++items;
        }
        else if (part->literal_length_or_match_index == UINT_MAX - 1)
        {
            if (matches[0].start > 0)
            {
                length += matches[0].start;
                ++items;
            }
        }
        else if (part->literal_length_or_match_index == UINT_MAX)
        {
            if (matches[0].start + matches[0].length < Length(source))
            {
                length += Length(source) - matches[0].start - matches[0].length;
                ++items;
            }
        }
        else
        {
            const RegExpMatch &match = matches[part->literal_length_or_match_index];

            unsigned part_length = match.length;
            if (match.length != UINT_MAX && part_length != 0)
            {
                length += part_length;
                ++items;
            }
        }
}

void
ReplaceValueGenerator::Generate(ES_Context *context, ReplacementItem *items, unsigned item_idx, JString *source, const RegExpMatch *matches)
{
    Part *part = GetParts();
    for (unsigned index = 0; index < PartsCount(); ++index, ++part)
        if (part->literal_offset + 1 != 0)
        {
            SetFinalItem(items, item_idx++, context, replacement(), part->literal_offset, part->literal_length_or_match_index);
        }
        else if (part->literal_length_or_match_index == UINT_MAX - 1)
        {
            if (matches[0].start > 0)
                SetFinalItem(items, item_idx++, context, source, 0, matches[0].start);
        }
        else if (part->literal_length_or_match_index == UINT_MAX)
        {
            if (matches[0].start + matches[0].length < Length(source))
                SetFinalItem(items, item_idx++, context, source, matches[0].start + matches[0].length, Length(source) - matches[0].start - matches[0].length);
        }
        else
        {
            const RegExpMatch &match = matches[part->literal_length_or_match_index];

            unsigned part_length = match.length;
            if (match.length != UINT_MAX && part_length != 0)
                SetFinalItem(items, item_idx++, context, source, match.start, part_length);
        }
}

static BOOL
DetectLexicalMemberLookupFunction(ES_Execution_Context *context, ES_Object *function, ES_Object *&lookup_object)
{
    if (!function->IsHostObject())
        if (ES_FunctionCode *code = static_cast<ES_Function *>(function)->GetFunctionCode())
            if (code->data->codewords_count < 64)
            {
                code->data->FindInstructionOffsets(context);

                if (code->data->instruction_count == 3)
                {
                    ES_CodeWord *codewords = code->data->codewords;
                    unsigned *instruction_offsets = code->data->instruction_offsets;

                    if (codewords[0].instruction == ESI_GET_LEXICAL &&
                        codewords[instruction_offsets[1]].instruction == ESI_GET &&
                        codewords[instruction_offsets[1] + 2].index == codewords[1].index &&
                        codewords[instruction_offsets[1] + 3].index == 2 &&
                        codewords[instruction_offsets[2]].instruction == ESI_RETURN_VALUE &&
                        codewords[instruction_offsets[2] + 1].index == codewords[instruction_offsets[1] + 1].index)
                    {
                        ES_Value_Internal value;
                        static_cast<ES_Function *>(function)->GetLexical(value, codewords[2].index, ES_PropertyIndex(codewords[3].index));
                        if (value.IsObject())
                            lookup_object = value.GetObject(context);
                        return TRUE;
                    }
                }
            }

    return FALSE;
}

static BOOL
Unpack(ES_Execution_Context *context, ES_Value_Internal *result, ES_Value_Internal *value, JString *base, ES_RegExp_Object *regexp, ES_Object *lookup_object)
{
    enum { SEGMENT_LENGTH = 16384 };

    ES_Boxed_Array *intermediate;

    result->SetBoxed(intermediate = ES_Boxed_Array::Make(context, 16, 0));

    JStringStorage *current = NULL;
    unsigned offset = SEGMENT_LENGTH;

    const uni_char *source = Storage(context, base);
    uni_char *write = NULL;
    unsigned length = Length(base), last_index = 0;

    ES_Property_Value_Table *values = lookup_object->GetHashTable();

    while (TRUE)
    {
        RegExpMatch *matches = regexp->Exec(context, base, last_index);

        const uni_char *item = source + last_index;
        unsigned item_length;

        if (matches)
            item_length = matches[0].start - last_index;
        else
            item_length = length - last_index;

        while (item_length)
        {
            if (offset == SEGMENT_LENGTH)
            {
                if (intermediate->nused == intermediate->nslots)
                    result->SetBoxed(intermediate = ES_Boxed_Array::Grow(context, intermediate));

                current = JStringStorage::Make(context, (const char *) NULL, SEGMENT_LENGTH + 1, SEGMENT_LENGTH);
                write = current->storage;
                offset = 0;

                intermediate->slots[intermediate->nused++] = current;
            }

            unsigned copy = MIN(item_length, SEGMENT_LENGTH - offset);

            op_memcpy(write + offset, item, UNICODE_SIZE(copy));

            offset += copy;
            item += copy;
            item_length -= copy;
        }

        if (matches)
        {
            if (IsEmpty(matches[0]))
                last_index = matches[0].start + 1;
            else
                last_index = matches[0].start + matches[0].length;

            GetResult res;
            JString *string;

            const uni_char *name = source + matches[0].start;
            unsigned name_length = matches[0].length;

            unsigned index;
            if (name_length && *name >= '0' && *name <= '9' && convertindex(name, name_length, index))
                res = lookup_object->GetL(context, index, *value);
            else
            {
                JTemporaryString temporary(name, name_length);
                ES_Value_Internal *location;
                ES_Property_Info info;

                if (values && values->FindLocation(temporary, info, location) && location->IsString())
                {
                    string = location->GetString();
                    goto handle_string;
                }

                res = lookup_object->GetL(context, temporary, *value);
            }

            if (GET_OK(res))
            {
                if (!value->ToString(context))
                    return FALSE;

                string = value->GetString();
            }
            else if (GET_NOT_FOUND(res))
                string = context->rt_data->strings[STRING_undefined];
            else
                return FALSE;

        handle_string:
            item = Storage(context, string);
            item_length = Length(string);

            while (item_length)
            {
                if (offset == SEGMENT_LENGTH)
                {
                    if (intermediate->nused == intermediate->nslots)
                        result->SetBoxed(intermediate = ES_Boxed_Array::Grow(context, intermediate));

                    current = JStringStorage::Make(context, (const char *) NULL, SEGMENT_LENGTH + 1, SEGMENT_LENGTH);
                    write = current->storage;
                    offset = 0;

                    intermediate->slots[intermediate->nused++] = current;
                }

                unsigned copy = MIN(item_length, SEGMENT_LENGTH - offset);

                op_memcpy(write + offset, item, UNICODE_SIZE(copy));

                offset += copy;
                item += copy;
                item_length -= copy;
            }
        }
        else
            break;
    }

    unsigned nsegments = intermediate->nused;
    JStringSegmented *segmented;

    if (nsegments)
    {
        result->SetBoxed(segmented = JStringSegmented::Make(context, nsegments));

        op_memcpy(segmented->Bases(), intermediate->slots, nsegments * sizeof(ES_Boxed *));
        op_memset(segmented->Offsets(), 0, nsegments * sizeof(unsigned));

        for (unsigned index = 0; index < nsegments - 1; ++index)
            segmented->Lengths()[index] = SEGMENT_LENGTH;

        segmented->Lengths()[nsegments - 1] = offset;

        context->heap->Free(intermediate);

        result->SetString(JString::Make(context, segmented, (nsegments - 1) * SEGMENT_LENGTH + offset));
    }
    else
        result->SetString(context->rt_data->strings[STRING_empty]);

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::replace(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    ES_RegExp_Object *regexp_object = NULL;
    RegExp *regexp = NULL;
    JString *needle = NULL;
    unsigned ncaptures;
    BOOL global;
    RegExpMatch *matches, local_match;

    if (argc >= 1 && argv[0].IsObject() && argv[0].GetObject(context)->IsRegExpObject())
    {
        regexp_object = static_cast<ES_RegExp_Object *>(argv[0].GetObject(context));
        regexp = regexp_object->GetValue();
        ncaptures = regexp->GetNumberOfCaptures();
        global = (regexp_object->GetFlagBits() & REGEXP_FLAG_GLOBAL) != 0;
        matches = regexp_object->GetMatchArray();
    }
    else
    {
        if (!argv[0].ToString(context))
            return FALSE;

        needle = argv[0].GetString();
        ncaptures = 0;
        global = FALSE;
        matches = &local_match;
    }

    ES_Object *function = NULL;
    ES_Object *lookup_object = NULL;
    ReplaceValueGenerator *generator = NULL;
    JString *replacement = NULL, *lookup_name = NULL;

    if (argc >= 2 && argv[1].IsObject() && argv[1].GetObject(context)->IsFunctionObject())
    {
        function = argv[1].GetObject(context);

        DetectLexicalMemberLookupFunction(context, function, lookup_object);
    }
    else
    {
        if (argc >= 2)
        {
            if (!argv[1].ToString(context))
                return FALSE;

            replacement = argv[1].GetString();
            generator = ReplaceValueGenerator::Make(context, replacement, ncaptures);
        }
        else
            replacement = context->rt_data->strings[STRING_undefined];
    }

    if (regexp_object && lookup_object)
        return Unpack(context, return_value, &argv[-1], this_string, regexp_object, lookup_object);

    enum {
        ridx_generator,
        ridx_lookup_name = ridx_generator,
        ridx_final_items,
        ridx_segmented,
        ridx_scratch,
        ridx_count
    };

    AutoRegisters regs(context, ridx_count);

    if (lookup_object)
        regs[ridx_lookup_name].SetBoxed(lookup_name = JString::MakeTemporary(context));

    if (generator)
        regs[ridx_generator].SetBoxed(generator);

    const uni_char *this_storage = !IsSegmented(this_string) || !needle || Length(needle) > 1 ? Storage(context, this_string) : NULL;
    unsigned this_length = Length(this_string);

    ReplacementItem *final_items = NULL;
    unsigned final_items_used = 0, final_items_allocated = 0, final_length = 0, previous_match_end = 0, last_index = 0;

    BOOL is_segmented_search = FALSE, no_matches = TRUE;
    unsigned segment_index = 0, segment_offset = 0;

    JSegmentIterator iter(this_string);

    do
    {
        if (needle)
        {
            const uni_char *needle_storage = Storage(context, needle), *ptr = this_storage, *ptr_end = 0;
            unsigned needle_length = Length(needle);
            unsigned segment_match_start = 0;

            if (needle_length == 1)
            {
                uni_char ch = needle_storage[0];

                if (IsSegmented(this_string))
                {
                    is_segmented_search = TRUE;

                    unsigned sindex = 0;

                    while (iter.Next())
                    {
                        const uni_char *ptr_start = ptr = (iter.GetBase())->storage + iter.GetOffset();
                        ptr_end = ptr + iter.GetLength();

                        while (ptr != ptr_end && *ptr != ch) ++ptr;

                        if (ptr != ptr_start)
                        {
                            AllocateItems(context, final_items, final_items_used, final_items_allocated, 1, regs[ridx_final_items]);
                            SetFinalItem(final_items, final_items_used++, iter.GetBase(), iter.GetOffset(), ptr - ptr_start);
                            final_length += ptr - ptr_start;
                        }

                        if (ptr != ptr_end)
                        {
                            segment_index = sindex;
                            segment_offset = ptr - ptr_start + 1;
                            segment_match_start += ptr - ptr_start;

                            break;
                        }

                        segment_match_start += iter.GetLength();
                        ++sindex;
                    }
                }
                else
                {
                    ptr_end = this_storage + this_length;
                    while (ptr != ptr_end && *ptr != ch)
                        ++ptr;
                }
            }
            else
            {
                if (needle_length > this_length)
                    goto return_this;

                if (needle_length > 0)
                {
                    ptr_end = this_storage + this_length - needle_length + 1;
                    uni_char ch = needle_storage[0];
                    while (ptr != ptr_end)
                    {
                        if (*ptr == ch)
                        {
                            unsigned i;
                            for (i = needle_length; i > 0;)
                                if (ptr[i - 1] == needle_storage[i - 1])
                                    --i;
                                else
                                    break;
                            if (i == 0)
                                break;
                        }
                        ++ptr;
                    }
                }
            }

            if (ptr == ptr_end)
                goto return_this;

            local_match.start = is_segmented_search ? segment_match_start : (ptr - this_storage);
            local_match.length = needle_length;
        }
        else
        {
            matches = regexp_object->Exec(context, this_string, last_index);

            if (!matches)
                break;

            if (IsEmpty(matches[0]))
                last_index = matches[0].start + 1;
            else
                last_index = matches[0].start + matches[0].length;

            argv[0].GetObject(context)->PutCachedAtIndex(ES_PropertyIndex(ES_RegExp_Object::LASTINDEX), last_index);
        }

        no_matches = FALSE;

        if (!is_segmented_search)
        {
            if (previous_match_end < matches[0].start)
            {
                AllocateItems(context, final_items, final_items_used, final_items_allocated, 1, regs[ridx_final_items]);
                unsigned length = matches[0].start - previous_match_end;
                SetFinalItem(final_items, final_items_used++, context, this_string, previous_match_end, length);
                final_length += length;
            }

            previous_match_end = matches[0].start + matches[0].length;
        }

        if (lookup_object)
        {
            unsigned length = matches[0].length, index;

            GetResult result;
            ES_Value_Internal &value = regs[ridx_scratch];

            if (convertindex(this_storage + matches[0].start, length, index))
                result = lookup_object->GetL(context, index, value);
            else
            {
                lookup_name->SetTemporary(this_string, matches[0].start, length);
                result = lookup_object->GetL(context, lookup_name, value);
            }

            JString *string;

            if (GET_OK(result))
            {
                if (!value.ToString(context))
                    goto return_false;

                string = value.GetString();
            }
            else if (GET_NOT_FOUND(result))
                string = context->rt_data->strings[STRING_undefined];
            else
                goto return_false;

            if (Length(string) > 0)
            {
                AllocateItems(context, final_items, final_items_used, final_items_allocated, 1, regs[ridx_final_items]);
                SetFinalItem(final_items, final_items_used++, context, string, 0, Length(string));
                final_length += Length(string);
            }
        }
        else if (function)
        {
            ES_Value_Internal *registers = context->SetupFunctionCall(function, ncaptures + 3);

            registers[0].SetUndefined();
            registers[1].SetObject(function);
            registers[2].SetString(MatchSubString(context, this_string, matches[0]));

            for (unsigned index = 0; index < ncaptures; ++index)
                if (matches[1 + index].length == UINT_MAX)
                    registers[3 + index].SetUndefined();
                else
                    registers[3 + index].SetString(MatchSubString(context, this_string, matches[1 + index]));

            registers[3 + ncaptures].SetNumber(matches[0].start);
            registers[4 + ncaptures].SetString(this_string);

            ES_Value_Internal &returned = regs[ridx_scratch];

            if (!context->CallFunction(registers, ncaptures + 3, &returned))
                goto return_false;

            if (!returned.ToString(context))
                goto return_false;

            JString *returned_string = returned.GetString();
            if (Length(returned_string) > 0)
            {
                AllocateItems(context, final_items, final_items_used, final_items_allocated, 1, regs[ridx_final_items]);
                SetFinalItem(final_items, final_items_used++, context, returned_string, 0, Length(returned_string));
                final_length += Length(returned_string);
            }
        }
        else if (generator)
        {
            unsigned items = 0;
            generator->Measure(final_length, items, this_string, matches);
            AllocateItems(context, final_items, final_items_used, final_items_allocated, items, regs[ridx_final_items]);
            generator->Generate(context, final_items, final_items_used, this_string, matches);
            final_items_used += items;
        }
        else if (Length(replacement) != 0)
        {
            AllocateItems(context, final_items, final_items_used, final_items_allocated, 1, regs[ridx_final_items]);
            SetFinalItem(final_items, final_items_used++, context, replacement, 0, Length(replacement));
            final_length += Length(replacement);
        }
    }
    while (global && last_index <= this_length);

    if (global)
        argv[0].GetObject(context)->PutCachedAtIndex(ES_PropertyIndex(ES_RegExp_Object::LASTINDEX), 0);

    if (no_matches)
        goto return_this;

    if (is_segmented_search)
    {
        if (iter.IsValid())
        {
            if (segment_offset != iter.GetLength())
            {
                AllocateItems(context, final_items, final_items_used, final_items_allocated, 1, regs[ridx_final_items]);
                SetFinalItem(final_items, final_items_used++, iter.GetBase(), iter.GetOffset() + segment_offset, iter.GetLength() - segment_offset);
                final_length += iter.GetLength() - segment_offset;
            }

            ++segment_index;

            if (iter.Next())
            {
                AllocateItems(context, final_items, final_items_used, final_items_allocated, GetSegmentCount(this_string) - segment_index, regs[ridx_final_items]);

                do
                {
                    SetFinalItem(final_items, final_items_used++, iter.GetBase(), iter.GetOffset(), iter.GetLength());
                    final_length += iter.GetLength();
                }
                while (iter.Next());
            }
        }
    }
    else if (previous_match_end < this_length)
    {
        AllocateItems(context, final_items, final_items_used, final_items_allocated, 1, regs[ridx_final_items]);
        unsigned length = this_length - previous_match_end;
        SetFinalItem(final_items, final_items_used++, context, this_string, previous_match_end, length);
        final_length += length;
    }

    JString *result;

    if (final_length == 0)
        result = context->rt_data->strings[STRING_empty];
    else if (final_length > 64 && final_length > final_items_used * (sizeof(JStringStorage *) + 2 * sizeof(unsigned)) * 2)
    {
        JStringSegmented *segmented = JStringSegmented::Make(context, final_items_used);
        regs[ridx_segmented].SetBoxed(segmented);

        JStringStorage **base = segmented->Bases();
        unsigned *offset = segmented->Offsets();
        unsigned *length = segmented->Lengths();

        for (unsigned item_idx = 0; item_idx < final_items_used; ++item_idx)
        {
            *base++ = static_cast<JStringStorage*>(final_items->slots[item_idx]);
            *offset++ = final_items->uints[final_items->nused + 2*item_idx];
            *length++ = final_items->uints[final_items->nused + 2*item_idx+1];
            OP_ASSERT(length[-1] != ~0u);
        }

        result = JString::Make(context, segmented, final_length);
    }
    else
    {
        uni_char *storage;

        result = JString::Make(context, final_length);
        storage = Storage(context, result);

        OP_ASSERT(storage);

        for (unsigned item_idx = 0; item_idx < final_items_used; ++item_idx)
        {
            uni_char* str = static_cast<JStringStorage*>(final_items->slots[item_idx])->storage;
            unsigned offset = final_items->uints[final_items->nused + 2*item_idx];
            unsigned length = final_items->uints[final_items->nused + 2*item_idx+1];
            op_memcpy(storage, str + offset, length * sizeof(uni_char));
            storage += length;
        }

        result = Finalize(context, result);
    }

    return_value->SetString(result);
    return TRUE;

return_this:
    return_value->SetString(this_string);
    return TRUE;

return_false:
    return FALSE;
}

/* static */ BOOL
ES_StringBuiltins::match(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    ES_RegExp_Object *object;

    if (argc >= 1 && argv[0].IsObject() && argv[0].GetObject(context)->IsRegExpObject())
        object = static_cast<ES_RegExp_Object *>(argv[0].GetObject(context));
    else
    {
        JString *source, *empty = context->rt_data->strings[STRING_empty];

        if (argc >= 1 && !argv[0].IsUndefined())
        {
            if (!argv[0].ToString(context))
                return FALSE;
            source = argv[0].GetString();
        }
        else
            source = empty;

        RegExpFlags flags;
        unsigned flagbits;

        ES_RegExp_Object::ParseFlags(context, flags, flagbits, empty);

        object = ES_GET_GLOBAL_OBJECT()->GetDynamicRegExp(context, source, flags, flagbits);

        if (!object)
        {
            context->ThrowSyntaxError("String.prototype.match: invalid regular expression");
            return FALSE;
        }
    }

    ES_CollectorLock gclock(context, TRUE);

    if ((object->GetFlagBits() & REGEXP_FLAG_GLOBAL) == 0)
    {
        ES_Value_Internal *registers;

        if (argc == 0)
            registers = context->AllocateRegisters(3);
        else
            registers = argv - 2;

        registers[0].SetObject(object);
        registers[1].SetObject(argv[-1].GetObject(context));
        registers[2].SetString(this_string);

        BOOL success = ES_RegExpBuiltins::exec(context, 1, registers + 2, return_value);

        if (argc == 0)
            context->FreeRegisters(3);

        return success;
    }
    else
    {
        ES_Object *array = ES_Array::Make(context, ES_GET_GLOBAL_OBJECT());
        unsigned last_index = 0, nmatches = 0, length = Length(this_string);

        do
        {
            RegExpMatch *matches = object->Exec(context, this_string, last_index);

            if (!matches)
                break;
            else
                array->PutNoLockL(context, nmatches++, JString::Make(context, this_string, matches[0].start, matches[0].length));

            last_index = matches[0].start + es_maxu(1, matches[0].length);
        }
        while (last_index <= length);

        if (nmatches == 0)
            return_value->SetNull();
        else
        {
            ES_Array::SetLength(context, array, nmatches);

            ES_Value_Internal lastIndex;
            lastIndex.SetUInt32(0);
            object->PutCachedAtIndex(ES_PropertyIndex(ES_RegExp_Object::LASTINDEX), lastIndex);

            return_value->SetObject(array);
        }

        return TRUE;
    }
}

/* static */ BOOL
ES_StringBuiltins::split(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    ES_RegExp_Object *separator_regexp = NULL;
    JString *separator_string = NULL;

    unsigned limit = UINT_MAX;

    if (argc >= 2 && !argv[1].IsUndefined())
    {
        if (!argv[1].ToNumber(context))
            return FALSE;

        limit = argv[1].GetNumAsUInt32();
    }

    if (limit == 0)
    {
        ES_Object *array = ES_Array::Make(context, ES_GET_GLOBAL_OBJECT());
        return_value->SetObject(array);
        return TRUE;
    }

    if (argc >= 1 && argv[0].IsObject() && argv[0].GetObject(context)->IsRegExpObject())
        separator_regexp = static_cast<ES_RegExp_Object *>(argv[0].GetObject(context));
    else if (argc < 1 || argv[0].IsUndefined())
    {
        ES_Object *array = ES_Array::Make(context, ES_GET_GLOBAL_OBJECT());
        ES_CollectorLock gclock(context);

        ES_Value_Internal value;
        value.SetString(this_string);
        array->PutNoLockL(context, 0u, value);

        ES_Array::SetLength(context, array, 1);
        return_value->SetObject(array);
        return TRUE;
    }
    else
    {
        if (!argv[0].ToString(context))
            return FALSE;

        separator_string = argv[0].GetString();
    }

    ES_Object *array = ES_Array::Make(context, ES_GET_GLOBAL_OBJECT());
    ES_CollectorLock gclock(context);

    ES_Value_Internal value;
    unsigned array_length = 0, this_length = Length(this_string);

    if (separator_regexp)
    {
        unsigned index = 0, previous_end = 0, ncaptures = separator_regexp->GetValue()->GetNumberOfCaptures();

        while (index <= this_length && limit > 0)
        {
            RegExpMatch *matches = separator_regexp->Exec(context, this_string, index);

            // No match or empty match at end of string.
            if (!matches || matches[0].start == this_length)
            {
                // Return empty array if not a match failure on an empty string (only.)
                if (matches != NULL && this_length == 0)
                    limit = 0;
                break;
            }
            else if (matches[0].start > previous_end || !IsEmpty(matches[0]))
            {
                value.SetString(JString::Make(context, this_string, previous_end, matches[0].start - previous_end));
                array->PutNoLockL(context, array_length++, value);

                previous_end = matches[0].start + matches[0].length;

                for (unsigned capture_index = 0; capture_index < ncaptures; ++capture_index)
                {
                    SetMatchValue(context, value, this_string, matches[capture_index + 1]);
                    array->PutNoLockL(context, array_length++, value);
                }
                --limit;
            }

            index = (IsEmpty(matches[0]) ? matches[0].start + 1 : matches[0].start + matches[0].length);
        }

        if (limit > 0)
        {
            value.SetString(JString::Make(context, this_string, previous_end, this_length - previous_end));
            array->PutNoLockL(context, array_length++, value);
        }
    }
    else
    {
        unsigned separator_length = Length(separator_string);

        if (separator_length == 0)
        {
            OP_ASSERT(array->GetIndexedProperties() == NULL);

            array->SetIndexedProperties(ES_Compact_Indexed_Properties::Make(context, ES_Compact_Indexed_Properties::AppropriateCapacity(this_length)));

            for (unsigned index = 0; index < this_length && limit > 0; ++index, --limit)
            {
                value.SetString(JString::Make(context, this_string, index, 1));
                array->PutNoLockL(context, array_length++, value);
            }
        }
        else
        {
            const uni_char *this_storage = Storage(context, this_string);
            const uni_char *separator_storage = Storage(context, separator_string);
            unsigned index = 0, previous_end = 0;

            if (this_length >= separator_length)
            {
                unsigned adjusted_length = this_length - separator_length;
                unsigned separator_bytes = separator_length * sizeof(uni_char);
                int first = separator_storage[0];

                while (index <= adjusted_length && limit > 0)
                {
                    while (index <= adjusted_length && this_storage[index] != first)
                        ++index;

                    if (index <= adjusted_length)
                        if (separator_length == 1 || op_memcmp(this_storage + index, separator_storage, separator_bytes) == 0)
                        {
                            value.SetString(JString::Make(context, this_string, previous_end, index - previous_end));
                            array->PutNoLockL(context, array_length++, value);

                            index += separator_length;
                            previous_end = index;
                            --limit;
                        }
                        else
                            ++index;
                }
            }

            if (limit > 0)
            {
                if (previous_end == 0)
                    value.SetString(this_string);
                else
                    value.SetString(JString::Make(context, this_string, previous_end, this_length - previous_end));
                array->PutNoLockL(context, array_length++, value);
            }
        }
    }

    ES_Array::SetLength(context, array, array_length);
    return_value->SetObject(array);
    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::search(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    ES_RegExp_Object *object;

    if (argc >= 1 && argv[0].IsObject() && argv[0].GetObject(context)->IsRegExpObject())
        object = static_cast<ES_RegExp_Object *>(argv[0].GetObject(context));
    else
    {
        JString *source, *empty = context->rt_data->strings[STRING_empty];

        if (argc >= 1 && !argv[0].IsUndefined())
        {
            if (!argv[0].ToString(context))
                return FALSE;
            source = argv[0].GetString();
        }
        else
            source = empty;

        RegExpFlags flags;
        unsigned flagbits;

        ES_RegExp_Object::ParseFlags(context, flags, flagbits, empty);

        object = ES_GET_GLOBAL_OBJECT()->GetDynamicRegExp(context, source, flags, flagbits);

        if (!object)
        {
            context->ThrowSyntaxError("String.prototype.search: invalid regular expression");
            return FALSE;
        }
    }

    ES_CollectorLock gclock(context, TRUE);

    RegExpMatch *matches = object->Exec(context, this_string, 0);
    if (matches)
        return_value->SetUInt32(matches[0].start);
    else
        return_value->SetInt32(-1);

    return TRUE;
}

#define TRIM(ch) (ES_Lexer::IsWhitespace(ch) || ES_Lexer::IsLinebreak(ch))

/* static */ BOOL
ES_StringBuiltins::trim(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_STRING();

    unsigned length = Length(this_string);

    if (length == 0)
        return_value->SetString(this_string);
    else
    {
        unsigned offset = 0;
        const uni_char *srcstr = Storage(context, this_string);

        while (TRIM(srcstr[offset]) && ++offset != length)
            ;

        if (offset != length)
            while (TRIM(srcstr[length - 1]) && --length != offset)
                ;

        if (offset != length)
        {
            length -= offset;
            return_value->SetString(JString::Make(context, this_string, offset, length));
        }
        else
            return_value->SetString(context->rt_data->strings[STRING_empty]);
    }

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::link(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "a", "href");
}

/* static */ BOOL
ES_StringBuiltins::anchor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "a", "name");
}

/* static */ BOOL
ES_StringBuiltins::fontcolor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "font", "color");
}

/* static */ BOOL
ES_StringBuiltins::fontsize(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "font", "size");
}

/* static */ BOOL
ES_StringBuiltins::big(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "big");
}

/* static */ BOOL
ES_StringBuiltins::blink(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "blink");
}

/* static */ BOOL
ES_StringBuiltins::bold(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "b");
}

/* static */ BOOL
ES_StringBuiltins::fixed(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "tt");
}

/* static */ BOOL
ES_StringBuiltins::italics(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "i");
}

/* static */ BOOL
ES_StringBuiltins::small(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "small");
}

/* static */ BOOL
ES_StringBuiltins::strike(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "strike");
}

/* static */ BOOL
ES_StringBuiltins::sub(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "sub");
}

/* static */ BOOL
ES_StringBuiltins::sup(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return ES_StringBuiltins::htmlify(context, argc, argv, return_value, "sup");
}

static void append_to_storage(uni_char *s, int& pos, const char* str)
{
    int str_len = op_strlen(str);
    make_doublebyte_in_buffer(str, str_len, s + pos, str_len + 1);
    pos += str_len;
}

/* static */ BOOL
ES_StringBuiltins::htmlify(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, const char *tag_name)
{
    ES_THIS_STRING();

    int this_len = Length(this_string);
    int tag_len = op_strlen(tag_name);
    int slen = this_len + tag_len * 2 + 5;

    ES_CollectorLock gclock(context);
    JString *res = JString::Make(context, slen);
    uni_char *s = Storage(context, res);
    int pos = 0;
    append_to_storage(s, pos, "<");
    append_to_storage(s, pos, tag_name);
    append_to_storage(s, pos, ">");
    uni_char *this_s = Storage(context, this_string);
    uni_strncpy(s + pos, this_s, this_len);
    pos += this_len;
    append_to_storage(s, pos, "</");
    append_to_storage(s, pos, tag_name);
    s[pos] = '>'; // avoid writing outside string.

    return_value->SetString(res);

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::htmlify(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, const char *tag_name, const char *attr_name)
{
    ES_THIS_STRING();

    JString* str = context->rt_data->strings[STRING_undefined];
    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;

        str = argv[0].GetString();
    }

    int this_len = Length(this_string);
    int tag_len = op_strlen(tag_name);
    int attr_len = op_strlen(attr_name);
    int str_len = Length(str);
    int slen = this_len + tag_len * 2 + attr_len + str_len + 9;

    ES_CollectorLock gclock(context);
    JString *res = JString::Make(context, slen);
    uni_char *s = Storage(context, res);
    int pos = 0;
    append_to_storage(s, pos, "<");
    append_to_storage(s, pos, tag_name);
    append_to_storage(s, pos, " ");
    append_to_storage(s, pos, attr_name);
    append_to_storage(s, pos, "=\"");
    uni_char *str_s = Storage(context, str);
    uni_strncpy(s + pos, str_s, str_len);
    pos += str_len;
    append_to_storage(s, pos, "\">");
    uni_char *this_s = Storage(context, this_string);
    uni_strncpy(s + pos, this_s, this_len);
    pos += this_len;
    append_to_storage(s, pos, "</");
    append_to_storage(s, pos, tag_name);
    s[pos] = '>'; // avoid writing outside string.

    return_value->SetString(res);

    return TRUE;
}

/* static */ BOOL
ES_StringBuiltins::fromCharCode(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *result_string;

    if (argc == 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        uni_char ch = static_cast<uni_char>(argv[0].GetNumAsUInt32());

        result_string = JString::Make(context, &ch, 1);
    }
    else if (argc == 0)
        result_string = context->rt_data->strings[STRING_empty];
    else
    {
        result_string = JString::Make(context, argc);
        argv[-1].SetString(result_string);

        for (unsigned index = 0; index < argc; ++index)
        {
            if (!argv[index].ToNumber(context))
                return FALSE;

            Storage(context, result_string)[index] = static_cast<uni_char>(argv[index].GetNumAsUInt32());
        }
    }

    return_value->SetString(result_string);
    return TRUE;
}

/* static */ void
ES_StringBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_StringBuiltins);

    ES_Function *fn_charCodeAt = MAKE_BUILTIN(1, charCodeAt);
    ES_Function *fn_charAt = MAKE_BUILTIN(1, charAt);

#ifdef ES_NATIVE_SUPPORT
    fn_charCodeAt->SetFunctionID(ES_BUILTIN_FN_charCodeAt);
    fn_charAt->SetFunctionID(ES_BUILTIN_FN_charAt);
#endif // ES_NATIVE_SUPPORT

    APPEND_PROPERTY(ES_StringBuiltins, length,              0);
    APPEND_PROPERTY(ES_StringBuiltins, constructor,         undefined);
    APPEND_PROPERTY(ES_StringBuiltins, toString,            MAKE_BUILTIN(0, toString));
    APPEND_PROPERTY(ES_StringBuiltins, valueOf,             MAKE_BUILTIN(0, valueOf));
    APPEND_PROPERTY(ES_StringBuiltins, toLowerCase,         MAKE_BUILTIN(0, toLowerCase));
    APPEND_PROPERTY(ES_StringBuiltins, toLocaleLowerCase,   MAKE_BUILTIN(0, toLocaleLowerCase));
    APPEND_PROPERTY(ES_StringBuiltins, toUpperCase,         MAKE_BUILTIN(0, toUpperCase));
    APPEND_PROPERTY(ES_StringBuiltins, toLocaleUpperCase,   MAKE_BUILTIN(0, toLocaleUpperCase));
    APPEND_PROPERTY(ES_StringBuiltins, charCodeAt,          fn_charCodeAt);
    APPEND_PROPERTY(ES_StringBuiltins, charAt,              fn_charAt);
    APPEND_PROPERTY(ES_StringBuiltins, substring,           MAKE_BUILTIN(2, substring));
    APPEND_PROPERTY(ES_StringBuiltins, substr,              MAKE_BUILTIN(2, substr));
    APPEND_PROPERTY(ES_StringBuiltins, slice,               MAKE_BUILTIN(2, slice));
    APPEND_PROPERTY(ES_StringBuiltins, concat,              MAKE_BUILTIN(1, concat));
    APPEND_PROPERTY(ES_StringBuiltins, indexOf,             MAKE_BUILTIN(1, indexOf));
    APPEND_PROPERTY(ES_StringBuiltins, lastIndexOf,         MAKE_BUILTIN(1, lastIndexOf));
    APPEND_PROPERTY(ES_StringBuiltins, localeCompare,       MAKE_BUILTIN(1, localeCompare));
    APPEND_PROPERTY(ES_StringBuiltins, replace,             MAKE_BUILTIN(2, replace));
    APPEND_PROPERTY(ES_StringBuiltins, match,               MAKE_BUILTIN(1, match));
    APPEND_PROPERTY(ES_StringBuiltins, split,               MAKE_BUILTIN(2, split));
    APPEND_PROPERTY(ES_StringBuiltins, search,              MAKE_BUILTIN(1, search));
    APPEND_PROPERTY(ES_StringBuiltins, trim,                MAKE_BUILTIN(0, trim));
    APPEND_PROPERTY(ES_StringBuiltins, link,                MAKE_BUILTIN(1, link));
    APPEND_PROPERTY(ES_StringBuiltins, anchor,              MAKE_BUILTIN(1, anchor));
    APPEND_PROPERTY(ES_StringBuiltins, fontcolor,           MAKE_BUILTIN(1, fontcolor));
    APPEND_PROPERTY(ES_StringBuiltins, fontsize,            MAKE_BUILTIN(1, fontsize));
    APPEND_PROPERTY(ES_StringBuiltins, big,                 MAKE_BUILTIN(0, big));
    APPEND_PROPERTY(ES_StringBuiltins, blink,               MAKE_BUILTIN(0, blink));
    APPEND_PROPERTY(ES_StringBuiltins, bold,                MAKE_BUILTIN(0, bold));
    APPEND_PROPERTY(ES_StringBuiltins, fixed,               MAKE_BUILTIN(0, fixed));
    APPEND_PROPERTY(ES_StringBuiltins, italics,             MAKE_BUILTIN(0, italics));
    APPEND_PROPERTY(ES_StringBuiltins, small,               MAKE_BUILTIN(0, small));
    APPEND_PROPERTY(ES_StringBuiltins, strike,              MAKE_BUILTIN(0, strike));
    APPEND_PROPERTY(ES_StringBuiltins, sub,                 MAKE_BUILTIN(0, sub));
    APPEND_PROPERTY(ES_StringBuiltins, sup,                 MAKE_BUILTIN(0, sup));

    ASSERT_OBJECT_COUNT(ES_StringBuiltins);
}

/* static */ void
ES_StringBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_StringBuiltinsCount);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_StringBuiltins, length,             RO | DE | DD,   ES_STORAGE_INT32);
    DECLARE_PROPERTY(ES_StringBuiltins, constructor,        DE,             ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_StringBuiltins, toString,           DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, valueOf,            DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, toLowerCase,        DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, toLocaleLowerCase,  DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, toUpperCase,        DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, toLocaleUpperCase,  DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, charCodeAt,         DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, charAt,             DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, substring,          DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, substr,             DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, slice,              DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, concat,             DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, indexOf,            DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, lastIndexOf,        DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, localeCompare,      DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, replace,            DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, match,              DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, split,              DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, search,             DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, trim,               DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, link,               DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, anchor,             DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, fontcolor,          DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, fontsize,           DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, big,                DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, blink,              DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, bold,               DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, fixed,              DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, italics,            DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, small,              DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, strike,             DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, sub,                DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_StringBuiltins, sup,                DE,             ES_STORAGE_OBJECT);
}

/* static */ void
ES_StringBuiltins::PopulateConstructor(ES_Context *context, ES_Global_Object *global_object, ES_Object *constructor)
{
    ES_Function *fn_fromCharCode = MAKE_BUILTIN_NO_PROTOTYPE(1, fromCharCode);
#ifdef ES_NATIVE_SUPPORT
    fn_fromCharCode->SetFunctionID(ES_BUILTIN_FN_fromCharCode);
#endif // ES_NATIVE_SUPPORT
    constructor->InitPropertyL(context, context->rt_data->idents[ESID_fromCharCode], fn_fromCharCode, DE);
}

/* static */ BOOL
ES_StringBuiltins::ProcessThis(ES_Execution_Context *context, ES_Value_Internal &this_value)
{
    if (this_value.IsNullOrUndefined())
    {
        context->ThrowTypeError("'this' is not coercible to object");
        return FALSE;
    }

    if (!this_value.ToString(context))
        return FALSE;

    return TRUE;
}

#undef ES_THIS_STRING
#undef ES_CHECK_CALLABLE
#undef TYPE_ERROR_TO_STRING
#undef TYPE_ERROR_VALUE_OF
