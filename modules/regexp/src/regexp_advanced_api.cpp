/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2002-2007
 *
 * ECMAScript regular expression matcher -- interface & documentation
 */

/**
\file regexp_advanced_api.cpp

\brief ECMAScript regular expression compiler and matcher

\author Lars T Hansen

\section client_interface             Client interface

The client interface to the engine is documented in regexp_advanced_api.h.
This comment block documents the implementation.

\section spec                         Specification

The relevant specification is section 15.10 of the ECMAScript 3rd
Edition Specification, ECMA-262.  This implementation supports a
slight variant of that specification:

<ul>
<li>  We allow a greater range of characters for identity-escape,
      see "NOTE 1" in regexp_parser.cpp for details

<li>  The implementation of Canonicalize() does not cover the case
      where a lower-case character is converted to more than one
      upper-case character; see "NOTE 2" in regexp_private.h

<li>  When REGEXP_STRICT is disabled (the default), we allow
      some extensions to the regexp syntax, notably octal escapes, but
      also loosening some restrictions.  (Search for REGEXP_STRICT.)
</ul>

In addition, in the non-subset engine we support (in an experimental
manner) the following features from the upcoming ECMAScript 4th Edition
Specification:

<ul>
<li>  Comments of the form (?# whatever ). These comments turn into the
      empty pattern (?:).

	  These comments are available when REGEXP_STRICT is disabled (the
	  default).

<li>  The /x flag, which says to ignore whitespace and Unicode format
      control characters and to allow line comments consisting of a
	  octothorpe character (#) and arbitrary text up until the end of
	  the line.

	  Spaces and line comments are seen as token separators and may be
	  used in Atom contexts and in entity contexts inside a character set.

      This functionality is always available and requires setting a flag
	  in the regular expression when it is created (flags.ignore_whitespace).

<li>  Named captures, which allow capturing parens to be given readable
      names, like in /(?P<year>\d{4})-(?P<month>\d{2})-(?P<day>\d{2})/,
	  which binds the three submatches to names "year", "month", "day"
	  respectively.

      With this comes named backreferences, eg (?P=year).

	  Regexes that use names retain a mapping from names to capture indices
	  that can be used by clients following matching to set up client
	  data structures mapping those names to the submatch values.
</ul>


\section literature                   Literature

A very useful companion is Jeffrey E. F. Friedl, "Mastering Regular
Expressions", published by O'Reilly.  It does not describe ECMAScript
regexps but it describes many other kinds, most of the issues involved,
and many optimizations.


\section overall_strategy             Overall strategy

The specification uses a continuation-passing style (CPS) interpreter
to express the semantics of regular expressions.  This implementation
follows the specification quite closely.

<b><center>

      DO NOT ALTER THE CODE WITHOUT UNDERSTANDING THE SPEC

</center></b>


There are some problems with implementing a CPS interpreter in C++.
Generally, tail calls are not reliably implemented as such by the
compiler.  This means that in the worst case, stack depth is
proportional to the length of the input string (consider the pattern
"a*" matching on a string of a's).  This is unacceptable, both because
stack frame can be large (on some RISC systems very large), and
because stack overflows cannot reliably be caught as OOM errors.

Instead the matcher engine uses an explicit stack; the worst-case
stack depth is just the same, but it is controllable and the frame
size can be optimized and is platform-independent.

A small number of other optimizations have been made to improve
matching speed or space usage; for example, character strings are not
matched as character sets, but use special-purpose matchers.

Capture arrays are not represented as arrays that are copied and
updated.  The insight is that every copy+update of a capture array
is equivalently viewed as a nondestructive extension of an environment
of capture bindings, with extensive sharing.


\section representation               Representation and matching

A compiled regular expression is represented as a tree of nodes derived
from RE_Node or RE_Class.  RE_Node instances have a field 'op' that
describes each instance.

Matching is performed by the static RE_Node::RunEngine; it dispatches
on the 'op' field.  It maintains an explicit stack which it uses to
represent both success and failure continuations.  Stack frames are
pushed by submatches and tail calls are used when possible.


\section storage_mgmt                    Storage management

The regexp itself is allocated on the C++ heap with 'OP_NEW' and
deleted with 'OP_DELETE', in the normal fashion.  This is also true
for the array of matches returned by the regexp matcher.

Internally, things are more complicated, and there are two kinds
of data: static and dynamic.

Static data are used for the regexp representation and are
allocated from a common block, which is managed by an RE_Store
data structure (one for each regexp).  This reduces overheads of
both time and space: no object need ever be deallocated until
all objects are dead, so there are no per-object headers;
allocation just moves a pointer; and deallocation is very fast.
The strategy also simplifies OOM handling -- if an allocation
fails, then there is no need to clean up except at the top level,
where the entire store can be deleted as part of cleanup.

Dynamic data are allocated on the C++ heap but are managed in
data structures that do not require cleanup structures being
allocated for them at runtime.  There is only one kind of
dynamic structure, the captures store, used to manage capturing
parentheses matches.

The captures "array" is implemented as a garbage-collected pool
of nodes.  The pool can be deleted en masse when the matcher
returns or leaves.


\section impl_limits                    Implementation limits and deviations

<ul>
<li>   The regexp and input strings may be no more than 2^32-2
       characters long.

<li>   The largest repetition count in a quantifier is 2^32-2.

<li>   There may be no more than 2^16-1 capturing parentheses.
</ul>
*/

