/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_global_builtins.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser.h"
#include "modules/ecmascript/carakan/src/compiler/es_lexer.h"
#include "modules/ecmascript/carakan/src/compiler/es_disassembler.h"
#include "modules/ecmascript/carakan/src/util/es_formatter.h"

static int
NumericValue(uni_char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    else
    {
        ch &= ~0x20;
        if (ch >= 'A' && ch <= 'Z')
            return 10 + ch - 'A';
    }
    return INT_MAX;
}

enum FloatParserState
{
    FLOAT_PARSER_STATE_START,
    FLOAT_PARSER_STATE_NUMBER,
    FLOAT_PARSER_STATE_POINT,
    FLOAT_PARSER_STATE_FRACTION,
    FLOAT_PARSER_STATE_EXPONENT_SIGN,
    FLOAT_PARSER_STATE_EXPONENT
};

class FloatParser
{
public:
    FloatParser(ES_Context *context, JString *s);

    void Parse();

    BOOL IsNaN() { return !isnumber; }
    BOOL IsInfinity() { return infinity; }
    BOOL IsPlus() { return plus; }
    double GetValue() { return value; }
    uni_char *GetStart() { return start; }
    int       GetStartPos() { return startpos; }

private:
    void MatchWhiteSpace();
    void MatchSign();
    BOOL MatchInfinity();
    void ParseInternal();

    uni_char *s;
    uni_char *start;
    BOOL plus;
    BOOL infinity;
    BOOL isnumber;
    int startpos;
    int currpos;
    int totlen;
    FloatParserState state;
    double number;
    double fraction;
    int fractions;
    BOOL exponentplus;
    double exponent;
    double value;
};

FloatParser::FloatParser(ES_Context *context, JString *s)
{
    this->s = Storage(context, s);
    start = this->s;
    plus = TRUE;
    infinity = FALSE;
    isnumber = FALSE;
    startpos = 0;
    currpos = 0;
    totlen = Length(s);
    state = FLOAT_PARSER_STATE_START;
    number = 0;
    fraction = 0;
    fractions = 0;
    exponentplus = TRUE;
    exponent = 0;
    value = 0;
}

void
FloatParser::MatchWhiteSpace()
{
    while (currpos < totlen)
    {
        if (!uni_isspace(s[currpos]))
            break;
        currpos++;
    }
    startpos = currpos;
}

void
FloatParser::MatchSign()
{
    if (currpos < totlen)
    {
        uni_char uc = s[currpos];
        if (uc == '+')
            currpos++;
        else if (uc == '-')
        {
            currpos++;
            plus = FALSE;
        }
    }
}

BOOL
FloatParser::MatchInfinity()
{
    if (totlen - currpos < 8)
        return FALSE;
    if (uni_strncmp(&s[currpos], UNI_L("Infinity"), 8) == 0)
    {
        isnumber = TRUE;
        infinity = TRUE;
        return TRUE;
    }
    return FALSE;
}

void
FloatParser::ParseInternal()
{
    while (currpos < totlen)
    {
        uni_char uc = s[currpos];
        switch (state)
        {
        case FLOAT_PARSER_STATE_START:
            if (uc >= '0' && uc <= '9')
            {
                isnumber = TRUE;
                number = uc - '0';
                state = FLOAT_PARSER_STATE_NUMBER;
            }
            else if (uc == '.')
                state = FLOAT_PARSER_STATE_POINT;
            else
                return;
            break;
        case FLOAT_PARSER_STATE_NUMBER:
            if (uc >= '0' && uc <= '9')
                number = number * 10 + uc - '0';
            else if (uc == '.')
                state = FLOAT_PARSER_STATE_FRACTION;
            else if (uc == 'E' || uc == 'e')
                state = FLOAT_PARSER_STATE_EXPONENT_SIGN;
            else
                return;
            break;
        case FLOAT_PARSER_STATE_POINT:
            if (uc >= '0' && uc <= '9')
            {
                isnumber = TRUE;
                fractions = 1;
                fraction = uc - '0';
                state = FLOAT_PARSER_STATE_FRACTION;
            }
            else
                return;
            break;
        case FLOAT_PARSER_STATE_FRACTION:
            if (uc >= '0' && uc <= '9')
            {
                fractions += 1;
                fraction = fraction * 10 + uc - '0';
            }
            else if (uc == 'E' || uc == 'e')
                state = FLOAT_PARSER_STATE_EXPONENT_SIGN;
            else
                return;
            break;
        case FLOAT_PARSER_STATE_EXPONENT_SIGN:
            state = FLOAT_PARSER_STATE_EXPONENT;
            if (uc == '+')
            {
            }
            else if (uc == '-')
                exponentplus = FALSE;
            else
                continue;
            break;
        case FLOAT_PARSER_STATE_EXPONENT:
            if (uc >= '0' && uc <= '9')
                exponent = exponent * 10 + uc - '0';
            else
                return;
            break;
        }
        currpos++;
    }
}

