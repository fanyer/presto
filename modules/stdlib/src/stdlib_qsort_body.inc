/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined(QSORT_COMP_FUNC_LEADING_ARGS) || !defined(QSORT_COMP_FUNC_TRAILING_ARGS) || !defined(QSORT_COMP_FUNC_CALL)
# error must define QSORT_COMP_FUNC_LEADING_ARGS, QSORT_COMP_FUNC_TRAILING_ARGS and QSORT_COMP_FUNC_CALL before including
#endif
void (* exch)(void *base, size_t size, int fst, int snd) = &exchcpy;

if (size == sizeof(UINT32) || size == 2 * sizeof(UINT32))
#ifdef NEEDS_RISC_ALIGNMENT
	if ((reinterpret_cast<UINTPTR>(base) % sizeof(UINT32)) == 0)
#endif
		exch = &exchmov;
tail:
int i=l-1;
int j=r;
int p=l-1;
int q=r;
int k;

if (r <= l)
	return;

void* v = ELEM(base, r, size);
int center = (l+r)/2;

if ( r - l > 8 )
{
	void *left_v = ELEM(base, l, size);
	void *center_v = ELEM(base, center, size);

	if ( compar(QSORT_COMP_FUNC_LEADING_ARGS left_v, v) > 0 )
		exch(base, size, l, r);
	if ( compar(QSORT_COMP_FUNC_LEADING_ARGS v, center_v) > 0 )
		exch(base, size, r, center);
	if ( compar(QSORT_COMP_FUNC_LEADING_ARGS left_v, v) > 0 )
		exch(base, size, l, r);
}
else
{
	exch(base, size, center, r);
}

for (;;)
{
	while (compar(QSORT_COMP_FUNC_LEADING_ARGS ELEM(base, ++i, size), v) < 0)
		;
	while (compar(QSORT_COMP_FUNC_LEADING_ARGS v, ELEM(base, --j, size)) < 0)
		if (j == l)
			break;
	if (i >= j)
		break;
	exch(base, size, i, j);
	if (compar(QSORT_COMP_FUNC_LEADING_ARGS ELEM(base, i, size), v) == 0)
	{
		p++;
		exch(base, size, p, i);
	}
	if (compar(QSORT_COMP_FUNC_LEADING_ARGS v, ELEM(base, j, size)) == 0)
	{
		q--;
		exch(base, size, j, q);
	}
}

exch(base, size, i, r);

j = i-1;
i = i+1;
for ( k=l ; k < p ; k++, j-- )
	exch(base, size, k, j);
for ( k=r-1 ; k > q ; k--, i++ )
	exch(base, size, i, k);

/* Shortest subfile first so that longest is done in the tail */
if (j - l < i - r)
{
	QSORT_COMP_FUNC_CALL(base, l, j, size, compar QSORT_COMP_FUNC_TRAILING_ARGS);
	l = i;
	goto tail;
}
else
{
	QSORT_COMP_FUNC_CALL(base, i, r, size, compar QSORT_COMP_FUNC_TRAILING_ARGS);
	r = j;
	goto tail;
}
