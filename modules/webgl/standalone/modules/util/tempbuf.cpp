/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright 2001 Opera Software AS.  All rights reserved.
 *
 * A simple expandable 0-terminated char buffer.
 */

#include "core/pch.h"
#include "modules/util/tempbuf.h"

const int alignment = 16;			// Must be power of 2

#define CHECK_ALLOCATION( expr ) \
  if (expr == 0) return OpStatus::ERR_NO_MEMORY;

#define CHECK_INVARIANTS() \
    do { \
		OP_ASSERT( (storage == 0) == (allocated == 0) ); \
	} while(0)

TempBuffer::TempBuffer()
{
	cached_length = 0;
	storage = 0;
	allocated = 0;
	flags.expansion_policy = TIGHT;
	flags.cached_length_policy = UNTRUSTED;
	CHECK_INVARIANTS();
}

TempBuffer::~TempBuffer()
{
	delete [] storage;
}

TempBuffer::ExpansionPolicy
TempBuffer::SetExpansionPolicy(TempBuffer::ExpansionPolicy p)
{
	ExpansionPolicy old=GetExpansionPolicy();
	flags.expansion_policy=p;
	return old;
}

TempBuffer::CachedLengthPolicy
TempBuffer::SetCachedLengthPolicy(TempBuffer::CachedLengthPolicy p)
{
	CachedLengthPolicy old=GetCachedLengthPolicy();
	flags.cached_length_policy=p;
	return old;
}

size_t TempBuffer::Length() const
{
	return storage != 0 ? (GetCachedLengthPolicy() == TRUSTED ? cached_length : uni_strlen(storage)) : 0;
}

OP_STATUS TempBuffer::EnsureConstructed( size_t capacity )
{
	size_t nallocated;

	if (storage && allocated >= capacity)
		return OpStatus::OK;

	if (GetExpansionPolicy() == AGGRESSIVE)
		capacity = MAX( capacity, allocated*2 );

	nallocated = (capacity + (alignment-1)) & ~(alignment-1);
	uni_char *nstorage = new uni_char[ nallocated ];
	CHECK_ALLOCATION( nstorage );

	if (storage) {
		uni_strcpy( nstorage, storage );
		delete [] storage;
	}
	else
		nstorage[0] = 0;
	storage = nstorage;
	allocated = nallocated;

	CHECK_INVARIANTS();
	return OpStatus::OK;
}

void TempBuffer::FreeStorage()
{
	delete [] storage;

	cached_length = 0;
	storage = 0;
	allocated = 0;
	flags.expansion_policy = TIGHT;
	flags.cached_length_policy = UNTRUSTED;

	CHECK_INVARIANTS();
}

void TempBuffer::Clear()
{
	CHECK_INVARIANTS();

	if (storage != 0)
		storage[0] = 0;
	cached_length = 0;
	/* I am not sure that it is entirely the best thing to reset the
	   policy parameters here, but I think it is the least surprising
	   thing, so I do it. */
	flags.expansion_policy = TIGHT;
	flags.cached_length_policy = UNTRUSTED;

	CHECK_INVARIANTS();
}

OP_STATUS TempBuffer::Expand( size_t capacity )
{
	CHECK_INVARIANTS();

	if (capacity == 0)
		capacity = 1;

	RETURN_IF_ERROR( EnsureConstructed( capacity ) );

	CHECK_INVARIANTS();
	return OpStatus::OK;
}

OP_STATUS TempBuffer::Append( const uni_char s )
{
	CHECK_INVARIANTS();

	size_t used = Length()+1;

	RETURN_IF_ERROR( EnsureConstructed( used+1 ) );

	uni_char *p;
	p=storage+used-1;
	*p++ = s;
	*p = 0;
	cached_length++;

	CHECK_INVARIANTS();
	return OpStatus::OK;
}

OP_STATUS TempBuffer::Append( const uni_char *s, size_t n )
{
	CHECK_INVARIANTS();

	size_t len = 0;
	const uni_char* q = s;
	while (len < n && *q)
	{
		++q; ++len;
	}

	size_t used = Length()+1;
	size_t orig_len = len;

	RETURN_IF_ERROR( EnsureConstructed( used+len ) );

	uni_char *p;
	for ( p=storage+used-1 ; len ; len-- )
		*p++ = *s++;
	*p = 0;
	cached_length += orig_len;

	CHECK_INVARIANTS();
	return OpStatus::OK;
}

OP_STATUS TempBuffer::AppendUnsignedLong( unsigned long int l )
{
	uni_char buf[ DECIMALS_FOR_128_BITS + 1 ]; // ARRAY OK 2007-03-21 adame
	if (uni_snprintf( buf, DECIMALS_FOR_128_BITS + 1, UNI_L("%u"), l ) < 1)
		return OpStatus::ERR_NO_MEMORY;
	return TempBuffer::Append( buf );
}

OP_STATUS TempBuffer::Append( const char *s, size_t n )
{
	CHECK_INVARIANTS();

	size_t len = 0;

	const char* q = s;
	while (len < n && *q)
	{
		++q; ++len;
	}

	size_t used = Length()+1;
	size_t orig_len = len;

	RETURN_IF_ERROR( EnsureConstructed( used+len ) );

	uni_char *p;
	for ( p=storage+used-1 ; len ; len-- )
		*p++ = (uni_char)*s++;
	*p = 0;
	cached_length += orig_len;

	CHECK_INVARIANTS();
	return OpStatus::OK;
}

TempBuffer* TempBuffer::ExpandL( size_t capacity )
{
	LEAVE_IF_ERROR( Expand( capacity ) );
	return this;
}

TempBuffer* TempBuffer::AppendL( const uni_char s )
{
	LEAVE_IF_ERROR( Append( s ) );
	return this;
}

TempBuffer* TempBuffer::AppendL( const uni_char *s, size_t n )
{
	LEAVE_IF_ERROR( Append( s, n ) );
	return this;
}

TempBuffer* TempBuffer::AppendUnsignedLongL( unsigned long int l )
{
	LEAVE_IF_ERROR( AppendUnsignedLong( l ) );
	return this;
}


TempBuffer* TempBuffer::AppendL( const char *s, size_t n )
{
	LEAVE_IF_ERROR( Append( s, n ) );
	return this;
}

/* eof */
