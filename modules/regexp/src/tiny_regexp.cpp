/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-

   Copyright (C) Opera Software ASA  2003-2004

   Tiny regular expressions
   Lars T Hansen


   See tinyregex.h for most documentation.


   Possible syntactic extensions to consider.

    - decimal/hex designators for chars -- easily done in Char()


   Strategy for nondeterministic search.

   Keeping in mind that we want to minimize code and data space,
   creating an NFA and then converting the NFA to a DFA is not
   plausible.  We've two choices:

   - represent the AST and walk this AST.  This represents the success
     continuation in the program stack and the failure continuation in
     an explicit choice point stack, a la Prolog.

   - represent the NFA and walk the NFA.  This represents the success
     continuation in the NFA itself and and the failure continuation
     in the program stack.

   There are complications with both; creating the NFA in the second
   case is basically not any different from walking the AST during
   matching in the first case.  I have chosen the first technique,
   representing the choice point stack as a tree of generators.
*/

#include "core/pch.h"

#ifdef REGEXP_SUBSET

#include "modules/regexp/include/regexp_advanced_api.h"
#include "modules/regexp/src/tiny_regexp.h"

/* External API compatible with the full engine */

RegExp::RegExp()
{
	re = NULL;
}

RegExp::~RegExp()
{
	OP_DELETE(re);
}

void RegExp::IncRef()
{
	++refcount;
}

void RegExp::DecRef()
{
	--refcount;
	if (refcount == 0)
		OP_DELETE(this);
}

OP_STATUS RegExp::Init( const uni_char *pattern, const uni_char **slashpoint, RegExpFlags *flags )
{
	if (re != NULL)
		return OpStatus::ERR;
	TRAPD( r, re=RE_TinyRegex::MakeL( pattern, flags->ignore_case==YES, slashpoint ) );
	RETURN_IF_ERROR(r);
	if (re == NULL)
		return OpStatus::ERR;
	return OpStatus::OK;
}

int RegExp::Exec( const uni_char *input, unsigned int length, unsigned int last_index, RegExpMatch** results ) const
{
	if (re == NULL)
		LEAVE(OpStatus::ERR);

	size_t matchlen;
	BOOL matches;

	do 
	{
		matches = re->MatchL( input, last_index, &matchlen );
		last_index++;
	} while (!matches && last_index < length && input[last_index] != 0);
	if (matches)
	{
		*results = OP_NEWA_L(RegExpMatch, 1);
		(*results)[0].first_char = last_index-1;
		(*results)[0].last_char = (last_index-1) + matchlen - 1;
		return 1;
	}
	else
	{
		*results = NULL;
		return 0;
	}
}

#define INFTY  3								// hey, it's *tiny* regexes, OK?

union RE_REFactorInfo
{
	struct
	{
		unsigned int anchorleft:1;				// nodes for anchors carry
		unsigned int anchorright:1;				//   no other data
		unsigned int is_wildcard:1;				// matches '.'
		unsigned int is_chars:1;				// matches string of chars
		unsigned int is_term:1;					// matches an arbitrary subexpression
		unsigned int is_charset:1;				// matches an element in a set
		unsigned int is_complemented_charset:1;	//   or does not match ditto
		unsigned int is_caseinsensitive:1;		// convert input to upper case before comparing
		unsigned int quantmin:2;                // 0, 1, 3 (infinity)
		unsigned int quantmax:2;                // 0, 1, 3 (infinity)
		unsigned int anchor_done:1;				// used during matching
		unsigned int shorter_done:1;			// ditto
		unsigned int inner_done:1;				// ditto
		unsigned int inner_finished:1;			// ditto
	} bits;
	int init;
};

struct RE_RETerm
{
    RE_REFactor *factor;        // can be NULL
    RE_RETerm *next;            // in list of disjuncts

    RE_RETerm() { factor=NULL; next=NULL; }
    ~RE_RETerm();
};

struct RE_REFactor
{
	RE_REFactorInfo info;
    union 
    {
		uni_char *chars;			// if info.bits.is_chars
		RE_RETerm  *term;			// if info.bits.is_term
		uni_char *charset;			// if info.bits.is_charset
    } data;
    RE_REFactor *next;				// in list of conjuncts