void
FloatParser::Parse()
{
    MatchWhiteSpace();
    start = &s[currpos];
    MatchSign();
    if (MatchInfinity())
        return;
    ParseInternal();
    if (isnumber)
    {
        value = number;
        if (fractions > 0)
        {
            value += fraction / op_pow(10, fractions);
        }
        value *= op_pow(10, exponentplus ? exponent : -exponent);
    }
}

/* static */ JString *
ES_GlobalBuiltins::Decode(ES_Context *context, const uni_char *string, int length, const uni_char *reservedSet)
{
    if (length == 1 && *string < STRING_NUMCHARS && *string != '%')
        return context->rt_data->strings[STRING_nul + *string];

    JString *result = JString::Make(context);
    ES_CollectorLock gclock(context);

    while (length > 0)
    {
        const uni_char *start = string;
        uni_char character = *string;
        ++string;
        --length;

        if (character != '%')
        {
            Append(context, result, character);
        }
        else
        {
            if (length < 2 || !uni_isxdigit(string[0]) || !uni_isxdigit(string[1]))
                return NULL;

            unsigned int B = (ES_Lexer::HexDigitValue(string[0]) << 4) + ES_Lexer::HexDigitValue(string[1]);
            string += 2;
            length -= 2;

            if ((B & 0x80) == 0)
            {
                if (uni_strchr(reservedSet, (uni_char) B) == 0 || B == '\0')
                    Append(context, result, (uni_char) B);
                else
                    Append(context, result, start, 3);
            }
            else
            {
                int n = 1;
                while (((B << n) & 0x80) != 0)
                    ++n;

                if (n == 1 || n > 4 || length < (3 * (n - 1)))
                    return NULL;

                unsigned char Octets[4]; // ARRAY OK 2008-06-09 stanislavj
                Octets[0] = B;
                Octets[1] = Octets[2] = Octets[3] = 0;

                for (int j = 1; j < n; ++j)
                {
                    if (string[0] != '%' || !uni_isxdigit(string[1]) || !uni_isxdigit(string[2]))
                        return NULL;

                    B = ((uni_isdigit(string[1]) ? string[1]-'0' : uni_tolower(string[1])-'a'+10) << 4) +
                        (uni_isdigit(string[2]) ? string[2]-'0' : uni_tolower(string[2])-'a'+10);

                    if ((B & 0xc0) != 0x80)
                        return NULL;

                    Octets[j] = B;
                    string += 3;
                    length -= 3;
                }

                unsigned int V;

                /* Decode and check if value wasn't encoded via an overlong sequence. */
                if (n == 2)
                {
                    V = ((Octets[0] & 0x1f) << 6) | (Octets[1] & 0x3f);
                    if (V < 0x80)
                        return NULL;
                }
                else if (n == 3)
                {
                    V = ((Octets[0] & 0x0f) << 12) | ((Octets[1] & 0x3f) << 6) | (Octets[2] & 0x3f);
                    if (V < 0x800)
                        return NULL;
                }
                else
                {
                    V = ((Octets[0] & 0x07) << 18) | ((Octets[1] & 0x3f) << 12) | ((Octets[2] & 0x3f) << 6) | (Octets[3] & 0x3f);
                    if (V < 0x10000)
                        return NULL;
                }

                if (V < 0x10000)
                {
                    if (uni_strchr(reservedSet, (uni_char) V) == 0)
                        Append(context, result, (uni_char) V);
                    else
                        Append(context, result, start, 3 * n);
                }
                else if (V <= 0x10ffff)
                {
                    Append(context, result, (uni_char) ((((V - 0x10000) >> 10) & 0x3ff) + 0xd800));
                    Append(context, result, (uni_char) (((V - 0x10000) & 0x3ff) + 0xdc00));
                }
                else
                    return NULL;
            }
        }
    }

    return Finalize(context, result);
}

/* static */ BOOL
NeedToEscape(int character, const uni_char *unescapedSet)
{
    return !((character >= 'A' && character <= 'Z') ||
             (character >= 'a' && character <= 'z') ||
             (character >= '0' && character <= '9') ||
             (character != '\0' && uni_strchr(unescapedSet, character) != NULL));
}

