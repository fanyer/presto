/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2006
 */

#include "core/pch.h"

#include "modules/regexp/include/regexp_api.h"
#include "modules/regexp/include/regexp_advanced_api.h"
#include "modules/regexp/src/regexp_private.h"

OP_STATUS
OpRegExp::CreateRegExp( OpRegExp **re, const uni_char *pattern, OpREFlags *inflags )
{
	RegExp *regex = OP_NEW(RegExp, ());
	if (regex == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RegExpFlags flags;
	flags.ignore_case = inflags->case_insensitive ? YES : NO;
	flags.multi_line = inflags->multi_line ? YES : NO;
	flags.ignore_whitespace = inflags->ignore_whitespace;

	OP_STATUS res = regex->Init(pattern, uni_strlen(pattern), NULL, &flags);

	if (OpStatus::IsMemoryError(res))
	{
		regex->DecRef();
		return res;
	}

	if (OpStatus::IsError(res))
	{
		regex->DecRef();
		return OpStatus::ERR;
	}

	*re = OP_NEW(OpRegExp, (regex));
	if (*re == NULL)
	{
		regex->DecRef();
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OpRegExp::OpRegExp(RegExp *regex)
	: regex(regex)
{
}

OpRegExp::~OpRegExp()
{
	if (regex)
		regex->DecRef();
	regex = NULL;
}

OP_STATUS
OpRegExp::Match( const uni_char *string, OpREMatchLoc *match )
{
	RegExpMatch* matchlocs;
	int OP_MEMORY_VAR nmatches = 0;
	OP_STATUS res = OpStatus::OK;

	TRAP( res, nmatches = regex->ExecL(string, uni_strlen(string), 0, &matchlocs));
	if (OpStatus::IsError(res))
		return res;

	if (nmatches == 0)
	{
		match->matchloc = (unsigned int)-1;
		match->matchlen = (unsigned int)-1;
	}
	else
	{
		match->matchloc = matchlocs[0].start;
		match->matchlen = matchlocs[0].length;
	}

	return OpStatus::OK;
}

OP_STATUS
OpRegExp::Match( const uni_char *string, int *nmatches, OpREMatchLoc **matches )
{
	RegExpMatch* matchlocs;
	OP_STATUS res;
	int i;

	BOOL matched = FALSE;

	TRAP( res, matched = regex->ExecL(string, uni_strlen(string), 0, &matchlocs));
	if (OpStatus::IsError(res))
		return res;

	if (!matched)
	{
		*nmatches = 0;
		*matches = NULL;

		return OpStatus::OK;
	}

	*nmatches = regex->GetNumberOfCaptures() + 1;
	*matches = OP_NEWA(OpREMatchLoc, *nmatches);

	if (*matches == NULL)
		return OpStatus::ERR_NO_MEMORY;

	for ( i=0 ; i < *nmatches ; i++ )
	{
		(*matches)[i].matchloc = matchlocs[i].start;
		(*matches)[i].matchlen = matchlocs[i].length;
	}

	return OpStatus::OK;
}
