/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PUBLICDOMAINLIST_H
#define PUBLICDOMAINLIST_H

#ifdef PUBLIC_DOMAIN_LIST

#include "modules/util/opfile/opfile.h"

class PublicDomainTreeNode;
class DomainPartNamesStorage;

/*
 * If you want to use this system, Look for PublicDomainDatabase further down in this file.
 */


/**
 * This is hash table from string to nodes, where the strings are "domain part names"
 * and stored in the
 * domain part name database. It exists in hundreds or thousands of copies and some have hundreds of
 * nodes so it has to be memory efficiant (as well as fast).
 *
 * It uses a hashtable with open addressing and with a growth strategy of doubling (to keep
 * sizes at a power of two). Also has some optimizations for common cases, like only one node.
 */
class DomainPartToNodeMap
{
	/**
	 * Optimized the case where it's only a single child and store a pointer directly to that.
	 * This is fairly common.
	 */
	union
	{
		PublicDomainTreeNode** m_storage;
		PublicDomainTreeNode* m_single_node;
	};
	unsigned short m_size;
	unsigned short m_used;

	OP_STATUS Grow(const DomainPartNamesStorage* domain_part_names);
	static UINT32 Hash(const char* key);
public:
	DomainPartToNodeMap() : m_storage(NULL), m_size(0), m_used(0) {}
	~DomainPartToNodeMap();
	OP_STATUS Add(const DomainPartNamesStorage* domain_part_names, PublicDomainTreeNode* node);
	PublicDomainTreeNode* GetData(const DomainPartNamesStorage* domain_part_names, const char* key) const;

#ifdef _DEBUG
	unsigned EstimateMemoryUsage(int* used, int* sizes) const;
#endif // _DEBUG
};


/**
 * All domain names are stored in a single buffer with NULL between them. This keeps malloc overhead
 * down alot.
 */
class DomainPartNamesStorage
{
	char* m_storage; // UTF-8
	unsigned m_size;
	unsigned m_used;
public:
	DomainPartNamesStorage() : m_storage(NULL), m_size(0), m_used(0) {}
	~DomainPartNamesStorage() { OP_DELETEA(m_storage); }
	OP_STATUS Add(const char* domain_part_name_utf8, unsigned domain_part_name_len, unsigned& added_offset);
	const char* Get(unsigned offset) const { return m_storage+offset; }

	/**
	 * Remove slack memory. Uses time proportional to the size of the storage.
	 */
	void Compact();
#ifdef _DEBUG
	unsigned EstimateMemoryUsage() const;
#endif // _DEBUG
};

class PublicDomainTreeNodeArena;

class PublicDomainTreeNode
{
public:
	PublicDomainTreeNode(int domain_part_offset) :
	  m_packed_init(0)
	{
		m_packed.m_domain_part_offset = domain_part_offset;
	}
	OP_STATUS GetOrCreateSubNode(const char* domain_part, size_t domain_part_len,
		PublicDomainTreeNodeArena* tree_node_arena,
		DomainPartNamesStorage* domain_part_names,
		PublicDomainTreeNode*& sub_node);
	const char* GetDomainPartName(const DomainPartNamesStorage* domain_part_names)
	{
		return domain_part_names->Get(m_packed.m_domain_part_offset);
	}

	void SetHasWildcardChild()
	{
		m_packed.m_has_wildcard_child_rule = TRUE;
	}

	void SetAsRule(BOOL is_exception_rule)
	{
		if (is_exception_rule)
		{
			m_packed.m_is_leaf_in_exception_rule = TRUE;
		}
		else
		{
			m_packed.m_is_leaf_in_rule = TRUE;
		}
	}

	BOOL IsRule() const { return m_packed.m_is_leaf_in_rule; }
	BOOL IsException() const { return m_packed.m_is_leaf_in_exception_rule; }
	BOOL HasWildcardChildRule() const { return m_packed.m_has_wildcard_child_rule; }

