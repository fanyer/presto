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
#include "modules/search_engine/ACTUtil.h"

#define NUR_MASK 0x7FFF
#define NUR_MAX  0x8000

class ACTPrefixIterator : public SearchIterator<ACT::PrefixResult>
{
public:
	ACTPrefixIterator(void) {m_prefix = NULL; m_current_result.id = 0; m_current_result.utf8_word = NULL; m_status = OpBoolean::IS_FALSE; m_act = NULL;}
	~ACTPrefixIterator(void) {op_free(m_prefix);}

	virtual BOOL Next(void)
	{
		if (m_status != OpBoolean::IS_TRUE)
			return FALSE;
		return (m_status = m_act->FindNext(m_current_result, m_prefix)) == OpBoolean::IS_TRUE;
	}

	virtual BOOL Prev(void) {return FALSE;}

	CHECK_RESULT(virtual OP_STATUS Error(void) const) {return OpStatus::IsError(m_status) ? m_status : OpStatus::OK;}

	virtual int Count(void) const {return m_status != OpBoolean::IS_TRUE ? 0 : -1;}

	virtual BOOL End(void) const {return m_status != OpBoolean::IS_TRUE;}

	virtual BOOL Beginning(void) const {return TRUE;}

	virtual const ACT::PrefixResult &Get(void) {return m_current_result;}

protected:
	friend class ACT;

	CHECK_RESULT(virtual OP_STATUS Init(const char *prefix, ACT *act))
	{
		RETURN_OOM_IF_NULL(m_prefix = op_strdup(prefix));
		m_act = act;
		m_status = m_act->FindFirst(m_current_result, m_prefix);
		return OpStatus::IsError(m_status) ? m_status : OpStatus::OK;
	}

	char *m_prefix;
	ACT::PrefixResult m_current_result;
	OP_BOOLEAN m_status;
	ACT *m_act;
};

/** Used as prefix iterator when the last word of the query is a whole word, not a prefix after all */
class ACTSingleWordIterator : public ACTPrefixIterator
{
	virtual BOOL Next(void)
	{
		m_status = OpBoolean::IS_FALSE;
		return FALSE;
	}

	CHECK_RESULT(virtual OP_STATUS Init(const char *prefix, ACT *act))
	{
		RETURN_OOM_IF_NULL(m_current_result.utf8_word = SetNewStr(prefix));
		m_current_result.id = act->CaseSearch(prefix);
		m_status = m_current_result.id ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
		return OpStatus::OK;
	}
};

ACT::ACT(void) : BSCache(ACT_MAX_CACHE_BRANCHES)
{
	op_memset(random_status, 0, sizeof(random_status));

	m_TailCallback = NULL;

#ifdef _DEBUG
	collision_count = 0;
#endif
}

OP_STATUS ACT::Open(const uni_char* path, BlockStorage::OpenMode mode, TailCallback tc, void *callback_val, OpFileFolder folder)
{
	int block_size;
	TrieBranch *t;

	m_TailCallback = tc;
	m_callback_val = callback_val;

	block_size = (TrieBranch::GetPackedSize() + 12 + 511) & ~511;
	RETURN_IF_ERROR(m_storage.Open(path, mode, block_size, 0, folder));

	if (m_storage.GetBlockSize() != block_size)
	{
		m_storage.Close();
		return OpStatus::ERR_PARSING_FAILED;
	}

	if (!m_storage.IsNativeEndian())
		m_storage.SetOnTheFlyCnvFunc(&TrieNode::SwitchEndian, NULL);

	if (m_storage.GetFileSize() < (OpFileLength)(m_storage.GetBlockSize() * 2))  // empty file
	{
		if ((t = OP_NEW(TrieBranch, (0, NULL, 0, 0))) == NULL)
		{
			m_storage.Close();
			return OpStatus::ERR_NO_MEMORY;
		}

		if (OpStatus::IsError(t->Write(&m_storage)))
		{
			m_storage.Close();
			return OpStatus::ERR_NO_DISK;
		}

		InitRandom();

#ifdef _DEBUG
		++branches_created;
#endif
		OP_DELETE(t);
	}
	else
		RestoreStatus(); // Restores Random

	return OpStatus::OK;
}

OP_STATUS ACT::Close(void)
{
	OP_STATUS err, err2;
	err = Flush(ReleaseAll);

	ClearCache();

	if (m_storage.InTransaction())
	{
		err2 = SaveStatus();
		err = OpStatus::IsError(err) ? err : err2;
		err2 = m_storage.Commit();
		err = OpStatus::IsError(err) ? err : err2;
	}
	m_storage.Close();

	return err;
}

OP_STATUS ACT::Clear(void)
{
	TrieBranch *t;

	Abort();
	RETURN_IF_ERROR(m_storage.Clear());

	if ((t = OP_NEW(TrieBranch, (0, NULL, 0, 0))) == NULL)
	{
		m_storage.Close();
		return OpStatus::ERR_NO_MEMORY;
	}

	if (OpStatus::IsError(t->Write(&m_storage)))
	{
		m_storage.Close();
		return OpStatus::ERR_NO_DISK;
	}

	InitRandom();

#ifdef _DEBUG
	++branches_created;
#endif
	OP_DELETE(t);

	return OpStatus::OK;
}

