/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef PARSER_NODE_H
#define PARSER_NODE_H

#include "adjunct/ui_parser/yaml/yaml.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "modules/util/OpHashTable.h"


typedef yaml_node_item_t ParserNodeID;			//< Avoiding to use YAML names directly.

/**
 * @brief Storing hashtable key and value in one struct.
 *
 * It might seem that the @a key member is redundant since it's a copy of a
 * piece of data from the document.  However, if we want to use the existing
 * string hash table, we need NUL-terminated strings to make comparisons.
 */
struct ParserNodeIDTableData
{
	ParserNodeIDTableData(ParserNodeID _data_id) : data_id(_data_id) {}

	OpString8			key;
	const ParserNodeID	data_id;
};


typedef OpAutoString8HashTable<ParserNodeIDTableData>	ParserNodeIDTable;		//< Table with quick access to ParserNodeID's. E.g. to store all parameters of a node.
typedef String8HashIterator<ParserNodeIDTableData>		ParserNodeIDIterator;	//< Iterating over ParserNodeIDTable

class ParserDocument;
class ParserNodeScalar;
class ParserNodeSequence;
class ParserNodeMapping;

/**
 * @brief Wrapper class for a YAML node.
 */
class ParserNode
{
public:
	/** Creates an empty node. */
	ParserNode() : m_node(0), m_document(0) {}

	/** Creates a valid node
	  * @param node Internal yaml node
	  * @param document Document this node is taken from
	  */
	ParserNode(yaml_node_t * node, ParserDocument * document) : m_node(node), m_document(document) { OP_ASSERT(node && document); }

	BOOL IsEmpty() const { return !m_node; }

	/**
	 * Get a node by ID
	 * @see ParserDocument::GetNodeByID
	 */
	BOOL GetChildNodeByID(ParserNodeID id, ParserNode& node);
	BOOL GetChildNodeByID(ParserNodeID id, ParserNodeScalar& node);
	BOOL GetChildNodeByID(ParserNodeID id, ParserNodeMapping& node);
	BOOL GetChildNodeByID(ParserNodeID id, ParserNodeSequence& node);

protected:
	yaml_node_t *		m_node;		//< The YAML node that data is retrieved from.
	ParserDocument *	m_document;	//< The document this node is part of. Used to retrieve other nodes by ID.
};

class ParserNodeScalar : public ParserNode
{
public:
	ParserNodeScalar() {}
	ParserNodeScalar(yaml_node_t* node, ParserDocument* document) : ParserNode(node, document)
		{ OP_ASSERT(node->type == YAML_SCALAR_NODE); }

	/**
	 * Retrieves string value from node
	 * @return Error if OOM or empty node
	 */
	OP_STATUS		GetString(OpString8& string) const;

	/** @return Whether contents of scalar are equal to a string
	  * @param string String to compare with
	  */
	BOOL			Equals(const OpStringC8& string) const;
};

class ParserNodeMapping : public ParserNode
{
public:
	ParserNodeMapping() {}
	ParserNodeMapping(yaml_node_t* node, ParserDocument* document) : ParserNode(node, document)
		{ OP_ASSERT(node->type == YAML_MAPPING_NODE); }

	/**
	 * Reads all key/value pairs from a hash node and stores them in the hash table provided.
	 * Hash table is emptied before being filled.
	 * @return	Err if OOM or empty node.
	 */
	OP_STATUS		GetHashTable(ParserNodeIDTable & hash_table) const;

	/**
	 * Traverses through all key/value pairs and returns the node where key == key.
	 * @param key Key to search for
	 * @return ID of retrieved value, or 0 if not found
	 */
	ParserNodeID	FindValueForKey(const OpStringC8& key) const;

private:
	OP_STATUS		GetHashTableInternal(ParserNodeIDTable & hash_table) const;
};

class ParserNodeSequence : public ParserNode
{
public:
	ParserNodeSequence() {}
	ParserNodeSequence(yaml_node_t* node, ParserDocument* document) : ParserNode(node, document)
		{ OP_ASSERT(node->type == YAML_SEQUENCE_NODE); }

private:
	friend class ParserSequenceIterator;
};

#endif // PARSER_NODE_H
