/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SEARCH_ENGINE  // to remove compilation errors with ADVANCED_OPVECTOR

#include "modules/search_engine/ACT.h"

// this file contains case-insensitive variants of ACT methods

static char *uni_strupr8c(const uni_char *word)
{
	uni_char *upper_word;
	char *utf8_word;
	int len;

	len = (int)uni_strlen(word);

	if ((upper_word = OP_NEWA(uni_char, len + 1)) == NULL)
		return NULL;

	uni_strcpy(upper_word, word);
	uni_strupr(upper_word);

	if ((utf8_word = OP_NEWA(char, len * 4 + 1)) == NULL)
	{
		OP_DELETEA(upper_word);
		return NULL;
	}

	to_utf8(utf8_word, upper_word, len * 4 + 1);

	OP_DELETEA(upper_word);

	return utf8_word;
}

static char *uni_str8c(const uni_char *word)
{
	char *utf8_word;
	int len;

	len = (int)uni_strlen(word);

	if ((utf8_word = OP_NEWA(char, len * 4 + 1)) == NULL)
	{
		OP_DELETEA(utf8_word);
		return NULL;
	}

	to_utf8(utf8_word, word, len * 4 + 1);

	return utf8_word;
}

static char *strupr8c(const char *utf8_word)
{
	uni_char *upper_word;
	char *up8_word;
	int len;

	len = (int)op_strlen(utf8_word);

	if ((upper_word = OP_NEWA(uni_char, len + 1)) == NULL)
		return NULL;

	len = from_utf8(upper_word, utf8_word, (len + 1) * sizeof(uni_char));
	uni_strupr(upper_word);

	if ((up8_word = OP_NEWA(char, len * 2 + 1)) == NULL)
	{
		OP_DELETEA(upper_word);
		return NULL;
	}

	to_utf8(up8_word, upper_word, len * 2 + 1);

	OP_DELETEA(upper_word);

	return up8_word;
}

static char *strlwr8(char *utf8_word)
{
	uni_char *lower_word;
	int len;

	len = (int)op_strlen(utf8_word) + 1;

	if ((lower_word = OP_NEWA(uni_char, len)) == NULL)
		return NULL;

	from_utf8(lower_word, utf8_word, len * sizeof(uni_char));
	uni_strlwr(lower_word);

	to_utf8(utf8_word, lower_word, len);

	OP_DELETEA(lower_word);

	return utf8_word;
}

OP_STATUS ACT::AddWord(const uni_char *word, WordID id, BOOL overwrite_existing)
{
	char *utf8_word;
	OP_STATUS err;

	RETURN_OOM_IF_NULL(utf8_word = uni_strupr8c(word));

	err = AddCaseWord(utf8_word, id, overwrite_existing);

	OP_DELETEA(utf8_word);

	return err;
}

OP_STATUS ACT::AddWord(const char *utf8_word, WordID id, BOOL overwrite_existing)
{
	char *up8_word;
	OP_STATUS err;

	RETURN_OOM_IF_NULL(up8_word = strupr8c(utf8_word));

	err = AddCaseWord(up8_word, id, overwrite_existing);

	OP_DELETEA(up8_word);

	return err;
}

OP_STATUS ACT::AddCaseWord(const uni_char *word, WordID id, BOOL overwrite_existing)
{
	char *utf8_word;
	OP_STATUS err;

	RETURN_OOM_IF_NULL(utf8_word = uni_str8c(word));

	err = AddCaseWord(utf8_word, id, overwrite_existing);

	OP_DELETEA(utf8_word);

	return err;
}


OP_STATUS ACT::DeleteWord(const uni_char *word)
{
	char *utf8_word;
	OP_STATUS err;

	RETURN_OOM_IF_NULL(utf8_word = uni_strupr8c(word));

	err = DeleteCaseWord(utf8_word);

	OP_DELETEA(utf8_word);

	return err;
}

