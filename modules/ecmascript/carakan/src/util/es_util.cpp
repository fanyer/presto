/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2011
 *
 * Utility functions for ECMAScript
 *
 * This file is reserved for very infrastructure-level functionality,
 * do not pollute it with references to JS_Value or Object types.
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_lexer.h"
#include "modules/stdlib/include/double_format.h"

static void
insertdecimalpoint(char *buffer, int steps)
{
    OP_ASSERT(steps > 0 && steps <= 2);
    int len = op_strlen(buffer);
    OP_ASSERT(len > 0);
    int num_digits = (buffer[0] == '-') ? len - 1 : len;
    int distance = 1;
    if (steps >= num_digits)
        distance += 1 + steps - num_digits;
    int num_to_move = steps < num_digits ? steps : num_digits;
    int insert_pos = len - num_to_move;

    for (int i = len; i >= insert_pos; --i)
        buffer[i + distance] = buffer[i];

    if (distance >= 2)
        buffer[insert_pos++] = '0';
    buffer[insert_pos++] = '.';
    if (distance >= 3)
        buffer[insert_pos] = '0';
}

class ES_SuspendedFormatDouble
    : public ES_SuspendedCall
{
public:
    ES_SuspendedFormatDouble(ES_Execution_Context *context, char *buffer, double value)
        : buffer(buffer)
        , value(value)
    {
        context->SuspendedCall(this);
    }

    char *buffer;
    double value;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        OpDoubleFormat::ToString(buffer, value);
    }
};

// According to section 9.8.1 of the spec
JString*
tostring(ES_Context *context, double val)
{
    // sect 9.8.1

    if (op_isnan(val))
        return JString::Make(context, "NaN");
    else if (val == 0.0)    // Handle -0 case
        return JString::Make(context, "0");
    else if (ES_Execution_Context *exec_context = context->GetExecutionContext())
        return tostring(exec_context, val);
    else
    {
        char buffer[32]; // ARRAY OK 2011-06-08 sof

        OpDoubleFormat::ToString(buffer, val);

        return JString::Make(context, buffer);
    }
}

JString*
tostring(ES_Execution_Context *context, double val)
{
    // sect 9.8.1

    if (op_isnan(val))
        return JString::Make(context, "NaN");
    else if (val == 0.0)    // Handle -0 case
        return JString::Make(context, "0");
    else
    {
        JString *result = context->GetNumberToStringCache(val);

        if (!result)
        {
            char buffer[32]; // ARRAY OK 2011-06-08 sof

            if (op_isintegral(val) && val >= INT_MIN && val <= INT_MAX)
                op_itoa(static_cast<int>(val), buffer, 10);
            else
            {
                double val10 = val * 10;
                if (op_isintegral(val10) && val10 >= INT_MIN && val10 <= INT_MAX)
                {
                    op_itoa(static_cast<int>(val10), buffer, 10);
                    insertdecimalpoint(buffer, 1);
                }
                else
                {
                    double val100 = val * 100;
                    if (op_isintegral(val100) && val100 >= INT_MIN && val100 <= INT_MAX)
                    {
                        op_itoa(static_cast<int>(val100), buffer, 10);
                        insertdecimalpoint(buffer, 2);
                    }
                    else
                        ES_SuspendedFormatDouble(context, buffer, val);
                }
            }

            result = JString::Make(context, buffer);

            context->SetNumberToStringCache(val, result);
        }

        return result;
    }
}

void
tostring(ES_Context *context, double val, char *buffer)
{
    // Section 9.8.1

    if (op_isnan(val))
        op_strcpy(buffer, "NaN");
    else if (val == 0.0)    // Handle -0 case
    {
        buffer[0] = '0';
        buffer[1] = 0;
    }
    else
    {
        JString *result = NULL;
        if (ES_Execution_Context *exec_context = context->GetExecutionContext())
            result = exec_context->GetNumberToStringCache(val);

        if (!result)
            if (op_isintegral(val) && val >= INT_MIN && val <= INT_MAX)
                op_itoa(static_cast<int>(val), buffer, 10);
            else
            {
                double val10 = val * 10;
                if (op_isintegral(val10) && val10 >= INT_MIN && val10 <= INT_MAX)
                {
                    op_itoa(static_cast<int>(val10), buffer, 10);
                    insertdecimalpoint(buffer, 1);
                }
                else
                {
                    double val100 = val * 100;
                    if (op_isintegral(val100) && val100 >= INT_MIN && val100 <= INT_MAX)
                    {
                        op_itoa(static_cast<int>(val100), buffer, 10);
                        insertdecimalpoint(buffer, 2);
                    }
                    else
                        if (ES_Execution_Context *exec_context = context->GetExecutionContext())
                            ES_SuspendedFormatDouble(exec_context, buffer, val);
                        else
                            OpDoubleFormat::ToString(buffer, val);
                }
            }
        else
            uni_cstrlcpy(buffer, StorageZ(context, result), 32);
    }
}

