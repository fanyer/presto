/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/ui_parser/ParserNode.h"
#include "adjunct/ui_parser/ParserIterators.h"


////////// GetChildNodeByID
BOOL ParserNode::GetChildNodeByID(ParserNodeID id, ParserNode& node)
{
	return m_document ? m_document->GetNodeByID(id, node) : FALSE;
}

BOOL ParserNode::GetChildNodeByID(ParserNodeID id, ParserNodeScalar& node)
{
	return m_document ? m_document->GetNodeByID(id, node) : FALSE;
}

BOOL ParserNode::GetChildNodeByID(ParserNodeID id, ParserNodeMapping& node)
{
	return m_document ? m_document->GetNodeByID(id, node) : FALSE;
}

BOOL ParserNode::GetChildNodeByID(ParserNodeID id, ParserNodeSequence& node)
{
	return m_document ? m_document->GetNodeByID(id, node) : FALSE;
}


////////// GetString
OP_STATUS
ParserNodeScalar::GetString(OpString8 & string) const
{
	if (IsEmpty())
		return OpStatus::ERR;

	return string.Set(m_node->data.scalar.value, m_node->data.scalar.length);
}


////////// Equals
BOOL
ParserNodeScalar::Equals(const OpStringC8& string) const
{
	if (IsEmpty() || string.IsEmpty())
		return FALSE;
	const char* string_buffer = string.CStr();
	return !string.Compare(m_node->data.scalar.value, m_node->data.scalar.length) &&
			string_buffer[m_node->data.scalar.length] == '\0';
}


////////// GetHashTable
OP_STATUS
ParserNodeMapping::GetHashTable(ParserNodeIDTable & hash_table) const
{
	if (IsEmpty())
		return OpStatus::ERR;

	hash_table.RemoveAll();

	return GetHashTableInternal(hash_table);
}


OP_STATUS
ParserNodeMapping::GetHashTableInternal(ParserNodeIDTable & hash_table) const
{
	for (yaml_node_pair_t* pair = m_node->data.mapping.pairs.top - 1; pair >= m_node->data.mapping.pairs.start; pair--)
	{
		if (m_document->IsMergeNode(pair->key))
		{
			ParserNodeMapping merged;
			if (m_document->GetNodeByID(pair->value, merged))
			{
				RETURN_IF_ERROR(merged.GetHashTableInternal(hash_table));
			}
		}
		else
		{
			ParserNodeScalar key_node;
			if (m_document->GetNodeByID(pair->key, key_node))
			{
				OpAutoPtr<ParserNodeIDTableData> value (OP_NEW(ParserNodeIDTableData, (pair->value)));
				if (!value.get())
					return OpStatus::ERR_NO_MEMORY;
				RETURN_IF_ERROR(key_node.GetString(value->key));
				if (hash_table.Contains(value->key.CStr()))
					continue;
				RETURN_IF_ERROR(hash_table.Add(value->key.CStr(), value.get()));
				value.release();
			}
		}
	}

	return OpStatus::OK;
}


////////// FindValueForKey
ParserNodeID
ParserNodeMapping::FindValueForKey(const OpStringC8& key) const
{
	if (IsEmpty())
		return OpStatus::ERR;

	for (yaml_node_pair_t* pair = m_node->data.mapping.pairs.top - 1; pair >= m_node->data.mapping.pairs.start; pair--)
	{
		if (m_document->IsMergeNode(pair->key))
		{
			ParserNodeMapping merged;
			ParserNodeID value;
			if (m_document->GetNodeByID(pair->value, merged) &&
				(value = merged.FindValueForKey(key)))
				return value;
		}
		else
		{
			ParserNodeScalar key_node_scalar;
			if (m_document->GetNodeByID(pair->key, key_node_scalar) &&
				key_node_scalar.Equals(key))
			{
				return pair->value;
			}
		}
	}

	return 0;
}
