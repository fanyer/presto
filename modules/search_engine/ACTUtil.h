/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef ACTUTIL_H
#define ACTUTIL_H

#include "modules/search_engine/ACT.h"
#include "modules/search_engine/BSCache.h"

#if FIRST_CHAR >= 2
#define TRIE_SIZE 254
#else
#define TRIE_SIZE 306
#endif

//#define MAX_OFFSET_VALUE (TRIE_SIZE - 20)
#define MAX_OFFSET_VALUE TRIE_SIZE - 1

/*
 * record representing one character in ACT structure
 *
 * Invariants:
 *
 * 1) Free nodes
 *    a) cannot be a word
 *    b) cannot have a child
 *    c) cannot be final
 *    d) offset, id and parent are undefined
 *
 * 2) Leaf nodes
 *    a) are final
 *    b) cannot have a child
 *    c) offset is undefined
 *    d) if it is a word, id identifies the word
 *    e) if it is not a word, tail compression must be in use and id identifies the word
 *    f) parent is defined, or 0 for the root node
 *
 * 3) Internal nodes
 *    a) are not final
 *    b) must have a disk child, a memory child or an interleaved child determined by offset
 *    c) if it is a word, id identifies the word
 *    d) if it is not a word, id is undefined
 *    e) parent is defined, or 0 for the root node
 */
class TrieNode
{
private:
	friend class TrieBranch;

	enum Flags
	{
		Free = 1,
		DiskChild = 2,
		MemoryChild = 4,
		Child = DiskChild | MemoryChild,
		Word = 8,
		Final = 16
	};

	TrieNode()
	{
		mem_child = 0;
		flags_parent = Free << 11;
		id = 0;
	}

	int GetFlags() const
	{
		return flags_parent >> 11;
	}
	BOOL IsFree()         const { return (GetFlags() & Free)        != 0; }
	BOOL IsWord()         const { return (GetFlags() & Word)        != 0; }
	BOOL IsFinal()        const { return (GetFlags() & Final)       != 0; }
	BOOL HasChild()       const { return (GetFlags() & Child)       != 0; }
	BOOL HasDiskChild()   const { return (GetFlags() & DiskChild)   != 0; }
	BOOL HasMemoryChild() const { return (GetFlags() & MemoryChild) != 0; }

	void SetFlags(int f)
	{
		flags_parent = (UINT16)((flags_parent & 0x7FF) | (f << 11));
		OP_ASSERT(!(HasDiskChild() && HasMemoryChild()));
		OP_ASSERT(!IsFree() || (!HasChild() && !IsFinal() && !IsWord()));
	}

	int GetOffset() const
	{
		OP_ASSERT(!IsFree() && !IsFinal() && !HasChild());
		OP_ASSERT(offset + 255 - FIRST_CHAR > 0 && offset < TRIE_SIZE);
		return offset;
	}
	void SetOffset(int offset)
	{
		OP_ASSERT(offset + 255 - FIRST_CHAR > 0 && offset < TRIE_SIZE);
		this->offset = offset;
	}

	BSCache::Item::DiskId GetDiskChild() const
	{
		OP_ASSERT(!IsFree() && !IsFinal() && HasDiskChild());
		return disk_child;
	}
	void SetDiskChild(BSCache::Item::DiskId disk_child)
	{
		this->disk_child = disk_child;
	}

	TrieBranch *GetMemoryChild() const
	{
		OP_ASSERT(!IsFree() && !IsFinal() && HasMemoryChild());
		return mem_child;
	}
	void SetMemoryChild(TrieBranch *mem_child)
	{
		this->mem_child = mem_child;
	}

	int GetParent() const
	{
		return flags_parent & 0x7FF;
	}
	void SetParent(int p)
	{
		flags_parent = (UINT16)((flags_parent & 0xF800) | p);
	}

	ACT::WordID GetId() const
	{
		return id;
	}
	void SetId(ACT::WordID id)
	{
		this->id = id;
	}

	BOOL Equals(const TrieNode& n) const
	{
		return (flags_parent == n.flags_parent && id == n.id &&
				IsFree() ||
				((!HasDiskChild() || disk_child == n.disk_child) &&
				 (!HasMemoryChild() || mem_child == n.mem_child) &&
				 (HasChild() || IsFinal() || offset == n.offset)));
	}

	void Pack(char *to);
	void Unpack(char *from);
	static int GetPackedSize() { return 10; }
	
	// 8B union int offset/int disk_child/* mem_child
	// 2B 5b flags, 11b parent
	// 4B word id
	union {
		BSCache::Item::DiskId disk_child;
		INT32 offset;
		TrieBranch *mem_child;
	};