    RE_REFactor() { info.init=0; next=NULL; }
    ~RE_REFactor();
};

/*** Compiler.

     Strategy: return NULL on syntax error, parser methods use no lookahead. 
*/

#define CHARSET_SPACES    (-'s')
#define CHARSET_DIGITS    (-'d')
#define CHARSET_WORDCHARS (-'w')

static const char * const charset_spaces = " \t\r\n\v\f";
static const char * const charset_digits = "01234567890";
static const char * const charset_wordchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPRSTUVWXYZ0123456789";

RE_TinyRegex*
RE_TinyRegex::MakeL( const uni_char *re, BOOL ci, const uni_char** slash )
{
	OpStackAutoPtr<RE_TinyRegex> regex( OP_NEW_L(RE_TinyRegex, ()) );
    int input = 0;
	if (slash)
		*slash = NULL;
    regex->re = TermList( re, &input, ci, slash != NULL );
	if (regex->re != NULL)
	{
		if (re[input] == 0 && slash == NULL || re[input] == '/' && slash != NULL)
		{
			if (slash != NULL)
				*slash = re+input;
			return regex.release();
		}
		if (slash != NULL && re[input] == 0)
			*slash = re+input;
	}
	else
	{
		if (slash != NULL)
			*slash = NULL;
	}
	return NULL;
}

RE_RETerm*
RE_TinyRegex::TermList( const uni_char *re, int *input, BOOL ci, BOOL slash )
{
    OpStackAutoPtr<RE_RETerm> first( Term( re, input, ci, slash ) );

    RE_RETerm* last = first.get();
    while (last != NULL && re[*input] == '|' && (slash || re[*input] != '/'))
    {
		++*input;
		last = last->next = Term( re, input, ci, slash );
    }

	return last != NULL ? first.release() : NULL;
}

RE_RETerm*
RE_TinyRegex::Term( const uni_char *re, int *input, BOOL ci, BOOL slash )
{
    OpStackAutoPtr<RE_RETerm> t( OP_NEW_L(RE_RETerm, ()) );
	RE_REFactor *last = NULL;

    if (re[*input]=='^')
    {
		RE_REFactor *f = OP_NEW_L(RE_REFactor, ());
		f->info.bits.anchorleft = TRUE;
		last = t->factor = f;
		++*input;
    }

	for (;;)
	{
		//int here = *input;
		RE_REFactor *probe = FactorAndQuant( re, input, ci, slash );
		if (probe == NULL)
		{
			//*input = here;
			break;
		}
		if (last == NULL)
			last = t->factor = probe;
		else
			last = last->next = probe;
	}

    if (re[*input] == '$')
    {
		RE_REFactor *f = OP_NEW_L(RE_REFactor, ());
		f->info.bits.anchorright = TRUE;
		++*input;
		if (last == NULL)
			last = t->factor = f;
		else
			last = last->next = f;
    }

	return t.release();
}

