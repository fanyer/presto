/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright 2001 Opera Software AS.  All rights reserved.
 *
 * A simple expandable 0-terminated uni_char buffer.  This buffer
 * makes explicit its policy for the use of underlying storage and
 * offers only a little abstraction.
 *
 * NOTE: char* and uni_char* arguments are assumed to be zero-terminated
 * strings, even when a length argument is present (as for AppendL).
 *
 * ----------
 *
 * It is possible that this should be replaced by OpString at some point,
 * but the semantics of OpString are currently too weak to support the
 * necessary functionality: storage management and zero-termination are
 * too well hidden for OpString to be useful as a tempbuffer.
 *
 * Please avoid extending the interfaces defined herein more than
 * absolutely necessary -- it is important to keep it as simple and
 * foolproof as possible.  This is not a general string class!
 */

#ifndef TEMPBUF_H
#define TEMPBUF_H

#define DECIMALS_FOR_32_BITS 10			// (ceil (log10 (expt 2 32)))
#define DECIMALS_FOR_64_BITS 20			// (ceil (log10 (expt 2 64)))
#define DECIMALS_FOR_128_BITS 39		// (ceil (log10 (expt 2 128)))

class TempBuffer
{
public:
	enum ExpansionPolicy
	{
		TIGHT,					// Do not extend buffer more than necessary (default)
		AGGRESSIVE				// At least double buffer size every time
	};

	enum CachedLengthPolicy
	{
		UNTRUSTED,				// Do not trust the cached length -- client may have placed a NUL (default)
		TRUSTED					// Trust cached length; avoid recomputing buffer size every time
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
	    /* Dummy operations to make them illegal to use in client code. */

	ExpansionPolicy GetExpansionPolicy() const { return (ExpansionPolicy)flags.expansion_policy; }
	CachedLengthPolicy GetCachedLengthPolicy() const { return (CachedLengthPolicy)flags.cached_length_policy; }

public:
	TempBuffer();
	    /* Initialize the buffer but allocate no memory. */

	~TempBuffer();

	size_t          GetCapacity() const { return allocated; }
	    /* Return the capacity of the buffer in characters, including
	       the space for the 0 terminator. */

	uni_char*		GetStorage() const { return storage; }
	    /* Return a pointer to buffer-internal storage containing the string.
		   This is valid only until the buffer is expanded or destroyed.

		   The value will be NULL if GetStorage() is called before any
		   calls to Expand or Append. */

	ExpansionPolicy	SetExpansionPolicy( ExpansionPolicy p );
		/* Set the buffer expansion policy to p and return the old policy setting. */

	CachedLengthPolicy SetCachedLengthPolicy( CachedLengthPolicy p );
		/* Set the length caching policy to p and return the old policy setting. */

	size_t          Length() const;
	    /* Return the length of the string in the buffer in characters,
	       not including the space for the 0 terminator. */

	void            FreeStorage();
	    /* Frees any storage associated with the TempBuffer.
		   */

	void            Clear();
	    /* Truncate the live portion of the buffer to 0 characters
		   (not counting the 0 terminator).  This does not free any storage.
		   */

	OP_STATUS       Expand( size_t capacity );
	    /* Expand the capacity of the buffer to hold at least the given
	       number of characters (including the 0 terminator!). */

	TempBuffer*     ExpandL( size_t capacity );
	    /* Like Expand(), but leaves if it can't expand the buffer.
		   Returns a pointer to the TempBuffer object. */

	OP_STATUS       Append( const uni_char s );
	    /* Append the character to the end of the buffer, expanding the
	       buffer only if absolutely necessary and then only as much
	       as necessary (plus an amount bounded by a small constant).*/

	TempBuffer*     AppendL( const uni_char s );
		/* Like the Append() above, but leaves if it can't expand the buffer.
	       Returns a pointer to the TempBuffer object. */

	OP_STATUS       Append( const uni_char *s, size_t n = (size_t)-1 );
	    /* Append the string to the end of the buffer, expanding the
	       buffer only if absolutely necessary and then only as much
	       as necessary (plus an amount bounded by a small constant).

		   If the second argument 'n' is not (size_t)-1, then a prefix of
		   at most n characters is taken from s and appended to the
		   buffer. */

	TempBuffer*     AppendL( const uni_char *s, size_t n = (size_t)-1 );
	    /* Like Append(), but leaves if it can't expand the buffer.
		   Returns a pointer to the TempBuffer object. */

	OP_STATUS		AppendUnsignedLong( unsigned long int x );
	/**< Append the printable representation of x to the end of the
	     buffer, expanding the buffer only as necessary (as for Append). */

	TempBuffer*		AppendUnsignedLongL( unsigned long int x );
	/**< Like AppendLong(), but leaves if it can't expand the buffer.
	     Returns a pointer to the TempBuffer object. */

	OP_STATUS		Append( const char *s, size_t n = (size_t)-1 );
	TempBuffer*     AppendL( const char *s, size_t n = (size_t)-1 );
		/* As above, but will widen the arguments when appending. */
};

#endif // defined(TEMPBUF_H)

/* eof */