double
fromradix(const uni_char* val, const uni_char** endptr, int radix, int len)
{
    if (radix == 10)
    {
        /* Fast path: attempt to convert it as an int(egral) first. */
        if (len >= 0 && len < 10)
        {
            BOOL sign = FALSE;
            unsigned i = 0;
            if (val[0] == '-')
            {
                sign = TRUE;
                i = 1;
            }

            int result = 0;
            for (; i < static_cast<unsigned>(len) && val[i] >= '0' && val[i] <= '9'; i++)
                result = result * 10 + (val[i] - '0');

            if (i == static_cast<unsigned>(len))
            {
                if (endptr)
                    *endptr = val + i;
                return sign ? -result : result;
            }
        }
        // Extremely correct code
        if (len >= 0)
            return uni_strntod(val,(uni_char**)endptr, len);
        else
            return uni_strtod(val,(uni_char**)endptr);
    }
    else
    {
        // More-or-less correct code
        const uni_char* end = val;
        double number;

#ifdef ES_USE_UINT64_IN_DOUBLE_CONVERSION
        UINT64 number64 = 0;

        for (; number64 * radix / radix == number64; ++end) {
            uni_char letter = *end;

            if (letter >= 'a' && letter <= 'z')
                letter -= 'a' - 10;
            else
                if (letter >= 'A' && letter <= 'Z')
                    letter -= 'A' - 10;
                else
                    if (letter >= '0' && letter <= '9')
                        letter -= '0';
                    else
                    {
                        number = static_cast<double>(number64);
                        goto stop;
                    }

            if (letter >= radix || len-- == 0)
            {
                number = static_cast<double>(number64);
                goto stop;
            }

            number64 = number64 * radix + letter;
        }

        number = static_cast<double>(number64);
#else
        number = 0;
#endif // ES_USE_UINT64_IN_DOUBLE_CONVERSION

        for (;; ++end) {
            uni_char letter = *end;

            if (letter >= 'a' && letter <= 'z')
                letter -= 'a' - 10;
            else
                if (letter >= 'A' && letter <= 'Z')
                    letter -= 'A' - 10;
                else
                    if (letter >= '0' && letter <= '9')
                        letter -= '0';
                    else
                        break;

            if (letter >= radix || len-- == 0)
                break;

            number = number * radix + letter;
        }

#ifdef ES_USE_UINT64_IN_DOUBLE_CONVERSION
    stop:
#endif // ES_USE_UINT64_IN_DOUBLE_CONVERSION
        if (endptr)
            *endptr = end;

        return number;
    }
}

class ES_SuspendedStringToDouble
    : public ES_SuspendedCall
{
public:
    ES_SuspendedStringToDouble(ES_Execution_Context *context, uni_char *buffer, uni_char **endptr, double *result)
        : buffer(buffer)
        , endptr(endptr)
        , result(result)
    {
        context->SuspendedCall(this);
    }

    uni_char *buffer;
    uni_char **endptr;
    double *result;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        *result = uni_strtod(buffer, endptr);
    }
};

double
tonumber(ES_Context *context, const uni_char* val, unsigned val_length, BOOL skip_trailing_characters, BOOL parsefloat_semantics)
{
    // sect 9.3.1

    double number = 0.0;
    BOOL negative = FALSE;

    // Strip off leading whitespace

    while (val_length != 0 && ES_Lexer::IsWhitespace(*val))
    {
        ++val;
        --val_length;
    }

    if (val_length == 0)
    {
        if (parsefloat_semantics)
            return context->rt_data->NaN;    // 15.1.2.3
        else
            return 0;
    }

    if (*val == '-')
    {
        negative = TRUE;
        if (--val_length == 0)
            return context->rt_data->NaN;
        ++val;
    }
    else if (*val == '+')
    {
        if (--val_length == 0)
            return context->rt_data->NaN;
        ++val;
    }

    if (val_length >= 8 && op_memcmp(val, UNI_L("Infinity"), 16) == 0)
    {
        val += 8;
        val_length -= 8;
        number = context->rt_data->PositiveInfinity;
    }
    else if (*val >= '0' && *val <= '9' || *val == '.')
    {
        ES_Box* box = ES_Box::Make(context, sizeof(uni_char) * (val_length + 1));
        uni_char* valz = reinterpret_cast<uni_char*>(box->Unbox());

        if (!valz)
            return context->rt_data->NaN;

        op_memcpy(valz, val, val_length * sizeof(uni_char));
        valz[val_length] = 0;

        uni_char* endptr;

        if (val[0] == '0' && (val[1] == 'x' || val[1] == 'X') && !parsefloat_semantics)
            number = fromradix(valz + 2, (const uni_char**) &endptr, 16);
        else if (ES_Execution_Context *exec_context = context->GetExecutionContext())
            ES_SuspendedStringToDouble(exec_context, valz, &endptr, &number);
        else
            number = uni_strtod(valz, &endptr);

        unsigned len = endptr - valz;

        context->heap->Free(box);

        if (len == 0)
            return context->rt_data->NaN;

        val_length -= len;
        val += len;
    }
    else
        return context->rt_data->NaN;

    if (!skip_trailing_characters)
    {
        // Strip off trailing whitespace

        while (val_length != 0 && ES_Lexer::IsWhitespace(*val))
        {
            ++val;
            --val_length;
        }

        if (val_length != 0)
            // If we get here, something's wrong

            number = context->rt_data->NaN;
    }

    if (negative)
        number = -number;

    return number;
}


