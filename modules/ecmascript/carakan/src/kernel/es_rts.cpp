/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2001-2010
 *
 * @author Lars T Hansen
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/es_program_cache.h"
#include "modules/memory/src/memory_executable.h"

/* static */ ESRT_Data *
ESRT::Initialize()
{
	ESRT_Data *rt_data = OP_NEW_L(ESRT_Data, ());

	g_esrt = rt_data;

	OP_ASSERT(sizeof(ES_Header) == 4);

	/* PHASE 1: Initialization, in case we need to abort later */

	rt_data->NaN = op_nan(NULL);
#ifdef ES_VALUES_AS_NANS
	ES_Value_Internal double_val;
	double_val.SetNan();
	rt_data->NaN = double_val.double_value;
#endif
	rt_data->NegativeZero = op_implode_double(0x80000000UL, 0x00000000UL);
#ifdef IEEE_DENORMALS
	rt_data->DoubleMin = op_implode_double(0x00000000UL, 0x00000001UL);
#else
	rt_data->DoubleMin = DBL_MIN;
#endif
	rt_data->PositiveInfinity = op_implode_double(0x7ff00000UL, 0UL);
	rt_data->NegativeInfinity = op_implode_double(0xfff00000UL, 0UL);

	rt_data->cur_exec_context = NULL;
	rt_data->initialized = FALSE;
	rt_data->in_collector = FALSE;
	rt_data->clean_latent_arguments = FALSE;
	rt_data->active_contexts = 0;
	rt_data->suspended_contexts = 0;
	rt_data->js_ident_hashcode = 0;
	rt_data->temp_counter = 0;
	rt_data->script_counter = 0;
    rt_data->unique = 2;
	rt_data->tmp_context = NULL;
	rt_data->saved_callback_context = NULL;
	for (int i=0 ; i < STRING_NUMSTRINGS ; i++)
		rt_data->strings[i] = NULL;

    rt_data->arguments_cookie.SetArgumentsCookie();
    rt_data->undefined.SetUndefined();

	for (int i=0 ; i < ESID_NUMIDS ; i++)
		rt_data->idents[i] = NULL;

	rt_data->load_factor = ES_PARM_INVERSE_LOAD_FACTOR;
	rt_data->offline_load_factor = ES_PARM_INVERSE_OFFLINE_LOAD_FACTOR;

#ifdef ECMASCRIPT_DEBUGGER
	rt_data->debugger_listener = NULL;
	rt_data->debugger_scope_chain = NULL;
	rt_data->debugger_scope_chain_length = 0;
	rt_data->debugger_call_stack = NULL;
	rt_data->debugger_call_stack_length = 0;
#endif

    op_srand((unsigned int)op_time(NULL));

    rt_data->script_guid_counter = 1;

    op_memset(&GC_DDATA, 0, sizeof(GC_DDATA));

    rt_data->program_cache = NULL;

#ifndef CONSTANT_DATA_IS_EXECUTABLE
    for (int i=0; i < ES_OPT_COUNT; ++i)
        rt_data->opt_meta_method_block[i] = NULL;
#endif // CONSTANT_DATA_IS_EXECUTABLE

    rt_data->markstack = NULL;
    rt_data->heap = NULL;
    rt_data->shared_strings = NULL;

#ifdef ES_PROPERTY_CACHE_PROFILING
    rt_data->pcache_misses = 0;
    rt_data->pcache_fails = 0;
    rt_data->pcache_fills = 0;
    rt_data->pcache_fills_poly = 0;
    rt_data->pcache_last_reported = 0;
    rt_data->pcache_allocated_property = 0;
    rt_data->pcache_allocated_global = 0;
#endif // ES_PROPERTY_CACHE_PROFILING

	/* PHASE 2: Initializing heap-allocated data */

    rt_data->markstack = ES_MarkStack::MakeL();
    rt_data->heap = ES_Heap::MakeL(rt_data->markstack);

    {
        ES_Allocation_Context context(rt_data);
        rt_data->heap->Lock();

        CreateStandardIdentifiers(&context, *rt_data);
        CreateStandardStrings(&context, *rt_data);

        rt_data->shared_strings = ES_Identifier_List::Make(&context, 256);
    }

    rt_data->program_cache = ES_Program_Cache::MakeL();

    // To prevent maintenance gc to pick this heap up.
    rt_data->heap->ResetActivity();

    /* PHASE 3: Commit */

    rt_data->initialized = TRUE;

    /* PHASE 4: Rudimentary self-tests for typical porting problems. */

#ifdef _DEBUG

# ifndef NAN_EQUALS_EVERYTHING
	// Typically it is a problem under Microsoft Visual C/C++ that NaN compares
	// equal with everything.  This is bizarre and very wrong.  If you hit the
	// assert below you must turn on NAN_EQUALS_EVERYTHING in your SYSTEM defines.
	if (rt_data->NaN == 1)
		OP_ASSERT(!"Wrong setting of NAN_EQUALS_EVERYTHING");
# endif

# ifndef ES_NEED_GUARD_AGAINST_NAN
	// On some platforms NaN cannot be used for computation, it will crash the program.
	// If your program crashes here, then set SYSTEM_NAN_ARITHMETIC_OK = NO in your
	// product's system.h.
	double d = rt_data->NaN;
	d = d+37;
# endif

#endif

#ifdef ES_SLOW_CASE_PROFILING
    for (int i = 0; i < ESI_LAST_INSTRUCTION; ++i)
        rt_data->slow_case_calls[i] = 0;
#endif // ES_SLOW_CASE_PROFILING

    // Check that the correct FP layout has been chosen.  We want to be sure to
    // run this on every device, but we cannot always rely on a debug build being
    // made for a target device (for example, this does not work for Symbian.)
    // So use OP_ASSERT in the default case, but special mechanisms for some systems.
#ifdef EPOC
    if (op_implode_double(0x40000000, 0x0) != 2.0)
        User::InfoPrint(_L("SERIOUS ERROR: Misconfigured floating point format"));
#else
    OP_ASSERT(op_implode_double(0x40000000, 0x0) == 2.0);
#endif

    /* PHASE 5: Return */

    return rt_data;
}