RE_REFactor*
RE_TinyRegex::FactorAndQuant( const uni_char *re, int *input, BOOL ci, BOOL slash )
{
	OpStackAutoPtr<RE_REFactor> f( OP_NEW_L(RE_REFactor, ()));
	BOOL complemented;

	f->info.bits.is_caseinsensitive = ci;

	switch (re[*input])
    {
    case '(':
		++*input;
		// Just gobble ?:, ?=, and ?!, this allows more expressions to be parsed,
		// ie, there will be fewer syntax errors, so if the expr isn't used
		// at all then we win and if not then we're not much worse off.
		if (re[*input] == '?' && (re[*input+1] == ':' || re[*input+1] == '!' || re[*input+1] == '='))
			*input += 2;
		f->data.term = TermList( re, input, ci, FALSE );
		f->info.bits.is_term = TRUE;
		if (f->data.term == NULL || re[*input] != ')')
			return NULL;
		++*input;
		break;
    case '[':
		++*input;
		f->data.charset = CharSet( re, input, ci, &complemented );	// eats the ]
		f->info.bits.is_charset = TRUE;
		f->info.bits.is_complemented_charset = complemented;
		if (f->data.charset == NULL)
			return NULL;
		break;
	case '.':
		++*input;
		f->info.bits.is_wildcard = TRUE;
		break;
    default:
	{
		ES_TempBuffer t;
		ANCHOR(ES_TempBuffer,t);

		// Require at least one char in a factor
		if (IsSpecial(re[*input], slash))
			return NULL;
		int c = Char(re,input);
		const char *cs = NULL;
		if (c == CHARSET_SPACES)
			cs = charset_spaces;
		else if (c == CHARSET_DIGITS)
			cs = charset_digits;
		else if (c == CHARSET_WORDCHARS)
			cs = charset_wordchars;
		if (cs)
		{
			f->data.charset = uni_up_strdup(cs);
			if (f->data.charset == NULL)
				LEAVE(OpStatus::ERR_NO_MEMORY);
			f->info.bits.is_charset = TRUE;
			f->info.bits.is_complemented_charset = FALSE;
			break;
		}

		f->info.bits.is_chars = TRUE;
		if (ci)
			c = uni_toupper(c);
		uni_char d = (uni_char)c;
		t.AppendL(&d, 1);

		// Add chars as long as they are not followed by quants
		while (!IsSpecial(re[*input], slash))
		{
			int here = *input;
			int c = Char(re,input);
			if (c < 0 || IsQuant(re[*input]))	// c < 0 if charset
			{
				*input = here;
				break;
			}
			if (ci)
				c = uni_toupper(c);
			uni_char d = (uni_char)c;
			t.AppendL(&d, 1);
		}

		f->data.chars = uni_strdup(t.GetStorage());
		if (f->data.chars == NULL)
			LEAVE(OpStatus::ERR_NO_MEMORY);
	}
    }

	switch (re[*input])
    {
    case '*': f->info.bits.quantmin=0; f->info.bits.quantmax=3; ++*input; break;
    case '?': f->info.bits.quantmin=0; f->info.bits.quantmax=1; ++*input; break;
    case '+': f->info.bits.quantmin=1; f->info.bits.quantmax=3; ++*input; break;
	default : f->info.bits.quantmin=1; f->info.bits.quantmax=1; break;
    }

	if (f->info.bits.quantmin != f->info.bits.quantmax)
	{
		// Gobble a non-greedy qualifier, to allow more expressions to be
		// parsed.  (See above for justification.)
		if (re[*input] == '?')
			++*input;
	}

	return f.release();
}

uni_char*
RE_TinyRegex::CharSet( const uni_char *re, int *input, BOOL ci, BOOL *complemented )
{
	ES_TempBuffer t;
	ANCHOR(ES_TempBuffer,t);

	*complemented = FALSE;
	if (re[*input] == '^')
	{
		*complemented = TRUE;
		++*input;
	}
	if (re[*input] == ']')
	{
		++*input;
		t.AppendL("]");
	}
	while (re[*input] != 0 && re[*input] != ']')
	{
		int c = Char( re, input );
		if (c == 0)
			return NULL;
		if (c == CHARSET_SPACES)
			t.AppendL(charset_spaces);
		else if (c == CHARSET_DIGITS)
			t.AppendL(charset_digits);
		else if (c == CHARSET_WORDCHARS)
			t.AppendL(charset_wordchars);
		else
		if (re[*input] == '-' && re[*input+1] != ']')
		{
			++*input;
			int d = Char( re, input );
			if (d == 0 || d < 0 || c > d)	// d < 0 if charset
				return NULL;
			for ( ; c <= d ; c++ )
			{
				uni_char e = ci ? uni_toupper(c) : c;
				t.AppendL(&e, 1);
			}
		}
		else
		{
			if (ci)
				c = uni_toupper(c);

			uni_char d = (uni_char)c;
			t.AppendL(&d, 1);
		}
	}
	if (re[*input] == 0)
		return NULL;
	++*input;
	uni_char *cs = uni_strdup(t.GetStorage());
	if (cs == NULL)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return cs;
}

