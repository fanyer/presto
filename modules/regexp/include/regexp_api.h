/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2007
 */

/**
\mainpage Regular Expression Engine

This is the auto-generated API documentation for the RegExp
module. For additional information about the module, see the module's
<a href="http://wiki/developerwiki/index.php/Modules/regexp">Wiki
page</a>, as well as the implementation documentation at the head of
the file src/regexp_advanced_api.cpp.

\author Lars T Hansen

\section intro                     Introduction

The regular expression engine provides regexp matching according to the
ECMAScript specification, ES-262 3rd edition, with some common syntactic
extensions.

\section interfaces                Interfaces

The regular expression engine has two interfaces, designated "high-level"
and "advanced".

The "high-level" api is centered on the class OpRegExp in regexp_api.h
and provides simple facilities to create and use regular expressions
against NUL-terminated strings.  It is sufficient for almost everyone.

The "advanced" api is centered on the class RegExp in
regexp_advanced_api.h and provides additional facilities to allow
sharing of compiled regular expressions, finer-grained control over
regular expression compilation, late binding of some of the matcher
flags, and use on non-NUL terminated strings.  (Eventually this API
will also provide access to time-sliced matching.)  Most likely, only
the ECMAScript engine needs to use the advanced API directly.

\section perf                      Performance

This is not the world's fastest regular expression engine; correcness and
footprint have been considered more important than compilation and matching
speed.  If you need to do a huge amount of matching and you can get away with
using something simpler than full regular expressions, you should probably
consider doing so.  */

/**
\file regexp_api.h

\brief High-level API to the regular expression engine.

\section howto                    How to use the high-level API.

Create a new regular expression object using the constructor method
OpRegExp::CreateRegExp.

(Do keep in mind when you create a regexp that these are ECMAScript regexes,
not Perl regexes or egrep regexes or Posix regexes or ... whatever.  Most
simple regexes will work as you expect.  For everything else, please read
the spec.)

A regular expression object can be used multiple times.  To match the regexp
against a string, call one of the OpRegExp::Match methods.

Destroy a regular expression object by deleting it.  */

#ifndef REGEXP_API_H
#define REGEXP_API_H

/** Lower-level compiled regular expression */
class RegExp;


/** Representation of 'no match result'. */
#define REGEXP_NO_MATCH ((unsigned)-1)


/** OpREFlags represent flags passed to CreateRegExp */
struct OpREFlags
{
	BOOL multi_line;			// TRUE if a multi-line match (see ECMAScript spec)
	BOOL case_insensitive;		// TRUE if a case-insensitive match (see ECMAScript spec)
	BOOL ignore_whitespace;		// TRUE if newlines and comments allowed and whitespace and
								// Unicode format control chararacters (ZWJ, NZWJ, ...)
								// should be ignored (see ECMAScript 4 draft spec).
};

/** OpREMatchLoc represents match locations returned from the regex engine. */
struct OpREMatchLoc
{
	unsigned int matchloc;		///< Location in the string or REGEXP_NO_MATCH
	unsigned int matchlen;		///< Length of match
};


/** OpRegExp represents a compiled regular expression. */
class OpRegExp
{
public:
	static OP_STATUS CreateRegExp( OpRegExp **re, const uni_char *pattern, OpREFlags *flags );
		/**< Compile the pattern with the given flags into a regex matcher.
			 \param re				 Result parameter for the regex, will receive a valid value
									 only if the function returns OpStatus::OK.
			 \param pattern			 The regular expression, as a string
			 \param flags			 Flags used to compile the expression
			 \return				 OpStatus::ERR if pattern has a syntax error.
									 OpStatus::ERR_NO_MEMORY if OOM.
									 Otherwise OpStatus::OK. */

	static OP_STATUS DEPRECATED(CreateRegExp( OpRegExp **re, const uni_char *pattern, BOOL multi_line, BOOL case_insensitive ));
		/**< Deprecated interface to method that accept a flag structure */

	~OpRegExp();
		/**< Delete the matcher */

	OP_STATUS Match( const uni_char *string, OpREMatchLoc *match );
		/**< Match the compiled pattern against a string.
			 \param string  The string to match against
			 \param match   A location in which to return information about the match
			 \return OpStatus::ERR_NO_MEMORY on OOM; otherwise OpStatus::OK.  You must
					 inspect *match to find out if there was a match or not: if there
					 was not a match, then the fields of *match are REGEXP_NO_MATCH. */

	OP_STATUS Match( const uni_char *string, int *nmatches, OpREMatchLoc **matches );
		/**< Match the compiled pattern against 'string', returning multiple matches.
			 \param string   The string to match against
			 \param nmatches The number of matches returned through *matches
			 \param matches  A location in which to return an array of information
							 about the matches
			 \return OpStatus::ERR_NO_MEMORY on OOM; otherwise OpStatus::OK.
					 If *nmatches is 0 then the expression did not match the string.
					 Otherwise, the first match in the match array will be for the
					 entire pattern; the subsequent entries are for submatches as per
					 the ECMAScript spec.  If *nmatches is > 0 then *matches is valid
					 and points to heap allocated storage; it must be deleted by
					 the client. */

private:
	RegExp *regex;

	OpRegExp(RegExp *regex);
};

/* static */ inline OP_STATUS
OpRegExp::CreateRegExp( OpRegExp **re, const uni_char *pattern, BOOL multi_line, BOOL case_insensitive )
{
	OpREFlags flags;
	flags.multi_line = multi_line;
	flags.case_insensitive = case_insensitive;
	flags.ignore_whitespace = FALSE;
	return CreateRegExp( re, pattern, &flags );
}

#endif // REGEXP_API_H