	UINT16 flags_parent;
	ACT::WordID id;

public:
	static int SwitchEndian(void *data, int size, void *user_arg);
};

/*
 * one branch in ACT structure, consists of TrieNode[TRIE_SIZE]
 */
class TrieBranch : public BSCache::Item
{
public:
	TrieBranch(int id, TrieBranch *rbranch, int rnode, unsigned short nur);
	TrieBranch(OpFileLength id, unsigned short nur);

	UINT32 NumFilled() const { return (UINT32)data[0].GetId(); }

	CHECK_RESULT(virtual OP_STATUS Read(BlockStorage *storage));
	CHECK_RESULT(virtual OP_STATUS Write(BlockStorage *storage));
	CHECK_RESULT(virtual OP_STATUS Flush(BlockStorage *storage));
	virtual void OnIdChange(DiskId new_id, DiskId old_id);

	static int GetPackedSize() { return TRIE_SIZE * TrieNode::GetPackedSize(); }

	static void MoveNode(TrieBranch* dst, int to, TrieBranch* src, int from, BOOL freeSrc);

#define AR OP_ASSERT(i >= 0 && i < TRIE_SIZE)
	BOOL        IsFree         (int i) const { AR; return data[i].IsFree(); }
	BOOL        IsWord         (int i) const { AR; return data[i].IsWord(); }
	BOOL        IsFinal        (int i) const { AR; return data[i].IsFinal(); }
	BOOL        HasChild       (int i) const { AR; return data[i].HasChild(); }
	BOOL        HasDiskChild   (int i) const { AR; return data[i].HasDiskChild(); }
	BOOL        HasMemoryChild (int i) const { AR; return data[i].HasMemoryChild(); }
	int         GetOffset      (int i) const { AR; return data[i].GetOffset(); }
	DiskId      GetDiskChild   (int i) const { AR; return data[i].GetDiskChild(); }
	TrieBranch* GetMemoryChild (int i) const { AR; return data[i].GetMemoryChild(); }
	int         GetParent      (int i) const { AR; return data[i].GetParent(); }
	ACT::WordID GetId          (int i) const { AR; return data[i].GetId(); }
	void        SetFree        (int i) { SetFlags(i, TrieNode::Free); }
	void        SetFinal       (int i) { SetFlags(i, TrieNode::Final); }
	void        SetFinalWord   (int i) { SetFlags(i, TrieNode::Final | TrieNode::Word); }
	void        SetIsWord      (int i) { SetFlags(i, (GetFlags(i) & ~TrieNode::Free) | TrieNode::Word); }
	void        SetIsNotWord   (int i) { SetFlags(i, GetFlags(i) & ~TrieNode::Word); }
	void        SetOffset      (int i, int offset);
	void        SetDiskChild   (int i, DiskId disk_child);
	void        SetMemoryChild (int i, TrieBranch *mem_child);
	void        SetParent      (int i, int p);
	void        SetId          (int i, ACT::WordID id);

#ifdef _DEBUG
	static BOOL CheckIntegrity(const TrieBranch *branch, ACT *act);
#endif

private:
	TrieBranch *parent_branch;
	int parent_pos;
	TrieNode data[TRIE_SIZE];

	void Pack();
	void Unpack();

	int GetFlags(int i) const { AR; return data[i].GetFlags(); }
	void SetFlags(int i, int f);

	void IncNumFilled()
	{
		OP_ASSERT(data[0].GetId() < TRIE_SIZE-1);
		data[0].SetId(data[0].GetId() + 1);
	}
	void DecNumFilled()
	{
		OP_ASSERT(data[0].GetId() > 0);
		data[0].SetId(data[0].GetId() - 1);
	}

#undef AR
};

/*
 * keep track of all children of a given parent character
 */
class Fitter : public NonCopyable
{
public:
	Fitter()
	{
		distances = NULL;
		size = 0;
		reserved_node = -1;
		src = NULL;
		parent = -1;
	}
	~Fitter() {Reset();}

	void Reset()
	{
		if (distances != NULL)
		{
			OP_DELETEA(distances);
			distances = NULL;
		}
	}

	CHECK_RESULT(OP_STATUS Parse(TrieBranch *b, int parent));
	CHECK_RESULT(OP_STATUS ParseAll(TrieBranch *b));
	void AddNode(char node);
	int Fit(TrieBranch *b, int start);

	int GetSize()
	{
		if (distances == NULL)
			return 0;
		return size;
	}

	int GetOrigin()
	{
		return size > 0 ? distances[0] : 0;
	}

	int GetOffset(int pos)
	{
		return pos == 0 ? 0 : distances[pos];
	}