int
RE_TinyRegex::Char( const uni_char *re, int *input )
{
	int c;
	
	c = re[*input];
	++*input;
	if (c == '\\')
	{
		c = re[*input];
		++*input;
		switch (c)
		{
		case 'r': c='\r'; break;
		case 'n': c='\n'; break;
		case 't': c='\t'; break;
		case 'v': c='\v'; break;
		case 'f': c='\f'; break;
		case 's': c=CHARSET_SPACES; break;
		case 'd': c=CHARSET_DIGITS; break;
		case 'w': c=CHARSET_WORDCHARS; break;
		}
	}
	return c;
}

BOOL
RE_TinyRegex::IsSpecial( uni_char c, BOOL slash )
{
    return c == 0 || uni_strchr( UNI_L("|()[]^$.+?*"), c ) != NULL || slash && c == '/';
}

BOOL
RE_TinyRegex::IsQuant( uni_char c )
{
    return c=='$' || c=='?' || c=='+';
}

/*** Matcher.

     The compiled re represents the AST of the regex:

       <disjunct> ::= <conjunct> ...
       <conjunct> ::= <factor> ...
       <factor> ::= <anchorleft> | <anchorright> | <chars><quant> | <charset><quant> | <disjunct><quant> | .<quant>

     The matcher is considered as a tree of generators, where a generator 
     returns either FALSE (no match) or {TRUE,next} (matched at requested
	 location with address of next char to match='next').  A generator must
	 be sure to return FALSE if it is backtracked into and has no alternative
	 matches.

     Bug: repeated matches are handled by creating a repeat matcher (this part
	 is right) and then calling it; this process recurs (this part is wrong).
	 A deep repeated match can blow the stack.  It would be better to keep a
	 chain of repeat matches and walk that chain iteratively.  This would also
	 be more efficient on backtracking because one would not have to traverse
	 the call chain repeatedly: a single pointer into the chain says where to
	 pick up the backtracking.
*/

class RE_DisjunctMatcher;
class RE_ConjunctMatcher;
class RE_FactorMatcher;
class RE_AtomMatcher;

class RE_ConjunctMatcher
{
	const RE_REFactor *conjunct;    // list of conjuncts, to be matched in order
	RE_FactorMatcher **inners;		// matchers for the conjuncts, in same order
	size_t			 *matchlocs;    // location to match at, for each conjunct
	int				 nconjuncts;    // number of conjuncts, matchers, and saved locs
	int				 nextconjunct;  // next conjunct to work on
	
public:
	RE_ConjunctMatcher( const RE_REFactor* conjunct );
	~RE_ConjunctMatcher();
	BOOL Run( const uni_char* input, size_t loc, size_t *nextloc );
};

class RE_DisjunctMatcher
{
	const RE_RETerm		*disjunct;	// list of terms; NULL if list is exhausted
	RE_ConjunctMatcher	*inner;		// matcher or NULL

public:
	RE_DisjunctMatcher( const RE_RETerm* disjunct ) : disjunct(disjunct), inner(NULL) {}
	~RE_DisjunctMatcher() { OP_DELETE(inner); }
	BOOL Run( const uni_char* input, size_t loc, size_t *nextloc );
};

class RE_AtomMatcher
{
	const RE_REFactor	*atom;
	RE_DisjunctMatcher	*inner;
	BOOL				matched;

public:
	RE_AtomMatcher( const RE_REFactor* atom ) : atom(atom), inner(NULL), matched(FALSE) {};
	~RE_AtomMatcher() { OP_DELETE(inner); }
	BOOL Run( const uni_char* input, size_t loc, size_t *nextloc );
};

class RE_FactorMatcher
{
	const RE_REFactor *factor;		// the factor to match against
	RE_AtomMatcher   *inner;		// matcher for nontrivial factors (some match repeatedly)
	RE_FactorMatcher *longer;		// a matcher for same factor, on iterated match
	RE_REFactorInfo  info;			// information about the matcher
	size_t			 inner_nextloc;

public:
	RE_FactorMatcher( const RE_REFactor* factor, int quantmin=-1, int quantmax=-1 );
	~RE_FactorMatcher() { OP_DELETE(inner); OP_DELETE(longer); }
	BOOL Run( const uni_char* input, size_t loc, size_t *nextloc );
};