/* These must be in the same order as the enum in es_rts.h, and there must be an
 * empty string for every private property too!   And watch out: if an identifier
 * starting with x is added to this table, then be careful so that \0x is not
 * treated as the start of a hex escape char.
 */
static const char identifier_names[] =
"Arguments\0Array\0ArrayBuffer\0BYTES_PER_ELEMENT\0Boolean\0DOMException\0DOMSTRING_SIZE_ERR\0DataView\0Date\0E\0Error\0EvalError\0EventException\
\0Float32Array\0Float64Array\0Function\0HIERARCHY_REQUEST_ERR\0INDEX_SIZE_ERR\0INUSE_ATTRIBUTE_ERR\0INVALID_ACCESS_ERR\
\0INVALID_CHARACTER_ERR\0INVALID_MODIFICATION_ERR\0INVALID_STATE_ERR\0Infinity\0Int8Array\0Int16Array\0Int32Array\0LN10\0LN2\
\0LOG10E\0LOG2E\0MAX_VALUE\0MIN_VALUE\0Math\0MemoryError\0NAMESPACE_ERR\0NEGATIVE_INFINITY\0NOT_FOUND_ERR\0NOT_SUPPORTED_ERR\0NO_DATA_ALLOWED_ERR\
\0NO_MODIFICATION_ALLOWED_ERR\0NaN\0Number\0Object\0PI\0POSITIVE_INFINITY\0QName\0RangeError\0ReferenceError\0RegExp\
\0SQRT1_2\0SQRT2\0SYNTAX_ERR\0String\0SyntaxError\0TypeError\0UNSPECIFIED_EVENT_TYPE_ERR\0URIError\0InternalError\
\0UTC\0Uint8Array\0Uint8ClampedArray\0Uint16Array\0Uint32Array\0Variable\0WRONG_DOCUMENT_ERR\0__defineGetter__\0__defineSetter__\0__lookupGetter__\0__lookupSetter__\0__proto__\
\0abs\0acos\0alert\0anchor\0apply\0arguments\0asin\0atan\0atan2\0big\0bind\0blink\0bold\0buffer\0byteLength\0byteOffset\0call\0callee\0caller\
\0ceil\0charAt\0charCodeAt\0code\0compile\0concat\0configurable\0constructor\0copysign\0cos\0create\0debugger\0decodeURI\0decodeURIComponent\0defineProperties\
\0defineProperty\0$1\0$2\0$3\0$4\0$5\0$6\0$7\0$8\0$9\0$_\0$&\0$+\0$*\0$`\0$'\
\0encodeURI\0encodeURIComponent\0enumerable\0escape\0eval\0every\0exec\0exp\0extended\0filter\0fixed\0floor\0fontcolor\0fontsize\0forEach\0freeze\0fromCharCode\0get\0getDate\0getDay\
\0getFloat32\0getFloat64\0getFullYear\0getHours\0getInt8\0getInt16\0getInt32\0getMilliseconds\0getMinutes\0getMonth\0getOwnPropertyDescriptor\0getOwnPropertyNames\0getPrototypeOf\
\0getSeconds\0getTime\0getTimezoneOffset\0getUTCDate\0getUTCDay\0getUTCFullYear\0getUTCHours\0getUTCMilliseconds\
\0getUTCMinutes\0getUTCMonth\0getUTCSeconds\0getUint8\0getUint16\0getUint32\0getYear\0global\0hasOwnProperty\0~";