	int GetAOffset()
	{
		if (size <= 0)
			return 0;
		return reserved_node;
	}

private:
	int *distances;
	int size;
	int reserved_node;
	TrieBranch *src;
	int parent;
};

/*
 * pointer to the given character in a branch,
 * loads/uloads the branches automatically as needed
 */
class NodePointer
{
public:
	NodePointer(const NodePointer &copy)
	{
		BSCache::Item *cp = NULL;

		act = copy.act;
		if (copy.branch != NULL)
		{
			act->Load(&cp, copy.branch);
			branch = (TrieBranch *)cp;
		}
		else
			branch = NULL;
		offset = copy.offset;
		parent = copy.parent;
	}

	NodePointer(ACT *owner)
	{
		act = owner;
		branch = NULL;
		offset = -1;
		parent = -1;
	}

	~NodePointer()
	{
		Reset();
	}

	NodePointer &operator=(const NodePointer &copy)
	{
		act = copy.act;
		Reset(copy.branch);
		offset = copy.offset;
		parent = copy.parent;
		return *this;
	}

	typedef BSCache::Item::DiskId DiskId;

	CHECK_RESULT(OP_STATUS Reset(DiskId block_no));

	void Reset(TrieBranch *b = NULL);

	BOOL ValidChar(char position);

	CHECK_RESULT(OP_STATUS Goto(char position));

	CHECK_RESULT(OP_STATUS NewNode(int position));

	int GetChildrenSize(int node_parent = 0);

	BOOL Reposition(int node_parent = 0, char next_char = 0);

	BOOL Merge(int branch_parent);

	CHECK_RESULT(OP_STATUS Move(int move_parent));
	CHECK_RESULT(OP_STATUS MoveChildren(NodePointer src, NodePointer dst, unsigned char next_char, BOOL *parent_moved));

	CHECK_RESULT(static OP_STATUS GetSubTree(char **result, int *count, NodePointer t, int max));
	CHECK_RESULT(static OP_STATUS GetSubTree(ACT::WordID *result, int *count, NodePointer t, int max));

	CHECK_RESULT(static OP_STATUS GetFirstEntry(ACT::PrefixResult &result, NodePointer t));
	CHECK_RESULT(static OP_BOOLEAN GetNextEntry(ACT::PrefixResult &result, NodePointer t, const char *prev_str));

	BOOL        IsFree         () const         { return branch->IsFree(offset); }
	BOOL        IsWord         () const         { return branch->IsWord(offset); }
	BOOL        IsFinal        () const         { return branch->IsFinal(offset); }
	BOOL        HasChild       () const         { return branch->HasChild(offset); }
	BOOL        HasDiskChild   () const         { return branch->HasDiskChild(offset); }
	BOOL        HasMemoryChild () const         { return branch->HasMemoryChild(offset); }
	int         GetOffset      () const         { return branch->GetOffset(offset); }
	DiskId      GetDiskChild   () const         { return branch->GetDiskChild(offset); }
	TrieBranch* GetMemoryChild () const         { return branch->GetMemoryChild(offset); }
	int         GetParent      () const         { return branch->GetParent(offset); }
	ACT::WordID GetId          () const         { return branch->GetId(offset); }
	void        SetFree        ()               { branch->SetFree(offset); }
	void        SetFinal       ()               { branch->SetFinal(offset); }
	void        SetFinalWord   ()               { branch->SetFinalWord(offset); }
	void        SetIsWord      ()               { branch->SetIsWord(offset); }
	void        SetIsNotWord   ()               { branch->SetIsNotWord(offset); }
	void        SetOffset      (int o)          { branch->SetOffset(offset, o); }
	void        SetDiskChild   (DiskId dc)      { branch->SetDiskChild(offset, dc); }
	void        SetMemoryChild (TrieBranch *mc) { branch->SetMemoryChild(offset, mc); }
	void        SetParent      (int p)          { branch->SetParent(offset, p); }
	void        SetId          (ACT::WordID id) { branch->SetId(offset, id); }

	CHECK_RESULT(OP_STATUS Flush(TrieBranch *branch));


	int GetSuperParent(int o)
	{
		register int p;
		while ((p = branch->GetParent(o)) != 0)
			o = p;

		return o;
	}

	int GetCurrentParent()
	{
		return parent;
	}

	int GetCurrentOffset()
	{
		return offset;
	}
	void SetCurrentOffset(int o)
	{
		offset = o;
	}

	DiskId GetCurrentBranch()
	{
		return branch->disk_id;
	}

	TrieBranch *GetCurrentPointer()
	{
		return branch;
	}

private:
	ACT *act;
	TrieBranch *branch;
	int offset;
	int parent;
};


#endif  // ACTUTIL_H


