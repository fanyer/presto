/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef PUBLIC_DOMAIN_LIST

#include "modules/security_manager/src/publicdomainlist.h"

#include "modules/util/opfile/opfile.h"
#include "modules/util/hash.h"

OP_STATUS PublicDomainTreeNode::GetOrCreateSubNode(const char* domain_part_p, size_t domain_part_len,
												   PublicDomainTreeNodeArena* tree_node_arena,
												   DomainPartNamesStorage* domain_part_names,
												   PublicDomainTreeNode*& sub_node)
{
	// A domain part longer than 256 is stupid and doesn't deserve to be classified as public
	char domain_part[256]; // ARRAY OK 2009-04-23 lasse
	if (domain_part_len >= 256)
	{
		domain_part_len = 255;
	}

	op_strncpy(domain_part, domain_part_p, domain_part_len);
	domain_part[domain_part_len] = '\0';

	const char* key = domain_part;
	PublicDomainTreeNode* data;

	data = m_sub_rules.GetData(domain_part_names, key);
	if (data)
	{
		// We've seen a rule like this before
	}
	else
	{
		// First rule looking like this

		// Input storage for the domain name
		unsigned domain_part_name_offset;
		RETURN_IF_ERROR(domain_part_names->Add(domain_part_p, domain_part_len, domain_part_name_offset));
		data = tree_node_arena->Allocate(domain_part_name_offset);
		if (!data)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		if (OpStatus::IsError(m_sub_rules.Add(domain_part_names, data)))
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	OP_ASSERT(data);

	sub_node = data;
	return OpStatus::OK;
}

PublicDomainTreeNode* PublicDomainTreeNode::FindSubNode(const DomainPartNamesStorage* domain_part_names, const char* domain_part_p, size_t domain_part_len) const
{
	// A domain part longer than 256 is stupid and doesn't deserve to be classified as public
	char domain_part[256];  // ARRAY OK 2009-04-23 lasse
	if (domain_part_len >= 256)
	{
		domain_part_len = 255;
	}

	op_strncpy(domain_part, domain_part_p, domain_part_len);
	domain_part[domain_part_len] = '\0';

	const char* key = domain_part;
	PublicDomainTreeNode* data;

	data = m_sub_rules.GetData(domain_part_names, key);
	return data;
}

OP_STATUS PublicDomainDatabase::ParseRule(const char* rule, size_t rule_len)
{
	OP_ASSERT(rule_len);
#ifdef _DEBUG_RULES
	char buf[512]; /* ARRAY OK 2008-12-10 lasse */
	size_t dbg_len = MIN(rule_len, 511);
	op_strncpy(buf, rule, dbg_len);
	buf[dbg_len] = '\n';
	buf[dbg_len+1] = '\0';
	OutputDebugStringA(buf);
#endif // _DEBUG_RULES
	
	const char* domain_p = rule;
	const char* domain_part_end = domain_p + rule_len+1; // FindEarlierDomainPart will back past a '.' or '\0'
	BOOL is_exception_rule = *domain_p == '!';
	if (is_exception_rule)
	{
		domain_p++;
	}

	const char* domain_part_start = domain_part_end;
	PublicDomainTreeNode* current_node = &m_root;

	OP_STATUS status = OpStatus::OK;

	BOOL simple_wildcard_rule = FALSE;
	while (FindEarlierDomainPart(domain_p, domain_part_start, domain_part_end))
	{
		if (domain_part_start == domain_p && domain_part_end - domain_part_start == 1 &&
			*domain_part_start == '*')
		{
			// Common case, wildcard at the start. Optimize this by not
			// creating a child node unless it's needed.
			simple_wildcard_rule = TRUE;
			break;
		}

		PublicDomainTreeNode* next_node;
		OP_STATUS status = current_node->GetOrCreateSubNode(domain_part_start, domain_part_end - domain_part_start,
			&m_tree_node_arena,
			&m_domain_part_names, next_node);
		if (OpStatus::IsError(status))
		{
			break;
		}

		if (current_node != &m_root)
		{
			// This isn't explicit in the documentation but as
			// discovered in CORE-17263, we have to treat parent
			// domains of rules as public domains too. This became
			// apparent since "*.uk" was in the database but not "uk".
			current_node->SetAsRule(FALSE);
		}

		OP_ASSERT(next_node);
		current_node = next_node;
	}

	if (OpStatus::IsSuccess(status))
	{
		if (simple_wildcard_rule)
		{
			current_node->SetHasWildcardChild();
		}
		else
		{
			OP_ASSERT(current_node != &m_root); // Since the line wasn't empty there must have been a rule.
			current_node->SetAsRule(is_exception_rule);
		}
	}

	return status;
}


OP_STATUS PublicDomainDatabase::ReadDatabase(OpFile& file)
{
	// Format documentation at http://publicsuffix.org/format/

	OP_STATUS status = file.Open(OPFILE_READ);
	OP_ASSERT(OpStatus::IsSuccess(status) || !"The public suffix list is missing. See FEATURE_PUBLIC_DOMAIN_LIST and look for a file in the resource folder of your installation. Security checks will be very restrictive now");
	char local_buf[512]; // ARRAY OK 2009-04-23 lasse
	char* buf = local_buf;
	int buf_size = 512;
	if (OpStatus::IsSuccess(status))
	{
		// Allocate one big buffer to make parsing as fast as possible
		// Add one byte to trigger the eof flag to prevent an extra call to Read
		OpFileLength file_len;
		status = file.GetFileLength(file_len);
		if (OpStatus::IsSuccess(status) && file_len < 1024*1024 && static_cast<int>(file_len) > buf_size)
		{
			char* big_buf = OP_NEWA(char, (int)file_len+1);
			if (big_buf)
			{
				buf = big_buf;
				buf_size = (int)file_len+1;
			}
		}
	}
	int buf_occupied_bytes = 0;
	BOOL in_comment = FALSE;
	while (OpStatus::IsSuccess(status) && !file.Eof())
	{
		OpFileLength read;
		status = file.Read(buf+buf_occupied_bytes, buf_size-buf_occupied_bytes, &read);
		if (OpStatus::IsError(status))
		{
			break;
		}

		OP_ASSERT(static_cast<int>(read) <= buf_size);
		buf_occupied_bytes += (int)read;
		OP_ASSERT(buf_occupied_bytes <= buf_size);

		// Process one "line" at a time
		char* line_start = buf;
		char* p = buf;
		char* buf_end = buf+buf_occupied_bytes;
		const char* rule = NULL;
		size_t rule_len = 0;
		for (p = buf; p != buf_end; p++)
		{
			if (*p == '\n' || *p == '\r')
			{
				if (in_comment)
				{
					in_comment = FALSE;
				}
				else if (p - line_start >= 2 && line_start[0] == '/' && line_start[1] == '/')
				{
					// Comment line, ignore
				}
				else if (p != line_start)
				{
					rule = line_start;
					rule_len = p-line_start;
				}
				line_start = p+1;
			}
			else if (!in_comment && op_isspace(*p))
			{
				if (p - line_start >= 2 && line_start[0] == '/' && line_start[1] == '/')
				{
					in_comment = TRUE;
				}
				else if (p == line_start)
				{
					// Leading whitespace isn't allowed so ignore this line
					in_comment = TRUE;
				}
				else
				{
					// we have a domain rule
					rule = line_start;
					rule_len = p-line_start;

					// treat the rest of the line as a comment
					in_comment = TRUE;
				}
			}
			else
			{
				continue;
			}

			if (rule)
			{
				status = ParseRule(rule, rule_len);
				if (OpStatus::IsError(status))
				{
					break;
				}
				rule = NULL;
			}
		}

		if (OpStatus::IsError(status))
		{
			break;
		}

		if (in_comment)
		{
			// Ignore everything
			line_start = buf_end;
		}

		if (line_start == buf)
		{
			OP_ASSERT(!in_comment);
			// Too long line. This would indicate a broken file since no domain rule should be 512 bytes long
			// Skip the line...
			line_start = buf_end;
		}

		if (line_start == buf_end)
		{
			buf_occupied_bytes = 0;
		}
		else
		{
			// Move the data to the start of the buffer
			buf_occupied_bytes = buf_end - line_start;
			op_memmove(buf, line_start, buf_occupied_bytes);
		}
	}

	if (OpStatus::IsSuccess(status) && buf_occupied_bytes != 0)
	{
		OP_ASSERT(!in_comment);
		// A final rule at the end of the file, with no line break after it
		status = ParseRule(buf, buf_occupied_bytes);
	}

	if (buf != local_buf)
	{
		OP_DELETEA(buf);
	}
	file.Close();

	m_domain_part_names.Compact();
	return status;
}


/* static */
BOOL PublicDomainDatabase::FindEarlierDomainPart(const char* domain, const char*& domain_part_start, const char*& domain_part_end)
{
	// Next domain part
	if (domain_part_start == domain ||
		domain_part_start-1 == domain)
	{
		// No more parts
		return FALSE;
	}
	else
	{
		// Back up to the next (earlier) part
		domain_part_end = domain_part_start-1; // Skip the dot/NULL char
		// OP_ASSERT(*domain_part_end == '.' || *domain_part_end == '\0' || op_isspace(*domain_part_end));
		domain_part_start = domain_part_end-1;

		while (domain_part_start != domain && *(domain_part_start-1) != '.')
		{
			domain_part_start--;
		}
		return TRUE;
	}
}


void PublicDomainDatabase::IsPublicDomainRecursive(
	PublicDomainTreeNode* current_node,
	int current_depth,
	const char* domain, const char* domain_part_start,
	const char* domain_part_end, int& deepest_found_rule,
	BOOL& found_exception, BOOL& top_domain_was_mentioned)
{
	OP_ASSERT(domain_part_start != domain_part_end);

	const char* earlier_domain_part_start = domain_part_start;
	const char* earlier_domain_part_end = domain_part_end;
	BOOL is_last_part = !FindEarlierDomainPart(domain, earlier_domain_part_start, earlier_domain_part_end);

	if (is_last_part && current_node->HasWildcardChildRule())
	{
		deepest_found_rule = current_depth;
	}

	// Check the real domain part and the wildcard, two passes
	const char* rule_part = domain_part_start;
	size_t rule_part_len = domain_part_end - domain_part_start;
	for (int i = 0; i < 2; i++)
	{
		PublicDomainTreeNode* sub_node = current_node->FindSubNode(&m_domain_part_names, rule_part, rule_part_len);
		if (sub_node)
		{
			if (sub_node->IsException())
			{
				found_exception = TRUE;
				return;
			}

			if (i == 0 && current_depth == 1)
			{
				// A match at the top level that wasn't a wildcard match (since i==0)
				top_domain_was_mentioned = TRUE;
			}

			if (is_last_part)
			{
				if (sub_node->IsRule() && current_depth > deepest_found_rule)
				{
					deepest_found_rule = current_depth;
				}
			}
			else
			{
				IsPublicDomainRecursive(sub_node, current_depth+1,
					domain, earlier_domain_part_start, earlier_domain_part_end,
					deepest_found_rule, found_exception, top_domain_was_mentioned);

				if (found_exception)
				{
					return;
				}
			}
		}
		rule_part = "*";
		rule_part_len = 1;
	}
}

OP_BOOLEAN PublicDomainDatabase::IsPublicDomain(const uni_char* domain16, BOOL *top_domain_was_mentioned)
{
	OP_ASSERT(domain16);

	if(top_domain_was_mentioned)
		*top_domain_was_mentioned = FALSE;

	const uni_char* domain_end = domain16+uni_strlen(domain16);
	if (*domain16 && *(domain_end-1) == '.')
	{
		// Ignore trailing dot
		domain_end--;
	}

	OpString16 domain_utf16;
	RETURN_IF_ERROR(domain_utf16.Set(domain16, domain_end-domain16));
	domain_utf16.MakeLower();

	OpString8 domain_utf8;
	RETURN_IF_ERROR(domain_utf8.SetUTF8FromUTF16(domain_utf16.CStr()));

	const char* domain = domain_utf8.CStr();
	const char* domain_part_end = domain + domain_utf8.Length();
	const char* domain_part_start = domain_part_end+1;
	
	int deepest_found_rule = 0;
	BOOL found_exception = FALSE;
	BOOL local_dummy_top_domain_mentioned;
	BOOL& top_domain_metioned_ref = top_domain_was_mentioned ? *top_domain_was_mentioned : local_dummy_top_domain_mentioned;

	if (FindEarlierDomainPart(domain, domain_part_start, domain_part_end))
	{
		PublicDomainTreeNode* current_node = &m_root;
		int current_depth = 1;
		IsPublicDomainRecursive(current_node, current_depth, domain, domain_part_start, domain_part_end, deepest_found_rule, found_exception, top_domain_metioned_ref);
	}

	return !found_exception && deepest_found_rule > 0 ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

#ifdef _DEBUG
unsigned PublicDomainDatabase::EstimateMemoryUsage() const
{
	unsigned size = sizeof(*this);
	int used[4096];
	int sizes[4096];
	int i;
	for (i = 0; i < 4096; i++)
	{
		used[i] = 0;
		sizes[i] = 0;
	}
	size += m_root.EstimateMemoryUsage(used, sizes);
#ifdef WIN32
	for (i = 0; i < 4096; i++)
	{
		if (used[i] || sizes[i])
		{
			char buf[1024]; /* ARRAY OK 2008-12-10 lasse */
			op_snprintf(buf, 1024, "%d: used: %d, size: %d\n", i, used[i], sizes[i]);
			OutputDebugStringA(buf);
		}
	}
#endif // WIN32
	size += m_domain_part_names.EstimateMemoryUsage();
	size -= sizeof(m_domain_part_names); // counted twice
	size += m_tree_node_arena.EstimateMemoryUsage();
	size -= sizeof(m_tree_node_arena);
	return size;
}

unsigned PublicDomainTreeNode::EstimateMemoryUsage(int* used, int* sizes) const
{
	unsigned this_object_size = 0; // sizeof(*this);
	this_object_size += m_sub_rules.EstimateMemoryUsage(used, sizes);
	this_object_size -= sizeof(m_sub_rules); // Counted twice, so remove one
	return this_object_size;
}

unsigned DomainPartToNodeMap::EstimateMemoryUsage(int* used, int* sizes) const
{
	unsigned this_size = sizeof(*this);
	unsigned sub_object_sizes = 0;
	if (m_size > 0)
	{
		if (m_storage)
		{
			this_size += m_size*sizeof(*m_storage)+40;  // malloc overhead
		}
		if (used && sizes)
		{
			used[m_used]++;
			sizes[m_size]++;
		}
		for (unsigned i = 0; i < m_size; i++)
		{
			if (m_storage[i])
			{
				sub_object_sizes += m_storage[i]->EstimateMemoryUsage(used, sizes);
			}
		}
	}
	else
	{
		OP_ASSERT(m_used == 0 && !m_single_node || m_used == 1 && m_single_node);
		if (m_used == 1)
		{
			sub_object_sizes += m_single_node->EstimateMemoryUsage(used, sizes);
		}
	}
	return this_size + sub_object_sizes;
}

unsigned DomainPartNamesStorage::EstimateMemoryUsage() const
{
	return sizeof(*this)+sizeof(*m_storage)*m_size + 40; // malloc overhead
}

#endif // _DEBUG

OP_STATUS DomainPartNamesStorage::Add(const char* domain_part_name, unsigned domain_part_len, unsigned& added_offset)
{
#if 0 // This costs ~150 ms and gains a couple of KB. Not worth it

	// Try to reuse the string storage
	const char* name = m_storage;
	const char* name_end = m_storage+m_used;
	while (name < name_end)
	{
		int len = op_strlen(name);
		if (len == domain_part_len && op_strncmp(domain_part_name, name, len) == 0)
		{
			added_offset = (name - m_storage)/sizeof(*m_storage);
			return OpStatus::OK;
		}
		name += len+1;
	}
#endif // 0

	// new string - allocate room for it
	unsigned buffer_size_needed = domain_part_len + 1+ m_used;
	if (m_size < buffer_size_needed)
	{
		unsigned new_size = m_size ? (m_size * 2): 2048;
		while (new_size < buffer_size_needed)
		{
			new_size*= 2;
		}

		char* new_storage = OP_NEWA(char, new_size);
		if (!new_storage)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		if (m_used)
		{
			op_memcpy(new_storage, m_storage, sizeof(*m_storage)*m_used);
		}
		OP_DELETEA(m_storage);
		m_storage = new_storage;
		m_size = new_size;
	}

	op_memcpy(m_storage + m_used, domain_part_name, domain_part_len*sizeof(*m_storage));
	m_storage[m_used+domain_part_len] = '\0';
	added_offset = m_used;
	m_used += domain_part_len + 1;
	return OpStatus::OK;
}

void DomainPartNamesStorage::Compact()
{
	char* new_storage = NULL;
	if (m_used)
	{
		new_storage = OP_NEWA(char, m_used);
		if (!new_storage)
		{
			return;
		}
		op_memcpy(new_storage, m_storage, sizeof(*m_storage)*m_used);
	}
	OP_DELETEA(m_storage);
	m_storage = new_storage;
	m_size = m_used;
}



DomainPartToNodeMap::~DomainPartToNodeMap()
{
	if (m_size > 0)
	{
		// m_storage[i] is owned by the arena
		OP_DELETEA(m_storage);
	}
	else
	{
		OP_ASSERT(m_used == 0 && !m_single_node || m_used == 1 && m_single_node);
	}
}

OP_STATUS DomainPartToNodeMap::Add(const DomainPartNamesStorage* domain_part_names, PublicDomainTreeNode* node)
{
	if (m_size == 0 && m_used == 0)
	{
		// Use the possibility to store a single child without allocating seperate memory
		m_single_node = node;
		m_used = 1;
		return OpStatus::OK;
	}

	if ((m_used+1) * 4 > m_size * 3) // load <= 3/4 after insertion
	{
		RETURN_IF_ERROR(Grow(domain_part_names));
	}
	OP_ASSERT((m_used+1)*4 <= m_size * 3);

	UINT32 hash = Hash(node->GetDomainPartName(domain_part_names));
	unsigned pos = hash % m_size;
	UINT32 secondary_hash = 6*hash + 1; // Has to be relative prime to the size, I will keep sizes at multiples of 2. This is odd, i.e. relative prime.
	while (m_storage[pos])
	{
		pos = (pos + secondary_hash) % m_size;
	}
	m_storage[pos] = node;
	m_used++;
	
	OP_ASSERT(m_used < m_size); // Need a NULL somewhere in m_storage to get GetData fast
	return OpStatus::OK;
}

PublicDomainTreeNode* DomainPartToNodeMap::GetData(const DomainPartNamesStorage* domain_part_names, const char* key) const
{
	if (m_size)
	{
		OP_ASSERT(m_used < m_size); // So that we will find a NULL somewhere and not stay here forever
		UINT32 hash = Hash(key);
		unsigned pos = hash % m_size;
		UINT32 secondary_hash = 6*hash + 1; // Has to be relative prime to the size, I will keep sizes at multiples of 2. This is odd, i.e. relative prime.
		while (m_storage[pos])
		{
			if (op_strcmp(m_storage[pos]->GetDomainPartName(domain_part_names), key) == 0)
			{
				return m_storage[pos];
			}

			pos = (pos + secondary_hash) % m_size;
		}
	}
	else
	{
		OP_ASSERT(m_used == 0 && !m_single_node || m_used == 1);
		if (m_used)
		{
			if (op_strcmp(m_single_node->GetDomainPartName(domain_part_names), key) == 0)
			{
				return m_single_node;
			}
		}
	}
	return NULL;
}

OP_STATUS DomainPartToNodeMap::Grow(const DomainPartNamesStorage* domain_part_names)
{
	unsigned new_size = m_size ? 2*m_size : 4; // Keep it at multiples of 2 to make secondary hash easy
	PublicDomainTreeNode** new_storage = OP_NEWA(PublicDomainTreeNode*, new_size);
	if (!new_storage)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	for (unsigned j = 0; j < new_size; j++)
	{
		new_storage[j] = NULL;
	}

	if (m_size == 0 && m_used == 1)
	{
		// Using the single pointer
		UINT32 hash = Hash(m_single_node->GetDomainPartName(domain_part_names));
		unsigned pos = hash % new_size;
		new_storage[pos] = m_single_node;
	}
	else
	{
		for (unsigned i = 0; i < m_size; i++)
		{
			if (m_storage[i])
			{
				UINT32 hash = Hash(m_storage[i]->GetDomainPartName(domain_part_names));
				unsigned pos = hash % new_size;
				UINT32 secondary_hash = 6*hash + 1; // Has to be relative prime to the size, I will keep sizes at multiples of 2. This is odd, i.e. relative prime.
				while (new_storage[pos])
				{
					pos = (pos + secondary_hash) % new_size;
				}
				new_storage[pos] = m_storage[i];
			}
		}
		OP_DELETEA(m_storage);
	}

	m_size = new_size;
	m_storage = new_storage;

	return OpStatus::OK;
}

PublicDomainTreeNode*  PublicDomainTreeNodeArena::Allocate(int domain_part_offset)
{
	if (m_next_free == m_end)
	{
		// Need a new block
		int count = 250; // update EstimateMemoryUsage and GetFirstAndEndOnBlock if this changes
		size_t size = count*sizeof(PublicDomainTreeNode)+sizeof(ArenaBlockHeader);
		char* new_mem = OP_NEWA(char, size);
		if (!new_mem)
		{
			return NULL; // OOM
		}
		ArenaBlockHeader* new_block = reinterpret_cast<ArenaBlockHeader*>(new_mem);
		new_block->next = m_first_block;
		PublicDomainTreeNode* first_node;
		PublicDomainTreeNode* end_node;
		GetFirstAndEndOnBlock(new_block, first_node, end_node);

		m_first_block = new_block;
		m_next_free = first_node;
		m_end = end_node;
	}
	OP_ASSERT(m_next_free != m_end);

	PublicDomainTreeNode* node = m_next_free++;
	// Run the constructor
	char* node_mem = reinterpret_cast<char*>(node);
	return new (node_mem) PublicDomainTreeNode(domain_part_offset);
}

void PublicDomainTreeNodeArena::GetFirstAndEndOnBlock(ArenaBlockHeader* block, PublicDomainTreeNode*& first, PublicDomainTreeNode*&end)
{
	int count = 250;
	char* mem = reinterpret_cast<char*>(block);
	char* first_block_mem = mem + sizeof(ArenaBlockHeader);
	first = reinterpret_cast<PublicDomainTreeNode*>(first_block_mem);
	end = first + count;
}

#ifdef _DEBUG
unsigned PublicDomainTreeNodeArena::EstimateMemoryUsage() const
{
	int count = 250;
	size_t block_mem_use = count*sizeof(PublicDomainTreeNode)+sizeof(ArenaBlockHeader) + 40; // malloc overhead
	unsigned this_size = sizeof(*this);
	unsigned block_count = 0;
	ArenaBlockHeader* block = m_first_block;
	while (block)
	{
		block_count++;
		block = block->next;
	}
	unsigned block_sizes = block_count*block_mem_use;
	return this_size + block_sizes;
}
#endif // _DEBUG


UINT32 DomainPartToNodeMap::Hash(const char* str)
{
	return static_cast<UINT32>(djb2hash(str));
}

#endif // PUBLIC_DOMAIN_LIST
