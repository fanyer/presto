/* -*- Mode: c; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Converts UNI_L() expressions to gcc array constructor expressions.
 *
 * GCC 3.0 have a new option -fshort-wchar which should do this for us.
 *
 * To build it,
 *    flex -L -t < convert.l > convert.c
 *    gcc -o convert convert.c
 *
 * To run it,
 *    ./convert < input > output
 */

%option noyywrap
%x COMMENT
%x STRING1
%x STRING2
%x UNI

%{
#include <string.h> /* strcmp() */
#include <ctype.h>  /* toupper() */
void outputByte(int byte);
void outputHalfWord(int halfword);
void outputEnd();
void flushBuf();
int  octalHalfWord(const char *digits, size_t len);
int  hexHalfWord(const char *digits, size_t len);
int  yylex(void);
int length = 0;
%}

%%

\/\/                    ECHO; /* comment */

\/\*                    BEGIN(COMMENT); ECHO;
<COMMENT>[^*]*          ECHO; /* comment */
<COMMENT>\*             ECHO; /* comment */
<COMMENT>\*\/           BEGIN(INITIAL); ECHO;

\"                      BEGIN(STRING1); ECHO;
<STRING1>\"             BEGIN(INITIAL); ECHO;
<STRING1>[^\"\\]+       ECHO; /* skip */
<STRING1>\\.            ECHO; /* skip */

\'                      BEGIN(STRING2); ECHO;
<STRING2>\'             BEGIN(INITIAL); ECHO;
<STRING2>[^\'\\]+       ECHO; /* skip */
<STRING2>\\.            ECHO; /* skip */

UNI_L[ ]*\([ ]*[^\'\"]*\"       BEGIN(UNI); printf( "((const uni_char *) L\"" );
<UNI>\\a                outputByte( '\a' );
<UNI>\\b                outputByte( '\b' );
<UNI>\\f                outputByte( '\f' );
<UNI>\\n                outputByte( '\n' );
<UNI>\\r                outputByte( '\r' );
<UNI>\\t                outputByte( '\t' );
<UNI>\\v                outputByte( '\v' );
<UNI>\\[0-9]{3}         outputHalfWord( octalHalfWord(yytext+1, 3) );
<UNI>\\[0-9]{2}         outputHalfWord( octalHalfWord(yytext+1, 2) );
<UNI>\\[0-9]            outputHalfWord( octalHalfWord(yytext+1, 1) );
<UNI>\\x[0-9a-fA-F]{4}  outputHalfWord( hexHalfWord(yytext+2, 4) );
<UNI>\\x[0-9a-fA-F]{3}  outputHalfWord( hexHalfWord(yytext+2, 3) );
<UNI>\\x[0-9a-fA-F]{2}  outputHalfWord( hexHalfWord(yytext+2, 2) );
<UNI>\\x[0-9a-fA-F]     outputHalfWord( hexHalfWord(yytext+2, 1) );
<UNI>\\.                outputByte( (unsigned char) *(yytext+1) );
<UNI>\"[^\)\"]*\)       BEGIN(INITIAL); outputEnd();
<UNI>\"[^\)\"]*\"       /* string catenation */
<UNI>.                  outputByte( (unsigned char) *yytext );

%%

#define LITTLE 1
#define BIG    2

int endian = LITTLE;

struct {
	int hi, lo;
} word;

int octalHalfWord(const char *c, size_t len)
{
	int code = 0;
	switch (len)
	{
	case 3:
		code += ((c[0]-'0')*64);
	case 2:
		code += ((c[len-2]-'0')*8);
	case 1:
		code += (c[len-1]-'0');
	}
	return code;
}

int hexHalfWord(const char *c, size_t len)
{
	int code = 0;
	switch (len)
	{
	case 4:
		code  = ((c[0] <= '9' ? c[0]-'0' : toupper(c[0])-'A'+10)*4096);
	case 3:
		code += ((c[len-3] <= '9' ? c[len-3]-'0' : toupper(c[len-3])-'A'+10)*256);
	case 2:
		code += ((c[len-2] <= '9' ? c[len-2]-'0' : toupper(c[len-2])-'A'+10)*16);
	case 1:
		code += ((c[len-1] <= '9' ? c[len-1]-'0' : toupper(c[len-1])-'A'+10));
	}
	return code;
}

void outputByte(int byte)
{
	if (length == 0)
		word.lo = byte;
	else
		word.hi = byte;

	length+=2;
	if (length % 4 == 0)
		flushBuf();
}

void flushBuf()
{
	if (length == 0)
		return;
	else if (length == 2)
		word.hi = 0;

	if (endian == LITTLE)
		printf("\\x%02x%02x%02x%02x",
			   (word.hi >> 8) & 255,
			   word.hi & 255,
			   (word.lo >> 8) & 255,
			   word.lo & 255);
	else
		printf("\\x%02x%02x%02x%02x",
			   (word.lo >> 8) & 255,
			   word.lo & 255,
			   (word.hi >> 8) & 255,
			   word.hi & 255);
	word.lo = 0;
	word.hi = 0;
	length=0;
}

void outputEnd()
{
	flushBuf();
	printf("\")");
}

void outputHalfWord(int halfword)
{
	// reuse the byte implementation
	outputByte(halfword);
}

int main( int argc, char **argv )
{
    if (argc > 1)
    {
        if (strcmp( argv[1], "-little" ) == 0)
            endian = LITTLE;
        else if (strcmp( argv[1], "-big" ) == 0)
			endian = BIG;
    }
    yylex();
    return 0;
}
