/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright 2001 - 2011 Opera Software AS.  All rights reserved.
 *
 */

#ifndef ES_TEMPBUF_H
#define ES_TEMPBUF_H

#define DECIMALS_FOR_32_BITS 10         // (ceil (log10 (expt 2 32)))
#define DECIMALS_FOR_64_BITS 20         // (ceil (log10 (expt 2 64)))
#define DECIMALS_FOR_128_BITS 39        // (ceil (log10 (expt 2 128)))

class ES_TempBuffer
{
private:
    size_t cached_length; // Trustworthy only when flag is set
    size_t allocated;     // Chars allocated
    uni_char *storage;    // The buffer

private:
    OP_STATUS EnsureConstructed(size_t capacity);
    OP_STATUS AppendSlow(int ch);

    ES_TempBuffer(const ES_TempBuffer &X);             // copy constructor
    ES_TempBuffer& operator=(const ES_TempBuffer &X);  // assignment operator
    /* Dummy operations to make them illegal to use in client code. */

public:
    ES_TempBuffer();
    /**< Initialize the buffer but allocate no memory. */

    ~ES_TempBuffer();

    size_t GetCapacity() const { return allocated; }
    /**< Return the capacity of the buffer in characters, including
         the space for the 0 terminator. */

    uni_char *GetStorage() const { return storage; }
    /**< Return a pointer to buffer-internal storage containing the string.
         This is valid only until the buffer is expanded or destroyed.

         The value will be NULL if GetStorage() is called before any
         calls to Expand or Append. */

    size_t Length() const { return cached_length; }
    /**< Return the length of the string in the buffer in characters,
         not including the space for the 0 terminator. */

    void FreeStorage();
    /**< Frees any storage associated with the ES_TempBuffer. */

    inline void Clear();
    /**< Truncate the live portion of the buffer to 0 characters
         (not counting the 0 terminator).  This does not free any storage. */

    OP_STATUS Expand(size_t capacity);
    /**< Expand the capacity of the buffer to hold at least the given
         number of characters (including the 0 terminator!). */

    inline void ExpandL(size_t capacity);
    /**< Like Expand(), but leaves if it can't expand the buffer. */

    OP_STATUS Append(int ch);
    /**< Append the character to the end of the buffer, expanding the
         buffer only if absolutely necessary and then only as much
         as necessary (plus an amount bounded by a small constant).*/

    inline void AppendL(const uni_char s);
    /**< Like the Append() above, but leaves if it can't expand the buffer. */

    OP_STATUS Append(const uni_char *s, size_t n = (size_t) - 1);
    /**< Append the string to the end of the buffer, expanding the
         buffer only if absolutely necessary and then only as much
         as necessary (plus an amount bounded by a small constant).

         If the second argument 'n' is not (size_t)-1, then a prefix of
         at most n characters is taken from s and appended to the
         buffer. */

    inline void AppendL(const uni_char *s, size_t n = (size_t) - 1);
    /**< Like Append(), but leaves if it can't expand the buffer. */

    OP_STATUS AppendUnsignedLong(unsigned long int x);
    /**< Append the printable representation of x to the end of the
         buffer, expanding the buffer only as necessary (as for Append). */

    inline void AppendUnsignedLongL(unsigned long int x);
    /**< Like AppendLong(), but leaves if it can't expand the buffer. */

    OP_STATUS Append(const char *s, size_t n = (size_t) - 1);
    inline void AppendL(const char *s, size_t n = (size_t) - 1);
    /* As above, but will widen the arguments when appending. */
};

inline void
ES_TempBuffer::Clear()
{
    if (storage != 0)
        storage[0] = 0;
    cached_length = 0;
}

inline OP_STATUS
ES_TempBuffer::Append(int ch)
{
    if (storage && cached_length + 2 <= allocated)
    {
        storage[cached_length++] = ch;
        storage[cached_length] = 0;
        return OpStatus::OK;
    }
    else
        return AppendSlow(ch);
}

inline void
ES_TempBuffer::ExpandL(size_t capacity)
{
    LEAVE_IF_ERROR(Expand(capacity));
}

inline void
ES_TempBuffer::AppendL(const uni_char s)
{
    LEAVE_IF_ERROR(Append(s));
}

inline void
ES_TempBuffer::AppendL(const uni_char *s, size_t n)
{
    LEAVE_IF_ERROR(Append(s, n));
}

inline void
ES_TempBuffer::AppendUnsignedLongL(unsigned long int l)
{
    LEAVE_IF_ERROR(AppendUnsignedLong(l));
}

inline void
ES_TempBuffer::AppendL(const char *s, size_t n)
{
    LEAVE_IF_ERROR(Append(s, n));
}

#endif // ES_TEMPBUF_H
