/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999-2006
 *
 * Primitive ECMAScript 'string' datatype.
 *
 * Generally the names 'string' and 'String' collide with too much in various
 * platforms' standard libraries and headers, so we use 'jstring' everywhere.
 *
 * JString uses two-level storage: a string node contains a pointer to a
 * storage node.  The two levels allow string concatenation to happen in-place
 * in many cases, which is important for the efficiency of code that creates
 * long strings through concatenating small strings onto the long string, but
 * it generally sucks for the space efficiency of constant strings.
 *
 * @author Lars T Hansen
 */

#ifndef ES_STRING_H
#define ES_STRING_H

/* DJ Bernstein's hashing function: simple and effective. */
#define ES_HASH_INIT(x) x = 5381
#define ES_HASH_UPDATE(x, c) x = ((x << 5) + x) + c

class JString;

class JStringStorage : public ES_Boxed
{
public:
    unsigned allocated;  // Amount of storage allocated in storage[]
    unsigned length;     // Amount of storage used
    uni_char storage[1]; // MUST BE LAST FIELD.  Normally longer than 1 element. // ARRAY OK 2011-05-26 sof

    static JStringStorage *Make(ES_Context *context, const uni_char *X, unsigned allocate, unsigned nchars, BOOL no_zero_tail = FALSE);
    static JStringStorage *Make(ES_Context *context, const char *X, unsigned allocate, unsigned nchars);
    static JStringStorage *Make(ES_Context *context, JString *string);

    static JStringStorage *MakeStatic(ES_Context *context, unsigned nchars);
    /**< Allocate a JStringStorage that is *not* on a GC heap. */
    static void FreeStatic(JStringStorage *storage);
    /**< Free a JStringStorage allocated via MakeStatic. */
};

class JStringSegmented : public ES_Boxed
{
public:
    unsigned nsegments;
    unsigned nallocated;

    static JStringSegmented *Make(ES_Context *context, unsigned nsegments);

    JStringStorage *Realize(ES_Context *context, unsigned offset, unsigned length, unsigned extra = 0);

    JStringStorage *MaybeRealize(ES_Context *context, unsigned &offset, unsigned length);

    JStringStorage **Bases() { return reinterpret_cast<JStringStorage **>(reinterpret_cast<char *>(this) + sizeof(JStringSegmented)); }
    unsigned *Offsets() { return reinterpret_cast<unsigned *>(reinterpret_cast<char *>(this) + sizeof(JStringSegmented) + nallocated * sizeof(JString *)); }
    unsigned *Lengths() { return reinterpret_cast<unsigned *>(reinterpret_cast<char *>(this) + sizeof(JStringSegmented) + nallocated * (sizeof(JString *) + sizeof(unsigned))); }
};

class JSegmentIterator
{
public:
    JSegmentIterator(JStringSegmented *segmented, unsigned soffset, unsigned slength);
    JSegmentIterator(const JString *string);
    JSegmentIterator(const JSegmentIterator &other);

    BOOL Next();
    BOOL IsValid() { return segmented ? index < segmented->nsegments && (slength || current_length) : index == 1; }

    JStringStorage *GetBase() { return current_base; }
    unsigned GetOffset() { return current_offset; }
    unsigned GetLength() { return current_length; }

private:
    void InitSegmented(JStringSegmented *segmented0, unsigned soffset0, unsigned slength0);

    unsigned index;
    JStringSegmented *segmented;
    JStringStorage **bases;
    unsigned *offsets;
    unsigned *lengths;
    JStringStorage *current_base;
    unsigned soffset;
    unsigned current_offset;
    unsigned slength;
    unsigned current_length;
};

class JString : public ES_Boxed
{
public:
    JStringStorage *value;
    /**< Storage, possibly shared. */

    unsigned length;
    /**< (Explicit) length of string. */

    unsigned offset;
    /**< Starting offset into 'value' storage. */

    unsigned hash;
    /**< Computed hash. */

    unsigned host_code_or_numval;
    /**< Cached values attached to the immutable string. It contains
         either the string's value as a number or its host code (atom.)
         Bits 0 & 1 are used to disambiguate:
           00 - uninitialised.
           01 - numeric value.
           10 - host code.
           11 - unassigned. */

    BOOL IsHashed() { return hash != 0; }

    unsigned Hash()
    {
        if (!hash)
            hash = CalculateHash();
        return hash;
    }

    int HostCode()
    {
        if ((host_code_or_numval & 3) == 0)
            host_code_or_numval = (static_cast<unsigned>(TranslatePropertyName()) << 2) | 2;
        else if ((host_code_or_numval & 3) == 1)
            return -1;

        return static_cast<int>(host_code_or_numval) >> 2;
    }

    int NumericValue() const
    {
        if ((host_code_or_numval & 3) == 1)
            return (static_cast<int>(host_code_or_numval)) >> 2;
        else
            return INT_MIN;
    }

    BOOL StaticIndex(unsigned &index);

    inline BOOL Equals(const JString *other) const;
    inline BOOL Equals(const uni_char *str, unsigned len) const;
    inline BOOL Equals(const char *str, unsigned len) const;

    /** Create a JString.  If nchars is the default value (-1) then Make
        must compute the length of X, and it does so using uni_strlen or strlen.
        As a result, X may not contain NULs.  If nchars is *not* the default,
        then the length of X is not computed at all; nchars characters are copied.
        In this case, X may contain NULs.

        NOTE!  If the number of characters requested is 1 and that character falls
        in the range of precomputed string values, then a precomputed value is
        returned.  Therefore Make() functions are not guaranteed to return fresh
        strings unless when it's explicitly stated. */