int ACT::WordsEqual(const char *w1, const char *w2, int max)
{
	int len = 0;

	if (max == 0)
		return TRUE;

	if (max < 0)
		max = (int)op_strlen(w1) + 1;

	while (*w1 != 0 && *w2 != 0)
	{
		SkipNonPrintableChars(w1);
		SkipNonPrintableChars(w2);

		if (*w1 != *w2)
			return len;

		if (--max <= 0)
			return -1;

		++w1;
		++len;
		++w2;
	}

	SkipNonPrintableChars(w1);
	SkipNonPrintableChars(w2);

	return *w1 == 0 && *w2 == 0 ? -1 : len;
}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t ACT::EstimateMemoryUsed() const
{
	return BSCache::EstimateMemoryUsed() +
		sizeof(m_TailCallback) +
		sizeof(m_callback_val);
}
#endif

OP_STATUS ACT::AddCaseWord(const char *utf8_word, WordID id, BOOL overwrite_existing)
{
	if (!m_storage.InTransaction())
	{
		RETURN_IF_ERROR(m_storage.BeginTransaction(m_flush_mode == JournalOnly));
	}

	if (m_TailCallback != NULL)
	{
		WordID tail_id;
		char *tail_word;
		int eq_len;
		OP_STATUS err;

		if ((tail_id = CaseSearch(utf8_word)) != 0)
		{
			RETURN_IF_ERROR(m_TailCallback(&tail_word, tail_id, m_callback_val));
			if ((eq_len = WordsEqual(tail_word, utf8_word)) > 0)
			{
				if (OpStatus::IsError((err = AddCaseWord(tail_word, tail_id, eq_len, overwrite_existing))))
				{
					OP_DELETEA(tail_word);
					return err;
				}
			}
			OP_DELETEA(tail_word);
		}
	}
	else
		m_NUR_mark = (m_NUR_mark + 1) & NUR_MASK;  // in the first case it's done in CaseSearch

	return AddCaseWord(utf8_word, id, 0, overwrite_existing);
}

OP_STATUS ACT::AddCaseWord(const char *utf8_word, WordID id, int new_len, BOOL overwrite_existing)
{
	NodePointer t(this);
	OP_STATUS err;
	BOOL e_exist = TRUE;
	int current_super, conflict_super;
	const char *next_char;

	if (!m_storage.InTransaction())
	{
		RETURN_IF_ERROR(m_storage.BeginTransaction(m_flush_mode == JournalOnly));
	}

	SkipNonPrintableChars(utf8_word);

	if (*utf8_word == 0)
		return OpStatus::OK;

	if (OpStatus::IsError(err = t.Reset(2)))
		goto add_word_rollback;

	while (*utf8_word != 0)
	{
		next_char = utf8_word + 1;
		SkipNonPrintableChars(next_char);

		if (!t.ValidChar(utf8_word[0]))
		{
			if (!t.Reposition(t.GetCurrentOffset(), utf8_word[0]))
				if (OpStatus::IsError(err = t.Move(t.GetCurrentOffset())))
					goto add_word_rollback;
		}
		if (OpStatus::IsError(err = t.Goto(utf8_word[0])))
			goto add_word_rollback;

		// conflict?
		if (!t.IsFree() && t.GetParent() != t.GetCurrentParent())
		{
			current_super = t.GetSuperParent(t.GetCurrentParent());  // the first predecessor in this branch
			conflict_super = t.GetSuperParent(t.GetParent());

			// there are better algorithms in theory, but the experiments proved that this one is the best (see older versions in cvs)
			if (t.GetParent() != 0 &&
				(t.GetCurrentParent() == 0 ||
				t.GetChildrenSize(current_super) + (int)op_strlen(utf8_word) < t.GetChildrenSize(conflict_super)))
			{
				if (!t.Reposition(t.GetParent()))  // repositioning the shorter chain makes the fill factor worse
					if (OpStatus::IsError(err = t.Move(conflict_super)))
						goto add_word_rollback;
			}
			else {
				if (!t.Reposition())
					if (OpStatus::IsError(err = t.Move(current_super)))
						goto add_word_rollback;
			}
		}

		if (!t.IsFree())  // is this node already used with the current word as a prefix?
		{
			if (*next_char == 0)
			{
				if (!t.IsWord())
				{
					e_exist = FALSE;
					t.SetIsWord();
				}
			}
			else if (t.IsFinal())  // is the current word longer?
			{
				if (m_TailCallback != NULL && !t.IsWord() && new_len <= 0)
					break;  // just overwriting the WordId of the same word

				if (OpStatus::IsError(err = t.NewNode((unsigned char)*next_char)))
					goto add_word_rollback;
			}
		}
		else {
			e_exist = FALSE;
			t.SetParent(t.GetCurrentParent());

			if (*next_char == 0)
				t.SetFinalWord();
			else if (m_TailCallback != NULL && new_len <= 0 && *next_char != 0)
			{
				t.SetFinal();
				break;  // end of input
			}
			else {
				t.SetFinal(); // Temporarily, until NewNode sets a child
				if (OpStatus::IsError(err = t.NewNode((unsigned char)*next_char)))
					goto add_word_rollback;
			}
		}

		utf8_word = next_char;
		if (new_len > 0)
			--new_len;
	}

	if (!e_exist || (overwrite_existing && t.GetId() != id))
		t.SetId(id);

#ifdef _DEBUG
	if (t.GetCurrentPointer()->modified)
		OP_ASSERT(TrieBranch::CheckIntegrity(t.GetCurrentPointer(), this));
#endif

	return e_exist ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;

add_word_rollback:
	t.Reset();
	Abort();
	return err;
}