/* static */ JString *
ES_GlobalBuiltins::Encode(ES_Context *context, const uni_char *string, int length, const uni_char *unescapedSet)
{
    if (length == 1 && *string < STRING_NUMCHARS && !NeedToEscape(*string, unescapedSet))
        return context->rt_data->strings[STRING_nul + *string];

    JString *result = JString::Make(context);
    ES_CollectorLock gclock(context);

    while (length > 0)
    {
        unsigned int character = *string;
        ++string;
        --length;

        if (!NeedToEscape(character, unescapedSet))
            Append(context, result, character);
        else
        {
            unsigned int V;

            if (character >= 0xdc00 && character <= 0xdfff)
                return NULL;

            if (character >= 0xd800 && character <= 0xdbff)
            {
                if (length == 0)
                    return NULL;

                unsigned int character2 = *string;
                ++string;
                --length;

                if (character2 < 0xdc00 || character2 > 0xdfff)
                    return NULL;

                V = (character - 0xd800) * 0x400 + (character2 - 0xdc00) + 0x10000;
            }
            else V = character;

            unsigned char c[4]; // ARRAY OK 2008-06-09 stanislavj
            int count;
            if ((V & 0x7f) == V)
            {
                count = 1;
                c[0] = (char) V & 0x7f;
            }
            else if ((V & 0x7ff) == V)
            {
                count = 2;
                c[0] = 0xc0 | ((V >> 6) & 0x1f);
                c[1] = 0x80 | (V & 0x3f);
            }
            else if ((V & 0xffff) == V)
            {
                count = 3;
                c[0] = 0xe0 | ((V >> 12) & 0x0f);
                c[1] = 0x80 | ((V >> 6) & 0x3f);
                c[2] = 0x80 | (V & 0x3f);
            }
            else
            {
                count = 4;
                c[0] = 0xf0 | ((V >> 18) & 0x07);
                c[1] = 0x80 | ((V >> 12) & 0x3f);
                c[2] = 0x80 | ((V >> 6) & 0x3f);
                c[3] = 0x80 | (V & 0x3f);
            }

            uni_char buf[13]; // ARRAY OK 2008-06-09 stanislavj
            for (int index = 0; index < count; ++index)
                uni_snprintf(buf + index*3, 4, UNI_L("%%%02X"), c[index]);

            Append(context, result, buf);
        }
    }

    return Finalize(context, result);
}

class ES_SuspendedFromRadix
    : public ES_SuspendedCall
{
public:
    ES_SuspendedFromRadix(ES_Execution_Context *context, const uni_char *val,  const uni_char** endptr, int radix, int len, double *result)
        : val(val)
        , endptr(endptr)
        , radix(radix)
        , len(len)
        , result(result)
    {
        context->SuspendedCall(this);
    }

    const uni_char *val;
    const uni_char** endptr;
    int radix;
    int len;
    double *result;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        *result = fromradix(val, endptr, radix, len);
    }
};

