/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_TEMPBUF_H
#define MODULES_UTIL_TEMPBUF_H

#define DECIMALS_FOR_32_BITS 10			// (op_ceil (log10 (expt 2 32)))
#define DECIMALS_FOR_64_BITS 20			// (op_ceil (log10 (expt 2 64)))
#define DECIMALS_FOR_128_BITS 39		// (op_ceil (log10 (expt 2 128)))

class TempBuffer
{
public:
	enum ExpansionPolicy
	{
		TIGHT,					// Do not extend buffer more than necessary
		AGGRESSIVE				// At least double buffer size every time (default)
	};

	enum CachedLengthPolicy
	{
		UNTRUSTED,				// Do not trust the cached length -- client may have placed a NUL
		TRUSTED					// Trust cached length; avoid recomputing buffer size every time (default)
	};

private:
	size_t			 cached_length;			// Trustworthy only when flag is set
	size_t           allocated;				// Chars allocated
	uni_char*        storage;				// The buffer
	struct {
		unsigned int expansion_policy:1;	// Really an ExpansionPolicy datum
		unsigned int cached_length_policy:1;// Really a CachedLengthPolicy datum
	} flags;

private:
	OP_STATUS EnsureConstructed( size_t capacity );

	TempBuffer( const TempBuffer &X );             // copy constructor
	TempBuffer& operator=( const TempBuffer &X );  // assignment operator
	/**< Dummy operations to make them illegal to use in client code. */

	ExpansionPolicy GetExpansionPolicy() const { return (ExpansionPolicy)flags.expansion_policy; }
	CachedLengthPolicy GetCachedLengthPolicy() const { return (CachedLengthPolicy)flags.cached_length_policy; }

public:
	TempBuffer();
	/**< Initialize the buffer but allocate no memory. */

	~TempBuffer();

	size_t          GetCapacity() const { return allocated; }
	/**< Return the capacity of the buffer in characters, including
	     the space for the 0 terminator. */

	uni_char*		GetStorage() const { return storage; }
	/**< Return a pointer to buffer-internal storage containing the string.
		 This is valid only until the buffer is expanded or destroyed.

		 The value will be NULL if GetStorage() is called before any
		 calls to Expand or Append. */

	ExpansionPolicy	SetExpansionPolicy( ExpansionPolicy p );
	/**< Set the buffer expansion policy to p and return the old policy setting. */

	CachedLengthPolicy SetCachedLengthPolicy( CachedLengthPolicy p );
	/**< Set the length caching policy to p and return the old policy setting. */

	size_t          Length() const;
	/**< Return the length of the string in the buffer in characters,
	     not including the space for the 0 terminator. */

	void            FreeStorage();
	/**< Frees any storage associated with the TempBuffer. Also resets
	     cached length and expansion policies to default values. */

	void            Clear();
	/**< Truncate the live portion of the buffer to 0 characters (not
		 counting the 0 terminator).  This does not free any
		 storage. */

	OP_STATUS       Expand( size_t capacity );
	/**< Expand the capacity of the buffer to hold at least the given
	     number of characters (including the 0 terminator!). */

	TempBuffer*     ExpandL( size_t capacity );
	/**< Like Expand(), but leaves if it can't expand the buffer.
		 Returns a pointer to the TempBuffer object. */

	OP_STATUS       Append( const uni_char s );
	/**< Append the character to the end of the buffer, expanding the
	     buffer if necessary. */

	TempBuffer*     AppendL( const uni_char s );
	/**< Like the Append() above, but leaves if it can't expand the buffer.
	     Returns a pointer to the TempBuffer object. */

	OP_STATUS       Append( const uni_char *s, size_t n = (size_t)-1 );
	/**< Append the string to the end of the buffer, expanding the
	     buffer if necessary.

	     If the second argument 'n' is not (size_t)-1, then a prefix of
	     at most n characters is taken from s and appended to the
	     buffer. */

	TempBuffer*     AppendL( const uni_char *s, size_t n = (size_t)-1 );
	/**< Like Append(), but leaves if it can't expand the buffer.
	     Returns a pointer to the TempBuffer object. */

	OP_STATUS		AppendUnsignedLong( unsigned long int x );
	/**< Append the printable representation of x to the end of the
	     buffer, expanding the buffer if necessary (as for Append). */

	TempBuffer*		AppendUnsignedLongL( unsigned long int x );
	/**< Like AppendUnsignedLong(), but leaves if it can't expand the buffer.
	     Returns a pointer to the TempBuffer object. */

	OP_STATUS		AppendLong( long int x );
	/**< Append the printable representation of x to the end of the
	     buffer, expanding the buffer if necessary (as for Append). */

	TempBuffer*		AppendLongL( long int x );
	/**< Like AppendLong(), but leaves if it can't expand the buffer.
	     Returns a pointer to the TempBuffer object. */

	OP_STATUS		AppendDouble( double x );
	/**< Append the printable representation of x to the end of the
	     buffer, expanding the buffer if necessary (as for Append).
	     Similiar to sprintf("%f") in format but not locale dependent. */

	OP_STATUS		AppendDoubleToPrecision( double x, int precision = 6 );
	/**< Append the printable representation of x to the end of the
	     buffer, expanding the buffer if necessary (as for Append).
	     Similiar to sprintf("%g") in format but not locale dependent.
	     The precision of %g is default 6 so that can be a good value. */

	OP_STATUS		Append( const char *s, size_t n = (size_t)-1 );
	TempBuffer*     AppendL( const char *s, size_t n = (size_t)-1 );
	/**< As above, but will widen the arguments when appending. */

	TempBuffer*		Delete( size_t startp, size_t n );
	/**< Delete portion of buf by removing (up to) n chars starting from startp. 
	 *   If startp is beyond buf len -- does nothing.
	 *   If startp + n is beyond buf len -- removes up to buf len only.
	 *   Moves the 0-terminator accordingly
	 *   Honours the currently set cached_length_policy (i.e. leaves as is)
	 */

	/**
	 * Appends a string from a format/vararg list.
	 *
	 * @param format A c-style uni_char* format % string
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AppendFormat(const uni_char* format, ...);
	OP_STATUS AppendVFormat(const uni_char* format, va_list args);
};

#endif // !MODULES_UTIL_TEMPBUF_H

/* eof */
