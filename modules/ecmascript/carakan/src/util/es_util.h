/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999 - 2005
 *
 * Miscellaneous utilities
 * Lars T Hansen
 */

#ifndef ES_UTIL_H
#define ES_UTIL_H

double fromradix(const uni_char* val, const uni_char** endptr, int radix, int len = -1);
    /**< Convert a string representing an integer, possibly followed by
         non-numeric data, to a double, and compute the address of the
         first character of the input that is not part of the number. */

double tonumber(ES_Context *context, const uni_char* val, unsigned val_length, BOOL skip_trailing_characters = FALSE, BOOL parsefloat_semantics = FALSE);
    /**< Convert a non-NUL-terminated string representing a number to a
         double. Return NaN if the input does not make sense.
         @param skip_trailing_characters  If TRUE, then other characters than
                blank are allowed following the characters taken for the number.
         @param parsefloat_semantics  If TRUE, then only decimal numbers are
                accepted and the empty string is parsed as NaN. */

BOOL convertindex(const uni_char* val, unsigned val_length, unsigned &index);
    /**< Converts a non-NUL-terminated string exactly representing an unsigned
         integer in the range [0, UINT_MAX - 1] to an unsigned and return TRUE,
         or return FALSE if the string doesn't exactly represent such an
         unsigned integer. */

void tostring(ES_Context *context, double val, char *buffer);
    /**< Convert a number to its ECMAScript string representation using
         the standard ToString algorithm (9.8.1 of the spec).

         The buffer must be at least 32 bytes, which is large enough to
         hold any value 'val'. */

JString* tostring(ES_Context *context, double val);
    /**< @see tostring */

JString* tostring(ES_Execution_Context *context, double val);
    /**< @see tostring */

JString* ecma_numbertostring(ES_Context *context, double x, int radix);
JString* ecma_suspended_numbertostring(ES_Execution_Context *context, double x, int radix);


double unsigned2double(unsigned x);
    /**< Safe conversion of 'unsigned' to 'double'.  May be slower than code
         in the platform libraries; is usually only used when the platform
         libraries are known to be bad. */

/* MIN and MAX functions.
 *
 * There are type-specific functions here in order to avoid implicit conversions
 * from int to double, since that does not work properly on some platforms.
 */
inline double
es_mind(double a, double b)
{
	return a < b ? a : b;
}

inline unsigned long int
es_minul(unsigned long a, unsigned long b)
{
	return a < b ? a : b;
}

inline unsigned
es_minu(unsigned a, unsigned b)
{
	return a < b ? a : b;
}

inline int
es_mini(int a, int b)
{
	return a < b ? a : b;
}

inline size_t
es_minz(size_t a, size_t b)
{
	return a < b ? a : b;
}

inline double
es_maxd(double a, double b)
{
	return a < b ? b : a;
}

inline unsigned long int
es_maxul(unsigned long int a, unsigned long int b)
{
	return a < b ? b : a;
}

inline unsigned
es_maxu(unsigned a, unsigned b)
{
	return a < b ? b : a;
}

inline int
es_maxi(int a, int b)
{
	return a < b ? b : a;
}

inline size_t
es_maxz(size_t a, size_t b)
{
	return a < b ? b : a;
}

/** Array of ES_Boxed*, allocated on the garbage-collected heap */

class ES_Boxed_Array
    : public ES_Boxed
{
public:
    unsigned nslots; // Number of slots allocated. 'unsigned short' is insufficient.
    unsigned nused; // Number of slots used.
    union {
        ES_Boxed *slots[1];
        UINTPTR uints[1]; // Used to store scalar data in the reserved but unused slots;
        INTPTR ints[1]; // Watch out if we go for a compacting GC or one that might trim in-place unused slots!
    };
    // MUST BE LAST FIELD.  More than one slot is usually allocated here.

    static ES_Boxed_Array *Make(ES_Context *context, unsigned nslots);
    static ES_Boxed_Array *Make(ES_Context *context, unsigned nslots, unsigned nused);
    static ES_Boxed_Array *Grow(ES_Context *context, ES_Boxed_Array *old);
};

class ES_Boxed_List
    : public ES_Boxed
{
public:
    ES_Boxed *head;
    ES_Boxed_List *tail;

    static ES_Boxed_List *Make(ES_Context *context, ES_Boxed *head = NULL, ES_Boxed_List *tail = NULL);
    static void Initialize(ES_Boxed_List *self, ES_Boxed *head, ES_Boxed_List *tail);
};

#endif // ES_UTIL_H