/* static */ BOOL
ES_GlobalBuiltins::parseInt(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *string;
    const uni_char *storage, *end, *useful_end;
    unsigned length;
    int sign, radix;

    if (argc < 1)
        goto return_nan;
    else
    {
        if (argv[0].IsNumber() && (argc < 2 || argc == 2 && argv[1].IsInt32() && argv[1].GetInt32() == 10))
            if (argv[0].IsInt32())
            {
                return_value->SetInt32(argv[0].GetInt32());
                return TRUE;
            }
            else
            {
                double number = argv[0].GetNumAsDouble();

                if (op_isnan(number) || !op_isfinite(number))
                    goto return_nan;
                else if (number == 0) // Catch -0
                {
                    return_value->SetInt32(0);
                    return TRUE;
                }
                else
                {
                    /* The spec (9.8.1) covering ToString() for numbers, requires
                       that scientific notation is not used if the decimal point
                       is in the [-6, 21) range. With

                       2 ^ (-20) < 10 ^ (-6) < 2 ^ (-19)
                       2 ^ 69 < 10 ^ 21 < 2 ^ 70

                       we check if the (base2) exponent is within [-19, 69]. If so,
                       returning the 'intpart' is correct. If not, ToString()
                       is first applied, and the digit prefix of it is then parsed
                       as an int. */

                    int base2_exp;
                    op_frexp(number, &base2_exp);
                    if (base2_exp >= -19 && base2_exp <= 69)
                    {
                        return_value->SetNumber(stdlib_intpart(number));
                        return TRUE;
                    }
                }
            }

        if (!argv[0].ToString(context))
            return FALSE;

        string = argv[0].GetString();
    }

    storage = Storage(context, string);
    length = Length(string);

    while (length != 0 && ES_Lexer::IsWhitespace(storage[0]))
    {
        ++storage;
        --length;
    }

    if (length == 0)
        goto return_nan;

    if (storage[0] == '-')
    {
        sign = -1;
        ++storage;
        --length;
    }
    else
    {
        sign = 1;
        if (storage[0] == '+')
        {
            ++storage;
            --length;
        }
    }

    if (length == 0)
        goto return_nan;

    if (argc < 2 || argv[1].IsUndefined())
    automatic_radix:
        if (length >= 2 && storage[0] == '0' && (storage[1] & ~0x20) == 'X')
        {
            if (length == 2)
                goto return_nan;
            else
                radix = 16;
        }
        else if (length > 1 && storage[0] == '0')
            radix = 8;
        else
            radix = 10;
    else
    {
        if (!argv[1].ToNumber(context))
            return FALSE;

        radix = argv[1].GetNumAsInt32();

        if (radix == 0)
            goto automatic_radix;
        else if (radix < 2 || radix > 36)
            goto return_nan;
    }

    if (radix == 16 && storage[0] == '0' && (storage[1] & ~0x20) == 'X')
    {
        storage += 2;
        length -= 2;

        if (length == 0)
            goto return_nan;
    }

    end = storage + length;
    useful_end = storage;

    while (useful_end != end && NumericValue(*useful_end) < radix)
        ++useful_end;

    if (useful_end != storage)
    {
        unsigned offset = storage - Storage(context, string);

        if (useful_end != end)
        {
            argv[0].SetString(string = JString::Make(context, string, offset, useful_end - storage));
            offset = 0;
        }

        const uni_char *endptr;
        double result;
        uni_char *start = StorageZ(context, string) + offset;

        if (context->IsUsingStandardStack())
            result = fromradix(start, &endptr, radix);
        else
            ES_SuspendedFromRadix(context, start, &endptr, radix, -1, &result);

        return_value->SetNumber(result * sign);
        return TRUE;
    }
    else
    {
return_nan:
        return_value->SetNan();
        return TRUE;
    }
}

/* static */ BOOL
ES_GlobalBuiltins::parseFloat(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;
        JString *s = argv[0].GetString();
        FloatParser parser(context, s);
        parser.Parse();
        if (parser.IsNaN())
            return_value->SetNan();
        else if (parser.IsInfinity())
        {
            if (parser.IsPlus())
                return_value->SetDouble(context->rt_data->PositiveInfinity);
            else
                return_value->SetDouble(context->rt_data->NegativeInfinity);
        }
        else
        {
            uni_char *start = parser.GetStart();
            int len = Length(s) - parser.GetStartPos();
            /* This 'no parse' condition should be caught by above, but just in case. */
            if (len > 0)
            {
                double value;
                if (context->IsUsingStandardStack())
                    value = fromradix(start, NULL, 10, len);
                else
                    ES_SuspendedFromRadix(context, start, NULL, 10, len, &value);
                return_value->SetDouble(value);
            }
            else
                return_value->SetNan();
        }
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_GlobalBuiltins::isNaN(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;
        double d = argv[0].GetNumAsDouble();
        if (op_isnan(d))
            return_value->SetTrue();
        else
            return_value->SetFalse();
    }
    else
        return_value->SetTrue();

    return TRUE;
}

/* static */ BOOL
ES_GlobalBuiltins::isFinite(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;
        double d = argv[0].GetNumAsDouble();
        if (op_isfinite(d))
            return_value->SetTrue();
        else
            return_value->SetFalse();
    }
    else
        return_value->SetFalse();

    return TRUE;
}

/* static */ BOOL
ES_GlobalBuiltins::decodeURI(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;
        JString *s = argv[0].GetString();
        ES_CollectorLock gclock(context);
        JString *encoded = Decode(context, StorageZ(context, s), Length(s), UNI_L(";/?:@&=+$,#"));
        if (encoded != NULL)
            return_value->SetString(encoded);
        else
        {
            context->ThrowURIError("Malformed URI");
            return FALSE;
        }
    }
    else
        return_value->SetString(context->rt_data->strings[STRING_undefined]);

    return TRUE;
}

/* static */ BOOL
ES_GlobalBuiltins::decodeURIComponent(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;
        JString *s = argv[0].GetString();
        ES_CollectorLock gclock(context);
        JString *encoded = Decode(context, StorageZ(context, s), Length(s), UNI_L(""));
        if (encoded != NULL)
            return_value->SetString(encoded);
        else
        {
            context->ThrowURIError("Malformed URI");
            return FALSE;
        }
    }
    else
        return_value->SetString(context->rt_data->strings[STRING_undefined]);

    return TRUE;
}

