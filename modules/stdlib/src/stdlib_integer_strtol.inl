/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2010
 */

/* The body of strtol and strtoul, to be included by stdlib_integer.cpp */

#ifdef HAVE_INT64
# define BIG_SIGNED INT64
#else
# define BIG_SIGNED long
#endif

#ifdef HAVE_UINT64
# define BIG_UNSIGNED UINT64
#else
# define BIG_UNSIGNED unsigned long
#endif

/** Helper function for strtol implementations
  * @param nptr NULL-terminated string
  * @param endptr If not NULL, set to first invalid character encountered
  * @param base Base of integer, must be between 2 and 36 or special value 0
  * @param overflow Whether the value overflowed
  * @param signed_multiplier
  *    If NULL, value in nptr assumed unsigned. If not NULL, it will
  *    be set to -1 if encountered value was negative, or 1 if it was
  *    positive, and value in nptr assumed signed.
  * @param minimum Minimum value for integer, overflow if less
  * @param maximum Maximum value for integer, overflow if more
  */
BIG_UNSIGNED STRTOX_NAME(const STRTOX_CHAR_TYPE *nptr, STRTOX_CHAR_TYPE **endptr, int base, BOOL* overflow, int* signed_multiplier, const BIG_SIGNED minimum, const BIG_UNSIGNED maximum)
{
	BIG_UNSIGNED answer = 0;

	if (overflow)
		*overflow = FALSE;
	if (signed_multiplier)
		*signed_multiplier = 1;

	if (base == 1 || base < 0 || base > 36)
	{
#ifdef HAVE_ERRNO
		errno = EDOM;
#endif
	}
	else if (nptr)
	{
		BOOL negate = FALSE;
		BOOL representable = TRUE;
		BOOL max_negative_num = FALSE;

		/* Leading space and sign */

		while (op_isspace(*nptr)) 
			nptr++;
		if (*nptr == '-')
		{
			negate = TRUE;
			nptr++;
		}
		else if (*nptr == '+')
			nptr++;

		/* Base cleanup */

		if (base == 0)
		{
			base = 10;
			if (nptr[0] == '0')
			{
				if (nptr[1] == 'x' || nptr[1] == 'X')
				{
					nptr += 2;
					base = 16;
				}
				else
					base = 8;
			}
		}
		else if (base == 16)
		{
			if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X'))
				nptr += 2;
		}

		/* Consume all valid digits */

		const BIG_UNSIGNED realbig = maximum / base;
		for (;;)
		{
			STRTOX_CHAR_TYPE c = *nptr;

			int dig = -1;
			if (c >= '0' && c <= '9')
				dig = c - '0';
			else if (c >= 'A' && c <= 'Z')
				dig = c - 'A' + 10;
			else if (c >= 'a' && c <= 'z')
				dig = c - 'a' + 10;

            if (dig < 0 || dig >= base)
				break;

            if (representable)
			{
				/* Check if the number is still representable as a positive
				   integer */
				if (answer > realbig || (answer *= base) > maximum - dig)
				{
					/* The maximum negative is a valid result if we are
					   parsing to a signed type */
					if (signed_multiplier && negate && -(BIG_SIGNED)answer - dig == minimum)
						max_negative_num = TRUE;
					representable = FALSE;
				}
                else
				{
					answer += dig;
				}
			}
            nptr++;
		}

		/* We are done consuming digits; return result or flag error */

        if (!representable) 
		{
			if (!max_negative_num)
			{
#ifdef HAVE_ERRNO
				errno = ERANGE;
#endif
				if (overflow)
					*overflow = TRUE;
			}

			/* Return either the underflow or the overflow indicator */

			if (negate && signed_multiplier)
			{
				*signed_multiplier = -1;
				answer = minimum * -1;
			}
			else
				answer = maximum;
		}
        else if (negate)
		{
			if (signed_multiplier)
				*signed_multiplier = -1;
			else
				answer = answer * -1;
		}
	}

	/* Record how much we did parse */

    if (endptr)
		*(const STRTOX_CHAR_TYPE **)endptr = nptr;

    return answer;
}

#undef BIG_SIGNED
#undef BIG_UNSIGNED
