/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright 2002 Opera Software ASA.  All rights reserved.
 *
 * ECMAScript regular expression matcher -- test code
 * Lars T Hansen
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//testing
#include "scaffold.h"
//testing

#include "regexp_advanced_api.h"

// Pass -2 for 'a' to indicate that failure is the expected result.

void test( uni_char *re, uni_char *input, unsigned int last_index, unsigned int a, unsigned int b, BOOL3 ignore_case=MAYBE, BOOL3 multi_line=MAYBE )
{
	RegExpMatch *tmp;
	RegExp e;
	int r;

	OP_STATUS status = e.Init(re,ignore_case,multi_line);
	if (OpStatus::IsError(status))
		printf( "Failed to compile/init: %s\n", re);
	else if ((r = e.Exec(input,uni_strlen(input),last_index,&tmp)) != 0 && (tmp[0].first_char != a || tmp[0].last_char != b) ||
		(r == 0 && a != -2))
		printf( "Failed: %s on %s, expected (%d,%d), got (%d,%d)\n", 
			    re, input,
				a, b,
				(r > 0 ? tmp[0].first_char : -2), (r > 0 ? tmp[0].last_char : -2) );
}

static void copydown( char *dest, const uni_char *src, int k )
{
	int j;
	for ( j=0 ; j < k && src[j] != 0 ; j++ )
		dest[j] = (char)(src[j]);
	if (j < k)
		dest[j] = 0;
}

void testshow( uni_char *re, uni_char *input, int last_index, BOOL3 ignore_case=MAYBE, BOOL3 multi_line=MAYBE )
{
	RegExpMatch *tmp;
	RegExp e;
	int r, i;

	OP_STATUS status = e.Init(re,ignore_case,multi_line);
	if (OpStatus::IsError(status))
		printf( "Failed to compile/init: %s\n", re);
	r = e.Exec(input,uni_strlen(input),last_index,&tmp);
	printf( "Matches: %d\n", r );
	for ( i=0 ; i < r ; i++ )
	{
		if (tmp[i].first_char != -1)
		{
			char buf[1000];  /* ARRAY OK 2007-02-22 chrispi */
			int  k=tmp[i].last_char-tmp[i].first_char+1;
			if (k >= 1000)
				k = 999;
			copydown( buf, input+tmp[i].first_char, k );
			buf[k] = 0;
			printf( "  %d,%d   \"%s\"\n", (int)(tmp[i].first_char), (int)(tmp[i].last_char), buf );
		}
		else
			printf( "  UNDEFINED\n" );
	}
}

void testc( uni_char *re, uni_char *input, int last_index, const uni_char *res[], int nres, BOOL3 ignore_case=MAYBE, BOOL3 multi_line=MAYBE )
{
	RegExpMatch *tmp;
	RegExp e;
	int r, i;

	OP_STATUS status = e.Init(re,ignore_case,multi_line);
	if (OpStatus::IsError(status))
		printf( "Failed to compile/init: %s\n", re);
	r = e.Exec(input,uni_strlen(input),last_index,&tmp);
	if (nres != r)
		printf( "Failed: %s on %s, expected %d results, got %d\n", 
			    re, input,
				nres, r );
	else
		for ( i=0 ; i<r ; i++ )
		{
			unsigned int len=tmp[i].last_char-tmp[i].first_char+1;

			if ((tmp[i].first_char == -1) != (res[i] == 0))
				printf( "Failed: %s on %s, on item %d: def/undef mismatch\n", 
						re, input, i );
			else if (res[i] != 0 && uni_strlen(res[i]) != len)
				printf( "Failed: %s on %s, on item %d: length mismatch\n", 
						re, input, i );
			else if (res[i] != 0 && uni_strncmp( res[i], input + tmp[i].first_char, len ) != 0)
			{
				char buf[1000];  /* ARRAY OK 2007-02-22 chrispi */
				if (len >= 1000)
					len = 999;
				copydown( buf, input+tmp[i].first_char, len );
				buf[len] = 0;
				printf( "Failed: %s on %s, on item %d: mismatch -- got %s wanted %s\n", 
						re, input, i,
						buf, res[i] );
			}
		}
}