OP_STATUS ACT::DeleteWord(const char *utf8_word)
{
	char *up8_word;
	OP_STATUS err;

	RETURN_OOM_IF_NULL(up8_word = strupr8c(utf8_word));

	err = DeleteCaseWord(up8_word);

	OP_DELETEA(up8_word);

	return err;
}

OP_STATUS ACT::DeleteCaseWord(const uni_char *word)
{
	char *utf8_word;
	OP_STATUS err;

	RETURN_OOM_IF_NULL(utf8_word = uni_str8c(word));

	err = DeleteCaseWord(utf8_word);

	OP_DELETEA(utf8_word);

	return err;
}


ACT::WordID ACT::Search(const uni_char *word)
{
	char *utf8_word;
	ACT::WordID id;

	if ((utf8_word = uni_strupr8c(word)) == NULL)
		return 0;

	id = CaseSearch(utf8_word);

	OP_DELETEA(utf8_word);

	return id;
}

ACT::WordID ACT::Search(const char *utf8_word)
{
	char *up8_word;
	ACT::WordID id;

	if ((up8_word = strupr8c(utf8_word)) == NULL)
		return 0;

	id = CaseSearch(up8_word);

	OP_DELETEA(up8_word);

	return id;
}

ACT::WordID ACT::CaseSearch(const uni_char *word)
{
	char *utf8_word;
	ACT::WordID id;

	if ((utf8_word = uni_str8c(word)) == NULL)
		return 0;

	id = CaseSearch(utf8_word);

	OP_DELETEA(utf8_word);

	return id;
}


int ACT::PrefixSearch(WordID *result, const uni_char *prefix, int max_results)
{
	char *utf8_word;
	int count;

	if ((utf8_word = uni_strupr8c(prefix)) == NULL)
		return 0;

	count = PrefixCaseSearch(result, utf8_word, max_results);

	OP_DELETEA(utf8_word);

	return count;
}

int ACT::PrefixSearch(WordID *result, const char *utf8_prefix, int max_results)
{
	char *up8_word;
	int count;

	if ((up8_word = strupr8c(utf8_prefix)) == NULL)
		return 0;

	count = PrefixCaseSearch(result, up8_word, max_results);

	OP_DELETEA(up8_word);

	return count;
}

int ACT::PrefixCaseSearch(WordID *result, const uni_char *prefix, int max_results)
{
	char *utf8_word;
	int count;

	if ((utf8_word = uni_str8c(prefix)) == NULL)
		return 0;

	count = PrefixCaseSearch(result, utf8_word, max_results);

	OP_DELETEA(utf8_word);

	return count;
}


int ACT::PrefixWords(uni_char **result, const uni_char *prefix, int max_results)
{
	char *utf8_word;
	int i, j, count, len, prefix_len;
	char **result8;

	if ((utf8_word = uni_strupr8c(prefix)) == NULL)
		return 0;

	if ((result8 = OP_NEWA(char *, max_results)) == NULL)
	{
		OP_DELETEA(utf8_word);
		return 0;
	}

	count = PrefixCaseWords(result8, utf8_word, max_results);

	OP_DELETEA(utf8_word);

	prefix_len = (int)uni_strlen(prefix);

	for (i = 0; i < count; ++i)
	{
		len = (int)op_strlen(result8[i]);
		if ((result[i] = OP_NEWA(uni_char, len + 1)) == NULL)
		{
			for (j = 0; j < i; ++j)
				OP_DELETEA(result[j]);

			for (j = 0; j < count; ++j)
				OP_DELETEA(result8[j]);

			OP_DELETEA(result8);

			return 0;
		}

		if (prefix_len > 0 && uni_islower(prefix[prefix_len - 1]))
		{
			uni_strncpy(result[i], prefix, prefix_len);
			from_utf8(result[i] + prefix_len, result8[i] + prefix_len, sizeof(uni_char) * (len + 1 - prefix_len));
			uni_strlwr(result[i] + prefix_len);
		}
		else from_utf8(result[i], result8[i], sizeof(uni_char) * (len + 1));
	}

	for (j = 0; j < count; ++j)
		OP_DELETEA(result8[j]);

	OP_DELETEA(result8);

	return count;
}