/* static */ BOOL
ES_GlobalBuiltins::encodeURI(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;
        JString *s = argv[0].GetString();
        ES_CollectorLock gclock(context);
        JString *encoded = Encode(context, StorageZ(context, s), Length(s), UNI_L(";/?:@&=+$,-_.!~*'()#"));
        if (encoded != NULL)
            return_value->SetString(encoded);
        else
        {
            context->ThrowURIError("Malformed URI");
            return FALSE;
        }
    }
    else
        return_value->SetString(context->rt_data->strings[STRING_undefined]);

    return TRUE;
}

/* static */ BOOL
ES_GlobalBuiltins::encodeURIComponent(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;
        JString *s = argv[0].GetString();
        ES_CollectorLock gclock(context);
        JString *encoded = Encode(context, StorageZ(context, s), Length(s), UNI_L("-_.!~*'()"));
        if (encoded != NULL)
            return_value->SetString(encoded);
        else
        {
            context->ThrowURIError("Malformed URI");
            return FALSE;
        }
    }
    else
        return_value->SetString(context->rt_data->strings[STRING_undefined]);

    return TRUE;
}

enum FastPathResult
{
    RES_THROW,
    RES_OK,
    RES_NOT_APPLICABLE
};

static FastPathResult
EvalFastPath(ES_Execution_Context *context, ES_Global_Object *global_object, ES_Value_Internal *return_value, ES_FunctionCode **closures, JString *name, BOOL is_plain_ident)
{
    if (closures[0]->klass)
    {
        ES_PropertyIndex index;

        if (closures[0]->klass->Find(name, index, closures[0]->klass->Count()))
        {
            ES_Value_Internal &reg = context->GetCallingRegisterFrame()[2 + index];

            if (is_plain_ident)
            {
                *return_value = reg;
                return RES_OK;
            }
            else if (reg.IsObject() && reg.GetObject()->IsFunctionObject())
            {
                ES_Value_Internal *registers = context->SetupFunctionCall(reg.GetObject(), 0);

                registers[0].SetObject(global_object);
                registers[1].SetObject(reg.GetObject());

                return context->CallFunction(registers, 0, return_value) ? RES_OK : RES_THROW;
            }
        }
    }

    return RES_NOT_APPLICABLE;
}

class ES_SuspendedAllocateParser
    : public ES_SuspendedCall
{
public:
    ES_SuspendedAllocateParser(ES_Execution_Context *context, ES_Global_Object *global_object)
        : global_object(global_object)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        result = OP_NEW(ES_Parser, (context, global_object, TRUE));
    }

    ES_Parser *result;

private:
    ES_Global_Object *global_object;
};

#ifdef ECMASCRIPT_DEBUGGER
class ES_SuspendedAllocateFormatter
    : public ES_SuspendedCall
{
public:
    ES_SuspendedAllocateFormatter(ES_Execution_Context *context)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        result = OP_NEW(ES_Formatter, (NULL, context, true));
    }

    ES_Formatter *result;
};

class ES_SuspendedWantReformatScript
    : public ES_SuspendedCall
{
public:
    ES_SuspendedWantReformatScript(ES_Execution_Context *context, const uni_char *string, unsigned length)
        : string(string),
          length(length),
          result(FALSE)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        result = g_ecmaManager->GetWantReformatScript(context->GetRuntime(), string, length);
    }

    const uni_char *string;
    unsigned length;
    BOOL result;
};
#endif // ECMASCRIPT_DEBUGGER