int main( int argc, char **argv )
{
	test(UNI_L("a"),UNI_L(""),0,-2,-2);
	test(UNI_L("a"),UNI_L("a"),0,0,0);
	test(UNI_L("a"),UNI_L("bbbbbbb"),0,-2,-2);
	test(UNI_L("a"),UNI_L("bbbbabb"),0,4,4);
	test(UNI_L("(?:a)"),UNI_L("a"),0,0,0);
	test(UNI_L("(?:a)"),UNI_L("bbbbabb"),0,4,4);
	test(UNI_L("(?:a)"),UNI_L("bbbbbbb"),0,-2,-2);
	test(UNI_L("(?:ab(?:c|d|f))"),UNI_L("123abcabdabf456"),4,6,8);
	test(UNI_L("(?:ab(?:c|d|f))+"),UNI_L("123abcabdabf456"),4,6,11);
	test(UNI_L("a*bc"),UNI_L("aaaaaaabcjunk"),0,0,8);
	test(UNI_L("a*bc"),UNI_L("xyzaaaaaaabcjunk"),0,3,11);
	test(UNI_L("a?bc"),UNI_L("xyzaaaaaaabcjunk"),0,9,11);
	test(UNI_L("a{3}bc"),UNI_L("xabcyzaaaaaaabcjunk"),0,10,14);
	test(UNI_L("a{3,}bc"),UNI_L("xabcyzaaaaaaabcjunk"),0,6,14);
	test(UNI_L("a{3,5}aa"),UNI_L("xabcyzaaaaaaabcjunk"),0,6,12);
	test(UNI_L("a{3,5}?aa"),UNI_L("xabcyzaaaaaaabcjunk"),0,6,10);
	test(UNI_L("^abc"),UNI_L("abcabc"),0,0,2);
	test(UNI_L("^abc"),UNI_L("abcabc"),1,-2,-2);
	test(UNI_L("^abc"),UNI_L("abd\nabc"),0,4,6,FALSE,TRUE);
	test(UNI_L("abc$"),UNI_L("abcabc"),0,3,5);
	test(UNI_L("abc$"),UNI_L("abcabd"),0,-2,-2);
	test(UNI_L("abc$"),UNI_L("abc\nabd"),0,0,2,FALSE,TRUE);
	test(UNI_L("[a-zA-Z][a-zA-Z0-9]+\\B.f"),UNI_L("123abcabdabg456 foo"),4,-2,-2);
	test(UNI_L("[a-zA-Z][a-zA-Z0-9]+\\B.f"),UNI_L("123abcabdabf456 foo"),4,4,11);
	test(UNI_L("[a-zA-Z][a-zA-Z0-9]+\\B.f"),UNI_L("123abcabdabg456afoo"),4,4,16);
	test(UNI_L("[a-zA-Z][a-zA-Z0-9]+\\b.f"),UNI_L("123abcabdabg456 foo"),4,4,16);
	test(UNI_L("[a-zA-Z][a-zA-Z0-9]+\\b.f"),UNI_L("123abcabdabg456afoo"),4,-2,-2);
	test(UNI_L("[a-zA-Z][a-zA-Z0-9]+\\B.f|[a-zA-Z][a-zA-Z0-9]+\\B.f|[a-zA-Z][a-zA-Z0-9]+\\B.f|[a-zA-Z][a-zA-Z0-9]+\\B.f|[a-zA-Z][a-zA-Z0-9]+\\B.f"),UNI_L("123abcabdabg456 foo"),4,-2,-2);	// Tests when RE_Store overflows one segment
	test(UNI_L("[a-k]+"),UNI_L("ABC"),0,0,2,TRUE,FALSE);
	test(UNI_L("ø"),UNI_L("ø"),0,0,0,TRUE,FALSE);	// o-slash
	test(UNI_L("ø"),UNI_L("Ø"),0,0,0,TRUE,FALSE);	// o-slash
	test(UNI_L("ñ"),UNI_L("Ñ"),0,0,0,TRUE,FALSE);	// tilde-n
	test(UNI_L("(?:\\d+)?do\\d+(?=.*,)\\w+"),UNI_L("do100xyzzy=10,20"),0,0,9);		// Fortran "do" loop
	test(UNI_L("(?:\\d+)?do\\d+(?=.*,)\\w+"),UNI_L("do100xyzzy=10.20"),0,-2,-2);	//  fails on assignment stmt
	test(UNI_L("(?:\\d+)?do\\d+\\w+=\\d+,\\d+"),UNI_L("do100xyzzy=10,20"),0,0,15);
	test(UNI_L("(?:\\d+)?do\\d+(?!.*,)\\w+"),UNI_L("do100xyzzy=10.20"),0,0,9);		// Fortran assignment stmt
	test(UNI_L("(?:\\d+)?do\\d+(?!.*,)\\w+"),UNI_L("do100xyzzy=10,20"),0,-2,-2);	//  fails on do loop
	test(UNI_L("(?:\\d+)?do\\d+(?=.*\\.)\\w+"),UNI_L("do100xyzzy=10.20"),0,0,9);	// Thinking positive!

	const uni_char *a1[] = { UNI_L("aaaaaaa"), UNI_L("aaaaaaa") };
	testc(UNI_L("(a*)"), UNI_L("aaaaaaa"),0,a1,2);

	const uni_char *a2[] = { UNI_L("abc"), UNI_L("a"), UNI_L("a"), 0, UNI_L("bc"), 0, UNI_L("bc") };
	testc(UNI_L("((a)|(ab))((c)|(bc))"), UNI_L("abc"),0,a2,7);

	const uni_char *a3[] = { UNI_L("aaaaaa"), UNI_L("aaa") };
	testc(UNI_L("(a{3})\\1"), UNI_L("aaaaaaaa"), 0,a3,2);

	const uni_char *a4[] = { UNI_L("baaabaac"), UNI_L("ba"), 0, UNI_L("abaac") };
	testc(UNI_L("(.*?)a(?!(a+)b\\2c)\\2(.*)"),UNI_L("baaabaac"),0,a4,4);

	// Tests the capture store management, esp in debug mode (with small store segments)
	testc(UNI_L("(((a)(a)(a)(a)(a)(a)(a)(a)(a))|((a)(a)(a)(a)(a)(a)(a)(a)(a)(a))|((a)(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)))|c"), UNI_L("aaaaabaaabaaaaabaabaaaabaaabbaaaaabaaaaab"),0,NULL,0);

	testshow(UNI_L("(?:\\d+)?"),UNI_L("do100xyzzy=10,20"),0);	// empty match at 0
  	testshow(UNI_L("\\w?"),UNI_L("@@@x"),0);					// empty match at 0
	testshow(UNI_L("ba+c"), UNI_L("baaac"), 0 );
	return 0;
}

#if defined RE_SCAFFOLDING
void *operator new( unsigned int n, TLeave )
{
	void *p = ::operator new(n);
	if (p == NULL)
		LEAVE( OpStatus::ERR_NO_MEMORY );
	return p;
}
#endif

