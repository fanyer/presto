/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-

   Copyright (C) Opera Software ASA  2003-2004

   Tiny regular expressions
   Lars T Hansen


   Purpose.

   Very small regular expression engine supporting 'most commonly
   used' syntax and semantics, for use in subset-versions of Opera's
   ECMAScript engine where code size matters most.


   Syntax.

   <re> ::= <term> ('|' <term>)*
   <term> ::= <anchorleft>?(<factor><quant>?)*<anchorright>?
   <factor> ::= <char> | '(' <re> ')' | . | '[' <setinitial>? <setmember>* <setfinal>? ']'
   <quant> ::= '*' | '?' | '+'
   <setinitial> ::= ']' | | '^' | '^]'
   <setmember> ::= <char> | <simple> '-' <simple>
   <setfinal> ::= '-'
   <anchorleft> ::= '^'
   <anchorright> ::= '$'
   <char> ::= <simple> | '\s' | '\d' | '\w' | '\r' | '\n' | '\t' | '\f' | '\v'
   <simple> ::= <any char that is not special, or \c for special chars c>


   Semantics.

   Matching is greedy and exhaustive.  Parentheses do not capture the 
   substring; only one match result is returned if the regex matches.

   The escape \s denotes the set of space characters: \r,\n,\v,\f,\t,space.
   The escape \w denotes the set of alphanumerics: a-z,A-Z,0-9.
   The escape \d denotes the set of digits: 0-9.


   Hacks.

   The regex compiler recognizes some standard but unsupported syntax in order
   to make it more likely that standard scripts run -- not all expressions that
   are parsed are used, and by avoiding syntax errors we increase the chance
   of correct behavior, at some cost in error reporting (which is disabled anyway).
*/

#ifdef REGEXP_SUBSET

struct RE_RETerm;
struct RE_REFactor;
struct RE_REMatchStack;

class RE_TinyRegex
{
public:
    /** Create a regex object.
	    @param re    NUL-terminated regular expression by above grammar
		@param ci    TRUE if case insensitive match, FALSE otherwise
		@param slash If NULL, has no effect.
			         If not NULL, the parsing of the regex will terminate at the
					 first toplevel / encountered in "re", and a pointer to the
					 location of this / will be stored in *slash.  If a / is
					 not found and no syntax error occurs before the end of the input
					 is reached, then an error is returned and *slash is set to
					 point to the terminating NUL in "re".  If a syntax error occurs
					 before the end of the re is reached, an error is returned but
					 *slash will be NULL.
		@return      A new regular expression object; NULL on syntax error
		@exceptions  Leaves on OOM
        */
    static RE_TinyRegex *MakeL( const uni_char* re, BOOL ci, const uni_char** slash=NULL );

    /** Match the regex against the input.
		@param input     NUL-terminated string to match against
		@param startloc  Location to perform matching at
		@param matchlen  OUT parameter receiving length of match, if any
		@return          TRUE on match, FALSE otherwise
		@exceptions      Leaves on OOM
        */
    BOOL MatchL( const uni_char* input, size_t startloc, size_t *matchlen ) const;

    /** Destroy object and all associated data */
    ~RE_TinyRegex();

private:
    RE_RETerm *re;

    RE_TinyRegex() { re=NULL; }

    /* Compiler */
    static RE_RETerm*   TermList( const uni_char *re, int *input, BOOL ci, BOOL slash );
    static RE_RETerm*   Term( const uni_char *re, int *input, BOOL ci, BOOL slash );
    static RE_REFactor* FactorAndQuant( const uni_char *re, int *input, BOOL ci, BOOL slash );
    static uni_char*    CharSet( const uni_char *re, int *input, BOOL ci, BOOL *complemented );
    static int		    Char( const uni_char *re, int *input );
    static BOOL         IsSpecial( uni_char c, BOOL slash );
	static BOOL			IsQuant( uni_char c );
};

#endif // REGEXP_SUBSET