BOOL
RE_TinyRegex::MatchL( const uni_char* input, size_t matchloc, size_t *matchlen ) const
{
	// The topmost disjunct matcher holds onto the entire generator tree and will 
	// nuke the tree when it goes out of scope.
	OpStackAutoPtr<RE_DisjunctMatcher> m( OP_NEW_L(RE_DisjunctMatcher, ( re )) );
	size_t nextloc;
	BOOL res = m->Run( input, matchloc, &nextloc );
	*matchlen = nextloc - matchloc;
	return res;
}

BOOL RE_DisjunctMatcher::Run( const uni_char* input, size_t loc, size_t *nextloc ) 
{
	/* Keep one conjunct matcher live at a time, move to next term
	   when the current term is exhausted.  
	   */
	while (disjunct != NULL || inner != NULL)
	{
		if (inner == NULL)
			inner = OP_NEW_L(RE_ConjunctMatcher, ( disjunct->factor ));
		while (inner->Run( input, loc, nextloc ))
			return TRUE;
		OP_DELETE(inner);
		inner = NULL;
		disjunct = disjunct->next;
	}
	return FALSE;
}

RE_ConjunctMatcher::RE_ConjunctMatcher( const RE_REFactor* conjunct )
	: conjunct(conjunct)
	, inners(NULL)
	, matchlocs(NULL)
	, nconjuncts(0)
	, nextconjunct(-1)
{}

RE_ConjunctMatcher::~RE_ConjunctMatcher() 
{
	for ( int j=0 ; j < nconjuncts ; ++j )
		OP_DELETE(inners[j]);
	OP_DELETE(inners);
	OP_DELETE(matchlocs);
}

BOOL RE_ConjunctMatcher::Run( const uni_char* input, size_t loc, size_t *nextloc ) 
{
	/* Create a list of factor matchers, one for each factor in the
	   conjunct.  To match, start at the beginning and move forward
	   in the list.  If we get to the end, we succeed; a failure in
	   a submatch causes backtracking to the previous submatch. */

	const RE_REFactor *p;
	int j;

	/* Setup */
	if (inners == NULL)
	{
		for ( p=conjunct, nconjuncts=0 ; p != 0 ; p=p->next, ++nconjuncts )
			;
		inners = OP_NEWA_L(RE_FactorMatcher*, nconjuncts+1);	// waste one loc to avoid some redundant testing
		for ( j=0 ; j <= nconjuncts ; j++ )
			inners[j] = NULL;
		matchlocs = OP_NEWA_L(size_t, nconjuncts+1);			// ditto
		nextconjunct=0;
		matchlocs[0]=loc;
	}
	else
		nextconjunct--;

	/* Match */
	while (0 <= nextconjunct && nextconjunct < nconjuncts)
	{
		if (inners[nextconjunct] == NULL)
		{
			for ( j=0, p=conjunct ; j < nextconjunct ; j++, p=p->next )
				;
			inners[nextconjunct] = OP_NEW_L(RE_FactorMatcher, (p));
		}
		if (inners[nextconjunct]->Run( input, matchlocs[nextconjunct], nextloc ))
		{
			nextconjunct++;
			matchlocs[nextconjunct] = *nextloc;
			OP_DELETE(inners[nextconjunct]);
			inners[nextconjunct] = NULL;
		}
		else
			nextconjunct--;
	}
	return nextconjunct == nconjuncts;
}

RE_FactorMatcher::RE_FactorMatcher( const RE_REFactor* factor, int quantmin, int quantmax )
	: factor(factor)
	, inner(NULL)
	, longer(NULL)
	, inner_nextloc(0)
{
	info.init = 0;
	if (quantmin >= 0)
	{
		info.bits.quantmin = quantmin;
		info.bits.quantmax = quantmax;
	}
	else
	{
		info.bits.quantmin = factor->info.bits.quantmin;
		info.bits.quantmax = factor->info.bits.quantmax;
		info.bits.anchorleft = factor->info.bits.anchorleft;
		info.bits.anchorright = factor->info.bits.anchorright;
	}
}