    static JString *Make(ES_Context *context, const uni_char *X, unsigned nchars = UINT_MAX);
    static JString *Make(ES_Context *context, const char *X, unsigned nchars = UINT_MAX);
    static JString *Make(ES_Context *context, JString *X, unsigned offset, unsigned length);
    static JString *Make(ES_Context *context, JStringStorage *X, unsigned offset, unsigned length);

    static JString*	Make(ES_Context *context, unsigned reserve=0);
    /**< Create a JString of length 'reserve', initialized to garbage.  Always
         returns a fresh string.  */

    static JString* MakeTemporary(ES_Context *context);
    /**< Creates an uninitialized temporary substring string.  Must be
         initialized using SetTemporary() before use. */

    inline void SetTemporary(JString *base, unsigned offset, unsigned length);

    static JString* Make(ES_Context *context, JStringSegmented *segmented, unsigned length);

    inline static void SetValue(JString *s, int v);

    inline static void SetHostCode(JString *s, unsigned h);

private:
    inline unsigned CalculateHash();
    unsigned CalculateHashSegmented();

    int TranslatePropertyName();
    BOOL EqualsSegmented(const uni_char *str) const;
    BOOL EqualsSegmented(const char *str) const;
};

class JTemporaryString
{
public:
    JTemporaryString(const uni_char *value, unsigned length, unsigned hash = 0)
    {
        /* Pretend 'value' is the inlined 'storage' array in a JStringStorage
           object.  This isn't pretty, and it will only work for simple uses
           of the JString object we initialize.

           What about alignment, you say?  I'm not sure, but as long as only the
           'storage' array in 'string.value' is accessed, alignment ought to be
           fine, since it is a real uni_char array.  The JStringStorage object
           itself might be completely unaligned, however.  Also, the lowest bit
           in the 'string.value' pointer must be zero, otherwise the string will
           be tagged as a segmented string, but that ought to be no problem as
           long as 'value' is 16 bit aligned, which it ought to be. */

        ES_DECLARE_NOTHING();

        string.value = reinterpret_cast<JStringStorage *>(reinterpret_cast<char *>(const_cast<uni_char *>(value)) - ES_OFFSETOF(JStringStorage, storage));
        string.length = length;
        string.offset = 0;
        string.hash = hash;
        string.host_code_or_numval = 0;
    }

    operator JString *() { return &string; }

    JString *Allocate(ES_Context *context, JString *base = NULL);
    JString *Allocate(ES_Context *context, JStringStorage *base);

private:
    JString string;
};

int Compare(ES_Context *context, JString *X, JString *Y);

unsigned Length(const JString *s) ;
uni_char Element(const JString *s, unsigned i);
uni_char Element(JStringSegmented *s, unsigned i);
uni_char *Storage(ES_Context *context, JString *s);
int ES_CALLING_CONVENTION Equals(const JString *s1, const JString *s2) REALIGN_STACK;

JString *Share(ES_Context *context, JString *s);
/**< Create a new String that shares StringStorage with s. */

JString *Finalize(ES_Context *context, JString *s);
/**< If 's' has been generated, finalize it.
     This returns either 's' or a shared string equal to 's'. */

/** StorageSegment returns a JStringStorage consisting of the data from
    the supplied string at the given offset. A new JStringStorage is only
    allocated in the case when the underlying value is a segmented string
    and in that case only if required string spans multiple segments. The
    in argument soffset is altered to reflect the offset into the returned
    string.
    */
JStringStorage *StorageSegment(ES_Context *context, JString *s, unsigned &soffset, unsigned slength);

/** StorageZ delivers the buffer of the string, with a zero byte appended,
	but leaves the string unchanged when possible, ie, the usual extension
	protocol is used only when necessary.  Thus this is probably more
	efficient than using Append("",1).Storage().
	*/
uni_char *StorageZ(ES_Context *context, JString *s);

/** Append X to 'this'.  If nchars is the default value (-1) then Append
    must compute the length of X, and it does so using uni_strlen or strlen.
    As a result, X may not contain NULs.  If nchars is *not* the default,
    then the length of X is not computed at all; nchars characters are copied.
    In this case, X may contain NULs.

    Note, Append is dangerous.  Never use Append on a string that may be
    shared, always confine its uses to situations where it is known that s
    has a single referent. */
JString *Append(ES_Context *context, JString *s, const uni_char *X, unsigned nchars = UINT_MAX);
JString *Append(ES_Context *context, JString *s, const char *X, unsigned nchars = UINT_MAX);
JString *Append(ES_Context *context, JString *s, uni_char c);
void Append(ES_Context *context, JString *s, JString *X, unsigned nchars = UINT_MAX);

JString *ConvertCase(ES_Context *context, JString *src, BOOL lower);

inline BOOL IsSegmented(const JString *s);
inline BOOL IsSegmented(const JStringStorage *s);
inline JStringSegmented *GetSegmented(const JString *s);
inline void SetSegmented(const JString *s, JStringSegmented *seg);
inline unsigned GetSegmentCount(const JString *s);

#include "modules/ecmascript/carakan/src/kernel/es_string_inlines.h"

#endif // ES_STRING_H