	PublicDomainTreeNode* FindSubNode(const DomainPartNamesStorage* domain_part_names,
		const char* domain_part, size_t domain_part_len) const;

#ifdef _DEBUG
	unsigned EstimateMemoryUsage(int* used, int* sizes) const;
#endif // _DEBUG

private:
	union
	{
		struct
		{
			unsigned m_domain_part_offset : 24;
			unsigned m_is_leaf_in_rule : 1;
			unsigned m_is_leaf_in_exception_rule : 1;
			unsigned m_has_wildcard_child_rule : 1;
		} m_packed;

		INT32 m_packed_init;
	};

	DomainPartToNodeMap m_sub_rules; // string -> PublicDomainTreeNode
};

/**
 * This is an arena for allocation of PublicDomainTreeNodes. It keeps a list of blocks and allocates
 * PublicDomainTreeNodes from them. Then when the program shuts down, they are all deleted.
 */
class PublicDomainTreeNodeArena
{
	struct ArenaBlockHeader
	{
		ArenaBlockHeader* next;
	};
	ArenaBlockHeader* m_first_block;
	PublicDomainTreeNode* m_next_free;
	PublicDomainTreeNode* m_end;

	void GetFirstAndEndOnBlock(ArenaBlockHeader* block, PublicDomainTreeNode*& first, PublicDomainTreeNode*&end);

public:
	PublicDomainTreeNodeArena() : m_first_block(NULL), m_next_free(NULL), m_end(NULL) {}
	~PublicDomainTreeNodeArena()
	{
		ArenaBlockHeader* block = m_first_block;
		while (block)
		{
			ArenaBlockHeader* next = block->next;
			// Run the destructor on all the nodes

			PublicDomainTreeNode* first;
			PublicDomainTreeNode* end;
			GetFirstAndEndOnBlock(block, first, end);
			if (block == m_first_block)
			{
				end = m_next_free;
			}

			while (first != end)
			{
				first->~PublicDomainTreeNode();
				first++;
			}

			OP_DELETEA(reinterpret_cast<char*>(block));
			block = next;
		}
	}

	/**
	 * Returns NULL if OOM.
	 */
	PublicDomainTreeNode* Allocate(int domain_part_offset);

#ifdef _DEBUG
	unsigned EstimateMemoryUsage() const;
#endif // _DEBUG
};

/**
 * The main API for this.
 */
class PublicDomainDatabase
{
	PublicDomainTreeNode m_root;
	DomainPartNamesStorage m_domain_part_names;
	PublicDomainTreeNodeArena m_tree_node_arena;

	/**
	 * Parse and insert a single rule in the database.
	 */
	OP_STATUS ParseRule(const char* rule, size_t rule_len);

	/**
	 * Helper method for stepping through domains.
	 */
	static BOOL FindEarlierDomainPart(const char* domain, const char*& domain_part_start, const char*& domain_part_end);

	/**
	 * Helper method for looking up data in the datbase.
	 */
	void IsPublicDomainRecursive(PublicDomainTreeNode* current_node, int current_depth,
		const char* domain, const char* domain_part_start,
		const char* domain_part_end, int& deepest_found_rule,
		BOOL& found_exception, BOOL& top_domain_was_mentioned);

public:
	PublicDomainDatabase() : m_root(0)
	{
	}

	/**
	 * Reads the database file. Must only be called one. If it's called more than once the world will explode, we will leak memory and probably crash Opera.
	 *
	 * Returns an error code for missing files or missing memory.
	 */
	OP_STATUS ReadDatabase(OpFile& file);

	/**
	 * The main API function.
	 *
	 * @param[out] top_domain_was_mentioned If an address to a BOOL is sent in here, that BOOL
	 * will be set to TRUE if the top domain was mentioned in the database.
	 */
	OP_BOOLEAN IsPublicDomain(const uni_char* domain, BOOL* top_domain_was_mentioned = NULL);

#if 0 // Future expected API for quotas in persistant storage. Not implemented yet
	/**
	 * Returns OpStatus::ERR is such a domain can't be extracted.
	 */
	OP_STATUS GetTopMostNonPublicDomain(const uni_char* domain, OpString& top_most_non_public_domain);
#endif // 0

#ifdef _DEBUG
	unsigned EstimateMemoryUsage() const;
#endif // _DEBUG

};
#endif // PUBLIC_DOMAIN_LIST

#endif // !PUBLICDOMAINLIST_H