/* static */ BOOL
ES_GlobalBuiltins::Eval(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, ES_Code *code, ES_CodeWord *instruction)
{
    if (argc == 0)
    {
        return_value->SetUndefined();
        return TRUE;
    }
    else if (!argv[0].IsString())
    {
        *return_value = argv[0];
        return TRUE;
    }
    else
    {
        ES_Object *this_object;

        if (code)
            this_object = NULL;
        else
            if (argv[-2].IsObject())
            {
                this_object = argv[-2].GetObject(context);
                if (!this_object->IsGlobalObject())
                    this_object = context->GetGlobalObject();
            }
            else
                this_object = context->GetGlobalObject();

        JString *source = argv[0].GetString();
        ES_Code *eval_code;

        if (code && code->eval_caches[instruction[4].index].source == source && !context->IsDebugged())
            eval_code = code->eval_caches[instruction[4].index].code;
        else
        {
            ES_FunctionCode *closures[1];
            unsigned closures_count = 0;

            if (ES_Code *calling_code = context->GetCallingCode(TRUE))
                if (!this_object)
                    if (calling_code->type == ES_Code::TYPE_FUNCTION)
                    {
                        closures[0] = static_cast<ES_FunctionCode *>(calling_code);
                        ++closures_count;
                    }

            const uni_char *body = Storage(context, source);
            unsigned body_length = Length(source);

            if (body_length != 0 && ES_Lexer::IsIdentifierStart(*body))
            {
                const uni_char *ptr = body + 1, *ptr_end = body + body_length;
                while (ptr != ptr_end && ES_Lexer::IsIdentifierPart(*ptr))
                    ++ptr;
                unsigned identifier_length = ptr - body;
                BOOL is_plain_ident = ptr == ptr_end, is_simple_call = FALSE;
                if (ptr + 2 == ptr_end && ptr[0] == '(' && ptr[1] == ')')
                    is_simple_call = TRUE;
                if (is_plain_ident || is_simple_call)
                {
                    if (closures_count == 1)
                    {
                        JString *name = JString::Make(context, source, 0, identifier_length);
                        FastPathResult res = EvalFastPath(context, ES_GET_GLOBAL_OBJECT(), return_value, closures, name, is_plain_ident);
                        if (res == RES_OK)
                            return TRUE;
                        else if (res == RES_THROW)
                            return FALSE;
                    }
                }
            }

#ifdef ECMASCRIPT_DEBUGGER
            if (context->IsDebugged() && ES_SuspendedWantReformatScript(context, body, body_length).result)
            {
                ES_Formatter *formatter = ES_SuspendedAllocateFormatter(context).result;

                if (!formatter)
                    context->AbortOutOfMemory();

                ES_SuspendedStackAutoPtr<ES_Formatter> formatter_anchor(context, formatter);

                ES_SuspendedFormatProgram suspended_format_program(*formatter, source);

                /* Original source is replaced with reformatted source on success. */
                context->SuspendedCall(&suspended_format_program);
            }
#endif // ECMASCRIPT_DEBUGGER

            ES_Parser *parser = ES_SuspendedAllocateParser(context, ES_GET_GLOBAL_OBJECT()).result;

            if (!parser)
                context->AbortOutOfMemory();

            ES_SuspendedStackAutoPtr<ES_Parser> parser_anchor(context, parser);
            unsigned register_frame_offset = 0;
            unsigned calling_code_first_temporary_register = UINT_MAX;

            if (ES_Code *calling_code = context->GetCallingCode())
            {
                calling_code_first_temporary_register = calling_code->data->first_temporary_register;
                if (!this_object && instruction)
                {
                    unsigned scope_idx = instruction[3].index;
                    if (scope_idx < UINT_MAX - 1)
                        parser->SetInnerScope(calling_code->data->inner_scopes[scope_idx].registers, calling_code->data->inner_scopes[scope_idx].registers_count);
#if 1
                    else if (scope_idx == UINT_MAX - 1)
                        parser->SetIsSimpleEval();
#endif // 0
                }

                register_frame_offset = calling_code->data->register_frame_size;
            }

            parser->SetClosures(closures, closures_count);
            parser->SetRegisterFrameOffset(register_frame_offset);

#ifdef ECMASCRIPT_DEBUGGER
            parser->SetIsDebugging(context->IsDebugged());
            parser->SetScriptType(SCRIPT_TYPE_EVAL);
#endif // ECMASCRIPT_DEBUGGER

            bool is_strict_mode = code ? code->data->is_strict_mode : false;

            ES_SuspendedParseProgram suspended_parse_program(*parser, source, return_value, is_strict_mode);

            context->SuspendedCall(&suspended_parse_program);

            if (!suspended_parse_program.success)
            {
                if (OpStatus::IsMemoryError(suspended_parse_program.status))
                    context->AbortOutOfMemory();

                if (suspended_parse_program.message)
                    context->ThrowSyntaxError(suspended_parse_program.message);
                else
                    context->ThrowSyntaxError("eval: program didn't parse");

                return FALSE;
            }

            eval_code = suspended_parse_program.code;

            if (!eval_code)
                goto finished;

            is_strict_mode = eval_code->data->is_strict_mode;

            if (is_strict_mode)
            {
                eval_code->type = ES_Code::TYPE_FUNCTION;
                /* Resolution of #caller looks past strict eval() contexts,
                   so record this function code as having that origin. */
                eval_code->is_strict_eval = TRUE;
            }
            else
            {
                if (calling_code_first_temporary_register != UINT_MAX)
                    eval_code->data->first_temporary_register = calling_code_first_temporary_register;

                eval_code->type = !this_object ? ES_Code::TYPE_EVAL_PLAIN : ES_Code::TYPE_EVAL_ODD;
            }

#ifdef ES_DISASSEMBLER_SUPPORT
#ifdef _STANDALONE
            extern BOOL g_disassemble_eval;
            if (g_disassemble_eval)
            {
                ES_Disassembler disassembler(context);

                if (eval_code->GCTag() == GCTAG_ES_FunctionCode)
                    disassembler.Disassemble(static_cast<ES_FunctionCode *>(eval_code));
                else
                    disassembler.Disassemble(static_cast<ES_ProgramCode *>(eval_code));

                unsigned length = disassembler.GetLength();
                const uni_char *unicode = disassembler.GetOutput();
                char *ascii = OP_NEWA(char, length + 1);
                if (!ascii)
                    context->AbortOutOfMemory();

                for (unsigned index = 0; index < length + 1; ++index)
                    ascii[index] = (char) unicode[index];

                fprintf(stdout, "--------------------------------------------------------------------------------\n%s--------------------------------------------------------------------------------\n", ascii);
            }
#endif // _STANDALONE
#endif // ES_DISASSEMBLER_SUPPORT

            if (code && !context->IsDebugged() && !is_strict_mode)
            {
                code->eval_caches[instruction[4].index].source = source;
                code->eval_caches[instruction[4].index].code = eval_code;
            }
        }

        if (eval_code->GCTag() == GCTAG_ES_ProgramCode)
            return context->Evaluate(eval_code, return_value, this_object);
        else
        {
            ES_CodeStatic::InnerScope *scope = NULL;

            if (ES_Code *calling_code = instruction ? context->GetCallingCode() : NULL)
            {
                unsigned scope_idx = instruction[3].index;

                if (scope_idx < UINT_MAX - 1)
                    scope = &calling_code->data->inner_scopes[scope_idx];
            }

            ES_FunctionCode *eval_fncode = static_cast<ES_FunctionCode *>(eval_code);
            ES_Function *eval_fn = NULL;
            {
                ES_StackPointerAnchor anchor_fncode(context, eval_fncode);
                eval_fn = context->NewFunction(eval_fncode, scope);
            }

            ES_Value_Internal *registers = context->SetupFunctionCall(eval_fn, 0);

            registers[0] = !code ? context->GetGlobalObject() : context->Registers()[0];
            registers[1] = eval_fn;

            return context->CallFunction(registers, argc, return_value);
        }
    }

finished:
    context->heap->CollectIfNeeded(context);
    return TRUE;
}