#include "core/pch.h"

#include "modules/regexp/include/regexp_advanced_api.h"

#include "modules/regexp/src/re_config.h"
#include "modules/regexp/src/re_compiler.h"
#include "modules/regexp/src/re_matcher.h"
#include "modules/regexp/src/re_object.h"
#include "modules/regexp/src/re_searcher.h"
#include "modules/regexp/src/re_native.h"

RegExpFlags::RegExpFlags()
	: ignore_case(MAYBE)
	, multi_line(MAYBE)
	, ignore_whitespace(FALSE)
	, searching(FALSE)
{
}

RegExp::RegExp()
	: refcount(1)
	, re_cs(NULL)
	, re_ci(NULL)
	, ignore_case(MAYBE)
	, multi_line(MAYBE)
	, matches(NULL)
{
}

RegExp::~RegExp()
{
	OP_DELETE(re_cs);
	OP_DELETE(re_ci);
	OP_DELETEA(matches);
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

static OP_STATUS
CompileRegExp( RE_Object *&result, RE_Compiler *compiler, const uni_char *pattern, unsigned length )
{
	TRAPD(status, result = compiler->CompileL(pattern, length));
	return status;
}

OP_STATUS RegExp::Init( const uni_char *pattern, unsigned length, const uni_char **slashpoint, RegExpFlags *flags )
{
	if (flags->ignore_case != YES)
	{
		RE_Compiler compiler(false, flags->multi_line != NO, !!flags->searching);

#ifdef RE_FEATURE__EXTENDED_SYNTAX
		if (flags->ignore_whitespace)
			compiler.SetExtended();
#endif // RE_FEATURE__EXTENDED_SYNTAX

		if (slashpoint)
			compiler.SetLiteral();

		RETURN_IF_ERROR(CompileRegExp(re_cs, &compiler, pattern, length));
		if (!re_cs)
		{
			if (slashpoint)
				*slashpoint = pattern + uni_strlen(pattern);

			return OpStatus::ERR;
		}

		if (slashpoint)
			*slashpoint = pattern + compiler.GetEndIndex();
	}

	if (flags->ignore_case != NO)
	{
		RE_Compiler compiler(true, flags->multi_line != NO, !!flags->searching);

		if (slashpoint)
			compiler.SetLiteral();

		RETURN_IF_ERROR(CompileRegExp(re_ci, &compiler, pattern, length));
		if (!re_ci)
		{
			if (slashpoint)
				*slashpoint = pattern + uni_strlen(pattern);

			return OpStatus::ERR;
		}

		if (slashpoint)
			*slashpoint = pattern + compiler.GetEndIndex();
	}

	unsigned ncaptures = 1 + (re_cs ? re_cs : re_ci)->GetCaptures();
	matches = OP_NEWA(RegExpMatch, ncaptures);
	if (!matches)
		return OpStatus::ERR_NO_MEMORY;

	for (unsigned index = 0; index < ncaptures; ++index)
		matches[index].length = UINT_MAX;

	ignore_case = flags->ignore_case;
	multi_line = flags->multi_line;

	return OpStatus::OK;
}

int RegExp::GetNumberOfCaptures() const
{
	RE_Object *object = re_cs ? re_cs : re_ci;
	return object->GetCaptures();
}

#if !defined REGEXP_STRICT

int RegExp::GetNumberOfSymbols() const
{
#ifdef RE_FEATURE__NAMED_CAPTURES
	RE_Object *object = re_cs ? re_cs : re_ci;
	return object->CountNamedCaptures();
#else // RE_FEATURE__NAMED_CAPTURES
	return 0;
#endif // RE_FEATURE__NAMED_CAPTURES
}

const uni_char *RegExp::GetSymbol(int i, int *index) const
{
#ifdef RE_FEATURE__NAMED_CAPTURES
	RE_Object *object = re_cs ? re_cs : re_ci;
	for (unsigned j = 0; j < object->GetCaptures(); ++j)
	{
		const uni_char *name = object->GetCaptureName(j);
		if (name)
			if (i-- == 0)
			{
				*index = j;
				return name;
			}
	}
#endif // RE_FEATURE__NAMED_CAPTURES
	OP_ASSERT(FALSE);
	return NULL;
}

#endif // !REGEXP_STRICT

#ifdef REGEXP_UNPARSER
void RegExp::Unparse( FILE *out )
{
	matcher->Unparse( out );
}
#endif

#ifdef DEBUG_ENABLE_PRINTF
//# define REGEXP_DUMP_TESTS
#endif // DEBUG_ENABLE_PRINTF

#ifdef REGEXP_DUMP_TESTS

static void RE_DumpSource(ES_TempBuffer &buffer, const uni_char *source)
{
	buffer.Append("/");

	for (unsigned index = 0, length = uni_strlen(source); index < length; ++index)
		switch (source[index])
		{
		case '/':
			buffer.Append("\\/");
			break;

		default:
			if (source[index] == 0 || source[index] >= 127)
			{
				OpString escape;
				if (source[index] <= 255)
					OpStatus::Ignore(escape.AppendFormat(UNI_L("\\x%02x"), source[index]));
				else
					OpStatus::Ignore(escape.AppendFormat(UNI_L("\\u%04x"), source[index]));
				buffer.Append(escape.CStr());
			}
			else
				buffer.Append(source[index]);
		}

	buffer.Append("/");
}

static void RE_DumpString(ES_TempBuffer &buffer, const uni_char *string, unsigned string_length)
{
	for (unsigned index = 0; index < string_length; ++index)
		switch (string[index])
		{
		case '"': case '\\':
			buffer.Append('\\');
			buffer.Append(string[index]);
			break;

		case '\n':
			buffer.Append("\\n");
			break;

		case '\r':
			buffer.Append("\\r");
			break;

		case '\t':
			buffer.Append("\\t");
			break;

		default:
			if (string[index] == 0 || string[index] >= 127)
			{
				OpString escape;
				if (string[index] <= 255)
					OpStatus::Ignore(escape.AppendFormat(UNI_L("\\x%02x"), string[index]));
				else
					OpStatus::Ignore(escape.AppendFormat(UNI_L("\\u%04x"), string[index]));
				buffer.Append(escape.CStr());
			}
			else
				buffer.Append(string[index]);
		}
}

static void RE_DumpTest(const uni_char *input, unsigned input_length, unsigned last_index, RE_Object *object, unsigned nmatches, RegExpMatch *results)
{
	ES_TempBuffer source_buffer;
	RE_DumpSource(source_buffer, object->GetSource ());

	ES_TempBuffer input_buffer;
	RE_DumpString(input_buffer, input + last_index, input_length - last_index);

	if (nmatches == 0)
		if (input_length - last_index != 0)
			dbg_printf("test (%S, \"%S\", null);\n", source_buffer.GetStorage(), input_buffer.GetStorage());
		else
			dbg_printf("test (%S, \"\", null);\n", source_buffer.GetStorage());
	else
	{
		if (input_length - last_index != 0)
			dbg_printf("test (%S, \"%S\", [", source_buffer.GetStorage(), input_buffer.GetStorage());
		else
			dbg_printf("test (%S, \"\", [", source_buffer.GetStorage());

		for (unsigned index = 0; index < nmatches; ++index)
		{
			if (results[index].first_char != (unsigned) -1)
				if (results[index].last_char + 1 - results[index].first_char != 0)
				{
					ES_TempBuffer match_buffer;
					RE_DumpString(match_buffer, input + results[index].first_char, results[index].last_char + 1 - results[index].first_char);

					dbg_printf("\"%S\"", match_buffer.GetStorage());
				}
				else
					dbg_printf("\"\"");
			else
				dbg_printf("undefined");

			if (index < nmatches - 1)
				dbg_printf(", ");
		}

		dbg_printf("]);\n");
	}
}

#endif // REGEXP_DUMP_TESTS

BOOL RegExp::ExecL(const uni_char *input, unsigned int length, unsigned int last_index, RegExpMatch **results, RegExpSuspension* suspend, BOOL searching) const
{
    BOOL result = RegExp::ExecL(input, length, last_index, matches, suspend, searching);
    if (result)
        *results = matches;
    return result;
}


BOOL RegExp::ExecL(const uni_char *input, unsigned int length, unsigned int last_index, RegExpMatch *results, RegExpSuspension* suspend, BOOL searching) const
{
	unsigned index = last_index, start = last_index, segment_index = 0;
	unsigned char segment_bitmap = 0;
	const unsigned *address = 0;
	RE_Object *re = ignore_case == YES ? re_ci : re_cs;

#ifdef RE_FEATURE__MACHINE_CODED
	if (RegExpNativeMatcher *fast_matcher = re->GetNativeMatcher())
	{
		for (unsigned index = 1; index <= re->GetCaptures(); ++index)
			results[index].length = UINT_MAX;

		if (fast_matcher(results, input, last_index, length - last_index))
			return TRUE;
		else
			return FALSE;
	}
#endif // RE_FEATURE__MACHINE_CODED

	unsigned match_offset = re->GetMatchOffset();

	RE_Searcher *searcher = searching ? re->GetSearcher() : 0;
	RE_Matcher the_matcher, *matcher = &the_matcher;

	matcher->SetSuspend(suspend);
	matcher->SetObjectL(re, ignore_case == YES, multi_line == YES);
	matcher->SetString(input, length);

	while (TRUE)
	{
		unsigned next_index;

		if (searcher)
		{
			segment_bitmap = searcher->Search(input, length, index, next_index);
			if (segment_bitmap != 0)
				start = index = next_index;
			else
				break;
		}
		else
		{
			address = matcher->GetFull ();
			start = index;
			goto match_single;
		}

		index += match_offset;

		for (segment_index = 0; segment_bitmap != 0; ++segment_index)
		{
			if (segment_bitmap & 1)
			{
				address = matcher->GetSegment(segment_index);

			match_single:
				RE_Matcher::ExecuteResult result = matcher->ExecuteL(address, index, length);

				if (result == RE_Matcher::EXECUTE_SUCCESS)
				{
					results[0].start = start;
					results[0].length = matcher->GetEndIndex() - start;

					for (unsigned cindex = 0; cindex < re->GetCaptures(); ++cindex)
						matcher->GetCapture(cindex, results[cindex + 1].start, results[cindex + 1].length);

#ifdef REGEXP_DUMP_TESTS
					RE_DumpTest(input, length, last_index, re, 1 + re->GetCaptures (), matches);
#endif // REGEXP_DUMP_TESTS

					return TRUE;
				}
			}
			if ((segment_bitmap >>= 1) != 0)
				matcher->Reset();
		}

		index -= match_offset;

		if (!searching)
			break;

		unsigned next_attempt_index;

		if (matcher->GetNextAttemptIndex (next_attempt_index) && next_attempt_index > index)
			index = next_attempt_index;
		else
			++index;

		if (index > length)
			break;

		matcher->Reset();
	}

#ifdef REGEXP_DUMP_TESTS
	RE_DumpTest(input, length, last_index, re, 0, NULL);
#endif // REGEXP_DUMP_TESTS

	return FALSE;
}

#ifdef ECMASCRIPT_NATIVE_SUPPORT
static OP_STATUS
CallCreateNativeMatcher(RE_Native *native, const OpExecMemory *&matcher, bool &supported)
{
	TRAPD(status, supported = native->CreateNativeMatcher(matcher));
	return status;
}

OP_STATUS
RegExp::CreateNativeMatcher(OpExecMemoryManager *executable_memory)
{
#ifdef RE_FEATURE__MACHINE_CODED
	RE_Object *object = re_cs ? re_cs : re_ci;
	RE_Native native(object, executable_memory);
	const OpExecMemory *matcher;

	if (!object->GetNativeFailed())
	{
		bool supported = false;

		RETURN_IF_ERROR(CallCreateNativeMatcher(&native, matcher, supported));
		if (supported)
		{
			object->SetNativeMatcher(reinterpret_cast<RegExpNativeMatcher *>(matcher->address), matcher);
			return OpStatus::OK;
		}
		else
			object->SetNativeFailed();
	}
#endif // RE_FEATURE__MACHINE_CODED

	return OpStatus::ERR;
}

RegExpNativeMatcher *
RegExp::GetNativeMatcher()
{
#ifdef RE_FEATURE__MACHINE_CODED
	return (re_cs ? re_cs : re_ci)->GetNativeMatcher();
#else // RE_FEATURE__MACHINE_CODED
	return NULL;
#endif // RE_FEATURE__MACHINE_CODED
}

#endif // ECMASCRIPT_NATIVE_SUPPORT


RegExpMatch *
RegExp::GetMatchArray()
{
	return matches;
}

void
RegExp::SetIgnoreCaseFlag(BOOL3 v)
{
	ignore_case = v;
}

void
RegExp::SetMultiLineFlag(BOOL3 v)
{
	multi_line = v;
}