int ACT::PrefixWords(char **result, const char *utf8_prefix, int max_results)
{
	char *up8_word;
	int i, count, len;
	uni_char last_char[3];  /* ARRAY OK 2010-09-24 roarl */

	if ((up8_word = strupr8c(utf8_prefix)) == NULL)
		return 0;

	count = PrefixCaseWords(result, up8_word, max_results);

	OP_DELETEA(up8_word);

	if (*utf8_prefix != 0)
	{
		len = (int)op_strlen(utf8_prefix);
		i = len - 1;
		while ((utf8_prefix[i] & 0x80) != 0 && i > 0 && (utf8_prefix[i - 1] & 0x80) != 0)
			--i;

		from_utf8(last_char, utf8_prefix + i, 3 * sizeof(uni_char));
		if (uni_islower(*last_char))
		{
			for (i = 0; i < count; ++i)
			{
				op_strncpy(result[i], utf8_prefix, len);
				strlwr8(result[i] + len);
			}
		}
		else {
			for (i = 0; i < count; ++i)
				op_strncpy(result[i], utf8_prefix, len);
		}
	}

	return count;
}

int ACT::PrefixCaseWords(uni_char **result, const uni_char *prefix, int max_results)
{
	char *utf8_word;
	int i, j, count, len;
	char **result8;

	if ((utf8_word = uni_str8c(prefix)) == NULL)
		return 0;

	if ((result8 = OP_NEWA(char *, max_results)) == NULL)
	{
		OP_DELETEA(utf8_word);
		return 0;
	}

	count = PrefixCaseWords(result8, utf8_word, max_results);

	OP_DELETEA(utf8_word);

	for (i = 0; i < count; ++i)
	{
		len = (int)op_strlen(result8[i]) + 1;
		if ((result[i] = OP_NEWA(uni_char, len)) == NULL)
		{
			for (j = 0; j < i; ++j)
				OP_DELETEA(result[j]);

			for (j = 0; j < count; ++j)
				OP_DELETEA(result8[j]);

			OP_DELETEA(result8);

			return 0;
		}

		from_utf8(result[i], result8[i], sizeof(uni_char) * len);
	}

	for (j = 0; j < count; ++j)
		OP_DELETEA(result8[j]);

	OP_DELETEA(result8);

	return count;
}

SearchIterator<ACT::PrefixResult> *ACT::PrefixSearch(const uni_char *prefix, BOOL single_word)
{
	char *utf8_word;
	SearchIterator<ACT::PrefixResult> *it;

	if ((utf8_word = uni_strupr8c(prefix)) == NULL)
		return 0;

	it = PrefixCaseSearch(utf8_word, single_word);

	OP_DELETEA(utf8_word);

	return it;
}

SearchIterator<ACT::PrefixResult> *ACT::PrefixSearch(const char *utf8_prefix, BOOL single_word)
{
	char *up8_word;
	SearchIterator<ACT::PrefixResult> *it;

	if ((up8_word = strupr8c(utf8_prefix)) == NULL)
		return 0;

	it = PrefixCaseSearch(up8_word, single_word);

	OP_DELETEA(up8_word);

	return it;
}

SearchIterator<ACT::PrefixResult> *ACT::PrefixCaseSearch(const uni_char *prefix, BOOL single_word)
{
	char *utf8_word;
	SearchIterator<ACT::PrefixResult> *it;

	if ((utf8_word = uni_str8c(prefix)) == NULL)
		return 0;

	it = PrefixCaseSearch(utf8_word, single_word);

	OP_DELETEA(utf8_word);

	return it;
}

#endif  // SEARCH_ENGINE