/* And another one (string was too long for compiler) -- watch the enum! */
static const char identifier_names2[]=
"ignoreCase\0index\0indexOf\0input\0isArray\0isExtensible\0isFinite\0isFrozen\0isIntegral\0isNaN\0isPrototypeOf\0isSealed\0italics\0join\0JSON\0keys\0lastIndex\0lastIndexOf\0lastMatch\0lastParen\0leftContext\0length\0link\
\0localeCompare\0log\0map\0match\0max\0message\0min\0multiline\0name\0noSearch\0now\0[object Array]\0[object Boolean]\0[object Function]\0[object Null]\0[object Number]\0[object Object]\0[object String]\0[object Undefined]\0[object Window]\0opera#sourceloc\0parse\0parseFloat\0parseInt\0pop\0pow\
\0preventExtensions\0prompt\0propertyIsEnumerable\0prototype\0push\0random\0readOnly\0reduce\0reduceRight\0replace\0reverse\0rightContext\0round\0seal\0search\
\0set\0setDate\0setFloat32\0setFloat64\0setFullYear\0setHours\0setInt8\0setInt16\0setInt32\0setMilliseconds\0setMinutes\
\0setMonth\0setSeconds\0setTime\0setUTCDate\0setUTCFullYear\0setUTCHours\0setUTCMilliseconds\0setUTCMinutes\0setUTCMonth\
\0setUTCSeconds\0setUint8\0setUint16\0setUint32\0setYear\0shift\0sin\0slice\0small\0some\0sort\0source\0splice\0split\
\0sqrt\0stack\0stacktrace\0strike\0stringify\0sub\0subarray\0substr\0substring\0sup\0tan\0test\0toDateString\0toExponential\
\0toFixed\0toGMTString\0toISOString\0toJSON\0toLocaleDateString\0toLocaleLowerCase\
\0toLocaleString\0toLocaleTimeString\0toLocaleUpperCase\0toLowerCase\0toPrecision\0toString\0toTimeString\0toUTCString\
\0toUpperCase\0trim\0typeOf\0undefined\0unescape\0unshift\0value\0valueOf\0writable\0Window\0~";

/* Same thing applies here -- watch the enum in es_rts.h! */
static const char reserved_names[] =
"break\0case\0catch\0continue\0default\0delete\0do\0else\0finally\0for\0function\0goto\0"
"if\0in\0instanceof\0new\0return\0switch\0this\0throw\0try\0typeof\0var\0void\0while\0with\0false\0true\0null\0~";

/* static */ void
ESRT::CreateStandardIdentifiers(ES_Context *context, ESRT_Data &rt_data)
{
	JString **idents = rt_data.idents;
	const char* p;
	int i, plen;

	idents[0] = NULL;

	for (p = identifier_names, i = 1, plen = op_strlen(p); *p != '~'; ++i, p += plen + 1, plen = op_strlen(p))
    {
        (idents[i] = JString::Make(context, p, plen))->Hash();
        idents[i]->SetBuiltinString();
    }

	for (p = identifier_names2, plen = op_strlen(p); *p != '~'; ++i, p += plen + 1, plen = op_strlen(p))
    {
		(idents[i] = JString::Make(context, p, plen))->Hash();
        idents[i]->SetBuiltinString();
    }

	for (p = reserved_names, plen = op_strlen(p); *p != '~'; ++i, p += plen + 1, plen = op_strlen(p))
    {
		(idents[i] = JString::Make(context, p, plen))->Hash();
        idents[i]->SetBuiltinString();
    }

	OP_ASSERT(i == ESID_NUMIDS);
}

// Corresponds to table in esrt.h
// Note representation of empty string and "~" carefully.
static const char string_characters[] =
"attributename~boolean~~false~function~length~null~number~object~__proto__~string~toJSON~true~undefined~xml~(?:)\
~0~1~2~3~4~5~6~7~8~9~10~11~12~13~14~15~16~17~18~19~20~21~22~23~24~25~26~27~28~29~30~31~32~33~34~35~36~37~38~39\
~40~41~42~43~44~45~46~47~48~49~50~51~52~53~54~55~56~57~58~59~60~61~62~63~64~65~66~67~68~69~70~71~72~73~74~75~76\
~77~78~79~80~81~82~83~84~85~86~87~88~89~90~91~92~93~94~95~96~97~98~99\
~\x00~\x01~\x02~\x03~\x04~\x05~\x06~\x07~\x08~\x09~\x0a~\x0b~\x0c~\x0d~\x0e~\x0f~\x10~\x11~\x12~\x13~\x14~\x15\
~\x16~\x17~\x18~\x19~\x1a~\x1b~\x1c~\x1d~\x1e~\x1f~ ~!~\"~#~$~%~&~'~(~)~*~+~,~-~.~/~0~1~2~3~4~5~6~7~8~9~:~;~<~=\
~>~?~@~A~B~C~D~E~F~G~H~I~J~K~L~M~N~O~P~Q~R~S~T~U~V~W~X~Y~Z~[~\\~]~^~_~`~a~b~c~d~e~f~g~h~i~j~k~l~m~n~o~p~q~r~s~t\
~u~v~w~x~y~z~{~|~}~~~";

