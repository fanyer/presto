/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/tempbuf.h"
#include "modules/stdlib/src/thirdparty_printf/uni_printf.h"
#include "modules/stdlib/include/double_format.h"

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
	flags.expansion_policy = AGGRESSIVE;
	flags.cached_length_policy = TRUSTED;
	CHECK_INVARIANTS();
}

TempBuffer::~TempBuffer()
{
	OP_DELETEA(storage);
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
	if (old == UNTRUSTED && p == TRUSTED && storage)
		cached_length = uni_strlen(storage);
	flags.cached_length_policy=p;
	return old;
}

size_t TempBuffer::Length() const
{
	return storage != 0 ? (GetCachedLengthPolicy() == TRUSTED ? cached_length : uni_strlen(storage)) : 0;
}

OP_STATUS TempBuffer::EnsureConstructed( size_t capacity )
{
	if (storage && allocated >= capacity)
		return OpStatus::OK;

	if (GetExpansionPolicy() == AGGRESSIVE)
		capacity = MAX( capacity, allocated*2 );

	size_t nallocated = (capacity + (alignment-1)) & ~(alignment-1);
	uni_char *nstorage = OP_NEWA(uni_char,  nallocated );
	CHECK_ALLOCATION( nstorage );

	if (storage)
	{
		OP_ASSERT(uni_strlen(storage) >= Length());
		OP_ASSERT(uni_strlen(storage) < allocated);
		uni_strcpy( nstorage, storage );
		OP_DELETEA(storage);
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
	OP_DELETEA(storage);

	cached_length = 0;
	storage = 0;
	allocated = 0;
	flags.expansion_policy = AGGRESSIVE;
	flags.cached_length_policy = TRUSTED;

	CHECK_INVARIANTS();
}

void TempBuffer::Clear()
{
	CHECK_INVARIANTS();

	if (storage)
		storage[0] = 0;
	cached_length = 0;

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

	if (s == NULL || n == 0)
	{
		return OpStatus::OK;
	}

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
	uni_char buf[ DECIMALS_FOR_128_BITS + 1 ]; // ARRAY OK 2009-04-24 johanh
	if (uni_snprintf( buf, DECIMALS_FOR_128_BITS + 1, UNI_L("%u"), l ) < 1)
		return OpStatus::ERR_NO_MEMORY;
	return TempBuffer::Append( buf );
}

OP_STATUS TempBuffer::AppendLong( long int i )
{
	uni_char buf[ DECIMALS_FOR_128_BITS + 2 ]; // ARRAY OK 2010-07-07 sof
	if (uni_snprintf( buf, DECIMALS_FOR_128_BITS + 2, UNI_L("%ld"), i ) < 1)
		return OpStatus::ERR_NO_MEMORY;
	return TempBuffer::Append( buf );
}

OP_STATUS TempBuffer::AppendDouble( double x )
{
	char buf[32]; // ARRAY OK 2011-11-07 sof
	OpDoubleFormat::ToString( buf, x );
	return TempBuffer::Append( buf );
}

OP_STATUS TempBuffer::AppendDoubleToPrecision( double x, int precision /* = 6 */)
{
	char buf[32]; // ARRAY OK 2011-11-07 sof
	OpDoubleFormat::ToPrecision( buf, x, precision );
	// Strip trailing 0 decimals
	char* period = op_strrchr( buf, '.' );
	if (period)
	{
		char* last_meaningful = period-1;
		char* c = period+1;
		while (*c)
		{
			if (*c == 'e' || *c == 'E')
			{
				RETURN_IF_ERROR(TempBuffer::Append( buf, last_meaningful - buf + 1 ));
				return TempBuffer::Append( c );
			}
			else if (*c != '0')
				last_meaningful = c;
			c++;
		}
		*(last_meaningful+1) = '\0';
	}
	return TempBuffer::Append( buf );
}

OP_STATUS TempBuffer::Append( const char *s, size_t n )
{
	CHECK_INVARIANTS();

	if (s == NULL || n == 0)
	{
		return OpStatus::OK;
	}

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

TempBuffer* TempBuffer::AppendLongL( long int l )
{
	LEAVE_IF_ERROR( AppendLong( l ) );
	return this;
}

TempBuffer* TempBuffer::AppendL( const char *s, size_t n )
{
	LEAVE_IF_ERROR( Append( s, n ) );
	return this;
}

TempBuffer* TempBuffer::Delete( size_t startp, size_t n )
{
	size_t len = Length();

	if (startp >= len || n == 0)
		(void)0; //nop
	else if (startp + n >= len)
	{
		storage[startp] = 0;

		if (flags.cached_length_policy == TRUSTED)
			cached_length = startp;
	}
	else
	{
		uni_char *p = storage + startp;
		uni_char  *q = p + n;
		size_t nmov = len - (startp + n);
		do
			*p++ = *q++;
		while (--nmov);
		*p = 0;

		if (flags.cached_length_policy == TRUSTED)
			cached_length = len - n;
	}

	CHECK_INVARIANTS();

	return this;
}


#if !defined(va_copy) && defined(__va_copy)
# define va_copy __va_copy
#endif // !va_copy && __va_copy

OP_STATUS
TempBuffer::AppendFormat(const uni_char* format, ...)
{
	va_list args;
	va_start(args, format);
	OP_STATUS result = AppendVFormat(format, args);
	va_end(args);
	return result;
}

OP_STATUS
TempBuffer::AppendVFormat(const uni_char* format, va_list args)
{
	CHECK_INVARIANTS();

#ifdef va_copy
	// One cannot assume (according to manual) that args will
	// remain unchanged after the call to OpPrintf::Format()

	va_list args_copy;
	va_copy(args_copy, args);

	OpPrintf printer(OpPrintf::PRINTF_UNICODE);
	int len = printer.Format(NULL, 0, format, args_copy);
	int current_length = Length();

	va_end(args_copy);
#else // va_copy
	OpPrintf printer(OpPrintf::PRINTF_UNICODE);
	int len = printer.Format(NULL, 0, format, args);
	int current_length = Length();
#endif // va_copy

	RETURN_IF_ERROR(Expand(current_length + len + 1));
	uni_char* buf = GetStorage() + current_length;
	uni_vsnprintf(buf, len+1, format, args);

	cached_length = current_length + len;

	CHECK_INVARIANTS();

	return OpStatus::OK;
}

/* eof */