BOOL convertindex(const uni_char* val, unsigned val_length, unsigned &index)
{
    if (val_length > 0 && val_length <= 10)
    {
        unsigned i = 0, l = es_minu(val_length, 9);

        BOOL first = TRUE;
        while (l-- != 0)
            if (*val >= '0' && *val <= '9' && (!first || *val != '0' || l == 0))
            {
                first = FALSE;
                i = i * 10 + (*val++ - '0');
            }
            else
                return FALSE;

        if (val_length == 10)
            if (*val >= '0' && (i < 429496729 && *val <= '9' || i == 429496729 && *val <= '4'))
                i = i * 10 + (*val - '0');
            else
                return FALSE;

        index = i;
        return TRUE;
    }
    else
        return FALSE;
}

#ifdef ECMASCRIPT_INTEGRAL_TO_FLOATING
double unsigned2double(unsigned int x)
{
    // The usual problem here is that numbers that have the high bit
    // set are interpreted as "negative" because the platform is not
    // able to handle unsigned numbers properly, only signed numbers.

    if (x >= 0x80000000U)
    {
        double d = (double)(x - 0x80000000U);
        d += 2147483648.0;
        return d;
    }
    else
        return (double)x;
}
#endif // ECMASCRIPT_INTEGRAL_TO_FLOATING

inline void
Initialize(ES_Boxed_Array *self, unsigned int nslots, unsigned int nused)
{
    self->InitGCTag(GCTAG_ES_Boxed_Array);
    self->nslots = nslots;
    self->nused = nused;
    for (unsigned int i=0 ; i < nslots ; i++)
        self->slots[i] = NULL;
}

/* static */ ES_Boxed_Array*
ES_Boxed_Array::Make(ES_Context *context, unsigned int nslots)
{
    ES_Boxed_Array* p;
    unsigned int extrabytes = nslots > 0 ? (nslots-1)*sizeof(ES_Boxed*) : 0;
    GC_ALLOCATE_WITH_EXTRA(context, p, extrabytes, ES_Boxed_Array, (p, nslots, nslots));
#ifdef ES_DBG_GC
    GC_DDATA.demo_ES_Boxed_Array[ilog2(sizeof(ES_Boxed_Array)+extrabytes)]++;
#endif
    return p;
}

/* static */ ES_Boxed_Array*
ES_Boxed_Array::Make(ES_Context *context, unsigned int nslots, unsigned int nused)
{
    ES_Boxed_Array* p;
    unsigned int extrabytes = nslots > 0 ? (nslots-1)*sizeof(ES_Boxed*) : 0;
    GC_ALLOCATE_WITH_EXTRA(context, p, extrabytes, ES_Boxed_Array, (p, nslots, nused));
#ifdef ES_DBG_GC
    GC_DDATA.demo_ES_Boxed_Array[ilog2(sizeof(ES_Boxed_Array)+extrabytes)]++;
#endif
    return p;
}

/* static */ ES_Boxed_Array *
ES_Boxed_Array::Grow(ES_Context *context, ES_Boxed_Array *old)
{
    unsigned nslots = old->nslots * 2;
    ES_Boxed_Array *p = Make(context, nslots, old->nused);
    op_memcpy(p->slots, old->slots, old->nused * sizeof(ES_Boxed *));
    return p;
}

/*static*/ ES_Boxed_List *
ES_Boxed_List::Make(ES_Context *context, ES_Boxed *head, ES_Boxed_List *tail)
{
    ES_Boxed_List *cons;
    GC_ALLOCATE(context, cons, ES_Boxed_List, (cons, head, tail));
    return cons;
}

/*static*/ void
ES_Boxed_List::Initialize(ES_Boxed_List *self, ES_Boxed *head, ES_Boxed_List *tail)
{
    self->InitGCTag(GCTAG_ES_Boxed_List);
    self->head = head;
    self->tail = tail;
}