/* static */ void
ESRT::CreateStandardStrings(ES_Context *context, ESRT_Data &rt_data)
{
	JString *all = JString::Make(context, string_characters, sizeof(string_characters) - 1);
	JString **strings = rt_data.strings;

    const char *s = string_characters;
    const char *limit = string_characters + sizeof(string_characters) - 1;
	int i;

    for (i = 0;; ++i)
    {
        const char *sep = s;

        while (sep != limit && *sep != '~')
            ++sep;

        // Handle '~'
        if (sep == limit - 2)
            ++sep;

        if (sep == limit)
            break;

        if (i >= STRING_nul + '0' && i <= STRING_nul + '9')
            /* Reuse string for number x, 0 <= x <= 9, for string for ASCII
               character c, '0' <= c <= '9'. */
            strings[i] = strings[STRING_0 + (i - (STRING_nul + '0'))];
        else
        {
            unsigned length = (sep - s);
            if (length > 0)
                ++length;
            strings[i] = JString::Make(context, all, s - string_characters, length);
            if (length > 0)
                --strings[i]->length;
            strings[i]->SetBuiltinString();
            // Replace separator ~ with \0
            Storage(context, all)[sep - string_characters] = 0;
        }

        s = sep + 1;
    }

    OP_ASSERT(i == STRING_NUMSTRINGS);

    // Number values
    for (int j = 0; j < STRING_NUMNUMBERS; ++j)
        JString::SetValue(strings[STRING_0 + j], j);


#ifdef _DEBUG
    // Verify that it looks sane
    OP_ASSERT(Length(strings[STRING_nul]) == 1);
    OP_ASSERT(Element(strings[STRING_nul], 0) == 0);

    JString *t = strings[STRING_nul + '~'];
    OP_ASSERT(Length(t) == 1);
    OP_ASSERT(Element(t, 0) == '~');
#endif // _DEBUG
}

JString*
ESRT_Data::GetSharedString(const char *value, unsigned length)
{
    shared_string_buffer.Clear();
    shared_string_buffer.AppendL(value, length);

    JTemporaryString name(shared_string_buffer.GetStorage(), shared_string_buffer.Length());
    unsigned index;

    if (shared_strings->IndexOf(name, index))
        return shared_strings->GetIdentifierAtIndex(index);
    else
    {
        ES_Allocation_Context context(this);

        JString *string = name.Allocate(&context);
        shared_strings->AppendL(&context, string, index);
        return string;
    }
}

JString*
ESRT_Data::GetSharedString(const uni_char *value, unsigned length)
{
    if (length == UINT_MAX)
        length = uni_strlen(value);

    JTemporaryString name(value, length);
    unsigned index;

    if (shared_strings->IndexOf(name, index))
        return shared_strings->GetIdentifierAtIndex(index);
    else
    {
        ES_Allocation_Context context(this);

        JString *string = name.Allocate(&context);
        shared_strings->AppendL(&context, string, index);
        return string;
    }
}

JString*
ESRT_Data::GetSharedString(JString *value)
{
    unsigned index;

    if (shared_strings->IndexOf(value, index))
        return shared_strings->GetIdentifierAtIndex(index);
    else
    {
        ES_Allocation_Context context(this);

        shared_strings->AppendL(&context, value, index);
        return value;
    }
}

/* static */ void
ESRT::Shutdown(ESRT_Data *rt_data)
{
    OP_DELETE(rt_data->program_cache);
    rt_data->heap->Unlock();

    ES_Heap::Destroy(rt_data->heap);

#ifndef CONSTANT_DATA_IS_EXECUTABLE
    for (int i=0; i < ES_OPT_COUNT; ++i)
        if (rt_data->opt_meta_method_block[i])
            OpExecMemoryManager::Free(rt_data->opt_meta_method_block[i]);
#endif // CONSTANT_DATA_IS_EXECUTABLE

    ES_MarkStack::DecRef(rt_data->markstack);

#ifdef ECMASCRIPT_DEBUGGER
	OP_DELETEA(rt_data->debugger_scope_chain);
	OP_DELETEA(rt_data->debugger_call_stack);
#endif // ECMASCRIPT_DEBUGGER

    OP_DELETE(rt_data);
}

/*static*/ unsigned
ESRT::GetGlobalUniqueScriptId(ESRT_Data *rt_data)
{
    return rt_data->script_guid_counter++;
}