BOOL RE_FactorMatcher::Run( const uni_char* input, size_t loc, size_t *nextloc )
{
	if (info.bits.anchorleft || info.bits.anchorright)
	{
		// anchors match just once (if at all)
		if (info.bits.anchor_done)
			return FALSE;
		else
		{
			info.bits.anchor_done=TRUE;
			*nextloc = loc;
			return info.bits.anchorleft ? loc == 0 : input[loc] == 0;
		}
	}
	else
	{
		if (inner == NULL)
			inner = OP_NEW_L(RE_AtomMatcher, ( factor ));

again:
		if (!info.bits.inner_done)
		{
			info.bits.inner_done = TRUE;
			if (inner->Run(input, loc, nextloc))
			{
				inner_nextloc = *nextloc;
				if (info.bits.quantmax == INFTY && loc < inner_nextloc)
					longer = OP_NEW(RE_FactorMatcher, ( factor, 0, INFTY ));
				else
					return TRUE;
			}
			else
			{
				info.bits.inner_finished = TRUE;
				if (info.bits.quantmin == 0)
				{
					*nextloc = loc;
					return TRUE;
				}
				else
					return FALSE;
			}
		}

		if (longer != NULL)
		{
			BOOL res = longer->Run(input, inner_nextloc, nextloc);
			if (!res)
			{
				*nextloc = inner_nextloc;
				OP_DELETE(longer);
				longer = NULL;
			}
			return TRUE;
		}
		else if (!info.bits.inner_finished)
		{
			info.bits.inner_done = FALSE;
			goto again;
		}
		else
			return FALSE;
	}
}

BOOL RE_AtomMatcher::Run( const uni_char* input, size_t loc, size_t *nextloc )
{
	if (atom->info.bits.is_chars)
	{
		if (matched)
			return FALSE;
		matched = TRUE;
		BOOL ci = atom->info.bits.is_caseinsensitive;
		uni_char *cp;
		for ( cp = atom->data.chars ; input[loc] && *cp ; ++loc, ++cp )
		{
			uni_char c = ci ? uni_toupper(input[loc]) : input[loc];
			if ( c != *cp )
				break;
		}
		if (*cp == 0)
		{
			*nextloc = loc;
			return TRUE;
		}
		else
			return FALSE;
	}
	else if (atom->info.bits.is_term)
	{
		if (inner == NULL)
			inner = OP_NEW_L(RE_DisjunctMatcher, (atom->data.term));
		return inner->Run( input, loc, nextloc );
	}
	else if (atom->info.bits.is_charset)
	{
		if (matched)
			return FALSE;
		matched = TRUE;
		uni_char c = input[loc];
		if (c == 0)
			return FALSE;
		if (atom->info.bits.is_caseinsensitive)
			c = uni_toupper(c);
		BOOL res = uni_strchr(atom->data.charset, c ) != NULL;
		if (res != (BOOL)atom->info.bits.is_complemented_charset)
		{
			*nextloc = loc+1;
			return TRUE;
		}
		else
			return FALSE;
	}
	else if (atom->info.bits.is_wildcard)
	{
		if (matched)
			return FALSE;
		matched = TRUE;
		if (input[loc] != 0)
		{
			*nextloc = loc+1;
			return TRUE;
		}
		else
			return FALSE;
	}
	else
	{
		OP_ASSERT(!"Should not happen");
		return FALSE;
	}
}

/*** Misc */

RE_TinyRegex::~RE_TinyRegex()
{
	OP_DELETE(re);
}

RE_RETerm::~RE_RETerm()
{
	OP_DELETE(factor);
	OP_DELETE(next);
}

RE_REFactor::~RE_REFactor()
{
	if (info.bits.is_chars)
		OP_DELETE(data.chars);
	if (info.bits.is_term)
		OP_DELETE(data.term);
	if (info.bits.is_charset)
		OP_DELETE(data.charset);
	OP_DELETE(next);
}

#endif // REGEXP_SUBSET