/* static */ BOOL
ES_GlobalBuiltins::eval(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return Eval(context, argc, argv, return_value, NULL, NULL);
}

/* static */ JString *
ES_GlobalBuiltins::Unescape(ES_Context *context, const uni_char *string, int length)
{
    if (length == 1 && *string < STRING_NUMCHARS && *string != '%')
        return context->rt_data->strings[STRING_nul + *string];

    JString *result = JString::Make(context);
    ES_CollectorLock gclock(context);

    while (length > 0)
    {
        uni_char character = *string;
        ++string;
        --length;

        if (character != '%')
        {
            Append(context, result, character);
        }
        else
        {
            unsigned c = '%';

            if (length >= 5)
            {
                if (string[0] == 'u' && uni_isxdigit(string[1]) && uni_isxdigit(string[2])
                    && uni_isxdigit(string[3]) && uni_isxdigit(string[4]))
                {
                    c = 0;
                    for (int i = 1; i < 5; i++)
                    {
                        OP_ASSERT(ES_Lexer::IsHexDigit(string[i]));
                        c += ES_Lexer::HexDigitValue(string[i]) << 4*(4 - i);
                    }
                    string += 5;
                    length -= 5;
                }
                else
                    goto step_14;
            }
            else if (length >= 2)
            {
            step_14:
                if (uni_isxdigit(string[0]) && uni_isxdigit(string[1]))
                {
                    c = 0;
                    for (int i = 0; i < 2; i++)
                    {
                        OP_ASSERT(ES_Lexer::IsHexDigit(string[i]));
                        c += ES_Lexer::HexDigitValue(string[i]) << 4*(1 - i);
                    }
                    string += 2;
                    length -= 2;
                }
            }

            Append(context, result, c);
        }
    }
    return Finalize(context, result);
}

