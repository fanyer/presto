/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SEARCH_ENGINE  // to remove compilation errors with ADVANCED_OPVECTOR

#include "modules/search_engine/ACTUtil.h"
#include "modules/search_engine/ACT.h"

//#define HTML_EXPORT

#ifdef HTML_EXPORT
#include "modules/search_engine/tests/HTMLExporter.h"
#endif

#ifdef _DEBUG
#define CHECK_INTEGRITY(branch, act) OP_ASSERT(TrieBranch::CheckIntegrity(branch, act))
#else
#define CHECK_INTEGRITY(branch, act)
#endif

OP_STATUS NodePointer::Reset(BSCache::Item::DiskId block_no)
{
	BSCache::Item *bi;
	offset = 0;
	parent = 0;

	if (branch != NULL && branch->disk_id == block_no)
		return OpStatus::OK;

	if (branch != NULL)
		act->Release(branch);

	if (block_no == 0)
	{
		branch = NULL;
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(act->Load(&bi, (OpFileLength)(block_no)));
	branch = (TrieBranch *)bi;
	return OpStatus::OK;
}

void NodePointer::Reset(TrieBranch *b)
{
	BSCache::Item *bi;
	if (branch == b)
		return;

	offset = 0;
	parent = 0;

	if (branch != NULL)
		act->Release(branch);

	if (b == NULL)
	{
		branch = NULL;
		return;
	}

	act->Load(&bi, b);
	branch = (TrieBranch *)bi;
}

BOOL NodePointer::ValidChar(char position)
{
	register int o;

	OP_ASSERT(!IsFinal());
	if (IsFinal())
		return FALSE;

	if (HasChild())
		return TRUE;

	o = GetOffset() + (unsigned char)position - FIRST_CHAR;
	return o > 0 && o < TRIE_SIZE;
}

OP_STATUS NodePointer::Goto(char position)
{
	BSCache::Item::DiskId branch_no;
	BSCache::Item *bi;
	TrieBranch *b;

#ifdef _DEBUG
	//if (HasChild() && branch->modified)
	// This produces false asserts 	//CHECK_INTEGRITY(branch, act);
#endif

	if (HasDiskChild())
	{
		branch_no = GetDiskChild();
		act->Release(branch);

		RETURN_IF_ERROR(act->Load(&bi, (OpFileLength)(branch_no)));
		branch = (TrieBranch *)bi;

		offset = 0;
	}
	else if (HasMemoryChild())
	{
		b = GetMemoryChild();
		act->Release(branch);

		act->Load(&bi, b);
		branch = (TrieBranch *)bi;

		offset = 0;
	}

	if (!ValidChar(position))
		return OpStatus::ERR_OUT_OF_RANGE;

	parent = offset;

	offset = GetOffset() + (unsigned char)position - FIRST_CHAR;

	return OpStatus::OK;
}

OP_STATUS NodePointer::NewNode(int position)
{
	int i, rndpos;
	TrieBranch *b;
	BSCache::Item *tmp_b;

	position -= FIRST_CHAR;

	rndpos = act->Random() % (MAX_OFFSET_VALUE - 1) + 1;//(MAX_OFFSET_VALUE - 2 - FIRST_CHAR) + 1 + FIRST_CHAR;

	for (i = rndpos; i <= MAX_OFFSET_VALUE; ++i)
	{
		if (branch->IsFree(i))
		{
			SetOffset(i - position);
			CHECK_INTEGRITY(branch, act);
			return OpStatus::OK;
		}
	}

	for (i = rndpos; i > 0; --i)
	{
		if (branch->IsFree(i))
		{
			SetOffset(i - position);
			CHECK_INTEGRITY(branch, act);
			return OpStatus::OK;
		}
	}

	// no free node found
	RETURN_IF_ERROR(act->Create(&tmp_b, branch, offset));
	b = (TrieBranch *)tmp_b;

	SetMemoryChild(b);

	act->Release(b);

	CHECK_INTEGRITY(branch, act);
	CHECK_INTEGRITY(b, act);

	return OpStatus::OK;
}

int NodePointer::GetChildrenSize(int node_parent)
{
	int size = 0;
	int i, sp, ep;

	if (node_parent == 0)
		node_parent = parent;

	if (branch->HasChild(node_parent) || branch->IsFinal(node_parent))
		return size;

	sp = branch->GetOffset(node_parent);
	ep = sp + 256;

	if (sp <= 0)
		sp = 1;
	if (ep > TRIE_SIZE)
		ep = TRIE_SIZE;

	for (i = sp; i < ep; ++i)
	{
		if (!branch->IsFree(i) && branch->GetParent(i) == node_parent)
			++size;
	}

	return size * 9 + size / 3;  // a guess based on statistics
}

OP_STATUS NodePointer::Move(int move_parent)
{
	TrieBranch *b;
	BSCache::Item *bi, *flush_b;
	int move_offset, min_offset, max_offset;
	int i;
	unsigned char c;
	NodePointer src(act), dst(act);
	BOOL free_node;

	RETURN_IF_ERROR(act->Create(&bi, branch, move_parent));
	b = (TrieBranch *)bi;

	dst.Reset(b);
	act->Release(b);

	src.Reset(branch);
	src.parent = parent;
	free_node = FALSE;
	c = offset - src.branch->GetOffset(parent) + FIRST_CHAR;

	move_offset = src.branch->GetOffset(move_parent);

	if (move_parent == parent)  // in this case the offset will be 0
	{
		free_node = TRUE;
		dst.branch->SetFinal(c - FIRST_CHAR);  // reserve this position
		dst.branch->SetParent(c - FIRST_CHAR, parent);
	}

	min_offset = src.branch->GetOffset(move_parent);
	if ((max_offset = min_offset + 256 - FIRST_CHAR) > TRIE_SIZE)
		max_offset = TRIE_SIZE;
	if (min_offset <= 0)
		min_offset = 1;

	src.branch->SetMemoryChild(move_parent, dst.branch);

	for (i = min_offset; i < max_offset; ++i)
	{
		if (!src.branch->IsFree(i) && src.branch->GetParent(i) == move_parent)
		{
			if (src.branch->HasMemoryChild(i))
			{
				flush_b = act->m_head;
				while (flush_b != NULL && flush_b != src.branch->GetMemoryChild(i))
				{
					if (flush_b != dst.branch && flush_b->disk_id < 0)
						RETURN_IF_ERROR(Flush((TrieBranch *)flush_b));
					flush_b = flush_b->previous;
				}
				RETURN_IF_ERROR(Flush(src.branch->GetMemoryChild(i)));
			}
			TrieBranch::MoveNode(dst.branch, i - move_offset, src.branch, i, FALSE);
			dst.branch->SetParent(i - move_offset, 0);
		}
	}


	for (i = min_offset; i < max_offset; ++i)
	{
		// use src.branch since this->branch may change in MoveChildren
		if (!src.branch->IsFree(i) && src.branch->GetParent(i) == move_parent)
		{
			if (!src.branch->IsFinal(i) && !src.branch->HasChild(i))
			{
				dst.offset = i - move_offset;
				src.offset = i;
				RETURN_IF_ERROR(MoveChildren(src, dst, c, &free_node));
			}
			src.branch->SetFree(i);
		}
	}

	for (i = 1; i < TRIE_SIZE; ++i)
	{
		if ((dst.branch->HasDiskChild(i) && src.branch->disk_id != dst.branch->GetDiskChild(i)) ||
			(dst.branch->HasMemoryChild(i) && src.branch != dst.branch->GetMemoryChild(i)))
			dst.Merge(i);  // rather don't merge if it was the current branch
	}

	if (free_node)
	{
		offset = parent;
		RETURN_IF_ERROR(Goto(c));
		OP_ASSERT(IsFree() || (IsFinal() && !IsWord() && !HasChild()));
		SetFree();
	}

#ifdef _DEBUG
	++act->collision_count;
#endif
	CHECK_INTEGRITY(src.branch, act);
	CHECK_INTEGRITY(dst.branch, act);

	return OpStatus::OK;
}

// input:
//   src.branch = source branch (branch before Move(...) operation)
//   src.offset = parent, whose children should be moved
//   src.parent = source parent (parent before Move(...) operation)
//   dst.branch = branch to move the children to
//   dst.offset = destination parent (new parent of the children)
//   next_char = next character on input
// output:
//   parent_moved = source parent (parent before Move(...) operation) has been moved and position for next_char is reserved
// changes:
//   this->branch might be changed (marked by *parent_moved)
//   this->parent might be changed (marked by *parent_moved)
//   this->offset should be rather considered as undefined if parent was moved
OP_STATUS NodePointer::MoveChildren(NodePointer src, NodePointer dst, unsigned char next_char, BOOL *parent_moved)
{
	Fitter f;
	int i, move_offset, reserved_node;
	TrieBranch *b;
	OP_STATUS err;
	BSCache::Item *bi, *flush_b;

	reserved_node = 0;
	if (OpStatus::IsError(err = f.Parse(src.branch, src.offset)))
	{
		if (err == OpStatus::ERR_NO_MEMORY && act->m_cache_count > 0)
		{
			RETURN_IF_ERROR(act->Flush(ACT::ReleaseAll));
			RETURN_IF_ERROR(f.Parse(src.branch, src.offset));
		}
		else return err;
	}

	if (!*parent_moved && src.offset == parent)
	{
		reserved_node = next_char;
		f.AddNode(reserved_node);
	}

	if ((move_offset = f.Fit(dst.branch, act->Random() % (MAX_OFFSET_VALUE - 1) + 1)) <= 0)
	{
		// flush everything, since dst.branch might contain references to memory branches at this point
		RETURN_IF_ERROR(act->Flush());
		RETURN_IF_ERROR(act->Create(&bi, dst.branch, dst.offset));
		b = (TrieBranch *)bi;

		dst.SetMemoryChild(b);
		dst.Reset(b);
		act->Release(b);

		move_offset = f.GetOrigin() - src.GetOffset();  // code of the first character
	}
	else {
		if (f.GetSize() > 0)
			dst.SetOffset(move_offset - f.GetOrigin() + src.GetOffset());
		else {
			OP_ASSERT(reserved_node != 0);
			dst.SetOffset(move_offset - reserved_node + FIRST_CHAR);
		}
	}

	for (i = 0; i < f.GetSize(); ++i)
	{
		int from = f.GetOrigin() + f.GetOffset(i);
		int to = move_offset + f.GetOffset(i);
		OP_ASSERT(from < TRIE_SIZE && from > 0);

		if (src.branch->HasMemoryChild(from))
		{
			flush_b = act->m_head;
			while (flush_b != NULL && flush_b != src.branch->GetMemoryChild(from))
			{
				if (flush_b != dst.branch && flush_b->disk_id < 0)
					RETURN_IF_ERROR(Flush((TrieBranch *)flush_b));
				flush_b = flush_b->previous;
			}
			RETURN_IF_ERROR(Flush(src.branch->GetMemoryChild(from)));
		}
		TrieBranch::MoveNode(dst.branch, to, src.branch, from, FALSE);
		dst.branch->SetParent(to, dst.offset);
	}

	if (reserved_node != 0)
	{
		*parent_moved = TRUE;
		Reset(dst.branch);
		parent = dst.offset;
		offset = move_offset + f.GetAOffset();
		dst.branch->SetFinal(offset);
		dst.branch->SetParent(offset, dst.offset);
	}

	for (i = 0; i < f.GetSize(); ++i)
	{
		src.offset = f.GetOrigin() + f.GetOffset(i);
		if (!src.IsFinal() && !src.HasChild())
		{
			dst.offset = move_offset + f.GetOffset(i);
			RETURN_IF_ERROR(MoveChildren(src, dst, next_char, parent_moved));
		}
		src.SetFree();
	}

	return OpStatus::OK;
}

OP_STATUS NodePointer::GetSubTree(char **result, int *count, NodePointer t, int max)
{
	Fitter f;
	int i;
	int prefix_len, prefix_pos;
	int char_offset;
	char *copy_buf;

	if (t.HasDiskChild())
	{
		RETURN_IF_ERROR(t.Reset(t.GetDiskChild()));
		char_offset = 0;
	}
	else if (t.HasMemoryChild())
	{
		t.Reset(t.GetMemoryChild());
		char_offset = 0;
	}
	else char_offset = t.GetOffset();

	RETURN_IF_ERROR(f.Parse(t.branch, t.offset));

	prefix_pos = *count - 1;
	prefix_len = (int)op_strlen(result[prefix_pos]);

	if (f.GetSize() > 0 && prefix_len > 0 && prefix_len % 32 == 0)
	{
		RETURN_OOM_IF_NULL(copy_buf = OP_NEWA(char, 32 * (prefix_len / 32 + 1) + 1));
		op_strcpy(copy_buf, result[prefix_pos]);
		OP_DELETEA(result[prefix_pos]);
		result[prefix_pos] = copy_buf;
	}

	for (i = 0; i < f.GetSize() && *count <= max; ++i)
	{
		if (i > 0)
		{
			if (*count >= max)
				return OpStatus::OK;
		
			RETURN_OOM_IF_NULL(copy_buf = OP_NEWA(char, 32 * (prefix_len / 32 + 1) + 1));

			op_strncpy(copy_buf, result[prefix_pos], prefix_len);

			result[(*count)++] = copy_buf;
		}

		t.offset = f.GetOrigin() + f.GetOffset(i);

		result[*count - 1][prefix_len] = t.offset - char_offset + FIRST_CHAR;
		result[*count - 1][prefix_len + 1] = 0;

		if (t.IsWord() && !t.IsFinal())
		{
			if (*count + 1 >= max)
				return OpStatus::OK;

			if (t.act->m_TailCallback == NULL)
			{
				size_t len = op_strlen(result[*count - 1]) + 1;
				RETURN_OOM_IF_NULL(copy_buf = OP_NEWA(char, len));
				op_strcpy(copy_buf, result[*count - 1]);

				result[*count] = result[*count - 1];
				result[*count - 1] = copy_buf;
			}
			else {
				RETURN_IF_ERROR(t.act->m_TailCallback(&copy_buf, t.GetId(), t.act->m_callback_val));
				result[*count] = result[*count - 1];
				result[*count - 1] = copy_buf;
			}

			++*count;
		}

		if (t.act->m_TailCallback != NULL && !t.IsWord() && t.IsFinal())
		{
			RETURN_IF_ERROR(t.act->m_TailCallback(&copy_buf, t.GetId(), t.act->m_callback_val));
			OP_DELETEA(result[*count - 1]);
			result[*count - 1] = copy_buf;
		}

		if (!t.IsFinal())
		{
			RETURN_IF_ERROR(GetSubTree(result, count, t, max));
		}
	}

	return OpStatus::OK;
}

OP_STATUS NodePointer::GetSubTree(ACT::WordID *result, int *count, NodePointer t, int max)
{
	Fitter f;
	int i;

	if (*count >= max)
		return OpStatus::OK;

	if (t.HasDiskChild())
	{
		RETURN_IF_ERROR(t.Reset(t.GetDiskChild()));
	}
	else if (t.HasMemoryChild())
	{
		t.Reset(t.GetMemoryChild());
	}

	RETURN_IF_ERROR(f.Parse(t.branch, t.offset));

	for (i = 0; i < f.GetSize(); ++i)
	{
		t.offset = f.GetOrigin() + f.GetOffset(i);

		if (t.IsWord() || (t.act->m_TailCallback != NULL && t.IsFinal()))
		{
			result[*count] = t.GetId();
			if (++*count >= max)
				return OpStatus::OK;
		}

		if (!t.IsFinal())
		{
			RETURN_IF_ERROR(GetSubTree(result, count, t, max));
			if (*count >= max)
				return OpStatus::OK;
		}
	}

	return OpStatus::OK;
}

OP_STATUS NodePointer::GetFirstEntry(ACT::PrefixResult &result, NodePointer t)
{
	do
	{
		Fitter f;
		int prefix_len;
		int char_offset;
		char *copy_buf;

		if (t.HasDiskChild())
		{
			RETURN_IF_ERROR(t.Reset(t.GetDiskChild()));
			char_offset = 0;
		}
		else if (t.HasMemoryChild())
		{
			t.Reset(t.GetMemoryChild());
			char_offset = 0;
		}
		else char_offset = t.GetOffset();

		RETURN_IF_ERROR(f.Parse(t.branch, t.offset));

		if (f.GetSize() <= 0)
		{
			OP_ASSERT(0);  // this branch doesn't contain any word
			result.id = 0;
			return OpStatus::OK;
		}

		prefix_len = (int)op_strlen(result.utf8_word);

		if (prefix_len > 0 && prefix_len % 32 == 0)
		{
			RETURN_OOM_IF_NULL(copy_buf = OP_NEWA(char, 32 * (prefix_len / 32 + 1) + 1));
			op_strcpy(copy_buf, result.utf8_word);
			OP_DELETEA(result.utf8_word);
			result.utf8_word = copy_buf;
		}

		t.offset = f.GetOrigin() + f.GetOffset(0);

		result.utf8_word[prefix_len] = t.offset - char_offset + FIRST_CHAR;
		result.utf8_word[prefix_len + 1] = 0;

		if (t.IsWord() || (t.act->m_TailCallback != NULL && t.IsFinal()))
		{
			if (t.act->m_TailCallback != NULL)
			{
				RETURN_IF_ERROR(t.act->m_TailCallback(&copy_buf, t.GetId(), t.act->m_callback_val));
				OP_DELETEA(result.utf8_word);
				result.utf8_word = copy_buf;
			}

			result.id = t.GetId();

			return OpStatus::OK;
		}

		OP_ASSERT(!t.IsFinal());
	}
	while (1);
}

OP_BOOLEAN NodePointer::GetNextEntry(ACT::PrefixResult &result, NodePointer t, const char *prev_str)
{
	Fitter f;
	int i;
	int prefix_len;
	int char_offset;
	char *copy_buf;
	OP_BOOLEAN rv;

	ACT::SkipNonPrintableChars(prev_str);

	if (*prev_str == 0)  // shouldn't happen
	{
		if (t.IsFinal())
			return OpBoolean::IS_FALSE;

		RETURN_IF_ERROR(GetFirstEntry(result, t));
		return result.id == 0 ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
	}

	if (t.HasDiskChild())
	{
		RETURN_IF_ERROR(t.Reset(t.GetDiskChild()));
		char_offset = 0;
	}
	else if (t.HasMemoryChild())
	{
		t.Reset(t.GetMemoryChild());
		char_offset = 0;
	}
	else char_offset = t.GetOffset();

	RETURN_IF_ERROR(f.Parse(t.branch, t.offset));

	prefix_len = (int)op_strlen(result.utf8_word);

	// avoid stack overflow
	if (prefix_len > 100)
		return OpBoolean::IS_FALSE;

	if (f.GetSize() > 0 && prefix_len > 0 && prefix_len % 32 == 0)
	{
		RETURN_OOM_IF_NULL(copy_buf = OP_NEWA(char, 32 * (prefix_len / 32 + 1) + 1));
		op_strcpy(copy_buf, result.utf8_word);
		OP_DELETEA(result.utf8_word);
		result.utf8_word = copy_buf;
	}

	for (i = 0; i < f.GetSize(); ++i)
	{
		t.offset = f.GetOrigin() + f.GetOffset(i);

		if (*prev_str == t.offset - char_offset + FIRST_CHAR)
			break;
	}

	if (i >= f.GetSize())
		return OpBoolean::IS_FALSE;

	if (!t.IsFinal())
	{
		result.utf8_word[prefix_len] = t.offset - char_offset + FIRST_CHAR;
		result.utf8_word[prefix_len + 1] = 0;

		if (prev_str[1] != 0)
		{
			if ((rv = GetNextEntry(result, t, prev_str + 1)) != OpBoolean::IS_FALSE)
				return rv;
		}
		else {
			RETURN_IF_ERROR(GetFirstEntry(result, t));  // can fail when the string is too long
			return result.id == 0 ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
		}
	}

	++i;

	while (i < f.GetSize())
	{
		t.offset = f.GetOrigin() + f.GetOffset(i);

		result.utf8_word[prefix_len] = t.offset - char_offset + FIRST_CHAR;
		result.utf8_word[prefix_len + 1] = 0;

		if (t.IsWord())
		{
			result.id = t.GetId();
			return OpBoolean::IS_TRUE;
		}

		if (t.act->m_TailCallback != NULL && !t.IsWord() && t.IsFinal())
		{
			RETURN_IF_ERROR(t.act->m_TailCallback(&copy_buf, t.GetId(), t.act->m_callback_val));
			OP_DELETEA(result.utf8_word);
			result.utf8_word = copy_buf;
			result.id = t.GetId();
			return OpBoolean::IS_TRUE;
		}

		if (!t.IsFinal())
		{
			RETURN_IF_ERROR(GetFirstEntry(result, t));
			return result.id == 0 ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
		}
	}

	return OpBoolean::IS_FALSE;
}

OP_STATUS NodePointer::Flush(TrieBranch *branch)
{
	BSCache::Item::DiskId new_id;
	
	if (act->m_flush_mode != BSCache::JournalOnly && act->m_flush_mode != BSCache::JournalAll)
		return branch->Flush(&(act->m_storage));

	if (branch->disk_id > 0)
		return OpStatus::OK;

	new_id = (BSCache::Item::DiskId)(act->m_storage.Reserve() / act->m_storage.GetBlockSize());

	if (new_id == 0)
		return OpStatus::ERR_NO_DISK;

	branch->id_reserved = TRUE;
	branch->OnIdChange(new_id, branch->disk_id);
	branch->disk_id = new_id;
	branch->journalled = TRUE;
	
	return OpStatus::OK;
}

BOOL NodePointer::Reposition(int node_parent, char next_char)
{
	Fitter f;
	OP_STATUS err;
	int i, j, move_offset, max_offset;
#ifdef HTML_EXPORT
	HTMLExporter exp;
#endif


	if (node_parent == 0)
		node_parent = parent;

	OP_ASSERT(node_parent > 0 && node_parent < TRIE_SIZE);

	if (OpStatus::IsError(err = f.Parse(branch, node_parent)))
	{
		if (err == OpStatus::ERR_NO_MEMORY && act->m_cache_count > 0)
		{
			if (OpStatus::IsError(act->Flush(ACT::ReleaseAll)) ||
				OpStatus::IsError(f.Parse(branch, node_parent)))
				return FALSE;
		}
		else return FALSE;
	}

	OP_ASSERT(f.GetSize() > 0);

	if (next_char != 0)
		f.AddNode(next_char);
	else if (node_parent == parent)
		f.AddNode(offset - branch->GetOffset(parent) + FIRST_CHAR);

	if ((move_offset = f.Fit(branch, act->Random() % (MAX_OFFSET_VALUE - 1) + 1)) <= 0)
	{
#ifdef HTML_EXPORT
		exp.SetClass("free", "white");
		exp.SetClass("occupied", "aqua");
		exp.SetClass("active", "maroon");
		exp.SetClass("conflict", "red");
		exp.SetClass("act_w_child", "fuchsia");
		exp.Open(UNI_L("modules/search_engine/tests/data/failed.html"), 1, 8);
		for (i = 1; i < TRIE_SIZE; ++i)
		{
			if (branch->IsFree(i))
			{
				exp.Add("free");
				continue;
			}

			if (branch->GetParent(i) == node_parent &&
				branch->HasChild(i))
			{
				exp.Add("act_w_child");
				continue;
			}

			if (branch->GetParent(i) == node_parent)
			{
				exp.Add("active");
				continue;
			}

			if (i == branch->GetOffset(node_parent) + (unsigned char)next_char - FIRST_CHAR)
			{
				exp.Add("conflict");
				continue;
			}

			exp.Add("occupied");
		}
		exp.Close();

		if (TRIE_SIZE - (int)branch->NumFilled() + f.GetSize() > TRIE_SIZE / 2)  // fill factor less than x%
			++i;  // just for a breakpoint
#endif
		return FALSE;
	}

	if (node_parent == parent)
		offset = move_offset + f.GetAOffset();

	branch->SetOffset(node_parent, move_offset - f.GetOrigin() + branch->GetOffset(node_parent));

	// set the children's parent of the nodes being moved
	for (i = 0; i < f.GetSize(); ++i)
	{
		if (branch->HasChild(f.GetOrigin() + f.GetOffset(i)) || branch->IsFinal(f.GetOrigin() + f.GetOffset(i)))
			continue;

		if (f.GetOrigin() + f.GetOffset(i) == parent)
			parent = move_offset + f.GetOffset(i);

		j = branch->GetOffset(f.GetOrigin() + f.GetOffset(i));

		if ((max_offset = j + 256 - FIRST_CHAR) > TRIE_SIZE)
			max_offset = TRIE_SIZE;
		if (j <= 0)
			j = 1;

		while (j < max_offset)
		{
			if (branch->GetParent(j) == f.GetOrigin() + f.GetOffset(i))
				branch->SetParent(j, move_offset + f.GetOffset(i));

			++j;
		}
	}

	if (move_offset > f.GetOrigin())
	{
		for (i = f.GetSize() - 1; i >= 0; --i)
		{
			int from = f.GetOrigin() + f.GetOffset(i);
			int to = move_offset + f.GetOffset(i);
			TrieBranch::MoveNode(branch, to, branch, from, TRUE);
		}
	}
	else {
		for (i = 0; i < f.GetSize(); ++i)
		{
			int from = f.GetOrigin() + f.GetOffset(i);
			int to = move_offset + f.GetOffset(i);
			TrieBranch::MoveNode(branch, to, branch, from, TRUE);
		}
	}

#ifdef _DEBUG
	++act->collision_count;
#endif
	CHECK_INTEGRITY(branch, act);

	return TRUE;
}

BOOL NodePointer::Merge(int branch_parent)
{
	Fitter f;
	OP_STATUS err;
	int i, move_offset, diff_offset;
	BSCache::Item *bi;
	TrieBranch *b;
#ifdef HTML_EXPORT
	HTMLExporter exp;
#endif

	if (branch->HasMemoryChild(branch_parent))
	{
		act->Load(&bi, branch->GetMemoryChild(branch_parent));
	}
	else
	{
		RETURN_VALUE_IF_ERROR(act->Load(&bi, (OpFileLength)(branch->GetDiskChild(branch_parent))), FALSE);
	}
	b = (TrieBranch *)bi;

#ifdef HTML_EXPORT
	exp.SetClass("free", "white");
	exp.SetClass("occupied", "aqua");
//	exp.SetClass("moved", "red");
	exp.Open(UNI_L("modules/search_engine/tests/data/merge.html"), 1, 8);
	for (i = 1; i < TRIE_SIZE; ++i)
	{
		if (branch->IsFree(i))
		{
			exp.Add("free");
			continue;
		}

		exp.Add("occupied");
	}

	exp.NewLine();

	for (i = 1; i < TRIE_SIZE; ++i)
	{
		if (b->IsFree(i))
		{
			exp.Add("free");
			continue;
		}

		exp.Add("occupied");
	}

	exp.Close();
#endif

	if (OpStatus::IsError(err = f.ParseAll(b)))
	{
		if (err == OpStatus::ERR_NO_MEMORY && act->m_cache_count > 0)
		{
			if (OpStatus::IsError(act->Flush(ACT::ReleaseAll)) ||
				OpStatus::IsError(f.ParseAll(b)))
			{
				act->Release(b);
				return FALSE;
			}
		}
		else {
			act->Release(b);
			return FALSE;
		}
	}

	if ((move_offset = f.Fit(branch, act->Random() % (MAX_OFFSET_VALUE - 1) + 1)) <= 0)
	{
		act->Release(b);
		return FALSE;
	}

	diff_offset = move_offset - f.GetOrigin();

	for (i = f.GetSize() - 1; i >= 0; --i)
	{
		int from = f.GetOrigin() + f.GetOffset(i);
		int to = move_offset + f.GetOffset(i);
		TrieBranch::MoveNode(branch, to, b, from, TRUE);

		if (branch->GetParent(to) == 0)
			branch->SetParent(to, branch_parent);
		else branch->SetParent(to, branch->GetParent(to) + diff_offset);

		if (!branch->HasChild(to) && !branch->IsFinal(to))
			branch->SetOffset(to, branch->GetOffset(to) + diff_offset);
	}

	act->Unlink(b);
	act->Release(b);

	branch->SetOffset(branch_parent, diff_offset);

	return TRUE;
}



OP_STATUS Fitter::Parse(TrieBranch *b, int parent)
{
	int i, j, d, max_offset;

	this->src = b;
	this->parent = parent;
	this->reserved_node = 0;

	size = 0;
	d = 0;

	i = b->GetOffset(parent);

	if ((max_offset = i + 256 - FIRST_CHAR) > TRIE_SIZE)
		max_offset = TRIE_SIZE;
	if (i <= 0)
		i = 1;

	while (i < max_offset)
	{
		if (!b->IsFree(i) && b->GetParent(i) == parent)
		{
			d = i++;
			++size;
			break;
		}
		++i;
	}

	while (i < max_offset)
	{
		if (!b->IsFree(i) && b->GetParent(i) == parent)
			++size;
		++i;
	}

	if (size > 0)
	{
		RETURN_OOM_IF_NULL(distances = OP_NEWA(int, size));
		distances[0] = d;

		for (i = d + 1, j = 1; j < size; ++i)
		{
			if (!b->IsFree(i) && b->GetParent(i) == parent)
			{
				distances[j] = i - d;
				++j;
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS Fitter::ParseAll(TrieBranch *b)
{
	int i, j, d;

	this->src = b;
	this->parent = -1;
	this->reserved_node = 0;

	if ((size = b->NumFilled()) == 0)
		return OpStatus::OK;

	d = 0;
	i = 1;

	RETURN_OOM_IF_NULL(distances = OP_NEWA(int, size));

	while (i < TRIE_SIZE)
	{
		if (!b->IsFree(i))
		{
			  // merging branches with mem. children may lead to crash, abort
			if (b->HasChild(i))
			{
				OP_DELETEA(distances);
				distances = NULL;
				size = 0;
				return OpStatus::ERR;
			}
			d = i++;
			break;
		}
		++i;
	}

	if (size > 0)
	{
		distances[0] = d;

		for (i = d + 1, j = 1; j < size; ++i)
		{
			if (!b->IsFree(i))
			{
				  // merging branches with mem. children may lead to crash, abort
				if (b->HasChild(i))
				{
					OP_DELETEA(distances);
					distances = NULL;
					size = 0;
					return OpStatus::ERR;
				}

				distances[j] = i - d;
				++j;
			}
		}

		OP_ASSERT(j == size);
	}

	return OpStatus::OK;
}

void Fitter::AddNode(char node)
{
	if (size <= 0)
		reserved_node = 0;
	else
	{
		OP_ASSERT(parent >= 0);
		if (parent >= 0)
			reserved_node = src->GetOffset(parent) + (unsigned char)node - FIRST_CHAR - distances[0];
	}
}

int Fitter::Fit(TrieBranch *b, int start)
{
	int offset, i, last;

	if (size > 1)
		last = distances[size - 1];
	else last = 0;

	if (start + last >= TRIE_SIZE)
		start = TRIE_SIZE - 1 - last;

	offset = start;

	if (offset + reserved_node <= 0)  // reserved_node may be negative
		offset = -reserved_node + 1;

	i = -1;

	do {
		while (offset + last < TRIE_SIZE && !b->IsFree(offset))
			++offset;

		i = -1;

		if (offset + last >= TRIE_SIZE || offset + reserved_node >= TRIE_SIZE)
			break;

		for (i = 1; i < size; ++i)
		{
			if (!b->IsFree(offset + distances[i]) /*&&
				(src != b || b->GetParent(offset + distances[i]) != parent)*/)
			{
				++offset;
				break;
			}
		}

		if (i >= size && !b->IsFree(offset + reserved_node) /*&&
			(src != b || b->GetParent(offset + reserved_node) != parent)*/)
		{
			++offset;
			i = -1;
		}

	} while (i < size);

	if (offset + last < TRIE_SIZE && i >= size)
		return offset;

	offset = start;

	if (offset + last >= TRIE_SIZE)
		offset = TRIE_SIZE - 1 - last;

	if (offset + reserved_node >= TRIE_SIZE)
		offset = TRIE_SIZE - 1 - reserved_node;

	do {
		while (offset > 0 && !b->IsFree(offset))
			--offset;

		i = -1;

		if (offset <= 0  || offset + reserved_node <= 0)
			return 0;

		for (i = 1; i < size; ++i)
		{
			if (!b->IsFree(offset + distances[i]) /*&&
				(src != b || b->data[offset + distances[i]].GetParent() != parent)*/)
			{
				--offset;
				break;
			}
		}

		if (i >= size && !b->IsFree(offset + reserved_node) /*&&
			(src != b || b->GetParent(offset + reserved_node) != parent)*/)
		{
			--offset;
			i = -1;
		}

	} while (i < size);

	return offset;  // not-found handled in the loop
}

TrieBranch::TrieBranch(int id, TrieBranch *rbranch, int rnode, unsigned short nur)
{
	disk_id = -id;
	parent_branch = rbranch;
	parent_pos = rnode;

	data[0].SetFlags(0);
	data[0].SetOffset(0);
	data[0].SetId(0);
	modified = TRUE;
	NUR_mark = nur;
}

TrieBranch::TrieBranch(OpFileLength id, unsigned short nur)
{
	disk_id = (DiskId)id;
	parent_branch = NULL;
	parent_pos = 0;
	NUR_mark = nur;
}

OP_STATUS TrieBranch::Read(BlockStorage *storage)
{
	if (!storage->Read(data, GetPackedSize(), (OpFileLength)(((OpFileLength)disk_id) * storage->GetBlockSize())))
		return OpStatus::ERR_NO_ACCESS;

	Unpack();

	return OpStatus::OK;
}

OP_STATUS TrieBranch::Write(BlockStorage *storage)
{
	Pack();

	if (storage->Write(data, GetPackedSize(), (OpFileLength)(((OpFileLength)disk_id) * storage->GetBlockSize())) == 0)
		return OpStatus::ERR_NO_ACCESS;

	Unpack();

	return OpStatus::OK;
}

OP_STATUS TrieBranch::Flush(BlockStorage *storage)
{
	OpFileLength did;

#ifdef _DEBUG
	OP_ASSERT(modified);
	for (int i = 1; i < TRIE_SIZE; ++i)
		OP_ASSERT(!HasMemoryChild(i));
	// This produces false asserts //OP_ASSERT(CheckIntegrity(this, storage));
#endif

	if (disk_id > 0 && !id_reserved)
	{
		Pack();

		if (!storage->Update(data, GetPackedSize(), (OpFileLength)(((OpFileLength)disk_id) * storage->GetBlockSize())))
			return OpStatus::ERR_NO_DISK;

		Unpack();
	}
	else {
		Pack();

		if (id_reserved)
		{
			if ((did = storage->Write(data, GetPackedSize(), (OpFileLength)(((OpFileLength)disk_id) * storage->GetBlockSize())) / storage->GetBlockSize()) == 0)
			{
				Unpack();

				return OpStatus::ERR_NO_DISK;
			}
		}
		else {
			if ((did = storage->Write(data, GetPackedSize()) / storage->GetBlockSize()) == 0)
			{
				Unpack();

				return OpStatus::ERR_NO_DISK;
			}
		}

		Unpack();

		OnIdChange((DiskId)did, disk_id);

		disk_id = (DiskId)did;

	}

	modified = FALSE;

	return OpStatus::OK;
}

void TrieBranch::OnIdChange(DiskId new_id, DiskId old_id)
{
	if (new_id == old_id)
		return;

	OP_ASSERT(parent_branch->GetMemoryChild(parent_pos) == this);

	parent_branch->SetDiskChild(parent_pos, new_id);
}

void TrieNode::Pack(char *to)
{
	OP_ASSERT(!HasMemoryChild());
	op_memmove(to,     &offset,       4);
	op_memmove(to + 4, &flags_parent, 2);
	op_memmove(to + 6, &id,           4);
}

void TrieNode::Unpack(char *from)
{
	op_memmove(&id,           from + 6, 4);
	op_memmove(&flags_parent, from + 4, 2);
	op_memmove(&offset,       from,     4);
	OP_ASSERT(!HasMemoryChild());
}

void TrieBranch::Pack()
{
	for (int i = 0; i < TRIE_SIZE; ++i)
		data[i].Pack((char*)data + i * TrieNode::GetPackedSize());
}

void TrieBranch::Unpack()
{
	for (int i = TRIE_SIZE - 1; i >= 0; --i)
		data[i].Unpack((char*)data + i * TrieNode::GetPackedSize());
}

void TrieBranch::MoveNode(TrieBranch* dst, int to, TrieBranch* src, int from, BOOL freeSrc)
{
	OP_ASSERT(!src->IsFree(from));
	OP_ASSERT(dst->IsFree(to));
	OP_ASSERT(from > 0 && from < TRIE_SIZE);
	OP_ASSERT(to > 0 && to < TRIE_SIZE);

	if (src->HasMemoryChild(from))
	{
		src->GetMemoryChild(from)->parent_branch = dst;
		src->GetMemoryChild(from)->parent_pos = to;
	}

	dst->data[to] = src->data[from];
	dst->IncNumFilled();
	dst->modified = TRUE;
	if (freeSrc)
		src->SetFree(from);
}

void TrieBranch::SetFlags(int i, int flags)
{
	OP_ASSERT(i > 0 && i < TRIE_SIZE);
	if (GetFlags(i) != flags)
	{
		BOOL wasFree = IsFree(i);
		modified = TRUE;
		data[i].SetFlags(flags);

		if (!wasFree && IsFree(i))
			DecNumFilled();
		else if (wasFree && !IsFree(i))
			IncNumFilled();
	}
}

void TrieBranch::SetOffset(int i, int offset)
{
	OP_ASSERT(i > 0 && i < TRIE_SIZE);
	modified = TRUE;
	SetFlags(i, GetFlags(i) & ~(TrieNode::Free | TrieNode::Child | TrieNode::Final));
	data[i].SetOffset(offset);
}

void TrieBranch::SetDiskChild(int i, BSCache::Item::DiskId disk_child)
{
	OP_ASSERT(i > 0 && i < TRIE_SIZE);
	modified = TRUE;
	SetFlags(i, (GetFlags(i) & ~(TrieNode::Free | TrieNode::Child | TrieNode::Final)) | TrieNode::DiskChild);
	data[i].SetDiskChild(disk_child);
}

void TrieBranch::SetMemoryChild(int i, TrieBranch *mem_child)
{
	OP_ASSERT(i > 0 && i < TRIE_SIZE);
	modified = TRUE;
	SetFlags(i, (GetFlags(i) & ~(TrieNode::Free | TrieNode::Child | TrieNode::Final)) | TrieNode::MemoryChild);
	data[i].SetMemoryChild(mem_child);
}

void TrieBranch::SetParent(int i, int p)
{
	OP_ASSERT(i > 0 && i < TRIE_SIZE);
	modified = TRUE;
	data[i].SetParent(p);
}

void TrieBranch::SetId(int i, ACT::WordID id)
{
	OP_ASSERT(i > 0 && i < TRIE_SIZE);
	modified = TRUE;
	data[i].SetId(id);
}


int TrieNode::SwitchEndian(void *data, int size, void *user_arg)
{
	BlockStorage::SwitchEndian((unsigned char *)data, 4);
	BlockStorage::SwitchEndian((unsigned char *)data + 4, 2);
	BlockStorage::SwitchEndian((unsigned char *)data + 6, 4);

	return 10;
}

#ifdef _DEBUG
BOOL TrieBranch::CheckIntegrity(const TrieBranch *branch, ACT *act)
{
	int fill_nodes, i;

	if (!branch->modified) // This does not happen often. If it does, the same contents should be on disk
	{
		TrieBranch b(branch->disk_id, 0);
		if (b.disk_id < 0)
			return FALSE;
		RETURN_VALUE_IF_ERROR(b.Read(&act->m_storage), FALSE);
		for (i = 0; i < TRIE_SIZE; ++i)
			if (!b.data[i].Equals(branch->data[i]))
				return FALSE;
	}

	if (branch->GetFlags(0) != 0 ||
		branch->GetParent(0) != 0 ||
		branch->GetOffset(0) != 0)
		return FALSE;

	fill_nodes = 0;
	for (i = 1; i < TRIE_SIZE; ++i)
	{
		if (!branch->IsFree(i))
		{
			++fill_nodes;

			if (branch->HasMemoryChild(i))
			{
				if (!branch->modified || branch->GetMemoryChild(i) == branch)
					return FALSE;
			}
			else if (branch->HasDiskChild(i))
			{
				if (branch->IsFinal(i))
					return FALSE;

				if (branch->GetDiskChild(i) == 0 || branch->GetDiskChild(i) == branch->disk_id)
					return FALSE;
			}
			else if (!branch->IsFinal(i))
			{
				if (branch->GetOffset(i) + 255 - FIRST_CHAR <= 0 || 
					branch->GetOffset(i) >= TRIE_SIZE)
					return FALSE;
			}

			if (!branch->IsWord(i) && branch->IsFinal(i) && act->m_TailCallback == NULL)
				return FALSE;

			if (branch->GetParent(i) == 0)
			{
				if (i > 256 - FIRST_CHAR)
					return FALSE;
			}
			else
			{
				if (branch->HasChild(branch->GetParent(i)))
					return FALSE;
				if (branch->IsFinal(branch->GetParent(i)))
					return FALSE;
			}
		}
	}

	if (fill_nodes != (int)branch->NumFilled())
		return FALSE;
	
	return TRUE;
}
#endif

#undef CHECK_INTEGRITY

#endif  // SEARCH_ENGINE