OP_STATUS ACT::DeleteCaseWord(const char *utf8_word)
{
	BSCache::Item::DiskId last_branch;
	int last_pos;
	const char *last_word, *word_origin;
	NodePointer t(this), tmp(this);
	OP_STATUS err;
	int i, max_offset, children;
	const char *next_char;

	SkipNonPrintableChars(utf8_word);

	if (*utf8_word == 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	m_NUR_mark = (m_NUR_mark + 1) & NUR_MASK;

	word_origin = utf8_word;

	last_branch = 1;
	last_pos = 0;
	last_word = word_origin;

	RETURN_IF_ERROR(t.Reset(2));

	i = 1;
	max_offset = 257 - FIRST_CHAR;

	// find the point from which the word is unique
	while (*utf8_word != 0)
	{
		next_char = utf8_word + 1;
		SkipNonPrintableChars(next_char);

		tmp = t;
		RETURN_IF_ERROR(tmp.Goto(utf8_word[0]));

		if (tmp.IsFree() || tmp.GetParent() != tmp.GetCurrentParent())
			return OpStatus::ERR_OUT_OF_RANGE;

		children = 0;

		while (i < max_offset)
		{
			if (!tmp.GetCurrentPointer()->IsFree(i) &&
				tmp.GetCurrentPointer()->GetParent(i) == tmp.GetCurrentParent())

				if (++children > 1)  // previous node seem to be unique, but it isn't
				{
					last_branch = 1;
					last_pos = 0;
					last_word = word_origin;
					break;
				}

			++i;
		}

		// current path might be unique, but another word is prefix of this one
		if (t.IsWord() && last_pos != 0)
		{
			last_branch = t.GetCurrentBranch();
			last_pos = t.GetCurrentOffset();
			last_word = utf8_word;
		}

		// this particular node has just one child
		if (children <= 1 && last_pos == 0)
		{
			last_branch = t.GetCurrentBranch();
			last_pos = t.GetCurrentOffset();
			last_word = utf8_word;
		}

		if (tmp.IsFinal() && *next_char != 0)
		{
			if (m_TailCallback == NULL || tmp.IsWord())
				return OpStatus::ERR_OUT_OF_RANGE;
			else {
				while (*next_char != 0)
					++next_char;
			}
		}

		utf8_word = next_char;

		t = tmp;
		tmp.Reset();

		if (utf8_word[0] != 0)
		{
			if (t.HasChild())
				i = 1;
			else i = t.GetOffset();

			if ((max_offset = i + 256 - FIRST_CHAR) > TRIE_SIZE)
				max_offset = TRIE_SIZE;
			if (i <= 0)
				i = 1;
		}
	}

	// currently found string isn't an indexed word
	if (!t.IsWord() && !t.IsFinal())
		return OpStatus::ERR_OUT_OF_RANGE;


	if (!m_storage.InTransaction())
	{
		RETURN_IF_ERROR(m_storage.BeginTransaction(m_flush_mode == JournalOnly));
	}

	// current word is a prefix of another word(s)
	if (!t.IsFinal())
	{
		t.SetIsNotWord();
		return OpStatus::OK;
	}
	// since here, Word and Final are set

	// not unique until the last character
	if (last_pos == 0)
	{
		t.SetFree();
		return OpStatus::OK;
	}

	// delete the nodes

	utf8_word = last_word;

	if (OpStatus::IsError(err = t.Reset(last_branch)))
		goto delete_word_rollback;

	t.SetCurrentOffset(last_pos);

	// set the last non-unique position
	tmp = t;

	if (OpStatus::IsError(err = tmp.Goto(utf8_word[0])))
		goto delete_word_rollback;

	if (t.IsWord())
		t.SetFinalWord();
	else t.SetFree();

	t = tmp;
	tmp.Reset();

	++utf8_word;
	SkipNonPrintableChars(utf8_word);

	if (!t.IsWord() && t.IsFinal() && m_TailCallback != NULL)
	{
		while (*utf8_word != 0)
			++utf8_word;
	}


	while (*utf8_word != 0)
	{
		next_char = utf8_word + 1;
		SkipNonPrintableChars(next_char);

		tmp = t;
		if (OpStatus::IsError(err = tmp.Goto(utf8_word[0])))
			goto delete_word_rollback;

		if (!tmp.IsWord() && tmp.IsFinal() && m_TailCallback != NULL)
		{
			while (*next_char != 0)
				++next_char;
		}

		t.SetFree();

		if (t.GetCurrentBranch() != tmp.GetCurrentBranch() && t.GetCurrentBranch() != last_branch)
			Unlink(t.GetCurrentPointer());

		t = tmp;
		tmp.Reset();

		utf8_word = next_char;
	}

	t.SetFree();

	if (t.GetCurrentBranch() != last_branch)
		Unlink(t.GetCurrentPointer());

#ifdef _DEBUG
	if (t.GetCurrentPointer()->modified)
		OP_ASSERT(TrieBranch::CheckIntegrity(t.GetCurrentPointer(), this));
#endif

	return OpStatus::OK;

delete_word_rollback:
	t.Reset();
	tmp.Reset();
	Abort();
	return err;
}


/*OP_STATUS ACT::ToNativeEndian(BlockStorage::ProgressCallback progress, void *user_value)
{
	return m_storage.ToNativeEndian((BlockStorage::EndianCallback)&TrieNode::SwitchEndian, 
		10,	progress, user_value);
}*/

void ACT::Abort(void)
{
	ClearCache();

	if (m_storage.InTransaction())
		OpStatus::Ignore(m_storage.Rollback());

	RestoreStatus(); // Restores Random
}

OP_STATUS ACT::Commit(void)
{
	OP_STATUS err, err2;
	err = Flush();

	if (!m_storage.InTransaction())
		return OpStatus::OK;

	if (m_flush_mode != BSCache::JournalOnly)
	{
		err2 = SaveStatus();
		err = OpStatus::IsError(err) ? err : err2;
	}		

	err2 = m_storage.Commit();
	err = OpStatus::IsError(err) ? err : err2;
	
	return err;
}


ACT::WordID ACT::CaseSearch(const char *utf8_word)
{
	const char *next_char;
	NodePointer t(this);

	SkipNonPrintableChars(utf8_word);

	if (*utf8_word == 0)
		return 0;

	m_NUR_mark = (m_NUR_mark + 1) & NUR_MASK;

	RETURN_VALUE_IF_ERROR(t.Reset(2), 0);

	while (*utf8_word != 0)
	{
		next_char = utf8_word + 1;
		SkipNonPrintableChars(next_char);

		RETURN_VALUE_IF_ERROR(t.Goto(utf8_word[0]), 0);

		if (t.IsFree() || t.GetParent() != t.GetCurrentParent())
			return 0;

		if (t.IsFinal())
		{
			if (m_TailCallback != NULL && !t.IsWord())
				return t.GetId();
			if (*next_char != 0)
				return 0;
		}

		utf8_word = next_char;
	}

	if (!t.IsWord())
		return 0;

	return t.GetId();
}

int ACT::PrefixCaseWords(char **result, const char *utf8_prefix, int max_results)
{
	NodePointer t(this);
	int rcount;
	int i;
	const char *prefix;
	const char *next_char;
	size_t size;

	RETURN_VALUE_IF_ERROR(t.Reset(2), 0);

	if (max_results <= 0)
		return 0;

	m_NUR_mark = (m_NUR_mark + 1) & NUR_MASK;

	for (i = 0; i < max_results; ++i)
		result[i] = NULL;

	prefix = utf8_prefix;

	SkipNonPrintableChars(utf8_prefix);

	while (*utf8_prefix != 0)
	{
		next_char = utf8_prefix + 1;
		SkipNonPrintableChars(next_char);

		RETURN_VALUE_IF_ERROR(t.Goto(utf8_prefix[0]), 0);

		if (t.IsFree() || t.GetParent() != t.GetCurrentParent())
			return 0;

		if (t.IsFinal() && *next_char != 0)
		{
			if (m_TailCallback == NULL || t.IsWord())
				return 0;
			else
				break;
		}

		utf8_prefix = next_char;
	}

	rcount = 0;

	if (m_TailCallback != NULL && !t.IsWord() && t.IsFinal())
	{
		RETURN_VALUE_IF_ERROR(m_TailCallback(result, t.GetId(), m_callback_val), 0);

		return 1;
	}

	size = prefix[0] == 0 ? 33 : 32 * (op_strlen(prefix) / 32 + 1) + 1;
	if ((result[0] = OP_NEWA(char, size)) == NULL)
		return 0;

	op_strcpy(result[0], prefix);

	++rcount;

	if (t.IsFinal() && t.IsWord())
		return rcount;

	if (t.GetCurrentOffset() != 0 && t.IsWord())
	{
		size = 32 * (op_strlen(prefix) / 32 + 1) + 1;
		if ((result[1] = OP_NEWA(char, size)) == NULL)
		{
			OP_DELETEA(result[0]);
			result[0] = NULL;
			return 0;
		}

		op_strcpy(result[1], prefix);
		++rcount;
	}

	if (!t.IsFinal() && OpStatus::IsError(NodePointer::GetSubTree(result, &rcount, t, max_results)))
	{
		for (i = 0; i < max_results; ++i)
			if (result[i] != NULL)
			{
				OP_DELETEA(result[i]);
				result[i] = NULL;
			}

		return 0;
	}

	if (rcount == 0)
	{
		OP_DELETEA(result[0]);
		result[0] = NULL;
	}

	return rcount;
}

int ACT::PrefixCaseSearch(WordID *result, const char *utf8_prefix, int max_results)
{
	NodePointer t(this);
	OP_STATUS err;
	int rcount;
	const char *next_char;

	RETURN_VALUE_IF_ERROR(t.Reset(2), 0);

	if (max_results <= 0)
		return 0;

	m_NUR_mark = (m_NUR_mark + 1) & NUR_MASK;

	SkipNonPrintableChars(utf8_prefix);

	while (*utf8_prefix != 0)
	{
		next_char = utf8_prefix + 1;

		SkipNonPrintableChars(next_char);

		if (OpStatus::IsError(err = t.Goto(utf8_prefix[0])))
			return 0;

		if (t.IsFree() || t.GetParent() != t.GetCurrentParent())
			return 0;

		if (t.IsFinal() && *next_char != 0)
		{
			if (m_TailCallback == NULL || t.IsWord())
				return 0;
			else {
				result[0] = t.GetId();
				return 1;
			}
		}

		utf8_prefix = next_char;
	}

	rcount = 0;

	if ((t.GetCurrentOffset() != 0 && t.IsWord()) || (m_TailCallback != NULL && t.IsFinal()))
		result[rcount++] = t.GetId();

	if (!t.IsFinal())
		RETURN_VALUE_IF_ERROR(NodePointer::GetSubTree(result, &rcount, t, max_results), 0);

	return rcount;
}

SearchIterator<ACT::PrefixResult> *ACT::PrefixCaseSearch(const char *utf8_prefix, BOOL single_word)
{
	ACTPrefixIterator *it;

	if (single_word)
	{
		if ((it = OP_NEW(ACTSingleWordIterator, ())) == NULL)
			return NULL;
	}
	else
	{
		if ((it = OP_NEW(ACTPrefixIterator, ())) == NULL)
			return NULL;
	}

	if (OpStatus::IsError(it->Init(utf8_prefix, this)))
	{
		OP_DELETE(it);
		return NULL;
	}

	return it;
}

OP_BOOLEAN ACT::FindFirst(PrefixResult &res, const char *utf8_prefix)
{
	NodePointer t(this);
	OP_STATUS err;
	const char *next_char;
	size_t size;

	RETURN_IF_ERROR(t.Reset(2));
	if ((int)t.GetCurrentPointer()->NumFilled() == 0)
		return OpBoolean::IS_FALSE;

	m_NUR_mark = (m_NUR_mark + 1) & NUR_MASK;

	OP_DELETEA(res.utf8_word);

	size = 32 * (op_strlen(utf8_prefix) / 32 + 1) + 1;
	RETURN_OOM_IF_NULL(res.utf8_word = OP_NEWA(char, size));

	op_strcpy(res.utf8_word, utf8_prefix);

	SkipNonPrintableChars(utf8_prefix);

	while (*utf8_prefix != 0)
	{
		next_char = utf8_prefix + 1;

		SkipNonPrintableChars(next_char);

		if (OpStatus::IsError(err = t.Goto(utf8_prefix[0])))
		{
			if (err == OpStatus::ERR_OUT_OF_RANGE)
				return OpBoolean::IS_FALSE;
			else return err;
		}

		if (t.IsFree() || t.GetParent() != t.GetCurrentParent())
			return OpBoolean::IS_FALSE;

		if (t.IsFinal() && *next_char != 0)
		{
			if (m_TailCallback == NULL || t.IsWord())
				return OpBoolean::IS_FALSE;
			else {
				res.id = t.GetId();
				return OpBoolean::IS_TRUE;
			}
		}

		utf8_prefix = next_char;
	}

	if (t.GetCurrentOffset() != 0 && t.IsWord())
	{
		res.id = t.GetId();
		return OpBoolean::IS_TRUE;
	}

	RETURN_IF_ERROR(NodePointer::GetFirstEntry(res, t));
	return res.id == 0 ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
}

OP_BOOLEAN ACT::FindNext(PrefixResult &res, const char *utf8_prefix)
{
	char *prev;
	char *match;
	int match_len;
	OP_BOOLEAN rv;
	NodePointer t(this);
	const char *next_char;
	size_t size;

	if ((prev = res.utf8_word) == NULL)
		return OpBoolean::IS_FALSE;

	if ((match_len = ACT::WordsEqual(res.utf8_word, utf8_prefix)) != -1)
	{
		match = res.utf8_word;
		while (match_len > 0 && *match != 0)
		{
			if ((unsigned char)*match > FIRST_CHAR)
				--match_len;
			++match;
		}

		match_len = 0;
		while (*match != 0)
			prev[match_len++] = *(match++);
		prev[match_len] = 0;
	}
	else
		prev[0] = 0;

	OpAutoArray<char> prev_a(prev);

	size = 32 * (op_strlen(utf8_prefix) / 32 + 1) + 1;
	RETURN_OOM_IF_NULL(res.utf8_word = OP_NEWA(char, size));

	op_strcpy(res.utf8_word, utf8_prefix);

	RETURN_IF_ERROR(t.Reset(2));

	m_NUR_mark = (m_NUR_mark + 1) & NUR_MASK;

	SkipNonPrintableChars(utf8_prefix);

	while (*utf8_prefix != 0)
	{
		next_char = utf8_prefix + 1;

		SkipNonPrintableChars(next_char);

		RETURN_IF_ERROR(t.Goto(utf8_prefix[0]));

		if (t.IsFree() || t.GetParent() != t.GetCurrentParent())
			return OpStatus::ERR;

		if (t.IsFinal())
			return OpBoolean::IS_FALSE;

		utf8_prefix = next_char;
	}

	// t is not at TrieNode::Final here
	if (t.GetCurrentOffset() != 0 && t.IsWord() && *prev == 0)
	{
		if (OpStatus::IsSuccess(rv = NodePointer::GetFirstEntry(res, t)))
			rv = res.id == 0 ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
	}
	else
		rv = NodePointer::GetNextEntry(res, t, prev);

	return rv;
}

#define R1 13
#define R2 9

static inline UINT32 ACT_ROTATE(UINT32 value, int shift)
{
	return (value << shift) | (value >> (32 - shift));
}

int ACT::Random(void)
{
	UINT32 rv;
	UINT32 &i1 = random_status[RANDOM_STATUS_SIZE];
	UINT32 &i2 = random_status[RANDOM_STATUS_SIZE + 1];

	rv = random_status[i1] = ACT_ROTATE(random_status[i2], R1) + ACT_ROTATE(random_status[i1], R2);

	if (i1 == 0) i1 = RANDOM_STATUS_SIZE;   --i1;
	if (i2 == 0) i2 = RANDOM_STATUS_SIZE;   --i2;

	return (int)rv & 0x7FFFFFFF;
}

void ACT::InitRandom(void)
{
	int i;

	random_status[0] = 1655855145U;

	for (i = 1; i < RANDOM_STATUS_SIZE; ++i)
		random_status[i] = random_status[i - 1] * 2891336453U + 1;

	random_status[RANDOM_STATUS_SIZE] = 0;
	random_status[RANDOM_STATUS_SIZE + 1] = 10;
}

OP_STATUS ACT::SaveStatus(void)
{
	return m_storage.WriteUserHeader(random_status, 4, 4, RANDOM_STATUS_SIZE+2) ? OpStatus::OK : OpStatus::ERR;
}

void ACT::RestoreStatus(void)
{
	if (!m_storage.ReadUserHeader(random_status, 4, 4, RANDOM_STATUS_SIZE+2))
		InitRandom();

	if (random_status[RANDOM_STATUS_SIZE] == random_status[RANDOM_STATUS_SIZE + 1] ||
		random_status[RANDOM_STATUS_SIZE] >= RANDOM_STATUS_SIZE ||
		random_status[RANDOM_STATUS_SIZE + 1] >= RANDOM_STATUS_SIZE ||
		(random_status[random_status[RANDOM_STATUS_SIZE]] == 0 &&
		 random_status[random_status[RANDOM_STATUS_SIZE + 1]] == 0))
		InitRandom();
}

BSCache::Item *ACT::NewMemoryItem(int id, Item *rbranch, int rnode, unsigned short nur)
{
	return OP_NEW(TrieBranch, (id, (TrieBranch *)rbranch, rnode, nur));
}

BSCache::Item *ACT::NewDiskItem(OpFileLength id, unsigned short nur)
{
	return OP_NEW(TrieBranch, (id, nur));
}

class DBlockReader : public BlockStorage
{
public:
	CHECK_RESULT(OP_STATUS ReadPtr(OpFileLength pos, OpFileLength *ptr))
	{
		*ptr = 0;
		RETURN_IF_ERROR(m_file->SetFilePos(pos));
		return ReadOFL(m_file, ptr);
	}

	CHECK_RESULT(OP_STATUS ReadDeletedBitField(void *bf, OpFileLength pos))
	{
		RETURN_IF_ERROR(m_file->SetFilePos(pos));
		return BlockStorage::ReadFully(m_file, bf, m_blocksize);
	}
};

#ifdef _DEBUG

int ACT::GetFillFactor(int *f_average, int *f_min, int *f_max, int *empty, int branch_type)
{
	TrieBranch b(0, 0);
	int j;
	int fill_nodes, branches_total;
	int num_children;
	OpFileLength i, fsize, bitfield_pos;
	int block_size;
	unsigned char *deleted;

	block_size = m_storage.GetBlockSize();
	fsize = m_storage.GetFileSize();

	*f_average = 0;
	*f_min = TRIE_SIZE;
	*f_max = 0;
	if (empty != NULL)
		*empty = 0;

	if ((deleted = OP_NEWA(unsigned char, m_storage.GetBlockSize())) == NULL)
		return 0;

	branches_total = 0;
	bitfield_pos = block_size;

	for (i = block_size; i < fsize; i += block_size)
	{
		if ((i - block_size) % (8 * block_size * block_size + block_size) == 0)
		{
			bitfield_pos = i;
			if (OpStatus::IsError(((DBlockReader *)&m_storage)->ReadDeletedBitField(deleted, i)))
				break;
			continue;
		}

		if (deleted[((i - bitfield_pos) / block_size - 1) >> 3] & (1 << ((((i - bitfield_pos) / block_size - 1)) & 7)))
		{
			if (empty != NULL)
				++(*empty);
			continue;
		}

		b.disk_id = (BSCache::Item::DiskId)(i/block_size);
		if (OpStatus::IsError(b.Read(&m_storage)))
			break;

		fill_nodes = 0;
		num_children = 0;
		for (j = 1; j < TRIE_SIZE; ++j)
		{
			if (!b.IsFree(j))
				++fill_nodes;

			if (b.HasChild(j))
				++num_children;
		}

		OP_ASSERT(fill_nodes == (int)b.NumFilled());
		OP_ASSERT(fill_nodes > 0 || i == (OpFileLength)(2 * block_size));
		OP_ASSERT(num_children <= fill_nodes);

		if (branch_type == 0 ||  // all
			(branch_type == 1 && num_children != 0) ||  // parents
			(branch_type == 2 && num_children == 0))  // !parents
		{
			*f_average += fill_nodes;
			if (fill_nodes < *f_min)
				*f_min = fill_nodes;
			if (fill_nodes > *f_max)
				*f_max = fill_nodes;
			++branches_total;
		}
	}
	
	OP_DELETEA(deleted);

//	*f_average = *f_average * 100 / (branches_created * (TRIE_SIZE - 1));
	if (branches_total != 0)
		*f_average = *f_average * 100 / (branches_total * (TRIE_SIZE - 1));
	else *f_average = 0;

	if (empty != NULL)
		*empty = *empty * 100 / branches_total;

	*f_min = *f_min * 100 / (TRIE_SIZE - 1);
	*f_max = *f_max * 100 / (TRIE_SIZE - 1);

	return branches_total;
}

int ACT::GetFillDistribution(int *levels, int *counts, int max_level, int *total, OpFileLength disk_id)
{
	TrieBranch b(0, 0), sub(0, 0);
	int i, node_count;
	int own_branches, branches;

	if (disk_id == 2)
	{
		if (total != NULL)
			*total = 1;

		if (max_level <= 1)
			return 0;

		for (i = 0; i < max_level; ++i)
		{
			levels[i] = 0;
			if (counts != NULL)
				counts[i] = 0;
		}

		b.disk_id = (BSCache::Item::DiskId)disk_id;
		RETURN_VALUE_IF_ERROR(b.Read(&m_storage), 0);

		*levels = (int)b.NumFilled() * 1000 / (TRIE_SIZE - 1);
		if (counts != NULL)
			*counts = 1;

		++levels;
		if (counts != NULL)
			++counts;
		--max_level;

		node_count = 0;
		for (i = 1; i < TRIE_SIZE; ++i)
			if (!b.IsFree(i))
				++node_count;
		OP_ASSERT((int)b.NumFilled() == node_count);
	}

	branches = 0;
	own_branches = 0;

	b.disk_id = (BSCache::Item::DiskId)disk_id;
	RETURN_VALUE_IF_ERROR(b.Read(&m_storage), 0);

	node_count = 0;

	for (i = 1; i < TRIE_SIZE; ++i)
	{
		if (!b.IsFree(i))
			++node_count;

		if (b.HasDiskChild(i))
		{
			if (total != NULL)
				++(*total);

			sub.disk_id = b.GetDiskChild(i);
			RETURN_VALUE_IF_ERROR(sub.Read(&m_storage), 0);

			OP_ASSERT(sub.NumFilled() > 0);

			*levels += (int)sub.NumFilled();
			if (counts != NULL)
				++(*counts);
			++own_branches;

			if (max_level > 0)
				branches += GetFillDistribution(levels + 1, counts == NULL ? NULL : counts + 1, max_level - 1, total, (OpFileLength)(b.GetDiskChild(i)));
		}
	}
	OP_ASSERT((int)b.NumFilled() == node_count);

	if (max_level > 0 && branches > 0)
	{
		levels[1] = levels[1] * 1000 / ((TRIE_SIZE - 1) * branches);
//		if (counts != NULL)
//			counts[1] = branches;
	}

	if (disk_id == 2 && own_branches > 0)
	{
		*levels = *levels * 1000 / ((TRIE_SIZE - 1) * own_branches);
//		if (counts != NULL)
//			*counts = own_branches;
	}

	return own_branches;
}

#endif  // DEBUG

OP_BOOLEAN ACT::CheckConsistency(void)
{
	TrieBranch branch(0, 0);
	int fill_nodes, i;
	char *block, *deleted, *refs;
	OpAutoArray<char> block_a, deleted_a, refs_a;
	OpFileLength fsize, pos;
	int block_size, block_len;
	UINT32 ptr, num_blocks, b, b2;

	block_size = m_storage.GetBlockSize();
	fsize = m_storage.GetFileSize();
	num_blocks = (UINT32)(fsize/block_size);

	if (fsize < (OpFileLength)3 * block_size)
		return OpBoolean::IS_TRUE;

	block_a  .reset(block   = OP_NEWA(char, block_size));
	deleted_a.reset(deleted = OP_NEWA(char, (num_blocks + 7) / 8));
	refs_a   .reset(refs    = OP_NEWA(char, (num_blocks + 7) / 8));
	RETURN_OOM_IF_NULL(block);
	RETURN_OOM_IF_NULL(deleted);
	RETURN_OOM_IF_NULL(refs);

	op_memset(deleted, 0, (num_blocks + 7) / 8);
	op_memset(refs,    0, (num_blocks + 7) / 8);

	RETURN_IF_ERROR(((DBlockReader *)&m_storage)->ReadPtr(0, &pos));
	if (pos >= fsize)
		return OpBoolean::IS_FALSE;  // corrupted file

	for (b = 1; b < num_blocks; b += 8 * block_size + 1)
	{
		pos = (OpFileLength)b * block_size;
		RETURN_IF_ERROR(((DBlockReader *)&m_storage)->ReadDeletedBitField(block, pos));
		for (b2 = b+1; b2 < b+1+8*block_size && b2 < num_blocks; ++b2)
			if (block[(b2 - b - 1) / 8] & (1 << ((b2 - b - 1) % 8)))
				deleted[b2 / 8] |= (1 << (b2 % 8));
		deleted[b / 8] |= (1 << (b % 8));
	}
	op_memcpy(refs, deleted, (num_blocks + 7) / 8);

	for (b = 2; b < num_blocks; ++b)
	{
		pos = (OpFileLength)b * block_size;

		if (deleted[b / 8] & (1 << (b % 8)))
		{
			if (m_storage.IsStartBlock(pos))
				return OpBoolean::IS_FALSE;
			continue;
		}

		if (m_storage.IsStartBlocksSupported() && !m_storage.IsStartBlock(pos))
			return OpBoolean::IS_FALSE;
		block_len = m_storage.DataLength(pos);
		//if (block_len == 0 && m_head != NULL)
		//	continue;  // empty branch might be tolerated for reserved pages during a transaction
		if (block_len != branch.GetPackedSize())
			return OpBoolean::IS_FALSE;

		branch.disk_id = b;
		RETURN_IF_ERROR(branch.Read(&m_storage));

		if (branch.IsWord(0) ||
			branch.IsFinal(0) ||
			branch.GetParent(0) != 0 ||
			branch.GetOffset(0) != 0)
			return OpBoolean::IS_FALSE;

		fill_nodes = 0;
		for (i = 1; i < TRIE_SIZE; ++i)
		{
			if (!branch.IsFree(i))
			{
				++fill_nodes;

				if (branch.HasMemoryChild(i))
				{
					return OpBoolean::IS_FALSE;
				}
				else if (branch.HasDiskChild(i))
				{
					if (branch.IsFinal(i))
						return OpBoolean::IS_FALSE;

					ptr = branch.GetDiskChild(i);
					if (   ptr == 0 // ptr_error
						|| (ptr >= num_blocks || (ptr - 1) % (8 * block_size + 1) == 0 || ptr == b) // ptr_bad_error
						|| (deleted[ptr / 8] & (1 << (ptr % 8))) != 0 // ptr_deleted_error
						|| (refs[ptr / 8] & (1 << (ptr % 8))) != 0) // ref_error
						return OpBoolean::IS_FALSE;
					refs[ptr / 8] |= (1 << (ptr % 8));
				}
				else if (!branch.IsFinal(i))
				{
					if (branch.GetOffset(i) + 255 - FIRST_CHAR <= 0 || 
						branch.GetOffset(i) >= TRIE_SIZE)
						return OpBoolean::IS_FALSE;
				}

				if (!branch.IsWord(i) && branch.IsFinal(i) && m_TailCallback == NULL)
					return FALSE;

				if (branch.GetParent(i) == 0)
				{
					if (i > 256 - FIRST_CHAR)
						return OpBoolean::IS_FALSE;
				}
				else
				{
					if (branch.HasChild(branch.GetParent(i)))
						return OpBoolean::IS_FALSE;
					if (branch.IsFinal(branch.GetParent(i)))
						return OpBoolean::IS_FALSE;
				}
			}
		}

		if (fill_nodes != (int)branch.NumFilled())
			return OpBoolean::IS_FALSE;
	}

	// Check that all branches are referenced, deleted or a deleted-bit-field
	for (b = 3; b < num_blocks; ++b)
		if ((refs[b / 8] & (1 << (b % 8))) == 0)
			return OpBoolean::IS_FALSE;

	return OpBoolean::IS_TRUE;
}

#endif  // SEARCH_ENGINE