/* static */ JString *
ES_GlobalBuiltins::Escape(ES_Context *context, const uni_char *string, int length, const uni_char *unescapedSet)
{
    if (length == 1 && *string < STRING_NUMCHARS && !NeedToEscape(*string, unescapedSet))
        return context->rt_data->strings[STRING_nul + *string];

    JString *result = JString::Make(context);
    ES_CollectorLock gclock(context);

    while (length > 0)
    {
        unsigned int character = *string;
        ++string;
        --length;

        if (!NeedToEscape(character, unescapedSet))
            Append(context, result, character);
        else
        {
            uni_char buf[7];
            if (character < 256)
                uni_snprintf(buf, 4, UNI_L("%%%02X"), (unsigned char) character);
            else
                uni_snprintf(buf, 7, UNI_L("%%u%02X%02X"), (unsigned char) (character >> 8), (unsigned char) character);

            Append(context, result, buf);
        }
    }

    return Finalize(context, result);
}

/* static */ BOOL
ES_GlobalBuiltins::escape(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;
        JString *s = argv[0].GetString();
        ES_CollectorLock gclock(context);
        JString *encoded = Escape(context, StorageZ(context, s), Length(s), UNI_L("-_.@*/+"));
        if (encoded != NULL)
            return_value->SetString(encoded);
        else
            return_value->SetNull();
    }
    else
        return_value->SetString(context->rt_data->strings[STRING_undefined]);

    return TRUE;
}

/* static */ BOOL
ES_GlobalBuiltins::unescape(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToString(context))
            return FALSE;
        JString *s = argv[0].GetString();
        ES_CollectorLock gclock(context);
        JString *encoded = Unescape(context, StorageZ(context, s), Length(s));
        if (encoded != NULL)
            return_value->SetString(encoded);
        else
            return_value->SetNull();
    }
    else
        return_value->SetString(context->rt_data->strings[STRING_undefined]);

    return TRUE;
}

/* static */ BOOL
ES_GlobalBuiltins::ThrowTypeError(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    context->ThrowTypeError("Illegal property access");
    return FALSE;
}

/* static */ void
ES_GlobalBuiltins::PopulateGlobalObject(ES_Context *context, ES_Global_Object *global_object)
{
    JString **idents = context->rt_data->idents;
    ES_Function *fn_parseInt = MAKE_BUILTIN(2, parseInt);
#ifdef ES_NATIVE_SUPPORT
    fn_parseInt->SetFunctionID(ES_BUILTIN_FN_parseInt);
#endif // ES_NATIVE_SUPPORT
    global_object->InitPropertyL(context, idents[ESID_parseInt], fn_parseInt, DE, ES_STORAGE_OBJECT);
    global_object->InitPropertyL(context, idents[ESID_parseFloat], MAKE_BUILTIN(1, parseFloat), DE, ES_STORAGE_OBJECT);
    global_object->InitPropertyL(context, idents[ESID_isNaN], MAKE_BUILTIN(1, isNaN), DE, ES_STORAGE_OBJECT);
    global_object->InitPropertyL(context, idents[ESID_isFinite], MAKE_BUILTIN(1, isFinite), DE, ES_STORAGE_OBJECT);
    global_object->InitPropertyL(context, idents[ESID_decodeURI], MAKE_BUILTIN(1, decodeURI), DE, ES_STORAGE_OBJECT);
    global_object->InitPropertyL(context, idents[ESID_decodeURIComponent], MAKE_BUILTIN(1, decodeURIComponent), DE, ES_STORAGE_OBJECT);
    global_object->InitPropertyL(context, idents[ESID_encodeURI], MAKE_BUILTIN(1, encodeURI), DE, ES_STORAGE_OBJECT);
    global_object->InitPropertyL(context, idents[ESID_encodeURIComponent], MAKE_BUILTIN(1, encodeURIComponent), DE, ES_STORAGE_OBJECT);
    ES_Function *fn_eval = MAKE_BUILTIN(1, eval);
#ifdef ES_NATIVE_SUPPORT
    fn_eval->SetFunctionID(ES_BUILTIN_FN_eval);
#endif // ES_NATIVE_SUPPORT
    global_object->InitPropertyL(context, idents[ESID_eval], fn_eval, DE, ES_STORAGE_OBJECT);
    global_object->InitPropertyL(context, idents[ESID_escape], MAKE_BUILTIN(1, escape), DE, ES_STORAGE_OBJECT);
    global_object->InitPropertyL(context, idents[ESID_unescape], MAKE_BUILTIN(1, unescape), DE, ES_STORAGE_OBJECT);
    ES_Value_Internal value;
    value.SetNan();
    global_object->InitPropertyL(context, idents[ESID_NaN], value, RO | DE | DD);
    value.SetNumber(context->rt_data->PositiveInfinity);
    global_object->InitPropertyL(context, idents[ESID_Infinity], value, RO | DE | DD);
    value.SetUndefined();
    global_object->InitPropertyL(context, idents[ESID_undefined], value, RO | DE | DD);
}
