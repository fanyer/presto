/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef LEX_SPLAY_TREE_H
#define LEX_SPLAY_TREE_H

#ifdef HISTORY_SUPPORT

/**
 * @brief A Lexicographic Splay Tree
 * @author Patricia Aas
 *
 * LexSplayTree is a Lexicographic Splay Tree :
 *
 * http://www.item.ntnu.no/fag/tm8105/Litteratur/Sleator%20and%20Tarjan%20(1985).pdf
 *
 * The Nodes of the tree are LexSplayNode these are ternary nodes, having left, right and center
 * subtrees. The left and right subtrees behave as regular binary trees. The center branch represents
 * a match. This means that only nodes left through a center branch are a part of the string.
 *
 * In other words, everything in the left subtree of the LexSplayNode are alphabetically less than the
 * LexSplayNodes key, the right subtree is, by the same token, alphabetically greater than the
 * LexSplayNodes key. The central subtree however contains all the strings that have the
 * LexSplayNodes key as their prefix, but are not equal to the LexSplayNodes key.
 *
 * The LexSplayNodes have a void* payload field where the indexed data can be placed. The strings that
 * the paths in the tree represents are tail compressed, where the remaining 
 *
 * See the OpSplayTree class (history/src/OpSplayTree.h) for an example of a class using the LexSplayTree.
 */

class LexSplayTree
{
public:

	LexSplayTree();

    virtual ~LexSplayTree() { }

    struct LexSplayNode
	{
		uni_char m_char;
		uni_char* m_key;
		void* m_pay_load;
		struct LexSplayNode* m_left;
		struct LexSplayNode* m_center;
		struct LexSplayNode* m_right;
		struct LexSplayNode* m_parent;

		LexSplayNode(uni_char c) : m_char(c),
								   m_key(0),
								   m_pay_load(0),
								   m_left(0),
								   m_center(0),
								   m_right(0),
								   m_parent(0) {}
    };

    enum MatchType
	{
		LONGEST_PREFIX_MATCH = 0,
		PERFECT_MATCH        = 1,
		PREFIX_MATCH         = 2
    };

    struct LexSplayNode* root;
	INT32 num_of_nodes;

	// ---------------------
	// Virtual interface
	// ---------------------
	virtual BOOL HasPayloadContent(LexSplayNode* subtree) const = 0;
	virtual BOOL HasInternalNode(LexSplayNode* node) const = 0;
	virtual void DeletePayload(LexSplayNode* node) = 0;
	virtual void CopyItems(LexSplayNode* from_node, LexSplayNode* to_node) = 0;

	// ---------------------
	// Inline interface
	// ---------------------
	BOOL IsCompressed(LexSplayNode* node) const
		{ return node && node->m_key; }

	BOOL HasKey(LexSplayNode* node) const
		{ return node && node->m_key != 0; }

	const uni_char* GetKey(LexSplayNode* node) const
		{ return node->m_key; }
	
	void SetKey(LexSplayNode* node, uni_char* key)
		{ if(node) node->m_key = key; }

    LexSplayNode* parent(LexSplayNode* node)
		{if(node) return node->m_parent; return 0;}

    LexSplayNode* grandparent(LexSplayNode* node)
		{return parent(parent(node));}

    BOOL isLeft(LexSplayNode* child, LexSplayNode* parent)
		{return (parent) && (child) && parent->m_left == child;}

    BOOL isRight(LexSplayNode* child, LexSplayNode* parent)
		{return (parent) && (child) && parent->m_right == child;}

    BOOL isCenter(LexSplayNode* child, LexSplayNode* parent)
		{return (parent) && (child) && parent->m_center == child;}

    void setLeft(LexSplayNode* child, LexSplayNode* parent)
		{parent->m_left = child; if(child) child->m_parent = parent;}

    void setRight(LexSplayNode* child, LexSplayNode* parent)
		{parent->m_right = child; if(child) child->m_parent = parent;}

    void setCenter(LexSplayNode* child, LexSplayNode* parent)
		{parent->m_center = child; if(child) child->m_parent = parent;}

	inline BOOL PerfectMatch(LexSplayNode* current_node, const uni_char* c) const
	{
		return
			(current_node)               &&
			(c)                          &&
			( ( !IsCompressed(current_node) && (*c == current_node->m_char) && (*(c+1) == '\0') ) ||
			  (  IsCompressed(current_node) &&  IsEqual(current_node, c)) );
	}

	inline BOOL OnPath(LexSplayNode* node, const uni_char* c, MatchType match_type) const
	{
		if(!node || !c)
			return FALSE;
		
		if( (*c == node->m_char) && IsCompressed(node) )
		{
			if( IsEqual(node, c)                                          ||
				(match_type == PREFIX_MATCH       && StartsWith(node, c)) ||
				(match_type == LONGEST_PREFIX_MATCH && Matches(node, c)))
			{
				return TRUE;
			}
			
			return FALSE;
		}
		else if(*c == node->m_char)
		{
			return TRUE;
		}
		
		return FALSE;
	}

	inline BOOL LastNode(LexSplayNode* current_node, const uni_char* c) const
	{
		return
			(current_node)               && // If we have a node
			(c)                          && // and we have a string
			( ( !IsCompressed(current_node) && (*c == current_node->m_char) && (*(c+1) == '\0')          ) ||
			  (  IsCompressed(current_node) && (IsEqual(current_node, c) || StartsWith(current_node, c)) ) ||
			  (  IsCompressed(current_node) && (*c == current_node->m_char) && !IsEqual(current_node, c) ) );
	}

	// ---------------------
	// Normal interface
	// ---------------------

	void replaceSubtree(LexSplayNode* parent,
						LexSplayNode* current,
						LexSplayNode* new_child);

	void rotateLeft(LexSplayNode* node);

	void rotateRight(LexSplayNode* node);

	void access(const uni_char* c, LexSplayNode* node);

	void splay(LexSplayNode* node);

	void PruneBranch(LexSplayNode* subtree);
	
	BOOL EmptyLeaf(LexSplayNode* subtree) const;

	LexSplayNode* FindFirst(LexSplayNode* current_node,
							const uni_char* c,
							BOOL include_failed_node = FALSE) const;
	
	LexSplayNode* NextHelper(LexSplayNode* current_node,
							 const uni_char* c,
							 UINT32* inc,
							 BOOL & done,
							 MatchType match_type,
							 BOOL include_failed_node) const;

	LexSplayNode* Find(LexSplayNode* node,
					   const uni_char* c,
					   MatchType match_type,
					   BOOL keep_last_node = FALSE,
					   BOOL keep_only_internal_nodes = FALSE,
					   uni_char** last_char = 0) const;

	LexSplayNode* makeNode(const uni_char* c);

	LexSplayNode* makeCompressedNode(const uni_char* c);

	BOOL IsEqual(LexSplayNode* node,
				 const uni_char* c) const;
	
	BOOL StartsWith(LexSplayNode* node,
					const uni_char* c) const;

	BOOL Matches(LexSplayNode* node,
				 const uni_char* c) const;

	BOOL InsertChild(LexSplayNode* child, LexSplayNode* parent);

	BOOL InsertCenterChild(LexSplayNode* child, LexSplayNode* parent);

	void DeleteNode(LexSplayNode* node);
	
	void DeleteTree();

	LexSplayNode* insertHelper(const uni_char* c);

	LexSplayNode* BranchOff(LexSplayNode* node, const uni_char* c);
};

#endif // HISTORY_SUPPORT
#endif // LEX_SPLAY_TREE_H

